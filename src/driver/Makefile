#DEBUG=-DCW_DEBUG

OBJECTS=cwmain.o cwhardware.o cwfloppy.o
TARGET24=../../module/cw.o
TARGET26=../../module/cw.ko

ifneq (${KERNELRELEASE},)
EXTRA_CFLAGS:=${DEBUG} -I${CW_DIR}/../include
obj-m:=cw.o
cw-y:=${OBJECTS}
else
KERNEL_DIR=/lib/modules/${shell uname -r}/build
VERSION=${shell uname -r | awk 'BEGIN { FS="."; }{ print($$1 $$2); }'}
INCLUDE=-I${KERNEL_DIR}/include -I../include
MAKE=make
CWD=${shell pwd}
CC=gcc -Wall -O2 -D__KERNEL__ -DMODULE ${DEBUG} ${INCLUDE}
LD=ld -r -s
RM=rm -f
CP=cp -f
LN=ln -f

.PHONY: all linux24 linux26 clean

all:
	if [ "${VERSION}" = "26" ]; then ${MAKE} linux26; else ${MAKE} linux24; fi

linux24: ${TARGET24}

linux26: cwmain.c
	CW_DIR=${CWD} ${MAKE} -C ${KERNEL_DIR} M=${CWD} modules && ${CP} cw.ko ${TARGET26}

# Linux 2.6 will create a cw.o containing all *.o files this will conflict
# with the object file generated from cw.c, so cw.c is simply linked to
# cwmain.c before compiling

cwmain.c:
	${LN} -s cw.c cwmain.c

%.o: %.c
	${CC} -c -o $@ $<

${TARGET24}: ${OBJECTS}
	${LD} -o ${TARGET24} ${OBJECTS}

clean:
	if [ "${VERSION}" = "26" ]; then ${MAKE} -C ${KERNEL_DIR} M=${CWD} clean; fi
	${RM} ${TARGET24} ${TARGET26} ${OBJECTS} cwmain.c *~ *.bak *.ko
endif
