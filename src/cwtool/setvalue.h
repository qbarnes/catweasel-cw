/****************************************************************************
 ****************************************************************************
 *
 * setvalue.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_SETVALUE_H
#define CWTOOL_SETVALUE_H

struct bounds;

extern int				setvalue_uchar_bit(unsigned char *, int, int);
extern int				setvalue_uchar(unsigned char *, int, int, int);
extern int				setvalue_ushort(unsigned short *, int, int, int);
extern int				setvalue_short(short *, int, int, int);
extern int				setvalue_uint(unsigned int *, int, int, int);
extern int				setvalue_bounds(struct bounds *, int, int);



#endif /* !CWTOOL_SETVALUE_H */
/******************************************************** Karsten Scheibler */
