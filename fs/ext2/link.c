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
#include "lyos/config.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "errno.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

PUBLIC int ext2_rdlink(dev_t dev, ino_t num, struct fsdriver_data * data, size_t * bytes)
{
    register int retval = 0;
    ext2_inode_t * pin = find_ext2_inode(dev, num);
    if (!pin) return EINVAL;

    char * text = NULL;

    ext2_buffer_t * pb = NULL;
    if (pin->i_size >= EXT2_MAX_FAST_SYMLINK_LENGTH) {
        block_t b = ext2_read_map(pin, 0);
        pb = ext2_get_buffer(dev, b);
        if (!pb) retval = EIO;
        else {
            text = (char *)pb->b_data;
            retval = 0;
        }
    } else {
        text = (char *)pin->i_block;
        retval = 0;
    }

    if (retval == 0) {
        if (*bytes >= pin->i_size) *bytes = pin->i_size;
        retval = fsdriver_copyout(data, 0, text, *bytes);
        if (pb) ext2_put_buffer(pb);
    }

    put_ext2_inode(pin);

    return retval;
}

/**
 * <Ring 1> Perform the FTRUNC syscall.
 * @param  p Ptr to the message.
 * @return   Zero on success.
 */
PUBLIC int ext2_ftrunc(dev_t dev, ino_t num, off_t start_pos, off_t end_pos)
{
    return 0;
}
