/****************************************************************************
 ****************************************************************************
 *
 * cw.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CW_H
#define CW_H

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include "ioctl.h"

#define CW_NAME				"cw"

#ifdef CW_DEBUG
#define cw_notice(args...)							\
	do									\
		{								\
		printk(KERN_NOTICE CW_NAME ":%s:%d: ", __FILE__, __LINE__);	\
		printk(args);							\
		printk("\n");							\
		}								\
	while (0)

#define cw_error(args...)							\
	do									\
		{								\
		printk(KERN_ERR CW_NAME ":%s:%d: ", __FILE__, __LINE__);	\
		printk(args);							\
		printk("\n");							\
		}								\
	while (0)

#define cw_debug(level, args...)						\
	do									\
		{								\
		if (cw_debug_level < level) break;					\
		printk(KERN_DEBUG CW_NAME ":%s:%d: ", __FILE__, __LINE__);	\
		printk(args);							\
		printk("\n");							\
		}								\
	while (0)
#else /* CW_DEBUG */
#define cw_notice(args...)					\
	do							\
		{						\
		printk(KERN_NOTICE CW_NAME ": " args);		\
		printk("\n");					\
		}						\
	while (0)

#define cw_error(args...)					\
	do							\
		{						\
		printk(KERN_ERR CW_NAME ": " args);		\
		printk("\n");					\
		}						\
	while (0)

#define cw_debug(args...)
#endif /* CW_DEBUG */



#define CW_MAX_CONTROLLERS		4
#define CW_MAX_FLOPPIES_PER_CONTROLLER	2
#define CW_MAX_FLOPPIES			(CW_MAX_CONTROLLERS * CW_MAX_FLOPPIES_PER_CONTROLLER)

#define CW_HARDWARE_MODEL_NONE		0
#define CW_HARDWARE_MODEL_MK3		3
#define CW_HARDWARE_MODEL_MK4		4

struct cw_controller;

struct cw_hardware
	{
	struct cw_controller		*cnt;
	int				model;
	int				iobase;
	volatile unsigned long		control_register;
	};

#define CW_FLOPPY_MODEL_NONE		0
#define CW_FLOPPY_MODEL_AUTO		1
#define CW_FLOPPY_FORMAT_RAW		31

struct cw_floppies;

struct cw_floppy
	{
	int				num;
	struct cw_floppies		*fls;
	int				busy;
	wait_queue_head_t		busy_wq;
	int				model;
	int				format;
	int				track;
	int				track_wanted;
	int				track_dirty;
	int				motor;
	int				motor_request;
	struct timer_list		motor_timer;
	u8				*track_data;
	struct cw_floppyinfo		fli;
	};

struct cw_floppies
	{
	struct cw_controller		*cnt;
	struct cw_floppy		flp[CW_MAX_FLOPPIES_PER_CONTROLLER];
	spinlock_t			lock;
	volatile int			open;
	int				busy;
	wait_queue_head_t		busy_wq;
	wait_queue_head_t		step_wq;
	struct timer_list		step_timer;
	int				mux;
	struct timer_list		mux_timer;
	int				rw_latch;
	int				rw_timeout;
	int				rw_jiffies;
	wait_queue_head_t		rw_wq;
	struct timer_list		rw_timer;
	};

struct cw_controller
	{
	int				num;
	int				ready;
	struct cw_hardware		hrd;
	struct cw_floppies		fls;
	};

extern int				cw_debug_level;
extern struct cw_controller		*cw_get_controller(int);
extern struct cw_hardware		*cw_get_hardware(int);
extern struct cw_floppies		*cw_get_floppies(int);
extern void				cw_register_hardware(struct cw_hardware *);
extern void				cw_unregister_hardware(struct cw_hardware *);



#endif /* !CW_H */
/******************************************************** Karsten Scheibler */
