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
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <errno.h>
#include <lyos/portio.h>
#include <arch_proto.h>
    
PRIVATE char vportio_buf[VPORTIO_BUF_SIZE];
PRIVATE pb_pair_t * pbp = (pb_pair_t *)vportio_buf;
PRIVATE pw_pair_t * pwp = (pw_pair_t *)vportio_buf;
PRIVATE pl_pair_t * plp = (pl_pair_t *)vportio_buf;

PUBLIC int sys_vportio(MESSAGE * m, struct proc * p_proc)
{
    int type = m->PIO_REQUEST & PIO_TYPE_MASK, dir = m->PIO_REQUEST & PIO_DIR_MASK;
    int vec_size = m->PIO_VECSIZE;

    if (vec_size <= 0) return EINVAL;
    
    int bytes;
    switch (type) {
        case PIO_BYTE:
            bytes = sizeof(pb_pair_t) * vec_size;
            break;
        case PIO_WORD:
            bytes = sizeof(pw_pair_t) * vec_size;
            break;
        case PIO_LONG:
            bytes = sizeof(pl_pair_t) * vec_size;
            break;
        default:
            return EINVAL;
    }

    if (bytes > VPORTIO_BUF_SIZE) return E2BIG;

    /* copy port-value pairs */
    memcpy(vportio_buf, m->PIO_BUF, bytes);

    int i;
    switch (type) {
        case PIO_BYTE:
            for (i = 0; i < vec_size; i++) {
                if (dir == PIO_IN) pbp[i].value = in_byte(pbp[i].port);
                else out_byte(pbp[i].port, pbp[i].value);
            }
            break;
        case PIO_WORD:
            for (i = 0; i < vec_size; i++) {
                if (dir == PIO_IN) pwp[i].value = in_word(pwp[i].port);
                else out_word(pwp[i].port, pwp[i].value);
            }
            break;
        case PIO_LONG:
            for (i = 0; i < vec_size; i++) {
                if (dir == PIO_IN) plp[i].value = in_long(plp[i].port);
                else out_long(plp[i].port, plp[i].value);
            }
            break;
        default:
            return EINVAL;
    }

    if (dir == PIO_IN) {
        memcpy(m->PIO_BUF, vportio_buf, bytes);
    }

    return 0;
}
