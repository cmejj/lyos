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

#include <lyos/type.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/driver.h>
#include <lyos/proto.h>
    
#include <libdevman/libdevman.h>

PRIVATE endpoint_t driver_table[NR_DEVICES];
PRIVATE endpoint_t self_ep = NO_TASK;

PUBLIC void bdev_init()
{
    int i;
    for (i = 0; i < NR_DEVICES; i++) driver_table[i] = NO_TASK;
}

PRIVATE void bdev_set(dev_t dev)
{
    dev_t major = MAJOR(dev);

    driver_table[major] = dm_get_bdev_driver(dev);

    if (driver_table[major] < 0) driver_table[major] = NO_TASK;
}

PRIVATE void bdev_update(dev_t dev)
{
    bdev_set(dev);
}

PUBLIC int bdev_driver(dev_t dev)
{
    static int first = 1;

    self_ep = get_endpoint();

    if (first) {
        bdev_init();
    }

    bdev_update(dev);

    return 0;
}

/*****************************************************************************
 *                                bdev_sendrec
 *****************************************************************************/
/**
 * Send a message to and receive the reply from the corresponding driver.
 * 
 * @param dev      device nr
 * @param msg      Ptr to the message
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int bdev_sendrec(dev_t dev, MESSAGE* msg)
{
    dev_t major = MAJOR(dev);
    if (driver_table[major] == NO_TASK) bdev_update(dev);

    return send_recv(BOTH, driver_table[major], msg);
}

/*****************************************************************************
 *                                bdev_open
 *****************************************************************************/
/**
 * Open the device.
 * 
 * @param dev      device nr
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int bdev_open(dev_t dev)
{
    MESSAGE driver_msg;

    driver_msg.type = BDEV_OPEN;
    driver_msg.DEVICE = MINOR(dev);

    bdev_sendrec(dev, &driver_msg);

    return driver_msg.RETVAL;
}

/*****************************************************************************
 *                                bdev_close
 *****************************************************************************/
/**
 * Close the device.
 * 
 * @param dev      device nr
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int bdev_close(dev_t dev)
{
    MESSAGE driver_msg;

    driver_msg.type = BDEV_OPEN;
    driver_msg.DEVICE = MINOR(dev);

    bdev_sendrec(dev, &driver_msg);

    return driver_msg.RETVAL;
}

/*****************************************************************************
 *                                bdev_readwrite
 *****************************************************************************/
/**
 * R/W a sector via messaging with the corresponding driver.
 * 
 * @param io_type  DEV_READ or DEV_WRITE
 * @param dev      device nr
 * @param pos      Byte offset from/to where to r/w.
 * @param bytes    r/w count in bytes.
 * @param proc_nr  To whom the buffer belongs.
 * @param buf      r/w buffer.
 * 
 * @return Bytes read/wrote or a negative error code.
 *****************************************************************************/
PUBLIC ssize_t bdev_readwrite(int io_type, int dev, u64 pos, int bytes, int proc_nr,
             void* buf)
{
    MESSAGE driver_msg;
    
    if (proc_nr == SELF) proc_nr = self_ep;

    driver_msg.type     = io_type;
    driver_msg.DEVICE   = MINOR(dev);
    driver_msg.POSITION = pos;
    driver_msg.BUF      = buf;
    driver_msg.CNT      = bytes;
    driver_msg.PROC_NR  = proc_nr;

    bdev_sendrec(dev, &driver_msg);

    return driver_msg.RETVAL;
}
