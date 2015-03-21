/*  
    (c)Copyright 2011 Jimx
    
    This file is part of Lyos.

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
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/service.h>
#include "libmemfs/libmemfs.h"

PRIVATE int init_procfs();

struct memfs_hooks fs_hooks = {
};

PUBLIC int main()
{
    serv_register_init_fresh_callback(init_procfs);
    serv_init();

	struct memfs_stat root_stat;
    root_stat.st_mode = (I_DIRECTORY | 0555);
    root_stat.st_uid = SU_UID;
    root_stat.st_gid = 0;

    return memfs_start("proc", &fs_hooks, &root_stat);
}

PRIVATE int init_procfs()
{
	printl("procfs: procfs is running.\n");

    return 0;
}
