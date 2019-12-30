# make DEBUG=1		to build cwtool with debugging messages(to increase
#			debug level use the command line option -d)
#
# make STANDALONE=1	to build cwtool standalone on non-linux platforms
#
# make CATWEASEL_OSX=1	to build cwtool together with catweasel MK3 and MK4 Mac
#			OS X drivers

PREFIX:=../..
include ${PREFIX}/Makefile.conf

CC:=${DIET} gcc -s -Wall -O2 -I${BUILD_INCLUDE_DIR}
LIBRARIES:=
STRIP:=strip -R .note -R .comment

ifdef DEBUG
CC+=-DCWTOOL_DEBUG
endif

ifdef STANDALONE
CC+=-DCW_STANDALONE
endif

ifdef CATWEASEL_OSX
LIBRARIES+=-F${BUILD_OSX_FRAMEWORK_DIR} -framework CatweaselMK4 -framework IOKit
CC+=-DCW_CATWEASEL_OSX -I${BUILD_OSX_INCLUDE_DIR}
endif

CONFIG:=${BUILD_CONF_DIR}/cwtoolrc.default
FILES:=cwtool error debug verbose global cmdline options trackmap disk  \
	drive string fifo file import export setvalue parse  \
	config config/disk config/drive config/options config/trackmap  \
	image image/raw image/g64 image/d64 image/plain  \
	format format/setvalue format/bounds format/crc16 format/mfmfm  \
	format/mfm format/fm format/raw format/fill format/fm_nec765  \
	format/mfm_nec765 format/mfm_amiga format/gcr_apple  \
	format/gcr_apple_test format/gcr_cbm format/gcr_g64  \
	format/gcr_v9000 format/tbe_cw format/postcomp_simple  \
	format/histogram format/match_simple format/container format/range  \
	format/bitstream

ifdef CATWEASEL_OSX
FILES+=osx/cwmac ${BUILD_OSX_CW2DMK_DIR}/cwpci ${BUILD_OSX_CW2DMK_DIR}/catweasl
endif

OBJECTS:=${patsubst %, %.o, ${FILES}}
TARGET:=${BUILD_BIN_DIR}/cwtool

.PHONY: all clean

all: ${TARGET}

cwtoolrc.c: ${CONFIG}
	${CONVERT_BASH} < ${CONFIG} > cwtoolrc.c

config.o: cwtoolrc.c

%.o: %.c
	${CC} -c -o $@ $<

${TARGET}: ${OBJECTS}
	${CC} -o ${TARGET} ${OBJECTS} ${LIBRARIES}
	${STRIP} ${TARGET} || true

clean:
	${RM} ${TARGET} ${OBJECTS} cwtoolrc.c *~ *.bak
