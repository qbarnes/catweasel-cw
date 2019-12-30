/****************************************************************************
 ****************************************************************************
 *
 * cwmac.h
 *
 ****************************************************************************
 *
 * This file supports the dispatcher between cwtool's ioctl() interface to
 * the Linux driver and instead routes requests to the Catweasel PCI driver
 * as delivered by the catweasel-osx sub-project CatweaselMK4.  The low-level
 * interfaces are not very different, but the calling conventions are of
 * course completely different.  This set of routines provides that glue.
 *
 * In order to build this layer, the catweasel-osx sub-project cw2dmk-osx
 * should be checked out as a peer to cw, and the catweasel-osx sub-project
 * CatweaselMK4 should be built and available in the builder's home directory.
 *
 ****************************************************************************
 ****************************************************************************/

#include "ioctl.h"
#include "cw2dmk-osx/cwfloppy.h"
#include "cw2dmk-osx/cwpci.h"

int cwmac_open(char *path,
		struct cw_floppyinfo *fli,
		catweasel_contr **c,
		int *drive);

void cwmac_close(catweasel_contr *c);

void cwmac_abort();

void cwmac_interrupt(int sig_no);

int cwmac_ioctl(cw_mode_t cmd, cw_ptr_t arg, cw_flag_t flags, catweasel_contr *c, int drive);

int cwmac_read_track(struct cw_trackinfo *tri, catweasel_contr *c, int drive);

int cwmac_write_track(struct cw_trackinfo *tri, catweasel_contr *c, int drive);

/************************************************************ David Schmidt */
