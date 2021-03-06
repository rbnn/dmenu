# dmenu - dynamic menu
# See LICENSE file for copyright and license details.
.PHONY: dmenu-{debug,release} install uninstall

include config.mk

CFLAGS += `pkg-config --cflags ${PACKAGES}`
LDFLAGS += `pkg-config --libs ${PACKAGES}`

dmenu-release: CFLAGS += ${RELEASE_CFLAGS}
dmenu-release: dmenu

dmenu-debug: CFLAGS += ${DEBUG_CFLAGS}
dmenu-debug: dmenu

dmenu: controller.o dmenu.o inputbuffer.o util.o viewer.o x.o xcmd.o
	$(CC) -o $@ ${LDFLAGS} $?

install: dmenu-release
	@$(INSTALL) --strip --mode=755 {,${PREFIX}/bin/}dmenu
	@$(INSTALL) --mode=755 {,${PREFIX}/bin/}dmenu_run
	@$(INSTALL) --mode=755 {,${PREFIX}/bin/}dmenu-i3_run
	@$(INSTALL) --mode=755 {,${PREFIX}/bin/}i3wm_commands.py

uninstall:
	@$(RM) ${PREFIX}/bin/{dmenu,dmenu_run,dmenu-i3_run}
