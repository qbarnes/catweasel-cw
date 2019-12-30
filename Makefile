MAKE=make
SH=sh
RM=rm -f
INSTALL=${SH} tools/install.sh
UPGRADE=${SH} tools/upgrade.sh
UNINSTALL=${SH} tools/uninstall.sh

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
