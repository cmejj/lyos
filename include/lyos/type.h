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

#ifndef _TYPE_H_
#define _TYPE_H_

/* routine types */
#define	PUBLIC		/* PUBLIC is the opposite of PRIVATE */
#define	PRIVATE	static	/* PRIVATE x limits the scope of x */

#define EXTERN extern

typedef	unsigned long long	u64;
PRIVATE inline u64 make64(unsigned long hi, unsigned long lo)
{
    return ((u64)hi << 32) | (u64)lo;
}
PRIVATE inline unsigned long ex64lo(u64 i)
{
	return (unsigned long)i;
}

PRIVATE inline unsigned long ex64hi(u64 i)
{
	return (unsigned long)(i>>32);
}

typedef int 				s32;
typedef	unsigned int		u32;
typedef	unsigned short		u16;
typedef	unsigned char		u8;

typedef unsigned int 		phys_bytes;
typedef unsigned int 		vir_bytes;
//#define __dev_t_defined
//typedef unsigned int        __dev_t;
typedef unsigned int        block_t;

typedef int 				endpoint_t;
typedef unsigned long 		priv_map_t;

typedef u32					bitchunk_t;

typedef	void	(*int_handler)	();
typedef	void	(*task_f)	();

typedef unsigned long irq_id_t;
typedef unsigned long irq_hook_id_t;
typedef unsigned long irq_policy_t;
typedef struct irq_hook{
    int irq;
    irq_hook_id_t id;
    int (*handler)(struct irq_hook *);
    struct irq_hook * next;
    endpoint_t	proc_ep;
    irq_hook_id_t notify_id;
    irq_policy_t irq_policy;
} irq_hook_t;

typedef	int 	(*irq_handler_t)	(irq_hook_t * irq_hook);

typedef u16 port_t;

struct hole {
	struct hole * h_next;
	int h_base;
	int h_len;
};

struct vir_addr {
	vir_bytes addr;
	endpoint_t proc_ep;
};

/**
 * MESSAGE mechanism is borrowed from MINIX
 */
struct mess1 {	/* 16 bytes */
	int m1i1;
	int m1i2;
	int m1i3;
	int m1i4;
};
struct mess2 {	/* 16 bytes */
	void* m2p1;
	void* m2p2;
	void* m2p3;
	void* m2p4;
};
struct mess3 {	/* 40 bytes */
	int	m3i1;
	int	m3i2;
	int	m3i3;
	int	m3i4;
	u64	m3l1;
	u64	m3l2;
	void*	m3p1;
	void*	m3p2;
};
struct mess4 {	/* 36 bytes */
	u64 m4l1;
	int m4i1, m4i2, m4i3; 
	void *m4p1, *m4p2, *m4p3, *m4p4; 
};
struct mess5 {	/* 40 bytes */
	int	m5i1;
	int	m5i2;
	int	m5i3;
	int	m5i4;
	int	m5i5;
	int	m5i6;
	int m5i7;
	int m5i8;
	int m5i9;
	int m5i10;
};

typedef struct {
	int source;
	int type;
	union {
		struct mess1 m1;
		struct mess2 m2;
		struct mess3 m3;
		struct mess4 m4;
		struct mess5 m5;
	} u;
} MESSAGE;

struct proc;
typedef int (*sys_call_handler_t)(MESSAGE * m, struct proc * p_proc);

struct boot_proc {
	int proc_nr;
	char name[16];
	endpoint_t endpoint;
	phys_bytes base;
	phys_bytes len;
};

struct ps_strings {
	char	*ps_argvstr;	/* first of 0 or more argument strings */
	int	ps_nargvstr;	/* the number of argument strings */
	char	*ps_envstr;	/* first of 0 or more environment strings */
	int	ps_nenvstr;	/* the number of environment strings */
};

struct siginfo {
	int mask;
	int signo;
	vir_bytes sig_handler;
	vir_bytes sig_return;
	vir_bytes stackptr;
};

#endif /* _TYPE_H_ */
