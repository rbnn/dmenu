# dmenu version
VERSION = 4.6x

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# Select packages
PACKAGES = glib-2.0 fontconfig libconfig x11 xft

# Xinerama, comment if you don't want it
PACKAGES += xinerama
CFLAGS += -DXINERAMA

# flags
# CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
CPPFLAGS = -DVERSION=\"${VERSION}\"
# CFLAGS   = -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
CFLAGS += -Wall -g3 -O0 ${CPPFLAGS}
# LDFLAGS  = -s ${LIBS}

# compiler and linker
CC = cc
