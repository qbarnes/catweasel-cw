/****************************************************************************
 ****************************************************************************
 *
 * cwmac.c
 *
 ****************************************************************************
 *
 * This file serves as a dispatcher between cwtool's ioctl() interface to the
 * Linux driver and instead routes requests to the Catweasel PCI driver as
 * delivered by the catweasel-osx sub-project CatweaselMK4.  The low-level
 * interfaces are not very different, but the calling conventions are of
 * course completely different.  This set of routines provides that glue.
 *
 * In order to build this layer, the catweasel-osx sub-project cw2dmk-osx
 * should be checked out as a peer to cw, and the catweasel-osx sub-project
 * CatweaselMK4 should be built and available in the builder's home directory.
 *
 ****************************************************************************
 ****************************************************************************/

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/errno.h>
#include "../string.h"
#include "../error.h"
#include "cwmac.h"
#include "ioctl.h"

struct sigaction old_action;
static catweasel_contr *CW_CONTROLLER = NULL;

/****************************************************************************
 * ipow
 *
 * Returns the integer result of x raised to the ith power
 ****************************************************************************/
int ipow (int x, unsigned i)
{
	double result = 1.0;

	while (i)
	{
		if (i & 1)
		result *= x;
		x *= x;
		i >>= 1;
	}

	return result;
}
 
/****************************************************************************
 * cwmac_ioctl
 ****************************************************************************/
int cwmac_ioctl(cw_mode_t cmd, cw_ptr_t arg, cw_flag_t flags, catweasel_contr *c, int drive)
{
	int rc = -1;
	switch (cmd)
	{
		case CW_IOC_GFLPARM:
			fprintf(stderr,"CW_IOC_GFLPARM (unimplemented) \n");
			break;
		case CW_IOC_SFLPARM:
			fprintf(stderr,"CW_IOC_SFLPARM (unimplemented) \n");
			break;
		case CW_IOC_READ: 
			rc = cwmac_read_track((struct cw_trackinfo*)arg, c, drive);
			break;
		case CW_IOC_WRITE:
			rc = cwmac_write_track((struct cw_trackinfo*)arg, c, drive);
			break;
		default:
			fprintf(stderr,"cwtool cwmac_ioctl(): Unknown command!\n");
			rc = -1;
			break;
	}
	return rc;
}

/****************************************************************************
 * cwmac_open
 *
 * Prepares the specified Catweasel controller for communications if it is
 * found within the system.
 ****************************************************************************/
int cwmac_open(char *path,
		struct cw_floppyinfo *fli,
		catweasel_contr **ppc,
		int *drive)
{
	int rc = 0;
	int ch = 0;
	int cw_port = 0;
	int cw_drive = 0;
	int cw_mk = 1;
	catweasel_contr *c;

	/* Allocate space for the controller - to be freed by cwmac_close */
	*ppc = malloc(sizeof(catweasel_contr));	
	c = *ppc;
	if (string_equal(path,"Cat1d0") || string_equal(path,"Cat1d1"))
		cw_port = 1;
	if (string_equal(path,"Cat0d1") || string_equal(path,"Cat1d1"))
		cw_drive = 1;
	cw_port = pci_find_catweasel(cw_port, &cw_mk);
	if (cw_port==-1)
	{
		rc = -1;
		fprintf(stderr, "cwtool: No Catweasel found.\n");
	}
	else
	{
		ch = catweasel_init_controller(*ppc, cw_port, cw_mk, getenv("CW4FIRMWARE")) && catweasel_memtest(*ppc);
		if (ch)
		{
			printf("cwtool: Catweasel %d detected.\n",cw_port);
			CW_CONTROLLER = c;
			fli->settle_time = CW_DEFAULT_SETTLE_TIME;
			fli->step_time = CW_DEFAULT_STEP_TIME;
			fli->wpulse_length = CW_DEFAULT_WPULSE_LENGTH;
			fli->nr_clocks = CW_NR_CLOCKS;
			fli->nr_tracks = CW_NR_TRACKS;
			fli->nr_sides = CW_NR_SIDES;;
			fli->nr_modes = CW_NR_MODES;
			fli->max_size = CW_MAX_TRACK_SIZE;
			fli->rpm = 300;
		}
		catweasel_detect_drive(&c->drives[cw_drive]);
		if (c->drives[cw_drive].type != 1)
		{
			rc = -1;
			fprintf(stderr, "cwtool: Catweasel drive not attached.\n");
		}
		else
		{
			/*
			 * Since cwtool assumes it can exit at any time, 
			 * register an exit handler to be called so we can 
			 * shut off the drive in case of error.
			 */
			atexit(&cwmac_abort);
			/*
			 * Also register a signal handler for ctrl-c.
			 */
			struct sigaction action;
			memset(&action, 0, sizeof(action));
			action.sa_handler = &cwmac_interrupt;
			sigaction(SIGINT, &action, &old_action);

			*drive = cw_drive;
			catweasel_select(c, !cw_drive, cw_drive);
			catweasel_set_motor(&c->drives[cw_drive], 1);
			catweasel_usleep(500000);
		}
	}
	return rc;
}

/****************************************************************************
 * cwmac_close
 *
 * Shuts down the selected Catweasel controller and stops any spinning drive
 ****************************************************************************/
void cwmac_close(catweasel_contr *c)
{
	if (c)
	{
		catweasel_select(c, 0, 0);
		catweasel_free_controller(c);
		CW_CONTROLLER = NULL;
	}
	pci_exit_catweasel();
}

/****************************************************************************
 * cwmac_read_track
 *
 * Returns: number of bytes read
 ****************************************************************************/
int cwmac_read_track(struct cw_trackinfo *tri, catweasel_contr *c, int drive)
{
	int i = 0;
	/*
	printf("seek: %d track: %d side: %d clock: %d mode: %d\n",
		tri->track_seek,
		tri->track,
		tri->side,
		tri->clock,
		tri->mode);
	*/
	catweasel_seek(&c->drives[drive],tri->track);

	catweasel_read(&c->drives[drive],tri->side,ipow(2,(tri->clock)),0,0);
	for(i=0; i<CW_MEMSIZE-8; i++)
	{
		tri->data[i] = catweasel_get_byte(c);
		if ((i > 1) && (tri->data[i-1] == 0x80) && (tri->data[i-0] == 0x00))
		{
			break;
		}
	}
	return i;
}

/****************************************************************************
 * cwmac_write_track
 *
 * Returns: number of bytes written
 ****************************************************************************/
int cwmac_write_track(struct cw_trackinfo *tri, catweasel_contr *c, int drive)
{
	int i = 0;
	/*
	printf("seek: %d track: %d side: %d clock: %d mode: %d size: %d flags: %d\n",
		tri->track_seek,
		tri->track,
		tri->side,
		tri->clock,
		tri->mode,
		tri->size,
		tri->flags);
	*/
	if (!catweasel_write_protected(&c->drives[drive]))
	{	
		catweasel_abort(c);
		catweasel_reset_pointer(c);
		for(i=0; i<tri->size; i++)
		{
			if (catweasel_put_byte(c,tri->data[i]))
			{
				error_perror_message("error while writing data to catweasel");
			}
		}
		catweasel_set_hd(c, 1);
		catweasel_seek(&c->drives[drive],tri->track);
		if (catweasel_write(&c->drives[drive],tri->side,ipow(2,(tri->clock)),-1))
		{
			error_perror_message("error while writing track to disk");
		}
	}
	else
		i = -EROFS;
	return i;
}

/****************************************************************************
 * cwmac_abort
 *
 * Shuts down the selected Catweasel controller and stops any spinning drive
 ****************************************************************************/
void cwmac_abort()
{
	cwmac_close(CW_CONTROLLER);
}

/****************************************************************************
 * cwmac_interrupt
 *
 * Shuts down the selected Catweasel controller and stops any spinning drive
 ****************************************************************************/
void cwmac_interrupt(int sig_no)
{
	cwmac_close(CW_CONTROLLER);
	sigaction(SIGINT, &old_action, NULL);
	kill(0, SIGINT);
}

/************************************************************ David Schmidt */
