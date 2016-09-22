# dmenu version
VERSION = 4.6x

# Install program (-D Create paths recursively)
INSTALL=install -D --verbose --preserve-timestamps
RM=rm -fv

# Paths (they're DESTDIR sensible)
PREFIX = ${DESTDIR}/usr
MANPREFIX = ${PREFIX}/share/man

# Select packages
PACKAGES = glib-2.0 fontconfig libconfig x11 xft

# Xinerama, comment if you don't want it
PACKAGES += xinerama
CFLAGS += -DXINERAMA

# Flags
# CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
CPPFLAGS = -DVERSION=\"${VERSION}\"
# CFLAGS   = -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
CFLAGS += -Wall ${CPPFLAGS}

RELEASE_CFLAGS = -g0 -O2 -DNDEBUG
DEBUG_CFLAGS = -g3 -O0

# Compiler and linker
CC = cc
