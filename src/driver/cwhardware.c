/****************************************************************************
 ****************************************************************************
 *
 * cwhardware.c
 *
 ****************************************************************************
 ****************************************************************************/





#include "config.h"	/* includes <linux/config.h> if needed */
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/version.h>

#include "cwhardware.h"
#include "cw.h"
#include "ioctl.h"



#define CW_NAME_MK3			CW_NAME "_mk3"
#define CW_NAME_MK4			CW_NAME "_mk4"

#define CW_BITW_DENSITY			0
#define CW_BITW_MOTOR0			1
#define CW_BITW_SELECT0			2
#define CW_BITW_SELECT1			3
#define CW_BITW_DIRECTION		4
#define CW_BITW_MOTOR1			5
#define CW_BITW_SIDE			6
#define CW_BITW_STEP			7
#define CW_BITR_DENSITY			8
#define CW_BITR_INDEX			9
#define CW_BITR_TRACK0			10
#define CW_BITR_WRITEPROTECT		11
#define CW_BITR_DISKREAD		12
#define CW_BITR_DISKCHANGED		13
#define CW_BITR_WRITING			14
#define CW_BITR_READING			15
#define CW_BITR_HOSTSELECT0		16
#define CW_BITR_HOSTSELECT1		17

#define CW_REG_DATADIR			0
#define CW_REG_SELECTBANK		1
#define CW_REG_INDIR			2
#define CW_REG_JOYDAT			3
#define CW_REG_CATMEM			4
#define CW_REG_CATABORT			5
#define CW_REG_CATCONTROL		6
#define CW_REG_CATCONTROL2		7
#define CW_REG_CATOPTION		8
#define CW_REG_CATSTARTA		9
#define CW_REG_CATSTARTB		10

#define CW_MK4_BANK_RESETFPGA		0x20
#define CW_MK4_BANK_COMPAT_MUX_OFF	0x41
#define CW_MK4_BANK_COMPAT_MUX_ON	0x61



/****************************************************************************
 * cwhardware_floppy_bit
 ****************************************************************************/
#define get_bit(bit)			cwhardware_floppy_bit(hrd, CW_BIT##bit)
#define get_mask(bit)			(1 << get_bit(bit))

static int
cwhardware_floppy_bit(
	struct cw_hardware		*hrd,
	int				bit)

	{
	if (hrd->model == CW_HARDWARE_MODEL_MK4)
		{
		if (bit == CW_BITR_HOSTSELECT0)  return (1);
		if (bit == CW_BITR_HOSTSELECT1)  return (0);
		}
	if ((hrd->model == CW_HARDWARE_MODEL_MK3) || (hrd->model == CW_HARDWARE_MODEL_MK4))
		{
		if (bit == CW_BITW_DENSITY)      return (0);
		if (bit == CW_BITW_MOTOR0)       return (1);
		if (bit == CW_BITW_SELECT0)      return (2);
		if (bit == CW_BITW_SELECT1)      return (3);
		if (bit == CW_BITW_DIRECTION)    return (4);
		if (bit == CW_BITW_MOTOR1)       return (5);
		if (bit == CW_BITW_SIDE)         return (6);
		if (bit == CW_BITW_STEP)         return (7);
		if (bit == CW_BITR_DENSITY)      return (0);
		if (bit == CW_BITR_INDEX)        return (1);
		if (bit == CW_BITR_TRACK0)       return (2);
		if (bit == CW_BITR_WRITEPROTECT) return (3);
		if (bit == CW_BITR_DISKREAD)     return (4);
		if (bit == CW_BITR_DISKCHANGED)  return (5);
		if (bit == CW_BITR_WRITING)      return (6);
		if (bit == CW_BITR_READING)      return (7);
		}

	/* should never be reached */

	cw_debug(1, "unknown bit requested");
	return (0);
	}



/****************************************************************************
 * cwhardware_floppy_register
 ****************************************************************************/
#define get_reg(reg)			cwhardware_floppy_register(hrd, CW_REG_##reg)

static int
cwhardware_floppy_register(
	struct cw_hardware		*hrd,
	int				reg)

	{
	if (hrd->model == CW_HARDWARE_MODEL_MK4)
		{
		if (reg == CW_REG_DATADIR)     return (hrd->iobase + 0x02);
		if (reg == CW_REG_SELECTBANK)  return (hrd->iobase + 0x03);
		if (reg == CW_REG_INDIR)       return (hrd->iobase + 0x07);
		if (reg == CW_REG_CATCONTROL2) return (hrd->iobase + 0xf8);
		}
	if ((hrd->model == CW_HARDWARE_MODEL_MK3) || (hrd->model == CW_HARDWARE_MODEL_MK4))
		{
		if (reg == CW_REG_JOYDAT)      return (hrd->iobase + 0xc0);
		if (reg == CW_REG_CATMEM)      return (hrd->iobase + 0xe0);
		if (reg == CW_REG_CATABORT)    return (hrd->iobase + 0xe4);
		if (reg == CW_REG_CATCONTROL)  return (hrd->iobase + 0xe8);
		if (reg == CW_REG_CATOPTION)   return (hrd->iobase + 0xec);
		if (reg == CW_REG_CATSTARTA)   return (hrd->iobase + 0xf0);
		if (reg == CW_REG_CATSTARTB)   return (hrd->iobase + 0xf4);
		}

	/* should never be reached */

	cw_debug(1, "unknown register requested");
	return (hrd->iobase + 0xff);
	}



#ifdef CONFIG_PCI
/****************************************************************************
 * cwhardware_mk3_mk4_remove
 ****************************************************************************/
static void
cwhardware_mk3_mk4_remove(
	struct pci_dev			*dev,
	char				*name)

	{
	struct cw_hardware		*hrd = (struct cw_hardware *) pci_get_drvdata(dev);

	cw_debug(1, "[c%d] unregistering %s from 0x%04x", hrd->cnt->num, name, hrd->iobase);
	cw_unregister_hardware(hrd);
	release_region(hrd->iobase, 256);
	pci_disable_device(dev);
	}



/****************************************************************************
 * cwhardware_mk3_mk4_probe
 ****************************************************************************/
static int
cwhardware_mk3_mk4_probe(
	struct pci_dev			*dev,
	const struct pci_device_id	*id,
	int				model,
	char				*name)

	{
	int				result, iobase;
	struct cw_hardware		*hrd;

	/* get next available controller struct */

	hrd = cw_get_hardware(-1);
	if (hrd == NULL)
		{
		cw_error("number of %d supported controllers exceeded", CW_MAX_CONTROLLERS);
		return (-ENODEV);
		}

	/* enable pci device */

	result = pci_enable_device(dev);
	if (result < 0) return (result);
	hrd->model = model;

	/* allocate io region */

	iobase = pci_resource_start(dev, 0);
	if (request_region(iobase, 256, name) == NULL)
		{
		pci_disable_device(dev);
		cw_error("[c%d] io ports 0x%04x-0x%04x already in use", hrd->cnt->num, iobase, iobase + 255);
		return (-EBUSY);
		}

	/* first stage of initialization finished */

	hrd->iobase           = iobase;
	hrd->control_register = 255;
	pci_set_drvdata(dev, hrd);
	cw_notice("[c%d] registering %s at 0x%04x", hrd->cnt->num, name, iobase);
	return (0);
	}



/****************************************************************************
 * cwhardware_mk3_remove
 ****************************************************************************/
static void
cwhardware_mk3_remove(
	struct pci_dev			*dev)

	{
	cwhardware_mk3_mk4_remove(dev, CW_NAME_MK3);
	}



/****************************************************************************
 * cwhardware_mk3_probe
 ****************************************************************************/
static int
cwhardware_mk3_probe(
	struct pci_dev			*dev,
	const struct pci_device_id	*id)

	{
	int				result;
	struct cw_hardware		*hrd;

	result = cwhardware_mk3_mk4_probe(dev, id, CW_HARDWARE_MODEL_MK3, CW_NAME_MK3);
	if (result < 0) return (result);
	hrd = (struct cw_hardware *) pci_get_drvdata(dev);

	/* magic PCI bridge initialization sequence for mk3 */

	cw_debug(1, "[c%d] sending magic PCI bridge sequence", hrd->cnt->num);
	outb(0xf1, hrd->iobase + 0x00);
	outb(0x00, hrd->iobase + 0x01);
	outb(0x00, hrd->iobase + 0x02);
	outb(0x00, hrd->iobase + 0x04);
	outb(0x00, hrd->iobase + 0x05);
	outb(0x00, hrd->iobase + 0x29);
	outb(0x00, hrd->iobase + 0x2b);

	/* all went fine controller is ready */

	cw_register_hardware(hrd);
	return (0);
	}



/****************************************************************************
 * cwhardware_mk3_pci_ids
 ****************************************************************************/
static struct pci_device_id		cwhardware_mk3_pci_ids[] =
	{
		{
		.vendor    = 0xe159,
		.device    = 0x0001,
		.subvendor = 0x1212,
		.subdevice = 0x0002
		},
		{ 0 }
	};
#if LINUX_VERSION_CODE >= 0x020600

/* multiple MODULE_DEVICE_TABLE entries seem to cause problems with 2.4 */

MODULE_DEVICE_TABLE(pci, cwhardware_mk3_pci_ids);
#endif /* LINUX_VERSION_CODE */



/****************************************************************************
 * cwhardware_mk3_pci_driver
 ****************************************************************************/
struct pci_driver			cwhardware_mk3_pci_driver =
	{
	.name     = CW_NAME_MK3,
	.id_table = cwhardware_mk3_pci_ids,
	.probe    = cwhardware_mk3_probe,
	.remove   = cwhardware_mk3_remove
	};



/****************************************************************************
 * cwhardware_mk4_remove
 ****************************************************************************/
static void
cwhardware_mk4_remove(
	struct pci_dev			*dev)

	{
	cwhardware_mk3_mk4_remove(dev, CW_NAME_MK4);
	}



/****************************************************************************
 * cwhardware_mk4_firmware
 ****************************************************************************/
static unsigned char __devinitdata	cwhardware_mk4_firmware[] =
	{
#include "cwfirmware.c"
	};



/****************************************************************************
 * cwhardware_mk4_firmware_upload
 ****************************************************************************/
static int
cwhardware_mk4_firmware_upload(
	struct cw_hardware		*hrd)

	{
	int				byte, bank, timeout;
	int				i = 0, version = 0xffff;
	unsigned char			memtest[] =
		{
		1, 2, 4, 8, 16, 32, 64, 128, 255, 44, 65, 3
		};

	/*
	 * determine firmware version, no version available if first bytes
	 * are 0xffff
	 */

	if (sizeof (cwhardware_mk4_firmware) > 1)
		{
		version = (cwhardware_mk4_firmware[0] << 8) | cwhardware_mk4_firmware[1];
		if (version != 0xffff) i = 2;
		}

	/*
	 * this code was written after looking into Dirk Jagdmann's
	 * cw_probe()-function
	 */

	cw_debug(1, "[c%d] uploading firmware version 0x%04x", hrd->cnt->num, version);
	for ( ; i < sizeof (cwhardware_mk4_firmware); i++)
		{

		/* set direction */

		byte = cwhardware_mk4_firmware[i];
		bank = (byte & 1) ? CW_MK4_BANK_COMPAT_MUX_ON + 2 : CW_MK4_BANK_COMPAT_MUX_ON;
		outb(bank, get_reg(SELECTBANK));

		/* wait for FPGA */

		for (timeout = 0; ! (inb(get_reg(INDIR)) & 0x08); timeout++)
			{
			udelay(1);
			if (timeout == 1000) return (-EBUSY);
			}

		/* write byte */

		outb(byte, get_reg(JOYDAT));
		}
	cw_debug(1, "[c%d] uploaded %d bytes", hrd->cnt->num, i);

	/* now wait until FPGA really comes alive */

	cw_debug(1, "[c%d] waiting for FPGA to come alive", hrd->cnt->num);
	for (timeout = 0; inb(get_reg(CATCONTROL)) == 0x0a; timeout++)
		{
		udelay(1);
		if (timeout == 1000) return (-EBUSY);
		}
	cw_debug(1, "[c%d] FPGA needed %d us to start up", hrd->cnt->num, timeout);

	/* check if memory access is working */

	cw_debug(1, "[c%d] testing memory access", hrd->cnt->num);
	outb(0x00, get_reg(CATABORT));
	for (i = 0; i < sizeof (memtest); i++) outb(memtest[i], get_reg(CATMEM));
	outb(0x00, get_reg(CATABORT));
	for (i = 0; i < sizeof (memtest); i++) if (inb(get_reg(CATMEM)) != memtest[i]) return (-EBUSY);
	cw_debug(1, "[c%d] memory test passed", hrd->cnt->num);

	return (0);
	}



/****************************************************************************
 * cwhardware_mk4_probe
 ****************************************************************************/
static int
cwhardware_mk4_probe(
	struct pci_dev			*dev,
	const struct pci_device_id	*id)

	{
	int				result;
	struct cw_hardware		*hrd;

	result = cwhardware_mk3_mk4_probe(dev, id, CW_HARDWARE_MODEL_MK4, CW_NAME_MK4);
	if (result < 0) return (result);
	hrd = (struct cw_hardware *) pci_get_drvdata(dev);

	/* magic PCI bridge initialization sequence for mk4 */

	cw_debug(1, "[c%d] sending magic PCI bridge sequence", hrd->cnt->num);
	outb(0xf1, hrd->iobase + 0x00);
	outb(0x00, hrd->iobase + 0x01);
	outb(0xe3, get_reg(DATADIR));
	outb(CW_MK4_BANK_COMPAT_MUX_ON, get_reg(SELECTBANK));
	outb(0x00, hrd->iobase + 0x04);
	outb(0x00, hrd->iobase + 0x05);
	outb(0x00, hrd->iobase + 0x29);
	outb(0x00, hrd->iobase + 0x2b);

	/* reset FPGA */

	cw_debug(1, "[c%d] resetting FPGA", hrd->cnt->num);
	outb(CW_MK4_BANK_RESETFPGA, get_reg(SELECTBANK));
	udelay(1000);
	outb(CW_MK4_BANK_COMPAT_MUX_ON, get_reg(SELECTBANK));

	/* upload firmware */

	result = cwhardware_mk4_firmware_upload(hrd);
	if (result < 0)
		{
		cw_error("[c%d] error while uploading firmware", hrd->cnt->num);
		cwhardware_mk4_remove(dev);
		return (result);
		}

	/* select mk3 compatible bank */

	cw_debug(1, "[c%d] finally selecting mk3 compat bank", hrd->cnt->num);
	outb(CW_MK4_BANK_COMPAT_MUX_ON, get_reg(SELECTBANK));

	/* all went fine controller is ready */

	cw_register_hardware(hrd);
	return (0);
	}



/****************************************************************************
 * cwhardware_mk4_pci_ids
 ****************************************************************************/
static struct pci_device_id		cwhardware_mk4_pci_ids[] =
	{

		/*
		 * the mk4 PCI bridge has a bug where the reported subdevice
		 * id may randomly change between 0x0002 and 0x0003
		 */

		{
		.vendor    = 0xe159,
		.device    = 0x0001,
		.subvendor = 0x5213,
		.subdevice = 0x0002
		},
		{
		.vendor    = 0xe159,
		.device    = 0x0001,
		.subvendor = 0x5213,
		.subdevice = 0x0003
		},
		{ 0 }
	};
#if LINUX_VERSION_CODE >= 0x020600

/* multiple MODULE_DEVICE_TABLE entries seem to cause problems with 2.4 */

MODULE_DEVICE_TABLE(pci, cwhardware_mk4_pci_ids);
#endif /* LINUX_VERSION_CODE */



/****************************************************************************
 * cwhardware_mk4_pci_driver
 ****************************************************************************/
struct pci_driver			cwhardware_mk4_pci_driver =
	{
	.name     = CW_NAME_MK4,
	.id_table = cwhardware_mk4_pci_ids,
	.probe    = cwhardware_mk4_probe,
	.remove   = cwhardware_mk4_remove
	};
#endif /* CONFIG_PCI */



/****************************************************************************
 * cwhardware_floppy_control_register
 ****************************************************************************/
#define cntr_reg(bit, set)		cwhardware_floppy_control_register(hrd, bit, set, 0)
#define cntr_reg_out(bit, set)		cwhardware_floppy_control_register(hrd, bit, set, 1)

static void
cwhardware_floppy_control_register(
	struct cw_hardware		*hrd,
	int				bit,
	int				set,
	int				out)

	{

	/*
	 * caller has to make sure that no concurrent access are made.
	 * assuming outb() is not atomic, so between reading a variable
	 * and really pushing it to the hardware register other things may
	 * happen
	 *
	 *              A               |              B
	 * -----------------------------+-----------------------------
	 * modify hrd->control_register |
	 * read hrd->control_register   |
	 *                              | modify hrd->control_register
	 *                              | read hrd->control_register
	 *                              | write to hardware
	 * write to hardware (would     |
	 * write outdated value)        |
	 */

	if (set) set_bit(bit, &hrd->control_register);
	else clear_bit(bit, &hrd->control_register);
	cw_debug(2, "[c%d] control_register = 0x%02lx", hrd->cnt->num, hrd->control_register);
	if (! out) return;
	cw_debug(2, "[c%d] writing 0x%02lx to hardware control_register", hrd->cnt->num, hrd->control_register);
	outb(hrd->control_register, get_reg(CATCONTROL));
	}



/****************************************************************************
 * cwhardware_floppy_host_select
 ****************************************************************************/
int
cwhardware_floppy_host_select(
	struct cw_hardware		*hrd)

	{
	int				creg, select = 3;

	if (hrd->model != CW_HARDWARE_MODEL_MK4) return (0);
	creg = inb(get_reg(CATCONTROL2));
	if (creg & get_mask(R_HOSTSELECT0)) select &= 2;
	if (creg & get_mask(R_HOSTSELECT1)) select &= 1;
	cw_debug(1, "[c%d] select = 0x%02x", hrd->cnt->num, select);
	return (select);
	}



/****************************************************************************
 * cwhardware_floppy_mux_on
 ****************************************************************************/
void
cwhardware_floppy_mux_on(
	struct cw_hardware		*hrd)

	{
	cw_debug(1, "[c%d] mux on", hrd->cnt->num);
	if (hrd->model != CW_HARDWARE_MODEL_MK4) return;
	outb(CW_MK4_BANK_COMPAT_MUX_ON, get_reg(SELECTBANK));
	}



/****************************************************************************
 * cwhardware_floppy_mux_off
 ****************************************************************************/
void
cwhardware_floppy_mux_off(
	struct cw_hardware		*hrd)

	{
	cw_debug(1, "[c%d] mux off", hrd->cnt->num);
	if (hrd->model != CW_HARDWARE_MODEL_MK4) return;
	outb(CW_MK4_BANK_COMPAT_MUX_OFF, get_reg(SELECTBANK));
	}



/****************************************************************************
 * cwhardware_floppy_motor_on
 ****************************************************************************/
void
cwhardware_floppy_motor_on(
	struct cw_hardware		*hrd,
	int				floppy)

	{
	cw_debug(1, "[c%d] motor on, floppy = %d", hrd->cnt->num, floppy);
	cntr_reg_out(floppy ? get_bit(W_MOTOR1) : get_bit(W_MOTOR0), 0);
	}



/****************************************************************************
 * cwhardware_floppy_motor_off
 ****************************************************************************/
void
cwhardware_floppy_motor_off(
	struct cw_hardware		*hrd,
	int				floppy)

	{
	cw_debug(1, "[c%d] motor off, floppy = %d", hrd->cnt->num, floppy);
	cntr_reg(floppy ? get_bit(W_MOTOR1) : get_bit(W_MOTOR0), 1);
	cntr_reg_out(floppy ? get_bit(W_SELECT1) : get_bit(W_SELECT0), 1);
	}



/****************************************************************************
 * cwhardware_floppy_select
 ****************************************************************************/
void
cwhardware_floppy_select(
	struct cw_hardware		*hrd,
	int				floppy,
	int				side,
	int				density)

	{
	cw_debug(1, "[c%d] select, floppy = %d, side = %d, density = %d", hrd->cnt->num, floppy, side, density);
	cntr_reg(get_bit(W_SELECT0), floppy ? 1 : 0);
	cntr_reg(get_bit(W_SELECT1), floppy ? 0 : 1);
	cntr_reg(get_bit(W_SIDE), side ? 0 : 1);
	cntr_reg_out(get_bit(W_DENSITY), density ? 0 : 1);
	}



/****************************************************************************
 * cwhardware_floppy_step
 ****************************************************************************/
void
cwhardware_floppy_step(
	struct cw_hardware		*hrd,
	int				step,
	int				direction)

	{
	cw_debug(1, "[c%d] step, step = %d, direction = %d", hrd->cnt->num, step, direction);
	cntr_reg(get_bit(W_STEP), step ? 0 : 1);
	cntr_reg_out(get_bit(W_DIRECTION), direction ? 0 : 1);
	}



/****************************************************************************
 * cwhardware_floppy_track0
 ****************************************************************************/
int
cwhardware_floppy_track0(
	struct cw_hardware		*hrd)

	{
	int				track0 = inb(get_reg(CATCONTROL)) & get_mask(R_TRACK0);

	cw_debug(1, "[c%d] track0 = 0x%02x", hrd->cnt->num, track0);
	return (track0 ? 0 : 1);
	}



/****************************************************************************
 * cwhardware_floppy_write_protected
 ****************************************************************************/
int
cwhardware_floppy_write_protected(
	struct cw_hardware		*hrd)

	{
	int				wr_prot = inb(get_reg(CATCONTROL)) & get_mask(R_WRITEPROTECT);

	cw_debug(1, "[c%d] wr_prot = 0x%02x", hrd->cnt->num, wr_prot);
	return (wr_prot ? 0 : 1);
	}



/****************************************************************************
 * cwhardware_floppy_disk_changed
 ****************************************************************************/
int
cwhardware_floppy_disk_changed(
	struct cw_hardware		*hrd)

	{
	int				changed = inb(get_reg(CATCONTROL)) & get_mask(R_DISKCHANGED);

	cw_debug(1, "[c%d] changed = 0x%02x", hrd->cnt->num, changed);
	return (changed ? 0 : 1);
	}



/****************************************************************************
 * cwhardware_floppy_abort
 ****************************************************************************/
void
cwhardware_floppy_abort(
	struct cw_hardware		*hrd)

	{
	inb(get_reg(CATABORT));
	}



/****************************************************************************
 * cwhardware_floppy_busy
 ****************************************************************************/
int
cwhardware_floppy_busy(
	struct cw_hardware		*hrd)

	{
	int				mask, busy;

	mask = get_mask(R_READING) | get_mask(R_WRITING);
	busy = inb(get_reg(CATCONTROL)) & mask;
	cw_debug(1, "[c%d] busy = 0x%02x", hrd->cnt->num, busy);
	return ((busy ^ mask) ? 1 : 0);
	}



/****************************************************************************
 * cwhardware_floppy_read_track_start
 ****************************************************************************/
void
cwhardware_floppy_read_track_start(
	struct cw_hardware		*hrd,
	int				clock,
	int				mode)

	{
	cw_debug(1, "[c%d] read track start, clock = %d, mode = %d", hrd->cnt->num, clock, mode);

	/* reset memory pointer and set clock */

	outb(0x00, get_reg(CATABORT));
	if (clock == CW_TRACKINFO_CLOCK_14MHZ) outb(0x00, get_reg(CATOPTION));
	if (clock == CW_TRACKINFO_CLOCK_28MHZ) outb(0x80, get_reg(CATOPTION));
	if (clock == CW_TRACKINFO_CLOCK_56MHZ) outb(0xc0, get_reg(CATOPTION));

	/* no IRQs, no MFM predecode */

	inb(get_reg(CATMEM));
	inb(get_reg(CATMEM));
	outb(0x00, get_reg(CATOPTION));

	/* check if suppression of index pulses in MSB is needed */

	if ((mode == CW_TRACKINFO_MODE_NORMAL) || (mode == CW_TRACKINFO_MODE_INDEX_WAIT))
		{
		inb(get_reg(CATMEM));
		outb(0x00, get_reg(CATOPTION));
		}

	/* reset memory pointer and start reading */

	outb(0x00, get_reg(CATABORT));
	if (mode == CW_TRACKINFO_MODE_INDEX_WAIT) inb(get_reg(CATSTARTB));
	else inb(get_reg(CATSTARTA));
	}



/****************************************************************************
 * cwhardware_floppy_read_track_copy
 ****************************************************************************/
int
cwhardware_floppy_read_track_copy(
	struct cw_hardware		*hrd,
	u8				*data,
	int				size)

	{
	int				i = 0, reg = get_reg(CATMEM);
	u8				d;

	/* append data end mark */

	outb(0x80, get_reg(CATMEM));

	/*
	 * reset memory pointer and transfer from catweasel memory to
	 * given buffer. ignore first byte in memory. if memory was full
	 * this first byte is the data end marker and will be seen again
	 * if the memory pointer wraps around
	 */

	outb(0x00, get_reg(CATABORT));
	inb(reg);
	while ((i < size) && (i < CW_MAX_SIZE))
		{
		d = inb(reg);
		if (d == 0x80) break;
		data[i++] = d;
		}
	cw_debug(1, "[c%d] read track copy, wanted size = %d, got size = %d", hrd->cnt->num, size, i);
	return (i);
	}



/****************************************************************************
 * cwhardware_floppy_write_track
 ****************************************************************************/
int
cwhardware_floppy_write_track(
	struct cw_hardware		*hrd,
	int				clock,
	int				mode,
	u8				*data,
	int				size)

	{
	int				i, reg = get_reg(CATMEM);

	/*
	 * reset memory pointer and transfer from given buffer to
	 * catweasel memory (skip first 7 bytes needed for write enable
	 * procedure) and write data end mark
	 */

	outb(0x00, get_reg(CATABORT));
	for (i = 0; i < 7; i++) inb(reg);
	for (i = 0; (i < size) && (i < CW_MAX_SIZE - CW_WRITE_OVERHEAD); i++)
		{

		/*
		 * currently special mk4 features (with opcodes >= 0x80)
		 * are not allowed
		 */

		if ((data[i] < 0x03) || (data[i] > 0x7f)) return (-EINVAL);
		outb(0x80 - data[i], reg);
		}
	outb(0xff, reg);
	cw_debug(1, "[c%d] write track, clock = %d, mode = %d, size = %d", hrd->cnt->num, clock, mode, i);

	/* reset memory pointer and set clock */

	outb(0x00, get_reg(CATABORT));
	if (clock == CW_TRACKINFO_CLOCK_14MHZ) outb(0x00, get_reg(CATOPTION));
	if (clock == CW_TRACKINFO_CLOCK_28MHZ) outb(0x80, get_reg(CATOPTION));
	if (clock == CW_TRACKINFO_CLOCK_56MHZ) outb(0xc0, get_reg(CATOPTION));

	/* start writing */

	outb(0x00, get_reg(CATABORT));
	inb(get_reg(CATMEM));
	outb(0x80, get_reg(CATOPTION));
	inb(get_reg(CATMEM));
	inb(get_reg(CATMEM));
	inb(get_reg(CATMEM));
	inb(get_reg(CATMEM));
	inb(get_reg(CATMEM));
	inb(get_reg(CATMEM));
	if (mode == CW_TRACKINFO_MODE_NORMAL) outb(0x00, get_reg(CATSTARTB));
	else outb(0x00, get_reg(CATSTARTA));
	return (size);
	}
/******************************************************** Karsten Scheibler */
