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
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/driver.h"
#include <errno.h>
#include <lyos/portio.h>
#include <lyos/interrupt.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <pci.h>
#include <sys/mman.h>

#include "libblockdriver/libblockdriver.h"
#include <libdevman/libdevman.h>

#include "ata.h"

PRIVATE int		init_hd				();
PRIVATE int     hd_open				(dev_t minor, int access);
PRIVATE int     hd_close			(dev_t minor);
PRIVATE ssize_t hd_rdwt				(dev_t minor, int do_write, u64 pos,
                                    endpoint_t endpoint, char* buf, unsigned int count);
PRIVATE int     hd_ioctl			(dev_t minor, int request, endpoint_t endpoint, char* buf);
PRIVATE void	hd_cmd_out			(struct hd_cmd* cmd);
PRIVATE void	get_part_table		(int drive, int sect_nr, struct part_ent * entry);
PRIVATE void	partition			(int device, int style);
PRIVATE void	print_hdinfo		(struct ata_info * hdi);
PRIVATE int		waitfor				(int mask, int val, int timeout);
PRIVATE int		waitfor_dma			(int mask, int val, int timeout);
PRIVATE void	interrupt_wait		();
PRIVATE int     interrupt_wait_check();
PRIVATE	int		hd_identify			(int drive);
PRIVATE void	print_identify_info	(u16* hdinfo);
PRIVATE void    register_hd			(struct ata_info * hdi);
PRIVATE void    start_dma			(struct ata_info* drive, int do_write);
PRIVATE void    stop_dma			(struct ata_info* drive);
PRIVATE int     setup_dma			(int do_write, endpoint_t endpoint, vir_bytes buf, unsigned int count);

PRIVATE	u8		hdbuf[SECTOR_SIZE * 2];
PRIVATE	struct ata_info	hd_info[MAX_DRIVES], *current_drive;
PRIVATE struct part_info* current_part;

#define select_drive(drive) do { current_drive = &hd_info[drive]; } while(0)

#define	DRV_OF_DEV(dev) (dev / NR_SUB_PER_DRIVE)

PRIVATE int         dma_disabled = 0;

/* Physical Region Descriptor Table Entry */
struct prdte
{
    phys_bytes prdte_base;
    u16 prdte_count;
    u8 prdte_reserved;
    u8 prdte_flags;
};
#define NUM_PRDTES		1024
#define PRDT_SIZE		(sizeof(struct prdte) * NUM_PRDTES)
PRIVATE struct prdte*   prdt;
PRIVATE phys_bytes      prdt_phys;
#define PRDTE_FL_EOT	0x80	/* End of PRDT */

// #define ATA_DEBUG
#ifdef ATA_DEBUG
#define DEB(x) printl("ata: "); x
#else
#define DEB(x)
#endif

#define ERR_BAD_SECTOR 1
#define ERR_OTHER      2

int getpagesize();
unsigned int _page_size;

struct blockdriver hd_driver = {
    .bdr_open = hd_open,
    .bdr_close = hd_close,
    .bdr_readwrite = hd_rdwt,
    .bdr_ioctl = hd_ioctl,
};

/*****************************************************************************
 *                                task_hd
 *****************************************************************************/
/**
 * Main loop of HD driver.
 *
 *****************************************************************************/
PUBLIC int main(int argc, char** argv)
{
    serv_register_init_fresh_callback(init_hd);
    serv_init();

    blockdriver_task(&hd_driver);

    return 0;
}

PRIVATE struct part_info* hd_prepare(dev_t device)
{
    int drive = DRV_OF_DEV(device);

    if (drive >= MAX_DRIVES) return NULL;

    int part_no = MINOR(device) % NR_SUB_PER_DRIVE;
    current_part = part_no < NR_PRIM_PER_DRIVE ?
        &hd_info[drive].primary[part_no] :
        &hd_info[drive].logical[part_no - NR_PRIM_PER_DRIVE];

    select_drive(drive);

    return current_part;
}

/*****************************************************************************
 *                                init_drive
 *****************************************************************************/
/**
 * <Ring 1> Initialize and try to identify a hard drive.
 *****************************************************************************/
PRIVATE int init_drive(int drive, int base_cmd, int base_ctl, int base_dma,
    int native, int slave, int hook)
{
    hd_info[drive].state = 0;

    hd_info[drive].base_cmd = base_cmd;
    hd_info[drive].base_ctl = base_ctl;
    hd_info[drive].base_dma = base_dma;
    hd_info[drive].native = native;
    hd_info[drive].irq_hook = hook;
    hd_info[drive].ldh = (slave << 4) | 0xA0;
    hd_info[drive].dma = 0;

    hd_info[drive].open_cnt = 0;

    return hd_identify(drive);
}

/*****************************************************************************
 *                                init_hd
 *****************************************************************************/
/**
 * <Ring 1> Check hard drive, set IRQ handler, enable IRQ and initialize data
 *          structures.
 *****************************************************************************/
PRIVATE int init_hd()
{
    int i;

    for (i = 0; i < MAX_DRIVES; i++) {
        hd_info[i].state = IGNORING;
    }

    if (!dma_disabled) {
        /* setup DMA buffer */
        prdt = mmap(NULL, PRDT_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_CONTIG|MAP_POPULATE, -1, 0);
        if (prdt == MAP_FAILED) {
            dma_disabled = 1;
            printl("ata: failed to allocate PRDT\n");
        }
        if (umap(SELF, prdt, &prdt_phys) != 0) {
            dma_disabled = 1;
        }
    }

    _page_size = getpagesize();

    u8 nr_drives = 0;

    int devind;
    u16 vid, did;

    int retval = pci_first_dev(&devind, &vid, &did);
    while (retval == 0) {
        u8 baseclass, subclass, interface;

        baseclass = pci_attr_r8(devind, PCI_BCR);
        subclass = pci_attr_r8(devind, PCI_SCR);
        interface = pci_attr_r8(devind, PCI_PIFR);

        int irq = pci_attr_r8(devind, PCI_ILR);
        int irq_hook = 0;
        int native_hook, compat_hook;

        int ide = (baseclass == PCI_BCR_MASS_STORAGE && subclass == PCI_MS_IDE);

        if (!ide || interface & (PCI_IDE_PRI_NATIVE | PCI_IDE_SEC_NATIVE)) {
            native_hook = irq_hook++;
            if (irq_setpolicy(irq, IRQ_REENABLE, &native_hook) != 0) panic("register IRQ failed");
            if (irq_enable(&native_hook) != 0) panic("can't enable native IRQ");
        }

        u32 base_cmd, base_ctl, base_dma;
        base_dma = pci_attr_r32(devind, PCI_BAR_5) & PCI_BAR_IO_MASK;
        if (!ide || interface & PCI_IDE_PRI_NATIVE) {
            base_cmd = pci_attr_r32(devind, PCI_BAR) & PCI_BAR_IO_MASK;
            base_ctl = pci_attr_r32(devind, PCI_BAR_2) & PCI_BAR_IO_MASK;

            if (init_drive(nr_drives, base_cmd, base_ctl, base_dma, 1, 0, native_hook)) nr_drives++;
            if (init_drive(nr_drives, base_cmd, base_ctl, base_dma, 1, 1, native_hook)) nr_drives++;
        } else {
            compat_hook = irq_hook++;
            if (irq_setpolicy(AT_WINI_IRQ, IRQ_REENABLE, &compat_hook) != 0) panic("register IRQ failed");
            if (irq_enable(&compat_hook) != 0) panic("can't enable compatible IRQ");

            if (init_drive(nr_drives, REG_CMD_BASE0, REG_CTL_BASE0, base_dma, 0, 0, compat_hook)) nr_drives++;
            if (init_drive(nr_drives, REG_CMD_BASE0, REG_CTL_BASE0, base_dma, 0, 1, compat_hook)) nr_drives++;
        }

        if (base_dma) base_dma += PCI_DMA_2ND_OFF;
        if (!ide || interface & PCI_IDE_SEC_NATIVE) {
            base_cmd = pci_attr_r32(devind, PCI_BAR_3) & PCI_BAR_IO_MASK;
            base_ctl = pci_attr_r32(devind, PCI_BAR_4) & PCI_BAR_IO_MASK;

            if (init_drive(nr_drives, base_cmd, base_ctl, base_dma, 1, 0, native_hook)) nr_drives++;
            if (init_drive(nr_drives, base_cmd, base_ctl, base_dma, 1, 1, native_hook)) nr_drives++;
        } else {
            compat_hook = irq_hook++;
            if (irq_setpolicy(AT_WINI_1_IRQ, IRQ_REENABLE, &compat_hook) != 0) panic("register IRQ failed");
            if (irq_enable(&compat_hook) != 0) panic("can't enable compatible IRQ");

            //if (init_drive(nr_drives, REG_CMD_BASE1, REG_CTL_BASE1, base_dma, 0, 0, compat_hook)) nr_drives++;
            if (init_drive(nr_drives, REG_CMD_BASE1, REG_CTL_BASE1, base_dma, 0, 1, compat_hook)) nr_drives++;

        }

        /* enable bus mastering */
        u32 cr = pci_attr_r16(devind, PCI_CR);
        if (!(cr & (1 << PCI_CR_MAST_EN))) {
            pci_attr_w16(devind, PCI_CR, (cr | PCI_CR_MAST_EN));
        }

        retval = pci_next_dev(&devind, &vid, &did);
    }

    printl("ata: %d hard drives\n", nr_drives);

    for (i = 0; i < nr_drives; i++) {
        if (hd_info[i].open_cnt++ == 0) {
            hd_prepare(i * NR_SUB_PER_DRIVE);

            partition(i * NR_SUB_PER_DRIVE, P_PRIMARY);
            print_hdinfo(current_drive);
            register_hd(current_drive);
        }
    }

    return 0;
}

/*****************************************************************************
 *                                hd_open
 *****************************************************************************/
/**
 * <Ring 1> This routine handles DEV_OPEN message. It identify the drive
 * of the given device and read the partition table of the drive if it
 * has not been read.
 *
 * @param device The device to be opened.
 *****************************************************************************/
PRIVATE int hd_open(dev_t minor, int access)
{
    hd_prepare(minor);

    current_drive->open_cnt++;

    return 0;
}

/*****************************************************************************
 *                                hd_close
 *****************************************************************************/
/**
 * <Ring 1> This routine handles DEV_CLOSE message.
 *
 * @param device The device to be opened.
 *****************************************************************************/
PRIVATE int hd_close(dev_t minor)
{
    hd_prepare(minor);

    current_drive->open_cnt--;

    return 0;
}

PRIVATE void start_dma(struct ata_info* drive, int do_write)
{
    u32 cmd = DMA_CMD_START | (do_write ? 0 : DMA_CMD_RW); /* clear Read/Write bit when writing to the disk */
    if (portio_outb(drive->base_dma + DMA_REG_CMD, cmd) != 0) {
        panic("ata: start_dma() failed");
    }
}

PRIVATE void stop_dma(struct ata_info* drive)
{
    if (portio_outb(drive->base_dma + DMA_REG_CMD, 0) != 0) {
        panic("ata: stop_dma() failed");
    }
}

PRIVATE int error_dma(struct ata_info* drive)
{
    int retval;
    u32 v = 0;

    retval = portio_inb(current_drive->base_dma + DMA_REG_STATUS, &v);
    if (retval != 0) panic("ata: error_dma(): sys_inb failed");

#define DMAERR(msg) \
    printl("ata: DMA error %s\n", msg);\
    return 1;

    if (!current_drive->dma_int) {
        if (v & DMA_ST_BM_ACTIVE) {
            DMAERR("DMA did not complete");
        } else if (v & DMA_ST_ERROR) {
            DMAERR("DMA error");
        } else {
            DMAERR("DMA buffer too small");
        }
    } else if ((v & DMA_ST_BM_ACTIVE)) {
        DMAERR("DMA buffer too large");
    }

    return 0;
}

PRIVATE int setup_dma(int do_write, endpoint_t endpoint, vir_bytes buf, unsigned int count)
{
    unsigned int n, offset;
    int prdt_idx = 0;
    phys_bytes user_phys;
    int retval;

    offset = 0;
    while (count > 0) {
        n = count;

        retval = umap(endpoint, buf, &user_phys);
        if (retval != 0) {
            panic("ata: setup_dma(): failed to map user buffer");
        }
        if (user_phys & 1) {
            printl("ata: setup_dma(): user buffer not aligned\n");
            return 0;
        }

        /* user buffer crosses pages or boundary of 64k */
        if (buf / _page_size != (buf + n - 1) / _page_size) {
            n = (buf / _page_size + 1) * _page_size - buf;
        }

        /* user buffer crosses boundary of 64k */
        if (user_phys / 0x10000 != (user_phys + n - 1) / 0x10000) {
            n = min(n, (user_phys / 0x10000 + 1) * 0x10000 - user_phys);
        }

        if (prdt_idx >= NUM_PRDTES) {
            printl("ata: setup_dma(): out of PRDT entries\n");
            return 0;
        }

        prdt[prdt_idx].prdte_base = user_phys;
        prdt[prdt_idx].prdte_count = n;
        prdt[prdt_idx].prdte_reserved = 0;
        prdt[prdt_idx].prdte_flags = 0;
        prdt_idx++;

        offset += n;
        buf += n;
        count -= n;
    }

    if (prdt_idx > NUM_PRDTES || prdt_idx <= 0) {
        panic("ata: setup_dma() bad number of PRDT entries: %d", prdt_idx);
    }
    prdt[prdt_idx - 1].prdte_flags |= PRDTE_FL_EOT;

#ifdef ATA_DEBUG
    int i;
    for (i = 0; i < prdt_idx; i++) {
        printl("ata: prdt[%d]: base 0x%lx, size %d, flags 0x%x\n",
            i, prdt[i].prdte_base, prdt[i].prdte_count,
            prdt[i].prdte_flags);
    }
#endif

    u32 dma_status;
    retval = portio_inb(current_drive->base_dma + DMA_REG_STATUS, &dma_status);
    if (retval != 0) panic("ata: setup_dma(): portio_inb failed");
    if (dma_status & DMA_ST_BM_ACTIVE) panic("ata: setup_dma(): Bus Master IDE active");

    if (prdt_phys & 3) panic("ata: setup_dma(): PRDT not aligned");
    retval = portio_outl(current_drive->base_dma + DMA_REG_PRDTP, prdt_phys);
    if (retval != 0) panic("ata: setup_dma(): portio_outl failed");

    retval = portio_outb(current_drive->base_dma + DMA_REG_STATUS, DMA_ST_INT | DMA_ST_ERROR);
    if (retval != 0) panic("ata: setup_dma(): portio_outb failed");

    return 1;
}

/*****************************************************************************
 *                                hd_rdwt
 *****************************************************************************/
/**
 * <Ring 1> This routine handles DEV_READ and DEV_WRITE message.
 *
 * @param p Message ptr.
 *****************************************************************************/
PRIVATE ssize_t hd_rdwt(dev_t minor, int do_write, u64 pos,
                                    endpoint_t endpoint, char* buf, unsigned int count)
{
    hd_prepare(minor);

    assert((pos >> SECTOR_SIZE_SHIFT) < (1 << 31));

    /**
     * We only allow to R/W from a SECTOR boundary:
     */
    assert((pos & 0x1FF) == 0);

    int do_dma = current_drive->dma;
    int errors = 0;

dma_failed_retry:
    if (do_dma) {
        stop_dma(current_drive);
        if (!setup_dma(do_write, endpoint, (vir_bytes) buf, count)) {
            do_dma = 0;
        }
    }

    u32 sect_nr = (u32)(pos >> SECTOR_SIZE_SHIFT); /* pos / SECTOR_SIZE */
    sect_nr += current_part->base;

    struct hd_cmd cmd;
    cmd.features	= 0;
    cmd.count	= count >> SECTOR_SIZE_SHIFT;
    cmd.lba_low	= sect_nr & 0xFF;
    cmd.lba_mid	= (sect_nr >>  8) & 0xFF;
    cmd.lba_high	= (sect_nr >> 16) & 0xFF;
    cmd.device	= MAKE_DEVICE_REG(1, current_drive->ldh, (sect_nr >> 24) & 0xF);
    if (do_dma) {
        cmd.command	= do_write ? ATA_WRITE_DMA : ATA_READ_DMA;
    } else {
        cmd.command	= do_write ? ATA_WRITE : ATA_READ;
    }
    hd_cmd_out(&cmd);

    if (do_dma) {
        start_dma(current_drive, do_write);
    }

    if (do_write) {
        u32 status;
        if (portio_inb(current_drive->base_ctl + REG_ALT_STATUS, &status) != 0) {
            panic("ata: hd_rdwt(): portio_inb failed");
        }
    }

    int bytes_left = count;
    int bytes_rdwt = 0;

    if (do_dma) {
        current_drive->dma_int = 0;

        int err = interrupt_wait_check();
        if (err) {
            errors ++;
            if (err == ERR_BAD_SECTOR || errors > 3) {
                return -EIO;
            }
            goto dma_failed_retry;
        }

        if (!current_drive->dma_int) {
            waitfor_dma(DMA_ST_INT, DMA_ST_INT, HD_TIMEOUT);
            current_drive->dma_int = 1;
        }

        if (error_dma(current_drive)) {
            do_dma = 0;
            goto dma_failed_retry;
        }

        stop_dma(current_drive);

        bytes_rdwt += bytes_left;
        return bytes_rdwt;
    }

    while (bytes_left) {
        int bytes = min(SECTOR_SIZE, bytes_left);
        if (!do_write) {
            interrupt_wait();
            portio_sin(current_drive->base_cmd + REG_DATA, hdbuf, bytes);
            data_copy(endpoint, buf, SELF, hdbuf, bytes);
        }
        else {
            if (!waitfor(STATUS_DRQ, STATUS_DRQ, HD_TIMEOUT))
                panic("hd writing error.");

            data_copy(SELF, hdbuf, endpoint, buf, bytes);
            portio_sout(current_drive->base_cmd + REG_DATA, hdbuf, bytes);
            interrupt_wait();
        }
        bytes_left -= bytes;
        buf += bytes;
        bytes_rdwt += bytes;
    }
    return bytes_rdwt;
}

/*****************************************************************************
 *                                hd_ioctl
 *****************************************************************************/
/**
 * <Ring 1> This routine handles the DEV_IOCTL message.
 *
 * @param p  Ptr to the MESSAGE.
 *****************************************************************************/
PRIVATE int hd_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf)
{
    hd_prepare(minor);

    if (request == DIOCTL_GET_GEO) {
        data_copy(endpoint, buf, SELF, current_part, sizeof(struct part_info));
    }
    else {
        return EINVAL;
    }

    return 0;
}

/*****************************************************************************
 *                                get_part_table
 *****************************************************************************/
/**
 * <Ring 1> Get a partition table of a drive.
 *
 * @param drive   Drive nr (0 for the 1st disk, 1 for the 2nd, ...)n
 * @param sect_nr The sector at which the partition table is located.
 * @param entry   Ptr to part_ent struct.
 *****************************************************************************/
PRIVATE void get_part_table(int drive, int sect_nr, struct part_ent * entry)
{
    struct hd_cmd cmd;
    cmd.features	= 0;
    cmd.count	= 1;
    cmd.lba_low	= sect_nr & 0xFF;
    cmd.lba_mid	= (sect_nr >>  8) & 0xFF;
    cmd.lba_high	= (sect_nr >> 16) & 0xFF;
    cmd.device	= MAKE_DEVICE_REG(1, /* LBA mode*/
                      current_drive->ldh,
                      (sect_nr >> 24) & 0xF);
    cmd.command	= ATA_READ;
    hd_cmd_out(&cmd);
    interrupt_wait();

    portio_sin(current_drive->base_cmd + REG_DATA, hdbuf, SECTOR_SIZE);
    memcpy(entry,
           hdbuf + PARTITION_TABLE_OFFSET,
           sizeof(struct part_ent) * NR_PART_PER_DRIVE);
}

/*****************************************************************************
 *                                partition
 *****************************************************************************/
/**
 * <Ring 1> This routine is called when a device is opened. It reads the
 * partition table(s) and fills the hd_info struct.
 *
 * @param device Device nr.
 * @param style  P_PRIMARY or P_EXTENDED.
 *****************************************************************************/
PRIVATE void partition(int device, int style)
{
    int i;
    int drive = DRV_OF_DEV(device);
    struct ata_info * hdi = &hd_info[drive];

    struct part_ent part_tbl[NR_SUB_PER_DRIVE];

    if (style == P_PRIMARY) {
        get_part_table(drive, drive, part_tbl);

        int nr_prim_parts = 0;
        for (i = 0; i < NR_PART_PER_DRIVE; i++) { /* 0~3 */
            if (part_tbl[i].sys_id == NO_PART)
                continue;

            nr_prim_parts++;
            int dev_nr = i + 1;       /* 1~4 */
            hdi->primary[dev_nr].base = part_tbl[i].start_sect;
            hdi->primary[dev_nr].size = part_tbl[i].nr_sects;

            if (part_tbl[i].sys_id == EXT_PART) /* extended */
                partition(device + dev_nr, P_EXTENDED);
        }
        assert(nr_prim_parts != 0);
    }
    else if (style == P_EXTENDED) {
        int j;
        /* find first free slot for logical partition */
        for (j = 0; j < NR_SUB_PER_DRIVE; j++) {
            if (hdi->logical[j].size == 0) break;
        }
        int k = device % NR_PRIM_PER_DRIVE; /* 1~4 */
        int ext_start_sect = hdi->primary[k].base;
        int s = ext_start_sect;
        int nr_1st_sub = j;

        for (i = 0; i < NR_SUB_PER_PART; i++) {
            int dev_nr = nr_1st_sub + i;

            get_part_table(drive, s, part_tbl);

            hdi->logical[dev_nr].base = s + part_tbl[0].start_sect;
            hdi->logical[dev_nr].size = part_tbl[0].nr_sects;

            s = ext_start_sect + part_tbl[1].start_sect;

            /* no more logical partitions
               in this extended partition */
            if (part_tbl[1].sys_id == NO_PART)
                break;
        }
    }
    else {
        assert(0);
    }
}

/*****************************************************************************  */
/*                                print_hdinfo									*/
/*****************************************************************************  */
/* <Ring 1> Print disk info.													*/
/*  *																			*/
/*  * @param hdi  Ptr to struct ata_info.										*/
/*  *****************************************************************************/
PRIVATE void print_hdinfo(struct ata_info * hdi)
 {
    int i;
    printl("Hard disk information:\n");
    for (i = 0; i < NR_PART_PER_DRIVE + 1; i++) {
        if (hdi->primary[i].size == 0)
            continue;
        printl("%spartition %d: base %d(0x%x), size %d(0x%x) (in sector)\n",
           i == 0 ? "  " : "     ",
               i,
               hdi->primary[i].base,
               hdi->primary[i].base,
               hdi->primary[i].size,
               hdi->primary[i].size);
    }
    for (i = 0; i < NR_SUB_PER_DRIVE; i++) {
        if (hdi->logical[i].size == 0)
            continue;
        printl("              "
               "%d: base %d(0x%x), size %d(0x%x) (in sector)\n",
               i,
               hdi->logical[i].base,
               hdi->logical[i].base,
               hdi->logical[i].size,
               hdi->logical[i].size);
    }
}

/*****************************************************************************  */
/*                                register_hd									*/
/*****************************************************************************  */
PRIVATE void register_hd(struct ata_info * hdi)
 {
    int i, drive = (hdi - hd_info) + 1;
    for (i = 0; i < NR_PART_PER_DRIVE + 1; i++) {
        if (hdi->primary[i].size == 0)
            continue;
        dm_bdev_add(MAKE_DEV(DEV_HD, (drive - 1) * NR_SUB_PER_DRIVE + i));
    }
    for (i = 0; i < NR_SUB_PER_DRIVE; i++) {
        if (hdi->logical[i].size == 0)
            continue;
        dm_bdev_add(MAKE_DEV(DEV_HD, (drive - 1) * NR_SUB_PER_DRIVE + NR_PRIM_PER_DRIVE + i));
    }
}

/*****************************************************************************
/ *                                hd_identify                               *
* ****************************************************************************/
/**
 * <Ring 1> Get the disk information.
 *
 * @param drive  Drive Nr.
 *****************************************************************************/
PRIVATE int hd_identify(int drive)
{
    struct hd_cmd cmd;
    memset(&cmd, sizeof(cmd), 0);

    select_drive(drive);

    cmd.device  = current_drive->ldh;
    cmd.command = ATA_IDENTIFY;
    hd_cmd_out(&cmd);

    u8 status;
    portio_inb(current_drive->base_cmd + REG_STATUS, &status);
    if (!status) return 0;	/* this drive does not exist */

    if (waitfor(STATUS_DRQ, STATUS_DRQ, HD_TIMEOUT) &&
         !(current_drive->status & (STATUS_DFSE | STATUS_ERR))) {
        portio_sin(current_drive->base_cmd + REG_DATA, hdbuf, SECTOR_SIZE);

        u16* hdinfo = (u16*)hdbuf;
        printl("ata: hd%d: ", drive);
        print_identify_info(hdinfo);

        current_drive->primary[0].base = 0;
        /* Total Nr of User Addressable Sectors */
        current_drive->primary[0].size = ((int)hdinfo[61] << 16) + hdinfo[60];

        /* check DMA */
        int capabilities = hdinfo[ID_CAPABILITIES];
        int id_dma = capabilities & ID_CAP_DMA;
        if (id_dma && current_drive->base_dma) {
            current_drive->dma = !dma_disabled;
        }

        current_drive->state |= IDENTIFIED;
        return 1;
    }

    return 0;
}

/*****************************************************************************
 *                            print_identify_info
 *****************************************************************************/
/**
 * <Ring 1> Print the hdinfo retrieved via ATA_IDENTIFY command.
 *
 * @param hdinfo  The buffer read from the disk i/o port.
 *****************************************************************************/
PRIVATE void print_identify_info(u16* hdinfo)
{
    int i, j, k;
    char s[64];

    struct iden_info_ascii {
        int idx;
        int len;
        char * desc;
    } iinfo[] = {{27, 40, "HD Model: "}, /* Serial number in ASCII */
             {10, 20, "HD Serial Number: "} /* Model number in ASCII */ };

    int sectors = ((int)hdinfo[61] << 16) + hdinfo[60];
    printl("%dMB ", sectors * 512 / 1000000);

    for (k = 0; k < sizeof(iinfo)/sizeof(iinfo[0]); k++) {
        char * p = (char*)&hdinfo[iinfo[k].idx];
        for (i = 0; i < iinfo[k].len/2; i++) {
            s[i*2+1] = *p++;
            s[i*2] = *p++;
        }
        s[i*2] = 0;
        /*printl(iinfo[k].desc);*/
        for (j = 0;j <= i*2; j++)
            if (s[j] != ' ') printl("%c", s[j]);
        printl(" ");
    }

    int capabilities = hdinfo[ID_CAPABILITIES];
    if (capabilities & ID_CAP_LBA) printl("LBA ");
    if (capabilities & ID_CAP_DMA) printl("DMA ");

    int cmd_set_supported = hdinfo[83];
    if (cmd_set_supported & 0x0400) printl("LBA48 ");

    printl("\n");
}

/*****************************************************************************
 *                                hd_cmd_out
 *****************************************************************************/
/**
 * <Ring 1> Output a command to HD controller.
 *
 * @param cmd  The command struct ptr.
 *****************************************************************************/
PRIVATE void hd_cmd_out(struct hd_cmd* cmd)
{
    /**
     * For all commands, the host must first check if BSY=1,
     * and should proceed no further unless and until BSY=0
     */
    if (!waitfor(STATUS_BSY, 0, HD_TIMEOUT))
        panic("hd error.");

    pb_pair_t cmd_pairs[8];
    /* Activate the Interrupt Enable (nIEN) bit */
    pv_set(cmd_pairs[0], current_drive->base_ctl + REG_DEV_CTRL, 0);
    /* Load required parameters in the Command Block Registers */
    pv_set(cmd_pairs[1], current_drive->base_cmd + REG_FEATURES, cmd->features);
    pv_set(cmd_pairs[2], current_drive->base_cmd + REG_NSECTOR, cmd->count);
    pv_set(cmd_pairs[3], current_drive->base_cmd + REG_LBA_LOW, cmd->lba_low);
    pv_set(cmd_pairs[4], current_drive->base_cmd + REG_LBA_MID, cmd->lba_mid);
    pv_set(cmd_pairs[5], current_drive->base_cmd + REG_LBA_HIGH, cmd->lba_high);
    pv_set(cmd_pairs[6], current_drive->base_cmd + REG_DEVICE, cmd->device);

    /* Write the command code to the Command Register */
    pv_set(cmd_pairs[7], current_drive->base_cmd + REG_CMD, cmd->command);
    portio_voutb(cmd_pairs, 8);
}

/*****************************************************************************
 *                                interrupt_wait
 *****************************************************************************/
/**
 * <Ring 1> Wait until a disk interrupt occurs.
 *
 *****************************************************************************/
PRIVATE void interrupt_wait()
{
    MESSAGE msg;
    send_recv(RECEIVE, INTERRUPT, &msg);

    int i;
    struct ata_info* drive;
    for (i = 0; i < MAX_DRIVES; i++) {
        drive = &hd_info[i];
        if (drive->state & IGNORING) continue;

        portio_inb(drive->base_cmd + REG_STATUS, &drive->status);

        u32 dma_status = 0;
        portio_inb(drive->base_dma + DMA_REG_STATUS, &dma_status);
        if (dma_status & DMA_ST_INT) {
            portio_outb(drive->base_dma + DMA_REG_STATUS, DMA_ST_INT);
            drive->dma_int = 1;
        }
    }
}

/*****************************************************************************
 *                            interrupt_wait_check
 *****************************************************************************/
/**
 * <Ring 3> Wait until a disk interrupt occurs. Check the status and return error/success.
 *
 *****************************************************************************/
PRIVATE int interrupt_wait_check()
{
    interrupt_wait();

    if ((current_drive->status & (STATUS_BSY | STATUS_DFSE | STATUS_ERR)) == 0) {
        return 0;
    }

    u32 error;
    portio_inb(current_drive->base_cmd + REG_ERROR, &error);

    if ((current_drive->status & STATUS_ERR) && (error & ERROR_BAD_BLOCK)) {
        return ERR_BAD_SECTOR;
    }

    return ERR_OTHER;
}

/*****************************************************************************
 *                                waitfor
 *****************************************************************************/
/**
 * <Ring 1> Wait for a certain status.
 *
 * @param mask    Status mask.
 * @param val     Required status.
 * @param timeout Timeout in milliseconds.
 *
 * @return One if sucess, zero if timeout.
 *****************************************************************************/
PRIVATE int waitfor(int mask, int val, int timeout)
{
    while(TRUE) {
        u8 status;
        portio_inb(current_drive->base_cmd + REG_STATUS, &status);

        current_drive->status = status;

        if ((status & mask) == val)
            return 1;
    }

    return 0;
}

PRIVATE int waitfor_dma(int mask, int val, int timeout)
{
    while(TRUE) {
        u8 status;
        portio_inb(current_drive->base_dma + DMA_REG_STATUS, &status);

        if ((status & mask) == val)
            return 1;
    }

    return 0;
}
