/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"     /* for NULL */
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "errno.h"
#include "fcntl.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

#define INODE_DEBUG
#ifdef INODE_DEBUG
#define DEB(x) printl("ext2fs: "); x
#else
#define DEB(x)
#endif

PRIVATE int rw_inode(ext2_inode_t * inode, int rw_flag);

PUBLIC void ext2_init_inode()
{
    int i;
    for (i = 0; i < EXT2_INODE_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&ext2_inode_table[i]);
    }
}

PRIVATE void ext2_addhash_inode(ext2_inode_t * inode)
{
    unsigned int hash = inode->i_num & EXT2_INODE_HASH_MASK;

    list_add(&(inode->list), &ext2_inode_table[hash]);
}

PRIVATE void ext2_unhash_inode(ext2_inode_t * inode)
{
    list_del(&(inode->list));
}

PUBLIC ext2_inode_t * get_ext2_inode(dev_t dev, ino_t num)
{
    int hash = num & EXT2_INODE_HASH_MASK;

    ext2_inode_t * inode;
    /* first look for it in the hash table */
    list_for_each_entry(inode, &ext2_inode_table[hash], list) {
        if ((inode->i_num == num) && (inode->i_dev == dev)) {   /* hit */
            ++inode->i_count;
            return inode;
        }
    }

    /* not found. allocate one */
    inode = (ext2_inode_t *)alloc_mem(sizeof(ext2_inode_t));
    assert(inode != NULL);
    DEB(printl("Allocated inode at 0x%x\n", (unsigned int)inode));

    /* fill the inode */
    inode->i_dev = dev;
    inode->i_num = num;
    inode->i_count = 1;
    rw_inode(inode, DEV_READ);  /* get it from the disk */

    /* add it to hash table */
    ext2_addhash_inode(inode);
    inode->i_dirt = INO_CLEAN;
    return inode;
}

PUBLIC ext2_inode_t * find_ext2_inode(dev_t dev, ino_t num)
{
    int hash = num & EXT2_INODE_HASH_MASK;

    ext2_inode_t * inode;
    list_for_each_entry(inode, &ext2_inode_table[hash], list) {
        if ((inode->i_num == num) && (inode->i_dev == dev)) {   /* hit */
            return inode;
        }
    }
    return NULL;
}

PUBLIC void put_ext2_inode(ext2_inode_t * pin)
{
    if (!pin) return;
    if (pin->i_count < 1) panic("ext2fs: put_inode: pin->i_count already < 1");
    /* no one is using it */
    if ((--pin->i_count) == 0) {
        if (pin->i_dirt == INO_DIRTY) rw_inode(pin, DEV_WRITE);
        ext2_unhash_inode(pin);
        free_mem((int)pin, sizeof(ext2_inode_t));
    }
}

PRIVATE void update_times(ext2_inode_t * pin)
{
    if (pin->i_update & CTIME) pin->i_ctime = now();
    if (pin->i_update & MTIME) pin->i_mtime = now();
    if (pin->i_update & ATIME) pin->i_atime = now();
    pin->i_update = 0;
}

PUBLIC int ext2_rw_inode(ext2_inode_t * inode, int rw_flag)
{
    return rw_inode(inode, rw_flag);
}

PRIVATE int rw_inode(ext2_inode_t * inode, int rw_flag)
{
    dev_t dev = inode->i_dev;

    ext2_superblock_t * psb = get_ext2_super_block(dev);
    inode->i_sb = psb;
    int desc_num = (inode->i_num - 1) / psb->sb_inodes_per_group;
    ext2_bgdescriptor_t * bgdesc = get_ext2_group_desc(psb, desc_num);

    if (bgdesc == NULL) panic("ext2fs: Can't get block descriptor to read/write inode");

    int offset = ((inode->i_num - 1) % psb->sb_inodes_per_group) * EXT2_INODE_SIZE(psb);

    block_t block_nr = (block_t) bgdesc->inode_table + (offset >> psb->sb_blocksize_bits);
    /* read the inode table */
    ext2_buffer_t * pb = ext2_get_buffer(dev, block_nr);
    if (!pb) return err_code;

    if (rw_flag == DEV_READ) {
        memcpy(inode, (void*)(pb->b_data + offset), EXT2_INODE_SIZE(psb));
    } else if (rw_flag == DEV_WRITE) {
        if (inode->i_update) update_times(inode);
        memcpy((void*)(pb->b_data + offset), inode, EXT2_INODE_SIZE(psb));
        pb->b_dirt = 1;
        /* write back to the device */
    } else return EINVAL;

    ext2_put_buffer(pb);

    return 0;
}
