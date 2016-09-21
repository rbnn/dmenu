#ifndef DMENU_X_H
#define DMENU_X_H
#include <locale.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/Xft/Xft.h>

typedef struct dmenu_x11 dx11_t;

struct dmenu_x11
{/*{{{*/
  Display *display;
  int screen;
  Window root;
  int monitor;
  int height;
  int width;
  int depth;
};/*}}}*/

void init_x11(dx11_t *x);
#endif /* DMENU_X_H */
