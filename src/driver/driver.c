/****************************************************************************
 ****************************************************************************
 *
 * driver.c
 *
 ****************************************************************************
 ****************************************************************************/





#ifdef MODULE
#include "config.h"	/* includes <linux/config.h> if needed */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/wait.h>

#include "driver.h"
#include "floppy.h"
#include "hardware.h"
#include "message.h"
#include "version.h"



#define CW_FLAG_CHRDEV_REGISTERED	(1 << 0)
#define CW_FLAG_MK2_REGISTERED		(1 << 2)
#define CW_FLAG_MK3_REGISTERED		(1 << 3)
#define CW_FLAG_MK4_REGISTERED		(1 << 4)
#define CW_FLAG_CLASS_REGISTERED	(1 << 5)
#define CW_FLAG_DEVICES_CREATED		(1 << 6)



cw_int_t				cw_debug_level = 0;
cw_int_t				cw_major = 120;
cw_int_t				cw_mk2_ports[CW_NR_CONTROLLERS + 1] = { };
static struct class			*dev_class;
static cw_count_t			cw_driver_controllers;
static cw_flag_t			cw_driver_flags;



/****************************************************************************
 * cw_class_devnode
 ****************************************************************************/
static char *
cw_class_devnode(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,0)
	const struct device	*dev,
#else
	struct device		*dev,
#endif
	umode_t			*mode)

	{
	if (mode)
		*mode = 0666;

	return NULL;
	}



/****************************************************************************
 * cw_driver_get_controller
 ****************************************************************************/
struct cw_controller *
cw_driver_get_controller(
	cw_index_t			index)

	{
	static struct cw_controller	cnt[CW_NR_CONTROLLERS];

	if (index == -1)
		{
		cw_debug(1, "request for unused controller struct, cw_driver_controllers = %d", cw_driver_controllers);
		if (cw_driver_controllers >= CW_NR_CONTROLLERS) return (NULL);
		index = cw_driver_controllers++;
		cnt[index].num     = index;
		cnt[index].hrd.cnt = &cnt[index];
		cnt[index].fls.cnt = &cnt[index];
		return (&cnt[index]);
		}
	if ((index >= 0) && (index < cw_driver_controllers))
		{
		if (cnt[index].ready) return (&cnt[index]);
		cw_debug(1, "controller %d is not ready", index);
		}
	return (NULL);
	}



/****************************************************************************
 * cw_driver_get_hardware
 ****************************************************************************/
struct cw_hardware *
cw_driver_get_hardware(
	cw_index_t			index)

	{
	struct cw_controller		*cnt;

	cnt = cw_driver_get_controller(index);
	if (cnt == NULL) return (NULL);
	return (&cnt->hrd);
	}



/****************************************************************************
 * cw_driver_get_floppies
 ****************************************************************************/
struct cw_floppies *
cw_driver_get_floppies(
	cw_index_t			index)

	{
	struct cw_controller		*cnt;

	cnt = cw_driver_get_controller(index);
	if (cnt == NULL) return (NULL);
	return (&cnt->fls);
	}



/****************************************************************************
 * cw_driver_register_hardware
 ****************************************************************************/
cw_void_t
cw_driver_register_hardware(
	struct cw_hardware		*hrd)

	{
	hrd->cnt->ready = 1;
	}



/****************************************************************************
 * cw_driver_unregister_hardware
 ****************************************************************************/
cw_void_t
cw_driver_unregister_hardware(
	struct cw_hardware		*hrd)

	{
	hrd->cnt->ready = 0;
	}



/****************************************************************************
 * cw_driver_module_exit
 ****************************************************************************/
static cw_void_t
cw_driver_module_exit(
	cw_void_t)

	{
	struct cw_floppies		*fls;
	cw_index_t			c;

	/*
	 * if we exit in the middle of a controller probe with an error we
	 * do not want to call cw_floppy_exit(). only if cw_driver_register_hardware()
	 * was called a controller is really ready
	 */

	cw_debug(1, "module exit");
	for (c = 0; c < cw_driver_controllers; c++)
		{
		fls = cw_driver_get_floppies(c);
		if (fls == NULL) continue;
		if (cw_driver_flags & CW_FLAG_DEVICES_CREATED)
			{
			int	f;

			for (f = 0; f < CW_NR_FLOPPIES_PER_CONTROLLER; ++f)
				{
				int	cw_minor = (c * 64) + ((f+1) * 32) - 1;
				dev_t	cw_dev   = MKDEV(cw_major, cw_minor);

				device_destroy(dev_class, cw_dev);
				}
			}
		cw_floppy_exit(fls);
		}

	if (cw_driver_flags & CW_FLAG_MK2_REGISTERED) cw_hardware_mk2_unregister();
#ifdef CONFIG_PCI
	if (cw_driver_flags & CW_FLAG_MK3_REGISTERED) pci_unregister_driver(&cw_hardware_mk3_pci_driver);
	if (cw_driver_flags & CW_FLAG_MK4_REGISTERED) pci_unregister_driver(&cw_hardware_mk4_pci_driver);
#endif /* CONFIG_PCI */
	if (cw_driver_flags & CW_FLAG_CLASS_REGISTERED) class_destroy(dev_class);
	if (cw_driver_flags & CW_FLAG_CHRDEV_REGISTERED) unregister_chrdev(cw_major, CW_NAME);
	}



/****************************************************************************
 * cw_driver_module_init
 ****************************************************************************/
static cw_int_t __init
cw_driver_module_init(
	cw_void_t)

	{
	struct cw_floppies		*fls;
	cw_count_t			c, ready;
	cw_int_t			result;

	/* register character device */

	cw_debug(1, "module init");
	cw_notice("driver version " VERSION "-" VERSION_DATE);
	result = register_chrdev(cw_major, CW_NAME, &cw_floppy_fops);
	if (result < 0)
		{
		cw_error("can't get major number %d", cw_major);
		goto error;
		}
	if (cw_major == 0) cw_major = result;
	cw_driver_flags |= CW_FLAG_CHRDEV_REGISTERED;

	/* search for controller hardware */

	if (cw_mk2_ports[0] != 0)
		{
		cw_mk2_ports[CW_NR_CONTROLLERS] = 0;
		cw_debug(1, "searching for catweasel mk2");
		result = cw_hardware_mk2_register(cw_mk2_ports);
		cw_driver_flags |= CW_FLAG_MK2_REGISTERED;
		if ((result < 0) && (result != -ENODEV)) goto error;
		}

#ifdef CONFIG_PCI
	cw_debug(1, "searching for catweasel mk3");
	result = pci_register_driver(&cw_hardware_mk3_pci_driver);
	cw_driver_flags |= CW_FLAG_MK3_REGISTERED;
	if ((result < 0) && (result != -ENODEV)) goto error;

	cw_debug(1, "searching for catweasel mk4");
	result = pci_register_driver(&cw_hardware_mk4_pci_driver);
	cw_driver_flags |= CW_FLAG_MK4_REGISTERED;
	if ((result < 0) && (result != -ENODEV)) goto error;
#endif /* CONFIG_PCI */

	dev_class = class_create(THIS_MODULE, "catweasel");
	if(IS_ERR(dev_class))
		{
		cw_error("Cannot create catweasel class\n");
		goto error;
		}
	dev_class->devnode = cw_class_devnode;
	cw_driver_flags |= CW_FLAG_CLASS_REGISTERED;

	/* initialize floppies on all found controllers */

	for (c = ready = 0; c < cw_driver_controllers; c++)
		{
		int	f;

		fls = cw_driver_get_floppies(c);
		if (fls == NULL) continue;
		result = cw_floppy_init(fls);
		if (result < 0) goto error;
		ready++;
		for (f = 0; f < CW_NR_FLOPPIES_PER_CONTROLLER; ++f)
			{
			int	cw_minor = (c * 64) + ((f+1) * 32) - 1;
			dev_t	cw_dev   = MKDEV(cw_major, cw_minor);

			if(IS_ERR(device_create(dev_class, NULL, cw_dev,
				NULL, "cw%draw%d", c, f)))
				{
				cw_error("Cannot create catweasel device"
					 "cw%draw%d\n", c, f);
				goto error;

				}
			cw_driver_flags |= CW_FLAG_DEVICES_CREATED;
			}
		}

	/* check if at least one controller is ready */

	if (ready > 0) return (0);
	result = -ENODEV;
	cw_error("no catweasel hardware found");
error:
	cw_driver_module_exit();
	return (result);
	}



/****************************************************************************
 * module macros
 ****************************************************************************/
module_init(cw_driver_module_init);
module_exit(cw_driver_module_exit);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param(cw_debug_level, int, 0);
module_param(cw_major, int, 0);
module_param_array(cw_mk2_ports, int, NULL, 0);
#else /* LINUX_VERSION_CODE */
MODULE_PARM(cw_debug_level, "i");
MODULE_PARM(cw_major, "i");
#if CW_NR_CONTROLLERS < 4
#error "CW_NR_CONTROLLERS < 4, need to change MODULE_PARM for cw_mk2_ports"
#endif
MODULE_PARM(cw_mk2_ports, "1-4i");
#endif /* LINUX_VERSION_CODE */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("device driver for the catweasel floppy controller");
MODULE_AUTHOR("Karsten Scheibler");
#endif /* MODULE */
/******************************************************** Karsten Scheibler */
