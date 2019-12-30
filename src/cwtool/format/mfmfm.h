/****************************************************************************
 ****************************************************************************
 *
 * format/mfmfm.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_MFMFM_H
#define CWTOOL_MFMFM_H

#include "import.h"
#include "export.h"

struct fifo;
struct disk_error;

extern const int			mfmfm_decode_table[];
extern const int			mfmfm_encode_table[];
extern int				mfmfm_read_sync(struct fifo *, int, int);
extern int				mfmfm_read_sync2(struct fifo *, int, int);
extern int				mfmfm_write_sync(struct fifo *, int, int);
extern int				mfmfm_write_fill(struct fifo *, int, int, int (*)(struct fifo *, int));
extern int				mfmfm_read_bytes(struct fifo *, struct disk_error *, unsigned char *, int, int (*)(struct fifo *, struct disk_error *, int));
extern int				mfmfm_write_bytes(struct fifo *, unsigned char *, int, int (*)(struct fifo *, int));
extern int				mfmfm_crc16(int, const unsigned char *, int);
extern int				mfmfm_get_sector_shift(unsigned char *, int, int);
extern int				mfmfm_set_sector_size(unsigned char *, int, int, int);
extern int				mfmfm_fill_sector_shift(unsigned char *, int, int, int);

#define mfmfm_read_ushort_be(data)		import_ushort_be(data)
#define mfmfm_write_ushort_be(data, val)	export_ushort_be(data, val)
#define mfmfm_read_ulong_le(data)		import_ulong_le(data)
#define mfmfm_write_ulong_le(data, val)		export_ulong_le(data, val)



#endif /* !CWTOOL_MFMFM_H */
/******************************************************** Karsten Scheibler */
