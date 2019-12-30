/****************************************************************************
 ****************************************************************************
 *
 * raw.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_RAW_H
#define CWTOOL_RAW_H

struct bounds;

#define RAW_COUNTER_INIT(b, p, s)	(struct raw_counter) { .bnd = b, .precomp = p, .bnd_size = s }

struct raw_counter
	{
	int				last;
	int				this;
	int				last_i;
	struct bounds			*bnd;
	short				*precomp;
	int				bnd_size;
	};

struct fifo;

extern int				raw_read_lookup(struct bounds *, int, int *);
extern int				raw_read_counter(struct fifo *, int *);
extern int				raw_write_counter(struct fifo *, struct raw_counter *, int);
extern int				raw_read(struct fifo *, struct fifo *, struct bounds *, int);
extern int				raw_write(struct fifo *, struct fifo *, struct bounds *, short *, int);
extern int				raw_histogram(struct fifo *, int);
extern int				raw_precomp_statistics(struct fifo *, struct bounds *, int);



#endif /* !CWTOOL_RAW_H */
/******************************************************** Karsten Scheibler */
