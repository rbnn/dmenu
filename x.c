#include "util.h"
#include "x.h"

void init_x11(dx11_t *x)
{/*{{{*/
  assert(x);
  debug("Initialize X Window System.");

  warn_if(!setlocale(LC_CTYPE, "") || !XSupportsLocale(), "No locale support: %m");

	x->display = XOpenDisplay(NULL);
	assert2(x->display, "Cannot open display");

	x->screen = DefaultScreen(x->display);
	x->root = RootWindow(x->display, x->screen);
	x->height = DisplayHeight(x->display, x->screen);
	x->width = DisplayWidth(x->display, x->screen);
	x->depth = DefaultDepth(x->display, x->screen);

  assert(0 <= x->depth);
  assert(0 <= x->width);
  assert(0 <= x->height);
}/*}}}*/
