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
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "termios.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"


/*****************************************************************************
 *                                clock_handler
 *****************************************************************************/
/**
 * <Ring 0> This routine handles the clock interrupt generated by 8253/8254
 *          programmable interval timer.
 * 
 * @param irq The IRQ nr, unused here.
 *****************************************************************************/
PUBLIC void clock_handler(int irq)
{	
	if (++jiffies >= MAX_TICKS)
		jiffies = 0;
     
	if (current->counter)
		current->counter--;

	if (key_pressed)
		inform_int(TASK_TTY);
}

/*****************************************************************************
 *                                milli_delay
 *****************************************************************************/
/**
 * <Ring 1~3> Delay for a specified amount of time.
 * 
 * @param milli_sec How many milliseconds to delay.
 *****************************************************************************/
PUBLIC void milli_delay(int milli_sec)
{
        int t = get_ticks();

        while(((get_ticks() - t) * 1000 / HZ) < milli_sec) {}
}

/*****************************************************************************
 *                                init_clock
 *****************************************************************************/
/**
 * <Ring 0> Initialize 8253/8254 PIT (Programmable Interval Timer).
 * 
 *****************************************************************************/
PUBLIC void init_clock()
{
        /* 初始化 8253 PIT */
        out_byte(TIMER_MODE, RATE_GENERATOR);
        out_byte(TIMER0, (u8) (TIMER_FREQ/HZ) );
        out_byte(TIMER0, (u8) ((TIMER_FREQ/HZ) >> 8));

        put_irq_handler(CLOCK_IRQ, clock_handler);    /* 设定时钟中断处理程序 */
        enable_irq(CLOCK_IRQ);                        /* 让8259A可以接收时钟中断 */
}


