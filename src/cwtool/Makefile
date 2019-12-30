#DEBUG=-DCWTOOL_DEBUG

CC=${DIET} gcc -s -Wall -O2 -I../include ${DEBUG}
STRIP=strip -R .note -R .comment
RM=rm -f
BASH=bash
CONVERT=${BASH} ../../tools/convert.bash

CONFIG=../../conf/cwtoolrc.default
OBJECTS=cwtool.o error.o debug.o verbose.o disk.o drive.o  \
	string.o fifo.o file.o import.o export.o setvalue.o  \
	config.o config/disk.o config/drive.o  \
	image.o image/raw.o image/g64.o image/d64.o image/plain.o  \
	format.o format/setvalue.o format/bounds.o format/mfmfm.o  \
	format/mfm.o format/fm.o format/raw.o format/fill.o  \
	format/fm_nec765.o format/mfm_nec765.o format/mfm_amiga.o  \
	format/gcr_apple.o format/gcr_cbm.o format/gcr_g64.o  \
	format/tbe_cw.o
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
