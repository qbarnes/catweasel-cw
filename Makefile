MAKE=make
BASH=bash
RM=rm -f
INSTALL=${BASH} tools/install.bash
UPGRADE=${BASH} tools/upgrade.bash
UNINSTALL=${BASH} tools/uninstall.bash

.PHONY: all clean

all:
	${MAKE} -C src all DIET="${DIET}"

clean:
	${RM} .uninstall
	${MAKE} -C src clean

install:
	${INSTALL}

upgrade:
	${UPGRADE}

uninstall:
	${UNINSTALL}
