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
#include "errno.h"
#include "stdio.h"
#include "stddef.h"
#include <stdarg.h>
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/sysutils.h>
#include "libmemfs/libmemfs.h"
#include "global.h"
#include "proto.h"

PUBLIC struct proc proc[NR_TASKS + NR_PROCS];
PUBLIC struct pmproc pmproc[NR_PROCS];

PRIVATE int slot_in_use(int slot)
{
    return !(proc[slot].state & PST_FREE_SLOT);
}

PRIVATE int update_proc_table()
{
    return get_proctab(proc);
}

PRIVATE int update_pmproc_table()
{
    return pm_getinfo(PM_INFO_PROCTAB, pmproc, sizeof(pmproc));
}

PRIVATE int update_tables()
{
    int retval;

    if ((retval = update_proc_table()) != 0) return retval;

    if ((retval = update_pmproc_table()) != 0) return retval;

    return 0;
}

PRIVATE void make_stat(struct memfs_stat* stat, int slot, int index)
{
    if (index == NO_INDEX) 
        stat->st_mode = (I_DIRECTORY | 0755);
    else
        stat->st_mode = pid_files[index].mode;
    stat->st_uid = SU_UID;
    stat->st_gid = 0;
}

PRIVATE void build_pid_dirs()
{
    struct memfs_inode* root = memfs_get_root_inode();
    char name[NAME_MAX + 1];

    int i;
    for (i = 0; i < NR_PROCS + NR_TASKS; i++) {
        if (!slot_in_use(i)) continue;

        if (memfs_find_inode_by_index(root, i) != NULL) continue;

        pid_t pid;
        if (i < NR_TASKS) {
            pid = i - NR_TASKS;
        } else {
            pid = pmproc[i - NR_TASKS].pid;
        }

        snprintf(name, NAME_MAX + 1, "%d", pid);

        struct memfs_stat stat;
        make_stat(&stat, i, NO_INDEX);

        memfs_add_inode(root, name, i, &stat, (cbdata_t) pid);
    }
}

PRIVATE int is_pid_dir(struct memfs_inode* pin)
{
    return (memfs_node_parent(pin) == memfs_get_root_inode()) && (memfs_node_index(pin) != NO_INDEX);
}

PRIVATE void build_one_pid_entry(struct memfs_inode* parent, char* name, int slot)
{
    if (memfs_find_inode_by_name(parent, name)) return;

    int i;
    for (i = 0; pid_files[i].name != NULL; i++) {
        if (!strcmp(pid_files[i].name, name)) {
            struct memfs_stat stat;
            make_stat(&stat, slot, i);

            memfs_add_inode(parent, name, i, &stat, (cbdata_t) 0);
        }
    }
}

PRIVATE void build_pid_entries(struct memfs_inode* pin, char* name)
{
    int slot = memfs_node_index(pin);

    if (name) {
        build_one_pid_entry(pin, name, slot);
    }
}

PUBLIC int procfs_lookup_hook(struct memfs_inode* parent, char* name, cbdata_t data)
{
    static clock_t last_update = 0;

    clock_t now;
    get_ticks(&now, NULL);

    if (last_update != now) {
        update_tables();

        last_update = now;
    }

    if (parent == memfs_get_root_inode()) {
        build_pid_dirs();
    } else if (is_pid_dir(parent)) {
        build_pid_entries(parent, name);
    }

    return 0;
}
