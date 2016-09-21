# dmenu - dynamic menu
# See LICENSE file for copyright and license details.

include config.mk

CFLAGS += `pkg-config --cflags ${PACKAGES}`
LDFLAGS += `pkg-config --libs ${PACKAGES}`

dmenu: dmenu.o inputbuffer.o util.o xcmd.o
	$(CC) -o $@ ${LDFLAGS} $?
