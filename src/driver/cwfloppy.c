/****************************************************************************
 ****************************************************************************
 *
 * cwfloppy.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <linux/config.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include "cwfloppy.h"
#include "cw.h"
#include "cwhardware.h"
#include "ioctl.h"



#if LINUX_VERSION_CODE >= 0x020600
#define do_sleep_on(cond, wq)					\
	do							\
		{						\
		DEFINE_WAIT(w);					\
								\
		prepare_to_wait(wq, &w, TASK_UNINTERRUPTIBLE);	\
		if (cond) schedule();				\
		finish_wait(wq, &w);				\
		}						\
	while (0)
#else /* LINUX_VERSION_CODE */
#define do_sleep_on(cond, wq)		sleep_on(wq)
#endif /* LINUX_VERSION_CODE */

#define get_controller(minor)		((minor >> 6) & 3)
#define get_floppy(minor)		((minor >> 5) & 1)
#define get_format(minor)		(minor & 0x1f)
#define cnt_num				flp->fls->cnt->num
#define cnt_hrd				flp->fls->cnt->hrd



/****************************************************************************
 * cwfloppy_default_parameters
 ****************************************************************************/
static struct cw_floppyinfo
cwfloppy_default_parameters(
	void)

	{
	struct cw_floppyinfo		fli =
		{
		.version     = CW_STRUCT_VERSION,
		.settle_time = 25,
		.step_time   = 6,
		.track_max   = CW_MAX_TRACK,
		.side_max    = 2,
		.clock_max   = CW_MAX_CLOCK,
		.mode_max    = CW_MAX_MODE,
		.size_max    = CW_MAX_SIZE
		};

	return (fli);
	}



/****************************************************************************
 * cwfloppy_get_parameters
 ****************************************************************************/
static int
cwfloppy_get_parameters(
	struct cw_floppy		*flp,
	struct cw_floppyinfo		*fli)

	{
	if (fli->version != CW_STRUCT_VERSION) return (-EINVAL);
	*fli = flp->fli;
	return (0);
	}



/****************************************************************************
 * cwfloppy_set_parameters
 ****************************************************************************/
static int
cwfloppy_set_parameters(
	struct cw_floppy		*flp,
	struct cw_floppyinfo		*fli)

	{
	if ((fli->track_max < 1) || (fli->track_max > CW_MAX_TRACK) ||
		(fli->settle_time < 1) || (fli->settle_time > 1000) ||
		(fli->step_time < 3) || (fli->step_time > 1000) ||
		(fli->side_max < 1) || (fli->side_max > 2) ||
		(fli->clock_max != flp->fli.clock_max) ||
		(fli->mode_max != flp->fli.mode_max) ||
		(fli->size_max != flp->fli.size_max) ||
		(fli->version != CW_STRUCT_VERSION)) return (-EINVAL);
	flp->fli.settle_time = fli->settle_time;
	flp->fli.step_time   = fli->step_time;
	flp->fli.track_max   = fli->track_max;
	flp->fli.side_max    = fli->side_max;
	return (0);
	}



/****************************************************************************
 * cwfloppy_check_parameters
 ****************************************************************************/
static int
cwfloppy_check_parameters(
	struct cw_floppyinfo		*fli,
	struct cw_trackinfo		*tri,
	int				write)

	{
	if ((tri->version != CW_STRUCT_VERSION) ||
		(tri->track >= fli->track_max) || (tri->side >= fli->side_max) ||
		(tri->clock >= fli->clock_max) || (tri->mode >= fli->mode_max) ||
		(tri->timeout <= CW_MIN_TIMEOUT) || (tri->timeout >= CW_MAX_TIMEOUT)) return (-EINVAL);
	if ((write) && ((tri->size >= fli->size_max - CW_WRITE_OVERHEAD) ||
		(tri->mode == CW_TRACKINFO_MODE_INDEX_STORE))) return (-EINVAL);
	return (0);
	}



/****************************************************************************
 * cwfloppy_add_timer2
 ****************************************************************************/
static void
cwfloppy_add_timer2(
	struct timer_list		*timer,
	unsigned long			expires,
	unsigned long			data)

	{
	timer->expires = expires;
	timer->data    = data;
	add_timer(timer);
	}



/****************************************************************************
 * cwfloppy_add_timer
 ****************************************************************************/
#define cwfloppy_add_timer(t, e, d)	cwfloppy_add_timer2(t, e, (unsigned long) d)



/****************************************************************************
 * cwfloppy_lock_controller
 ****************************************************************************/
static void
cwfloppy_lock_controller(
	struct cw_floppies		*fls)

	{
	wait_queue_t			w;
	unsigned long			flags;

	cw_debug(1, "[c%d] trying to lock controller", fls->cnt->num);
	init_waitqueue_entry(&w, current);
	add_wait_queue(&fls->busy_wq, &w);
	while (1)
		{
		set_current_state(TASK_UNINTERRUPTIBLE);
		spin_lock_irqsave(&fls->lock, flags);
		if (! fls->busy)
			{
			fls->busy = 1;
			spin_unlock_irqrestore(&fls->lock, flags);
			break;
			}
		spin_unlock_irqrestore(&fls->lock, flags);
		schedule();
		}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&fls->busy_wq, &w);
	cw_debug(1, "[c%d] locking controller", fls->cnt->num);
	}



/****************************************************************************
 * cwfloppy_unlock_controller
 ****************************************************************************/
static void
cwfloppy_unlock_controller(
	struct cw_floppies		*fls)

	{
	cw_debug(1, "[c%d] unlocking controller", fls->cnt->num);
	fls->busy = 0;
	wake_up(&fls->busy_wq);
	}



/****************************************************************************
 * cwfloppy_lock_floppy
 ****************************************************************************/
static void
cwfloppy_lock_floppy(
	struct cw_floppy		*flp)

	{
	wait_queue_t			w;
	unsigned long			flags;

	cw_debug(1, "[c%df%d] trying to lock floppy", cnt_num, flp->num);
	init_waitqueue_entry(&w, current);
	add_wait_queue(&flp->busy_wq, &w);
	while (1)
		{
		set_current_state(TASK_UNINTERRUPTIBLE);
		spin_lock_irqsave(&flp->fls->lock, flags);
		if (! flp->busy)
			{
			flp->busy = 1;
			spin_unlock_irqrestore(&flp->fls->lock, flags);
			break;
			}
		spin_unlock_irqrestore(&flp->fls->lock, flags);
		schedule();
		}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&flp->busy_wq, &w);
	cw_debug(1, "[c%df%d] locking floppy", cnt_num, flp->num);
	}



/****************************************************************************
 * cwfloppy_unlock_floppy
 ****************************************************************************/
static void
cwfloppy_unlock_floppy(
	struct cw_floppy		*flp)

	{
	cw_debug(1, "[c%df%d] unlocking floppy", cnt_num, flp->num);
	flp->busy = 0;
	wake_up(&flp->busy_wq);
	}



/****************************************************************************
 * cwfloppy_motor_on
 ****************************************************************************/
static void
cwfloppy_motor_on(
	struct cw_floppy		*flp)

	{
	unsigned long			flags;

	/*
	 * delete pending cwfloppy_motor_timer_func(),
	 * set flp->motor_request = 0 under flp->fls->lock, this will
	 * ensure that no parallel running cwfloppy_mux_timer_func()
	 * already read flp->motor_request and changes the state of
	 * flp->motor after we checked flp->motor
	 */

	cw_debug(1, "[c%df%d] motor on", cnt_num, flp->num);
	del_timer_sync(&flp->motor_timer);
	spin_lock_irqsave(&flp->fls->lock, flags);
	flp->motor_request = 0;
	spin_unlock_irqrestore(&flp->fls->lock, flags);

	/* motor is still on, nothing to do */

	if (flp->motor) return;

	/*
	 * switch floppy motor on, wait for spin up. this floppy stays
	 * locked, so only the other floppy on this controller may be used
	 * in the mean time
	 */

	spin_lock_irqsave(&flp->fls->lock, flags);
	cwhardware_floppy_motor_on(&cnt_hrd, flp->num);
	spin_unlock_irqrestore(&flp->fls->lock, flags);
	cw_debug(2, "[c%df%d] will wait for motor spin up", cnt_num, flp->num);
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ);
	cw_debug(2, "[c%df%d] spin up done", cnt_num, flp->num);
	flp->motor = 1;
	}



/****************************************************************************
 * cwfloppy_motor_timer_func
 ****************************************************************************/
static void
cwfloppy_motor_timer_func(
	unsigned long			arg)

	{
	struct cw_floppy		*flp = (struct cw_floppy *) arg;

	/* just signal cwfloppy_mux_timer_func() to switch motor off */

	cw_debug(2, "[c%df%d] preparing to switch motor off", cnt_num, flp->num);
	flp->motor_request = 1;
	}



/****************************************************************************
 * cwfloppy_motor_off
 ****************************************************************************/
static void
cwfloppy_motor_off(
	struct cw_floppy		*flp)

	{

	/*
	 * always called after cwfloppy_motor_on(), so there is no
	 * cwfloppy_motor_timer_func() running from a previous
	 * cwfloppy_motor_off()-call
	 */

	cw_debug(2, "[c%df%d] registering motor_timer", cnt_num, flp->num);
	cwfloppy_add_timer(&flp->motor_timer, jiffies + 2 * HZ, flp);
	}



/****************************************************************************
 * cwfloppy_step_timer_func
 ****************************************************************************/
static void
cwfloppy_step_timer_func(
	unsigned long			arg)

	{
	struct cw_floppy		*flp = (struct cw_floppy *) arg;
	int				direction = 1;
	int				time = flp->fli.settle_time;

	/* check if already at wanted position */

	if (flp->track_wanted == flp->track)
		{
		wake_up(&flp->fls->step_wq);
		return;
		}

	/* check if explicit track0 sensing is wanted */

	if (flp->track_wanted < 0)
		{
		cw_debug(2, "[c%df%d] doing explicit track0 sensing", cnt_num, flp->num);
		if (cwhardware_floppy_track0(&cnt_hrd))
			{
			flp->track        = 0;
			flp->track_wanted = 0;
			goto done;
			}
		}

	/* step one track */

	if (flp->track_wanted < flp->track) direction = 0;
	cw_debug(2, "[c%df%d] track_wanted = %d, track = %d, direction = %d", cnt_num, flp->num, flp->track_wanted, flp->track, direction);

	/*
	 * the spin_is_locked() call may be obsolete, but for the case that
	 * this timer routine interrupts another one of cwfloppy, which
	 * currently holds flp->fls->lock. without spin_is_locked() this
	 * would result in a dead lock. i think timer routines interrupting
	 * other timer routines will never happen on current linux kernels
	 */

	if (! spin_is_locked(&flp->fls->lock))
		{
		spin_lock(&flp->fls->lock);
		cwhardware_floppy_step(&cnt_hrd, 1, direction);
		udelay(50);
		cwhardware_floppy_step(&cnt_hrd, 0, direction);
		spin_unlock(&flp->fls->lock);
		flp->track += direction ? 1 : -1;
		if (flp->track_wanted != flp->track) time = flp->fli.step_time;
		}
	else cw_debug(2, "[c%df%d] will try again", cnt_num, flp->num);

	/* schedule next call */
done:
	cwfloppy_add_timer(&flp->fls->step_timer, jiffies + (time * HZ + 999) / 1000, flp);
	}



/****************************************************************************
 * cwfloppy_step
 ****************************************************************************/
static int
cwfloppy_step(
	struct cw_floppy		*flp,
	int				track_wanted)

	{
	flp->track_wanted = track_wanted;
	if (flp->track_wanted == flp->track) return (0);
	cw_debug(1, "[c%df%d] registering floppies_step_timer", cnt_num, flp->num);
	cwfloppy_add_timer(&flp->fls->step_timer, jiffies + 2, flp);
	do_sleep_on(flp->track_wanted != flp->track, &flp->fls->step_wq);
	cw_debug(1, "[c%df%d] track_wanted(%d) == track(%d)", cnt_num, flp->num, flp->track_wanted, flp->track);
	return (1);
	}



/****************************************************************************
 * cwfloppy_dummy_step
 ****************************************************************************/
static int
cwfloppy_dummy_step(
	struct cw_floppy		*flp)

	{
	int				track = flp->track;
	int				ofs = (track > 0) ? 1 : -1;

	cwfloppy_step(flp, track - ofs);
	cwfloppy_step(flp, track + ofs);
	return (1);
	}



/****************************************************************************
 * cwfloppy_calibrate
 ****************************************************************************/
static int
cwfloppy_calibrate(
	struct cw_floppy		*flp)

	{
	flp->track = 0;
	cwfloppy_step(flp, 3);
	cwfloppy_step(flp, -CW_MAX_TRACK);
	flp->track_dirty = 0;
	return (1);
	}



/****************************************************************************
 * cwfloppy_get_model
 ****************************************************************************/
static int
cwfloppy_get_model(
	struct cw_floppy		*flp)

	{

	/*
	 * a floppy is present if we get a track0 signal while moving
	 * the head, this routine is only called from cwfloppy_init(),
	 * so no concurrent accesses to the control register via
	 * cwhardware_*()-functions may happen, so no protection with
	 * flp->fls->lock is needed
	 */

	cwhardware_floppy_select(&cnt_hrd, flp->num, 0);
	cwfloppy_calibrate(flp);
	cwhardware_floppy_motor_off(&cnt_hrd, flp->num);
	if (flp->track == 0) return (CW_FLOPPY_MODEL_AUTO);
	return (CW_FLOPPY_MODEL_NONE);
	}



/****************************************************************************
 * cwfloppy_mux_timer_func
 ****************************************************************************/
static void
cwfloppy_mux_timer_func(
	unsigned long			arg)

	{
	struct cw_floppies		*fls = (struct cw_floppies *) arg;
	struct cw_floppy		*flp;
	int				f, open;

	/*
	 * the spin_is_locked() call may be obsolete, but for the case that
	 * this timer routine interrupts another one of cwfloppy, which
	 * currently holds flp->fls->lock. without spin_is_locked() this
	 * would result in a dead lock. i think timer routines interrupting
	 * other timer routines will never happen on current linux kernels
	 */

	if (spin_is_locked(&fls->lock)) goto done;
	spin_lock(&fls->lock);
	for (f = 0, open = fls->open; f < CW_MAX_FLOPPIES_PER_CONTROLLER; f++)
		{
		flp = &fls->flp[f];
		if (flp->model == CW_FLOPPY_MODEL_NONE) continue;
		if (! flp->motor_request)
			{
			open += flp->motor;
			continue;
			}
		cwhardware_floppy_motor_off(&cnt_hrd, flp->num);
		flp->motor_request = 0;
		flp->motor         = 0;
		}
	if ((! fls->mux) && (! open))
		{
		cwhardware_floppy_host_select(&cnt_hrd);
		cwhardware_floppy_mux_on(&fls->cnt->hrd);
		fls->mux = 1;
		}
	spin_unlock(&fls->lock);

	/* wait at least 100 ms until next check */
done:
	cwfloppy_add_timer(&fls->mux_timer, jiffies + (100 * HZ + 999) / 1000, fls);
	}



/****************************************************************************
 * cwfloppy_rw_timer_func
 ****************************************************************************/
static void
cwfloppy_rw_timer_func(
	unsigned long			arg)

	{
	struct cw_floppy		*flp = (struct cw_floppy *) arg;
	int				diff, busy;

	/*
	 * check if operation finished (with indexed read floppy gets busy
	 * only after the index pulse, we are done only if last state was
	 * busy and floppy is now not busy)
	 */

	busy = cwhardware_floppy_busy(&cnt_hrd);
	if ((! busy) && (flp->fls->rw_latch))
		{
		flp->fls->rw_timeout = 0;
		goto done;
		}
	flp->fls->rw_latch = busy;

	/*
	 * on jiffies overflow some precision is lost for timeout
	 * calculation (we may wait a little bit longer than requested)
	 */

	if (flp->fls->rw_jiffies > jiffies) diff = 0;
	else diff = jiffies - flp->fls->rw_jiffies;
	flp->fls->rw_timeout -= (1000 * diff) / HZ;
	flp->fls->rw_jiffies = jiffies;
	cw_debug(2, "[c%df%d] timeout = %d ms, latch = %d", cnt_num, flp->num, flp->fls->rw_timeout, flp->fls->rw_latch);

	/* check if operation timed out */

	if (flp->fls->rw_timeout <= 0)
		{
		cwhardware_floppy_abort(&cnt_hrd);
		flp->fls->rw_timeout = -1;
done:
		wake_up(&flp->fls->rw_wq);
		return;
		}

	/* wait at least 10 ms until next check */

	cwfloppy_add_timer(&flp->fls->rw_timer, jiffies + (10 * HZ + 999) / 1000, flp);
	}



/****************************************************************************
 * cwfloppy_read_write_track
 ****************************************************************************/
static int
cwfloppy_read_write_track(
	struct cw_floppy		*flp,
	struct cw_trackinfo		*tri,
	int				write)

	{
	int				result = -EIO, aborted = 0, stepped = 0;
	unsigned long			flags;

	/* motor on, lock controller, select floppy and move head */

	cwfloppy_motor_on(flp);
	cwfloppy_lock_controller(flp->fls);
	spin_lock_irqsave(&flp->fls->lock, flags);
	cwhardware_floppy_select(&cnt_hrd, flp->num, tri->side);
	spin_unlock_irqrestore(&flp->fls->lock, flags);
	if (flp->track_dirty) stepped = cwfloppy_calibrate(flp);
	stepped |= cwfloppy_step(flp, tri->track);

	/*
	 * check if disk is in drive, a set disk change flag means the disk
	 * was removed from drive, if a disk was inserted in the mean time,
	 * we need to step to update the flag
	 */

	while (cwhardware_floppy_disk_changed(&cnt_hrd))
		{
		if (stepped) goto done;
		stepped = cwfloppy_dummy_step(flp);
		}

	/* start reading or writing now */

	if (write)
		{
		if (cwhardware_floppy_write_protected(&cnt_hrd)) result = -EROFS;
		else result = cwhardware_floppy_write_track(&cnt_hrd, tri->clock, tri->mode, flp->track_data, tri->size);
		if (result < 0) goto done;
		}
	else cwhardware_floppy_read_track_start(&cnt_hrd, tri->clock, tri->mode);

	/* wait until operation finished or timed out */

	cw_debug(1, "[c%df%d] registering floppies_rw_timer", cnt_num, flp->num);
	flp->fls->rw_latch   = cwhardware_floppy_busy(&cnt_hrd);
	flp->fls->rw_jiffies = jiffies;
	flp->fls->rw_timeout = tri->timeout;
	cwfloppy_add_timer(&flp->fls->rw_timer, jiffies + 2, flp);
	do_sleep_on(flp->fls->rw_timeout > 0, &flp->fls->rw_wq);
	if (flp->fls->rw_timeout < 0) aborted = 1;
	cw_debug(1, "[c%df%d] track operation done, aborted = %d", cnt_num, flp->num, aborted);

	/*
	 * do remaining actions
	 *
	 * on read:  get the data from catweasel memory,
	 * on write: decrease the written bytes by one if the write was
	 *           aborted to signal the application that not all bytes
	 *           were written to disk
	 */

	if (write) result -= aborted;
	else result = cwhardware_floppy_read_track_copy(&cnt_hrd, flp->track_data, tri->size);

	/* done */
done:
	cwfloppy_unlock_controller(flp->fls);
	cwfloppy_motor_off(flp);
	return (result);
	}



/****************************************************************************
 * cwfloppy_read_track
 ****************************************************************************/
static int
cwfloppy_read_track(
	struct cw_floppy		*flp,
	struct cw_trackinfo		*tri)

	{
	int				result;

	/* check parameters */

	result = cwfloppy_check_parameters(&flp->fli, tri, 0);
	if (result < 0) return (result);
	if (tri->size == 0) return (0);
	if (! access_ok(VERIFY_WRITE, tri->data, tri->size)) return (-EFAULT);

	/* read track and copy data to user space */

	cwfloppy_lock_floppy(flp);
	result = cwfloppy_read_write_track(flp, tri, 0);
	if ((result > 0) && (copy_to_user(tri->data, flp->track_data, result) != 0)) result = -EFAULT;
	cwfloppy_unlock_floppy(flp);
	return (result);
	}



/****************************************************************************
 * cwfloppy_write_track
 ****************************************************************************/
static int
cwfloppy_write_track(
	struct cw_floppy		*flp,
	struct cw_trackinfo		*tri)

	{
	int				result;

	/* check parameters */

	result = cwfloppy_check_parameters(&flp->fli, tri, 1);
	if (result < 0) return (result);
	if (tri->size == 0) return (0);
	if (! access_ok(VERIFY_READ, tri->data, tri->size)) return (-EFAULT);

	/* copy data from user space and write track */

	cwfloppy_lock_floppy(flp);
	result = -EFAULT;
	if (copy_from_user(flp->track_data, tri->data, tri->size) == 0) result = cwfloppy_read_write_track(flp, tri, 1);
	cwfloppy_unlock_floppy(flp);
	return (result);
	}



/****************************************************************************
 * cwfloppy_char_open
 ****************************************************************************/
static int
cwfloppy_char_open(
	struct inode			*inode,
	struct file			*file)

	{
	struct cw_floppies		*fls;
	struct cw_floppy		*flp;
	int				f, minor = MINOR(inode->i_rdev);
	int				select1, select2;
	unsigned long			flags;

	cw_debug(1, "[c%df%d] open()", get_controller(minor), get_floppy(minor));
	fls = cw_get_floppies(get_controller(minor));
	f   = get_floppy(minor);
	if ((fls == NULL) || (f >= CW_MAX_FLOPPIES_PER_CONTROLLER)) return (-ENODEV);
	flp = &fls->flp[f];
	if ((get_format(minor) != CW_FLOPPY_FORMAT_RAW) || (flp->model == CW_FLOPPY_MODEL_NONE)) return (-ENODEV);

	/*
	 * check if host controller accessed the floppies in the past or is
	 * still accessing them, host select is latched, so we need to read
	 * it twice to get current state, if host controller accessed a
	 * floppy the head position may have changed, we mark it as dirty
	 * and recalibrate the floppy on next access (this is only relevant
	 * with mk4 controllers)
	 */

	spin_lock_irqsave(&fls->lock, flags);
	select1 = cwhardware_floppy_host_select(&cnt_hrd);
	select2 = cwhardware_floppy_host_select(&cnt_hrd);
	select1 |= select2;
	if (fls->mux)
		{
		if (select1 & 1) fls->flp[0].track_dirty = 1;
		if (select1 & 2) fls->flp[1].track_dirty = 1;
		if (select2)
			{
			spin_unlock_irqrestore(&fls->lock, flags);
			return (-EBUSY);
			}
		cwhardware_floppy_mux_off(&cnt_hrd);
		fls->mux = 0;
		}
	fls->open++;
	spin_unlock_irqrestore(&fls->lock, flags);

	/* open succeeded */

	cw_debug(1, "[c%df%d] open() succeeded", cnt_num, flp->num);
	file->private_data = flp;
	return (0);
	}



/****************************************************************************
 * cwfloppy_char_release
 ****************************************************************************/
static int
cwfloppy_char_release(
	struct inode			*inode,
	struct file			*file)

	{
	struct cw_floppy		*flp = (struct cw_floppy *) file->private_data;
	unsigned long			flags;

	cw_debug(1, "[c%df%d] close()", cnt_num, flp->num);
	spin_lock_irqsave(&flp->fls->lock, flags);
	flp->fls->open--;
	spin_unlock_irqrestore(&flp->fls->lock, flags);
	return (0);
	}



/****************************************************************************
 * cwfloppy_char_ioctl
 ****************************************************************************/
static ssize_t
cwfloppy_char_ioctl(
	struct inode			*inode,
	struct file			*file,
	unsigned int			cmd,
	unsigned long			arg)

	{
	struct cw_floppy		*flp = (struct cw_floppy *) file->private_data;
	struct cw_trackinfo		tri;
	struct cw_floppyinfo		fli;
	int				result = -ENOTTY;

	if (cmd == CW_IOC_GFLPARM)
		{
		cw_debug(1, "[c%df%d] ioctl(CW_IOC_GFLPARM, ...)", cnt_num, flp->num);
		result = -EFAULT;
		if (copy_from_user(&fli, (void *) arg, sizeof (struct cw_floppyinfo)) == 0) result = cwfloppy_get_parameters(flp, &fli);
		if ((result == 0) && (copy_to_user((void *) arg, &fli, sizeof (struct cw_floppyinfo)) != 0)) result = -EFAULT;
		}
	else if (cmd == CW_IOC_SFLPARM)
		{
		cw_debug(1, "[c%df%d] ioctl(CW_IOC_SFLPARM, ...)", cnt_num, flp->num);
		if ((file->f_flags & O_ACCMODE) == O_RDONLY) return (-EPERM);
		result = -EFAULT;
		if (copy_from_user(&fli, (void *) arg, sizeof (struct cw_floppyinfo)) == 0) result = cwfloppy_set_parameters(flp, &fli);
		}
	else if (cmd == CW_IOC_READ)
		{
		cw_debug(1, "[c%df%d] ioctl(CW_IOC_READ, ...)", cnt_num, flp->num);
		if ((file->f_flags & O_ACCMODE) == O_WRONLY) return (-EPERM);
		result = -EFAULT;
		if (copy_from_user(&tri, (void *) arg, sizeof (struct cw_trackinfo)) == 0) result = cwfloppy_read_track(flp, &tri);
		}
	else if (cmd == CW_IOC_WRITE)
		{
		cw_debug(1, "[c%df%d] ioctl(CW_IOC_WRITE, ...)", cnt_num, flp->num);
		if ((file->f_flags & O_ACCMODE) == O_RDONLY) return (-EPERM);
		result = -EFAULT;
		if (copy_from_user(&tri, (void *) arg, sizeof (struct cw_trackinfo)) == 0) result = cwfloppy_write_track(flp, &tri);
		}
	return (result);
	}



/****************************************************************************
 * cwfloppy_fops
 ****************************************************************************/
struct file_operations			cwfloppy_fops =
	{
	.owner   = THIS_MODULE,
	.open    = cwfloppy_char_open,
	.release = cwfloppy_char_release,
	.ioctl   = cwfloppy_char_ioctl
	};



/****************************************************************************
 * cwfloppy_init
 ****************************************************************************/
int
cwfloppy_init(
	struct cw_floppies		*fls)

	{
	struct cw_floppy		*flp;
	int				f;

	/*
	 * read host select status twice, because it is latched, the second
	 * read gives the current state
	 */

	cwhardware_floppy_host_select(&fls->cnt->hrd);
	if (cwhardware_floppy_host_select(&fls->cnt->hrd))
		{
		cw_error("[c%d] could not access floppy, host controller currently uses it", fls->cnt->num);
		return (-EBUSY);
		}

	/* disallow host controller accesses */

	cwhardware_floppy_mux_off(&fls->cnt->hrd);

	/* per controller initialization */

	spin_lock_init(&fls->lock);
	init_waitqueue_head(&fls->busy_wq);
	init_waitqueue_head(&fls->step_wq);
	init_waitqueue_head(&fls->rw_wq);
	init_timer(&fls->step_timer);
	init_timer(&fls->mux_timer);
	init_timer(&fls->rw_timer);
	fls->step_timer.function = cwfloppy_step_timer_func;
	fls->mux_timer.function  = cwfloppy_mux_timer_func;
	fls->rw_timer.function   = cwfloppy_rw_timer_func;

	/* per floppy initialization */

	for (f = 0; f < CW_MAX_FLOPPIES_PER_CONTROLLER; f++)
		{
		flp      = &fls->flp[f];
		flp->num = f;
		flp->fls = fls;
		cw_debug(1, "[c%df%d] initializing", cnt_num, flp->num);
		init_waitqueue_head(&flp->busy_wq);
		init_timer(&flp->motor_timer);
		flp->fli                  = cwfloppy_default_parameters();
		flp->motor_timer.function = cwfloppy_motor_timer_func;
		flp->model                = cwfloppy_get_model(flp);
		if (flp->model == CW_FLOPPY_MODEL_NONE) continue;

		cw_notice("[c%df%d] floppy found at 0x%04x", cnt_num, flp->num, cnt_hrd.iobase);
		flp->track_data = (u8 *) vmalloc(flp->fli.size_max);
		if (flp->track_data != NULL) continue;

		cw_error("[c%df%d] could not allocate memory for track buffer", cnt_num, flp->num);
		return (-ENOMEM);
		}

	/* now host controller accesses are allowed again */

	cwhardware_floppy_mux_on(&fls->cnt->hrd);
	fls->mux = 1;
	cwfloppy_add_timer(&fls->mux_timer, jiffies + 2, fls);
	return (0);
	}



/****************************************************************************
 * cwfloppy_exit
 ****************************************************************************/
void
cwfloppy_exit(
	struct cw_floppies		*fls)

	{
	struct cw_floppy		*flp;
	int				f;

	del_timer_sync(&fls->mux_timer);
	for (f = 0; f < CW_MAX_FLOPPIES_PER_CONTROLLER; f++)
		{
		flp = &fls->flp[f];
		if (flp->model == CW_FLOPPY_MODEL_NONE) continue;

		cw_debug(1, "[c%df%d] deinitializing", cnt_num, flp->num);
		del_timer_sync(&flp->motor_timer);
		flp->motor_request = 0;
		if (flp->motor) cwhardware_floppy_motor_off(&cnt_hrd, flp->num);
		if (flp->track_data == NULL) continue;

		vfree(flp->track_data);
		}
	cwhardware_floppy_mux_on(&fls->cnt->hrd);
	fls->mux = 1;
	}
/******************************************************** Karsten Scheibler */
