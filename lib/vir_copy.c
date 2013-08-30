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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/const.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/proto.h"

PUBLIC int vir_copy(int src_pid, void * src, int dest_pid, void * dest, int len)
{
    MESSAGE m;

    m.SRC_PID = src_pid;
    m.SRC_ADDR = src;
    m.DEST_PID = dest_pid;
    m.DEST_ADDR = dest;
    m.BUF_LEN = len;

    return vircopy(&m);
}