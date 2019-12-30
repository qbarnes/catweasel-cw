MAKE=make
SH=sh
RM=rm -f
INSTALL=${SH} tools/install.sh
UNINSTALL=${SH} tools/uninstall.sh

.PHONY: all clean

all:
	${MAKE} -C src all

clean:
	${RM} .uninstall
	${MAKE} -C src clean

install:
	${INSTALL}

uninstall:
	${UNINSTALL}
