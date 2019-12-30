/****************************************************************************
 ****************************************************************************
 *
 * cw.c
 *
 ****************************************************************************
 ****************************************************************************/





#ifdef MODULE
#include "config.h"	/* includes <linux/config.h> if needed */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/wait.h>

#include "cw.h"
#include "cwfloppy.h"
#include "cwhardware.h"
#include "version.h"



#define CW_FLAG_CHRDEV_REGISTERED	(1 << 0)
#define CW_FLAG_MK3_REGISTERED		(1 << 1)
#define CW_FLAG_MK4_REGISTERED		(1 << 2)



int					cw_debug_level = 0;
int					cw_major = 120;
static int				cw_controllers;
static int				cw_flags;



/****************************************************************************
 * cw_get_controller
 ****************************************************************************/
struct cw_controller *
cw_get_controller(
	int				c)

	{
	static struct cw_controller	cnt[CW_MAX_CONTROLLERS];

	if (c == -1)
		{
		cw_debug(1, "request for unused controller struct, cw_controllers = %d", cw_controllers);
		if (cw_controllers >= CW_MAX_CONTROLLERS) return (NULL);
		cnt[cw_controllers].num     = cw_controllers;
		cnt[cw_controllers].hrd.cnt = &cnt[cw_controllers];
		cnt[cw_controllers].fls.cnt = &cnt[cw_controllers];
		return (&cnt[cw_controllers++]);
		}
	if ((c >= 0) && (c < cw_controllers))
		{
		if (cnt[c].ready) return (&cnt[c]);
		cw_debug(1, "controller %d is not ready", c);
		}
	return (NULL);
	}



/****************************************************************************
 * cw_get_hardware
 ****************************************************************************/
struct cw_hardware *
cw_get_hardware(
	int				c)

	{
	struct cw_controller		*cnt;

	cnt = cw_get_controller(c);
	if (cnt == NULL) return (NULL);
	return (&cnt->hrd);
	}



/****************************************************************************
 * cw_get_floppies
 ****************************************************************************/
struct cw_floppies *
cw_get_floppies(
	int				c)

	{
	struct cw_controller		*cnt;

	cnt = cw_get_controller(c);
	if (cnt == NULL) return (NULL);
	return (&cnt->fls);
	}



/****************************************************************************
 * cw_register_hardware
 ****************************************************************************/
void
cw_register_hardware(
	struct cw_hardware		*hrd)

	{
	hrd->cnt->ready = 1;
	}



/****************************************************************************
 * cw_unregister_hardware
 ****************************************************************************/
void
cw_unregister_hardware(
	struct cw_hardware		*hrd)

	{
	hrd->cnt->ready = 0;
	}



/****************************************************************************
 * cw_module_exit
 ****************************************************************************/
static void __exit
cw_module_exit(
	void)

	{
	struct cw_floppies		*fls;
	int				c;

	/*
	 * if we exit in the middle of a controller probe with an error we
	 * do not want to call cwfloppy_exit(). only if cw_register_hardware()
	 * was called a controller is really ready
	 */

	cw_debug(1, "module exit");
	for (c = 0; c < cw_controllers; c++)
		{
		fls = cw_get_floppies(c);
		if (fls == NULL) continue;
		cwfloppy_exit(fls);
		}

#ifdef CONFIG_PCI
	if (cw_flags & CW_FLAG_MK3_REGISTERED) pci_unregister_driver(&cwhardware_mk3_pci_driver);
	if (cw_flags & CW_FLAG_MK4_REGISTERED) pci_unregister_driver(&cwhardware_mk4_pci_driver);
#endif /* CONFIG_PCI */
	if (cw_flags & CW_FLAG_CHRDEV_REGISTERED) unregister_chrdev(cw_major, CW_NAME);
	}



/****************************************************************************
 * cw_module_init
 ****************************************************************************/
static int __init
cw_module_init(
	void)

	{
	struct cw_floppies		*fls;
	int				c, ready, result;

	/* register character device */

	cw_debug(1, "module init");
	cw_notice("driver version " VERSION "-" VERSION_DATE);
	result = register_chrdev(cw_major, CW_NAME, &cwfloppy_fops);
	if (result < 0)
		{
		cw_error("can't get major number %d", cw_major);
		goto error;
		}
	if (cw_major == 0) cw_major = result;
	cw_flags |= CW_FLAG_CHRDEV_REGISTERED;

	/* search for controller hardware */

#ifdef CONFIG_PCI
	cw_debug(1, "searching for catweasel mk3");
	result = pci_register_driver(&cwhardware_mk3_pci_driver);
	cw_flags |= CW_FLAG_MK3_REGISTERED;
	if ((result < 0) && (result != -ENODEV)) goto error;

	cw_debug(1, "searching for catweasel mk4");
	result = pci_register_driver(&cwhardware_mk4_pci_driver);
	cw_flags |= CW_FLAG_MK4_REGISTERED;
	if ((result < 0) && (result != -ENODEV)) goto error;
#endif /* CONFIG_PCI */

	/* initialize floppies on all found controllers */

	for (c = ready = 0; c < cw_controllers; c++)
		{
		fls = cw_get_floppies(c);
		if (fls == NULL) continue;
		result = cwfloppy_init(fls);
		if (result < 0) goto error;
		ready++;
		}

	/* check if at least one controller is ready */

	if (ready > 0) return (0);
	result = -ENODEV;
	cw_error("no catweasel hardware found");
error:
	cw_module_exit();
	return (result);
	}



/****************************************************************************
 * module macros
 ****************************************************************************/
module_init(cw_module_init);
module_exit(cw_module_exit);
#if LINUX_VERSION_CODE >= 0x020600
module_param(cw_debug_level, int, 0);
module_param(cw_major, int, 0);
#else /* LINUX_VERSION_CODE */
MODULE_PARM(cw_debug_level, "i");
MODULE_PARM(cw_major, "i");
#endif /* LINUX_VERSION_CODE */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("device driver for the catweasel floppy controller");
MODULE_AUTHOR("Karsten Scheibler");
#endif /* MODULE */
/******************************************************** Karsten Scheibler */
