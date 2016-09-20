# dmenu - dynamic menu
# See LICENSE file for copyright and license details.

include config.mk

CFLAGS += `pkg-config --cflags ${PACKAGES}`
LDFLAGS += `pkg-config --libs ${PACKAGES}`

dmenu: dmenu.o util.o
	$(CC) -o $@ ${LDFLAGS} $?
