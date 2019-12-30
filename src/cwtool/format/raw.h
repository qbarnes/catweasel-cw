/****************************************************************************
 ****************************************************************************
 *
 * format/raw.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_RAW_H
#define CWTOOL_RAW_H

#include "../format.h"

struct bounds;

#define RAW_COUNTER_INIT(b, p, s)	(struct raw_counter) { .bnd = b, .precomp = p, .bnd_size = s }

struct raw_counter
	{
	int				last;
	int				this;
	int				last_i;
	int				invalid;
	struct bounds			*bnd;
	short				*precomp;
	int				bnd_size;
	};

struct fifo;

extern struct format_desc		raw_format_desc;
extern int				raw_read_lookup(struct bounds *, int, int *);
extern int				raw_read_counter(struct fifo *, int *);
extern int				raw_write_counter(struct fifo *, struct raw_counter *, int);
extern int				raw_postcomp(struct fifo *, struct bounds *, int);
extern int				raw_read(struct fifo *, struct fifo *, struct bounds *, int);
extern int				raw_write(struct fifo *, struct fifo *, struct bounds *, short *, int);
extern int				raw_histogram(struct fifo *, int, int);
extern int				raw_postcomp_histogram(struct fifo *, struct bounds *, int, int, int);
extern int				raw_precomp_statistics(struct fifo *, struct bounds *, int);



#endif /* !CWTOOL_RAW_H */
/******************************************************** Karsten Scheibler */
