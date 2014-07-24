#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#define	PROT_NONE	0x00	/* no permissions */
#define	PROT_READ	0x01	/* pages can be read */
#define	PROT_WRITE	0x02	/* pages can be written */
#define	PROT_EXEC	0x04	/* pages can be executed */

/*
 * Flags contain sharing type and options.
 * Sharing types; choose one.
 */
#define	MAP_SHARED	0x0001	/* share changes */
#define	MAP_PRIVATE	0x0002	/* changes are private */

/*
 * Mapping type
 */
#define MAP_ANON	0x0004  /* anonymous memory */

#define MAP_FIXED	0x0008

/*
 * Error indicator returned by mmap(2)
 */
#define	MAP_FAILED	((void *) -1)	/* mmap() failed */

#define MMAP_WHO	u.m5.m5i1
#define MMAP_OFFSET	u.m5.m5i2
#define MMAP_LEN	u.m5.m5i3
#define MMAP_DEV	u.m5.m5i4
#define MMAP_INO	u.m5.m5i5
#define MMAP_FD		u.m5.m5i6
#define MMAP_VADDR	u.m5.m5i7
#define MMAP_FLAGS	u.m5.m5i8
#define MMAP_PROT	u.m5.m5i9
#define MMAP_RETADDR u.m5.m5i10

PUBLIC int vfs_mmap(endpoint_t who, off_t offset, size_t len,
    dev_t dev, ino_t ino, int fd, int vaddr, int flags);
PUBLIC void * mmap_for(endpoint_t forwhom,
	void *addr, size_t len, int prot, int flags, int fd, off_t offset);

#endif