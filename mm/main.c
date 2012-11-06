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
#include "unistd.h"
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

PRIVATE int free_mem_size;
PRIVATE int paging_pages;
PRIVATE unsigned long * kernel_page_descs;

PUBLIC void do_fork_test();

PRIVATE void init_mm();

/*****************************************************************************
 *                                task_mm
 *****************************************************************************/
/**
 * <Ring 1> The main loop of TASK MM.
 * 
 *****************************************************************************/
PUBLIC void task_mm()
{
	init_mm();

	while (1) {
		send_recv(RECEIVE, ANY, &mm_msg);
		int src = mm_msg.source;
		int reply = 1;

		int msgtype = mm_msg.type;

		switch (msgtype) {
		case FORK:
			mm_msg.RETVAL = do_fork();
			break;
		case EXIT:
			do_exit(mm_msg.STATUS);
			reply = 0;
			break;
		case EXEC:
			mm_msg.RETVAL = do_exec();
			break;
		case WAIT:
			do_wait();
			reply = 0;
			break;
		case KILL:
			mm_msg.RETVAL = do_kill();
			break; 
		case RAISE:
			mm_msg.RETVAL = do_raise();
			break;
		case BRK:
			mm_msg.RETVAL = do_brk();
			break;
		case ACCT:
			mm_msg.RETVAL = do_acct();
			break;
		case GETUID:
			mm_msg.RETVAL = do_getuid();
			break;
		case SETUID:
            		mm_msg.RETVAL = do_setuid();
			break;
		case GETGID:
			mm_msg.RETVAL = do_getgid();
			break;
		case SETGID:
			mm_msg.RETVAL = do_setgid();
			break;
		case GETEUID:
			mm_msg.RETVAL = do_geteuid();
			break;
		case GETEGID:
			mm_msg.RETVAL = do_getegid();
			break;
		case SIGACTION:
			mm_msg.RETVAL = do_sigaction();
			break;
		case ALARM:
			mm_msg.RETVAL = do_alarm();
			break;
		case MALLOC:
			mm_msg.RETVAL = alloc_mem(mm_msg.CNT);
			break;
		default:
			dump_msg("MM::unknown msg", &mm_msg);
			assert(0);
			break;
		}

		if (reply) {
			mm_msg.type = SYSCALL_RET;
			send_recv(SEND, src, &mm_msg);
		}
	}
}

/*****************************************************************************
 *                                init_mm
 *****************************************************************************/
/**
 * Do some initialization work.
 *
 * Memory info is collected from boot_param, then set the buffer and ramdisk
 * area.
 *
 * 
 *****************************************************************************/
PRIVATE void init_mm()
{
	struct boot_params bp;
	get_boot_params(&bp);

	memory_size = bp.mem_size;
	kernel_addr = bp.kernel_file;
	
	printl("Memory:%dMB\n", memory_size / 1024 /1024);
	
	unsigned int k_base;
	unsigned int k_limit;
	int ret = get_kernel_map(&k_base, &k_limit);
	
	if (ret == 0)
		printl("Kernel memory: 0x%x - 0x%x(%dkB)\n",
					k_base, k_base + k_limit, k_limit/1024);
	
	/* int page_tbl_size = memory_size / 1024;
	buffer_base = (int)PAGE_TBL_BASE + page_tbl_size + (1024 * 1024);
	buffer_length = (2 * 1024 * 1024);
	init_buffer();
	
	rd_base = buffer_base + buffer_length + (256 * 1024);
	rd_length = 2 * 1024 * 1024;

	mem_start = rd_base + rd_length + (256 * 1024); */
	
	buffer_base = (unsigned char *)BUFFER_BASE;
	buffer_length = BUFFER_LENGTH;
	
#ifdef RAMDISK
	rd_base = (unsigned char *)RAMDISK_BASE;
	rd_length = RAMDISK_LENGTH;
#endif

	mem_start = PROCS_BASE;
	free_mem_size = memory_size - mem_start;
	printl("Free memory:%dMB\n", free_mem_size / 1024 / 1024);

	paging_pages = memory_size / PAGE_SIZE;
	printl("%d pages\n", paging_pages);

	/* initialize hole table */
	mem_init(mem_start, free_mem_size);
}

unsigned long get_user_pages(unsigned long len)
{
    unsigned long i, j, start;
    j = 0;
    for (i = 0; i < (paging_pages * PAGE_SIZE); i++)
    {
        if (kernel_page_descs[i] & KERNEL_USER_USED_MASK)  // used?
        {
            j = 0;
        }
        else
        {
            if (j == 0)
            {
                start = i;
            }
            j++;
            if (j >= len)
            {
                break;
            }
        }
    }
    if (j == 0)
    {
        return 0;
    }
    for (i = start; i < (start + j); i++)
    {
        kernel_page_descs[i] |= KERNEL_USER_USED_MASK;
    }
    return start * PAGE_SIZE;
}

unsigned long free_user_pages(unsigned long start, unsigned long len)
{
    unsigned long i;
    unsigned long pgstart = start / PAGE_SIZE;
    for (i = pgstart; i < (pgstart + len); i++)
    {
        kernel_page_descs[i] -= kernel_page_descs[i] & KERNEL_USER_USED_MASK;
    }
    return 0;
}

