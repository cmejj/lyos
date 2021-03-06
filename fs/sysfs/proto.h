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

#ifndef _SYSFS_PROTO_H_
#define _SYSFS_PROTO_H_

PUBLIC void init_node();
PUBLIC sysfs_node_t * new_node(char * name, int flags);
PUBLIC sysfs_node_t * find_node(sysfs_node_t * parent, char * name);
PUBLIC sysfs_node_t * lookup_node_by_name(char * name);
PUBLIC sysfs_node_t * create_node(char * name, int flags);

PUBLIC void init_buf(char* ptr, size_t len, off_t off);
PUBLIC void buf_printf(char * fmt, ...);
PUBLIC size_t buf_used();

PUBLIC ssize_t sysfs_read_hook(struct memfs_inode* inode, char* ptr, size_t count,
        off_t offset, cbdata_t data);
PUBLIC ssize_t sysfs_write_hook(struct memfs_inode* inode, char* ptr, size_t count,
        off_t offset, cbdata_t data);

#endif