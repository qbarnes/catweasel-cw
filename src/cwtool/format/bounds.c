/****************************************************************************
 ****************************************************************************
 *
 * format/bounds.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "format/bounds.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"



/****************************************************************************
 * bounds_set_read_low
 ****************************************************************************/
int
bounds_set_read_low(
	struct bounds			*bnd_last,
	struct bounds			*bnd,
	int				read_low)

	{
	if ((read_low < 0x0100) || (read_low > 0x7fff)) return (0);
	if (bnd_last != NULL) if (bnd_last->read_high >= read_low) return (0);
	bnd->read_low = read_low;
	return (1);
	}



/****************************************************************************
 * bounds_set_write
 ****************************************************************************/
int
bounds_set_write(
	struct bounds			*bnd,
	int				write)

	{
	if ((write < 0x0300) || (write > 0x7fff) || (bnd->read_low > write)) return (0);
	bnd->write = write;
	return (1);
	}



/****************************************************************************
 * bounds_set_read_high
 ****************************************************************************/
int
bounds_set_read_high(
	struct bounds			*bnd,
	int				read_high)

	{
	if ((read_high < 0x0100) || (read_high > 0x7fff) || (bnd->write > read_high)) return (0);
	bnd->read_high = read_high;
	return (1);
	}
/******************************************************** Karsten Scheibler */
