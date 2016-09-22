# dmenu - dynamic menu
# See LICENSE file for copyright and license details.

include config.mk

CFLAGS += `pkg-config --cflags ${PACKAGES}`
LDFLAGS += `pkg-config --libs ${PACKAGES}`

RELEASE_CFLAGS = -g0 -O2 -DNDEBUG
DEBUG_CFLAGS = -g3 -O0

dmenu-release: CFLAGS += ${RELEASE_CFLAGS}
dmenu-release: dmenu

dmenu-debug: CFLAGS += ${DEBUG_CFLAGS}
dmenu-debug: dmenu

dmenu: controller.o dmenu.o inputbuffer.o util.o viewer.o x.o xcmd.o
	$(CC) -o $@ ${LDFLAGS} $?
