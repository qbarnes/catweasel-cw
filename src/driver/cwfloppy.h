/****************************************************************************
 ****************************************************************************
 *
 * cwfloppy.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWFLOPPY_H
#define CWFLOPPY_H

#include <linux/fs.h>

struct cw_floppies;

extern struct file_operations		cwfloppy_fops;
extern int				cwfloppy_init(struct cw_floppies *);
extern void				cwfloppy_exit(struct cw_floppies *);



#endif /* !CWFLOPPY_H */
/******************************************************** Karsten Scheibler */
