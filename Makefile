# dmenu - dynamic menu
# See LICENSE file for copyright and license details.

include config.mk

CFLAGS += `pkg-config --cflags ${PACKAGES}`
LDFLAGS += `pkg-config --libs ${PACKAGES}`

dmenu: controller.o dmenu.o inputbuffer.o util.o viewer.o x.o xcmd.o
	$(CC) -o $@ ${LDFLAGS} $?
