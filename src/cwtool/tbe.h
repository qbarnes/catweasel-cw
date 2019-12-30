/****************************************************************************
 ****************************************************************************
 *
 * tbe.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_TBE_H
#define CWTOOL_TBE_H

#include "mfmfm.h"

#define tbe_read_ushort_be(data)	mfmfm_read_ushort_be(data)
#define tbe_write_ushort_be(data, val)	mfmfm_write_ushort_be(data, val)
#define tbe_crc16(init, data, size)	mfmfm_crc16(init, data, size)



#endif /* !CWTOOL_TBE_H */
/******************************************************** Karsten Scheibler */
