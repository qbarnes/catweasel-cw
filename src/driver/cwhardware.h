/****************************************************************************
 ****************************************************************************
 *
 * cwhardware.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWHARDWARE_H
#define CWHARDWARE_H

#include <linux/config.h>
#include <linux/pci.h>

struct cw_hardware;
struct cw_trackinfo;

#ifdef CONFIG_PCI
extern struct pci_driver		cwhardware_mk3_pci_driver;
extern struct pci_driver		cwhardware_mk4_pci_driver;
#endif /* CONFIG_PCI */
extern int				cwhardware_floppy_host_select(struct cw_hardware *);
extern void				cwhardware_floppy_mux_on(struct cw_hardware *);
extern void				cwhardware_floppy_mux_off(struct cw_hardware *);
extern void				cwhardware_floppy_motor_on(struct cw_hardware *, int);
extern void				cwhardware_floppy_motor_off(struct cw_hardware *, int);
extern void				cwhardware_floppy_select(struct cw_hardware *, int, int, int);
extern void				cwhardware_floppy_step(struct cw_hardware *, int, int);
extern int				cwhardware_floppy_track0(struct cw_hardware *);
extern int				cwhardware_floppy_write_protected(struct cw_hardware *);
extern int				cwhardware_floppy_disk_changed(struct cw_hardware *);
extern void				cwhardware_floppy_abort(struct cw_hardware *);
extern int				cwhardware_floppy_busy(struct cw_hardware *);
extern void				cwhardware_floppy_read_track_start(struct cw_hardware *, int, int);
extern int				cwhardware_floppy_read_track_copy(struct cw_hardware *, unsigned char *, int);
extern int				cwhardware_floppy_write_track(struct cw_hardware *, int, int, unsigned char *, int);



#endif /* !CWHARDWARE_H */
/******************************************************** Karsten Scheibler */
