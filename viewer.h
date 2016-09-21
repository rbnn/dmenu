#ifndef DMENU_VIEWER_H
#define DMENU_VIEWER_H
#include "x.h"
#include "xcmd.h"

typedef struct dmenu_viewer dview_t;
typedef struct dmenu_font dfnt_t;
typedef struct dmenu_style dstyle_t;

enum demenu_colorscheme
{/*{{{*/
  dmenu_colorscheme_normal_even,
  dmenu_colorscheme_normal_odd,
  dmenu_colorscheme_select,
  dmenu_colorscheme_prompt,
  dmenu_colorscheme_input_good,
  dmenu_colorscheme_input_bad,
  dmenu_colorscheme_last
};/*}}}*/

struct dmenu_style
{/*{{{*/
  dfnt_t *font;
  XftColor foreground;
  XftColor background;
};/*}}}*/

struct dmenu_font
{/*{{{*/
	const dx11_t *x;
	int height;
	int padding;
	XftFont *xfont;
	FcPattern *pattern;
	struct dmenu_font *next;
	struct dmenu_font *prev;
};/*}}}*/

struct dmenu_viewer
{/*{{{*/
  const dx11_t *x;  /* Handle to X window system */

  Window menu_hwnd;
  Pixmap pixmap;
  GC gc;
  Atom clip;
  Atom utf8;
  Visual *visual;
  Colormap colormap;
  GList *fonts;

  struct
  {
    char *text;
    long size;
    int width;
    dstyle_t style;
  } prompt;

  struct
  {
    int width;
    dstyle_t style_good;
    dstyle_t style_bad;
  } input;

  struct
  {
    int line_height;  /* height of each row/line */
    int height;   /* total menu height */
    int width;
    int y;  /* Origin of menu */
    int x;
    int lines;
    dstyle_t style_normal_even;
    dstyle_t style_normal_odd;
    dstyle_t style_select;
  } menu;

  int show_at_bottom;
};/*}}}*/

extern const char *colors[dmenu_colorscheme_last][2];
extern const char *fonts[];

void init_viewer(dview_t *view, const dx11_t *x, const char *colornames[][2], const char *fontnames[]);
void update_ui(dview_t *view, const xcmd_t *model);
#endif /* DMENU_VIEWER_H */
