/****************************************************************************
 ****************************************************************************
 *
 * format/bounds.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_BOUNDS_H
#define CWTOOL_BOUNDS_H

#define BOUNDS(l, w, h, c)		(struct bounds) { .read_low = l, .write = w, .read_high = h, .count = c }

struct bounds
	{
	unsigned short			read_low;
	unsigned short			write;
	unsigned short			read_high;
	unsigned short			count;
	};

extern int				bounds_set_read_low(struct bounds *, struct bounds *, int);
extern int				bounds_set_write(struct bounds *, int);
extern int				bounds_set_read_high(struct bounds *, int);



#endif /* !CWTOOL_BOUNDS_H */
/******************************************************** Karsten Scheibler */
