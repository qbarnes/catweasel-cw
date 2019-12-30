/****************************************************************************
 ****************************************************************************
 *
 * setvalue.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "setvalue.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "bounds.h"



/****************************************************************************
 * setvalue_uchar_bit
 ****************************************************************************/
int
setvalue_uchar_bit(
	unsigned char			*dst,
	int				val,
	int				bit)

	{
	debug_error_condition((bit < 0) || (bit > 0x80) || ((bit & (bit - 1)) != 0));
	if (val == 0) *dst &= ~bit;
	else if (val == 1) *dst |= bit;
	else return (0);
	return (1);
	}



/****************************************************************************
 * setvalue_uchar
 ****************************************************************************/
int
setvalue_uchar(
	unsigned char			*dst,
	int				val,
	int				low,
	int				high)

	{
	if ((val < low) || (val > high)) return (0);
	*dst = val;
	return (1);
	}



/****************************************************************************
 * setvalue_ushort
 ****************************************************************************/
int
setvalue_ushort(
	unsigned short			*dst,
	int				val,
	int				low,
	int				high)

	{
	if ((val < low) || (val > high)) return (0);
	*dst = val;
	return (1);
	}



/****************************************************************************
 * setvalue_short
 ****************************************************************************/
int
setvalue_short(
	short				*dst,
	int				val,
	int				low,
	int				high)

	{
	if ((val < low) || (val > high)) return (0);
	*dst = val;
	return (1);
	}



/****************************************************************************
 * setvalue_uint
 ****************************************************************************/
int
setvalue_uint(
	unsigned int			*dst,
	int				val,
	int				low,
	int				high)

	{
	if ((val < low) || (val > high)) return (0);
	*dst = val;
	return (1);
	}



/****************************************************************************
 * setvalue_bounds
 ****************************************************************************/
int
setvalue_bounds(
	struct bounds			*bnd,
	int				val,
	int				ofs)

	{
	int				i = ofs % 3, j = ofs / 3;
	struct bounds			*bnd_last = (j > 0) ? &bnd[j - 1] : NULL;

	if (i == 0) return (bounds_set_read_low(bnd_last, &bnd[j], val));
	if (i == 1) return (bounds_set_write(&bnd[j], val));
	return (bounds_set_read_high(&bnd[j], val));
	}
/******************************************************** Karsten Scheibler */
