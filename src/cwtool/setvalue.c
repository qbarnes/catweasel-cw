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
	unsigned int			val,
	unsigned int			low,
	unsigned int			high)

	{
	if ((val < low) || (val > high)) return (0);
	*dst = val;
	return (1);
	}



/****************************************************************************
 * setvalue_uint_bit
 ****************************************************************************/
int
setvalue_uint_bit(
	unsigned int			*dst,
	unsigned int			val,
	unsigned int			bit)

	{
	debug_error_condition((bit < 0) || (bit > 0x80000000) || ((bit & (bit - 1)) != 0));
	if (val == 0) *dst &= ~bit;
	else if (val == 1) *dst |= bit;
	else return (0);
	return (1);
	}
/******************************************************** Karsten Scheibler */
