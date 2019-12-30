/****************************************************************************
 ****************************************************************************
 *
 * ioctl.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CW_IOCTL_H
#define CW_IOCTL_H

/*
 * stdlib.h is not really needed here, but some glibc-versions cause compile
 * time warnings like "__NFDBITS redefined" if stdlib.h is included later
 */

#ifndef __KERNEL__
#include <stdlib.h>
#endif /* !__KERNEL__ */

#include <linux/ioctl.h>
#include <linux/types.h>

#define CW_IOC_MAGIC			0xca
#define CW_IOC_GFLPARM			_IOR(CW_IOC_MAGIC, 0, struct cw_floppyinfo)
#define CW_IOC_SFLPARM			_IOW(CW_IOC_MAGIC, 1, struct cw_floppyinfo)
#define CW_IOC_READ			_IOW(CW_IOC_MAGIC, 2, struct cw_trackinfo)
#define CW_IOC_WRITE			_IOW(CW_IOC_MAGIC, 3, struct cw_trackinfo)

/*
 * if structure or semantics of data changes, which is exchanged between
 * the driver and cwtool, then CW_STRUCT_VERSION is increased. so only
 * matching driver and cwtool versions work together:
 *
 *  0    cw-0.08 - cw-0.10
 *  1    cw-0.11 -
 */

#define CW_STRUCT_VERSION		1

/*
 * UGLY: should be CW_NR_TRACKS or 85, because 86 is never reached, but other
 *       max-parameters use the last value, like CW_MAX_SETTLE_TIME
 */

#define CW_MAX_TRACK			86
#define CW_MAX_SIDE			2
#define CW_MAX_CLOCK			3
#define CW_MAX_MODE			3
#define CW_MAX_SIZE			0x20000
#define CW_WRITE_OVERHEAD		8
#define CW_MIN_TIMEOUT			50
#define CW_DEFAULT_TIMEOUT		500
#define CW_MAX_TIMEOUT			8000
#define CW_MIN_SETTLE_TIME		1
#define CW_DEFAULT_SETTLE_TIME		25
#define CW_MAX_SETTLE_TIME		1000
#define CW_MIN_STEP_TIME		3
#define CW_DEFAULT_STEP_TIME		6
#define CW_MAX_STEP_TIME		1000
#define CW_MIN_RPM			100
#define CW_MAX_RPM			500

#define CW_FLOPPYINFO_FLAG_INVERTED_DISKCHANGE	(1 << 0)
#define CW_FLOPPYINFO_FLAG_IGNORE_DISKCHANGE	(1 << 1)
#define CW_FLOPPYINFO_FLAG_DENSITY		(1 << 2)
#define CW_FLOPPYINFO_FLAG_DOUBLE_STEP		(1 << 3)
#define CW_FLOPPYINFO_FLAG_ALL			((1 << 4) - 1)
#define CW_FLOPPYINFO_INIT			(struct cw_floppyinfo) { .version = CW_STRUCT_VERSION }

struct cw_floppyinfo
	{
	__u32				version;
	__u32				settle_time;
	__u32				step_time;
	__u8				track_max;
	__u8				side_max;
	__u8				clock_max;
	__u8				mode_max;
	__u32				size_max;
	__u32				rpm;
	__u32				flags;
	};

#define CW_TRACKINFO_CLOCK_14MHZ	0
#define CW_TRACKINFO_CLOCK_28MHZ	1
#define CW_TRACKINFO_CLOCK_56MHZ	2
#define CW_TRACKINFO_MODE_NORMAL	0
#define CW_TRACKINFO_MODE_INDEX_WAIT	1
#define CW_TRACKINFO_MODE_INDEX_STORE	2
#define CW_TRACKINFO_INIT		(struct cw_trackinfo) { .version = CW_STRUCT_VERSION }

struct cw_trackinfo
	{
	__u32				version;
	__u8				track;
	__u8				side;
	__u8				clock;
	__u8				mode;
	__u32				flags;
	__u32				timeout;
	__u8				*data;
	__u32				size;
	};



#endif /* !CW_IOCTL_H */
/******************************************************** Karsten Scheibler */
