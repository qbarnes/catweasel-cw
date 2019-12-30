#DEBUG=-DCWTOOL_DEBUG

INCLUDE=-I../include
CC=gcc -Wall -O2 ${DEBUG} ${INCLUDE}
STRIP=strip -R .note -R .comment
RM=rm -f
SH=sh
CONVERT=${SH} ../../tools/convert.sh

CONFIG=../../conf/cwtoolrc.default
OBJECTS=cwtool.o error.o debug.o verbose.o config.o format.o disk.o  \
	string.o fifo.o file.o raw.o setvalue.o bounds.o  \
	mfmfm.o mfm.o fm.o fm_nec765.o mfm_nec765.o mfm_amiga.o  \
	gcr_apple.o gcr_cbm.o tbe_cw.o
TARGET=../../bin/cwtool

.PHONY: all clean

all: ${TARGET}

cwtoolrc.c: ${CONFIG}
	${CONVERT} < ${CONFIG} > cwtoolrc.c

config.o: cwtoolrc.c

%.o: %.c
	${CC} -c -o $@ $<

${TARGET}: ${OBJECTS}
	${CC} -o ${TARGET} ${OBJECTS}
	${STRIP} ${TARGET}

clean:
	${RM} ${TARGET} ${OBJECTS} cwtoolrc.c *~ *.bak
