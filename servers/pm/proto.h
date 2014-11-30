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

#ifndef _PM_PROTO_H_
#define _PM_PROTO_H_

PUBLIC pid_t find_free_pid();
PUBLIC int pm_verify_endpt(endpoint_t ep, int * proc_nr);

PUBLIC int do_fork(MESSAGE * p);
PUBLIC int do_wait(MESSAGE * p);
PUBLIC int do_exit(MESSAGE * p);

#endif