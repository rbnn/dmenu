#include "util.h"
#include "viewer.h"

const char *colors[dmenu_colorscheme_last][2] =
{
	/*                                    fg         bg       */
	[dmenu_colorscheme_normal_even] = { "#bbbbbb", "#333333" },
	[dmenu_colorscheme_normal_odd] =  { "#bbbbbb", "#111111" },
	[dmenu_colorscheme_select] =      { "#eeeeee", "#005577" },
	[dmenu_colorscheme_prompt] =      { "#eeeeee", "#335577" },
  [dmenu_colorscheme_input_good] =  { "#bbbbbb", "#555555" },
  [dmenu_colorscheme_input_bad]  =  { "#ffb0b0", "#555555" }
	// [dmenu_colorscheme_normal_even] = { "#bbbbbb", "#222222" },
	// [dmenu_colorscheme_normal_odd] =  { "#bbbbbb", "#111111" },
	// [dmenu_colorscheme_select] =      { "#eeeeee", "#005577" },
	// [dmenu_colorscheme_prompt] =      { "#eeeeee", "#005577" },
  // [dmenu_colorscheme_input_good] =  { "#bbbbbb", "#222222" },
  // [dmenu_colorscheme_input_bad]  =  { "#ffb0b0", "#222222" }
};

const char *fonts[] =
{
  "inconsolata:size=12",
	"monospace:size=12",
	/* "monospace:size=10", */
	(char*)0
};

/* Calculate width of bounding box around text */
int get_textwidth(const dfnt_t *font, const char *text, size_t n)
{/*{{{*/
  assert(font);

  if(!text || !n) return 0;

  XGlyphInfo ext;
	XftTextExtentsUtf8(font->x->display, font->xfont, (XftChar8 *)text, n, &ext);
	debug("Width of text `%s' is %i.", text, ext.xOff);

	return ext.xOff;
}/*}}}*/

dfnt_t *load_xfont(const dx11_t *x, const char *fontname, FcPattern *fontpattern)
{/*{{{*/
  assert(x);

	dfnt_t *font;
	XftFont *xfont = NULL;
	FcPattern *pattern = NULL;

	if (fontname) {
		/* Using the pattern found at font->xfont->pattern does not yield the
		 * same substitution results as using the pattern returned by
		 * FcNameParse; using the latter results in the desired fallback
		 * behaviour whereas the former just results in missing-character
		 * rectangles being drawn, at least with some fonts. */
		debug("Load font from name: `%s'.", fontname);

		xfont = XftFontOpenName(x->display, x->screen, fontname);
		warn_if(!xfont, "Cannot load font from name: `%s'", fontname);

		if(!xfont) return NULL;

    pattern = FcNameParse((FcChar8*)fontname);
    warn_if(!pattern, "Cannot parse font name to pattern: `%s'", fontname);
    
    if(!pattern) {
      XftFontClose(x->display, xfont);
      return NULL;
    } /* if ... */

	} else if (fontpattern) {
	  debug("Load font from pattern.");

	  xfont = XftFontOpenPattern(x->display, fontpattern);
	  warn_if(!xfont, "Cannot load font from pattern.");

	  if(!xfont) return NULL;

	} else {
		die("No font specified.");
	} /* if ... */

  /* Allocate memory and initialize font font */
	font = xmalloc(sizeof(dfnt_t));
	font->x = x;
	font->xfont = xfont;
	font->pattern = pattern;
	font->height = xfont->ascent + xfont->descent;
	font->padding = font->height / 2;
	font->next = NULL;

	return font;
}/*}}}*/

void init_viewer_style(dstyle_t *style, const dview_t *view, const char *colornames[], size_t n, dfnt_t *font)
{/*{{{*/
  assert(style);
  assert(view);
  debug("Initialize style.");

	/* need (at least) two colors for a scheme */
  assert2(2 <= n, "Cannot initialize style without foreground and background color.");
  warn_if(2 < n, "Style initialization only requires foreground and background color.");

  /* Store font for this style */
  style->font = font;  
  warn_if(!font, "Style is created without a corresponding font.");

  /* Allocate colors */
  assert2(XftColorAllocName(view->x->display, view->visual, view->colormap, colornames[0], &style->foreground), "Cannot allocate color: `%s'", colornames[0]);
  assert2(XftColorAllocName(view->x->display, view->visual, view->colormap, colornames[1], &style->background), "Cannot allocate color: `%s'", colornames[1]);
}/*}}}*/

void setup_viewer(dview_t *view)
{/*{{{*/
  assert(view);
  debug("Setup user interface.");

	view->clip = XInternAtom(view->x->display, "CLIPBOARD",   False);
	view->utf8 = XInternAtom(view->x->display, "UTF8_STRING", False);

	/* Calculate menu geometry */
	view->menu.line_height = view->menu.style_normal_even.font->height + 2;
	view->menu.lines = max(0, view->menu.lines);
	view->menu.height = (1 + view->menu.lines) * view->menu.line_height;

  #ifdef XINERAMA
  /* The following block (until closing #endif) is all about finding where to
   * place the new window. The idea behind that is using Xinerama to identify
   * the screen, that
   * 1. contains the currently focused window,
   * 2. toverlaps most with the currently focused window or
   * 3. contains the pointer.
   */
  int monitor = view->x->monitor;
  int num_screens;
  Window focus;
  XineramaScreenInfo *info;

	if ((info = XineramaQueryScreens(view->x->display, &num_screens))) {
	  int di;

	  debug("Found %i screens.", num_screens);

		XGetInputFocus(view->x->display, &focus, &di);

    if(!is_betweeen(view->x->monitor, 0, num_screens) && (focus != view->x->root) && (PointerRoot != focus) && (None != focus)) {
      Window parent;

      debug("Cannot select monitor %i.", view->x->monitor);

			/* find top-level window containing current input focus */
			do {
			  Window root, *children;
        unsigned int num_children;

			  parent = focus;

			  XQueryTree(view->x->display, parent, &root, &focus, &children, &num_children);
			  if(children) XFree(children);

			} while((focus != view->x->root) && (focus != parent));

			/* find xinerama screen with which the window intersects most */
	    XWindowAttributes parent_attrs;
	    if(XGetWindowAttributes(view->x->display, parent, &parent_attrs)) {
	      int i;
	      int area = 0;
	      const int parent_x[] = { parent_attrs.x, parent_attrs.x + parent_attrs.width };
	      const int parent_y[] = { parent_attrs.y, parent_attrs.y + parent_attrs.height };

	      for(i = 0; i < num_screens; i += 1) {
	        const int screen_x[] = { info[i].x_org, info[i].x_org + info[i].width };
	        const int screen_y[] = { info[i].y_org, info[i].y_org + info[i].height };

          /* Separating axes theorem without projection */
	        const int separated_x = (parent_x[1] < screen_x[0]) || (screen_x[1] < parent_x[0]);
	        const int separated_y = (parent_y[1] < screen_y[0]) || (screen_y[1] < parent_y[0]);

          /* Window does not overlap with that screen */
	        if(!separated_x || !separated_y) continue;
	        
	        /* Calculate area */
	        const int dx = min(abs(parent_x[0] - screen_x[1]), abs(parent_x[1] - screen_x[0]));
	        const int dy = min(abs(parent_y[0] - screen_y[1]), abs(parent_y[1] - screen_y[0]));
	        const int tmp_area = dx * dy;

	        if(tmp_area > area) {
	          monitor = i;
	          area = tmp_area;
	        } /* if ... */
	      } /* for ... */
	    } /* if ... */

    } /* if ... */

		/* no focused window is on screen, so use pointer location instead */
		int pointer_x, pointer_y, win_x, win_y;
		unsigned int mask;
		if((0 > view->x->monitor) && XQueryPointer(view->x->display, view->x->root, &focus, &focus, &pointer_x, &pointer_y, &win_x, &win_y, &mask)) {
		  int i;
		  debug("Select screen from pointer location.");

		  for(i = 0; i < num_screens; i += 1) {
		    const int in_x = is_betweeen(pointer_x, info[i].x_org, info[i].x_org + info[i].width);
		    const int in_y = is_betweeen(pointer_y, info[i].y_org, info[i].y_org + info[i].height);

		    if(!in_x || !in_y) continue;

		    monitor = i;
		    break;
		  }
		} /* if ... */

    warn_if((monitor < 0) || (num_screens <= monitor), "Cannot find monitor %i", monitor);
    monitor = clip(monitor, 0, num_screens - 1);

    /* Save dimension and offset of screen, where the window will go to */
		view->menu.y = info[monitor].y_org;
		view->menu.y += (view->show_at_bottom ? info[monitor].height - view->menu.height : 0);
		view->menu.x = info[monitor].x_org;
		view->menu.width = info[monitor].width;

		XFree(info);

	} else
  #endif /* XINERAMA */
	{
		view->menu.x = 0;
		view->menu.y = view->show_at_bottom ? view->x->height - view->menu.height : 0;
		view->menu.width = view->x->width;
	} /* if ... */

  /* Calculate text width of prompt */
  if(view->prompt.text) {
    const dfnt_t *font = view->prompt.style.font;
    const glong len = g_utf8_strlen(view->prompt.text, -1);  /* nul-terminated string */
    assert(0 <= len);

    view->prompt.size = (size_t)len;
    view->prompt.width = get_textwidth(font, view->prompt.text, view->prompt.size);
    debug("Configure prompt: text=`%s', width=%i, padding=%i", view->prompt.text, view->prompt.width, font->padding);

    view->prompt.width += font->padding;
  } else {
    view->prompt.size = 0;
    view->prompt.width = 0;

  } /* if ... */

  /* Calculate width of input */
  view->input.width = view->menu.width - view->prompt.width;

	/* Create menu window */
	XSetWindowAttributes menu_attrs;
	menu_attrs.override_redirect = True;
	menu_attrs.background_pixel = view->menu.style_normal_even.background.pixel;
	menu_attrs.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

  assert(0 <= view->menu.height);
  assert(0 <= view->menu.width);
  const unsigned int border_width = 0;

	view->menu_hwnd = XCreateWindow(view->x->display, view->x->root,
	    view->menu.x, view->menu.y, view->menu.width, view->menu.height, border_width,
	    view->x->depth, CopyFromParent, view->visual,
	    CWOverrideRedirect | CWBackPixel | CWEventMask, &menu_attrs);

	XMapRaised(view->x->display, view->menu_hwnd);
}/*}}}*/

void init_viewer(dview_t *view, const dx11_t *x, const char *colornames[][2], const char *fontnames[])
{/*{{{*/
  assert(x);
  assert(view);
  debug("Initialize user interface (viewer).");

  view->x = x;
  view->pixmap = XCreatePixmap(x->display, x->root, x->width, x->height, x->depth);
  view->gc = XCreateGC(x->display, x->root, 0, NULL);
  XSetLineAttributes(x->display, view->gc, 1, LineSolid, CapButt, JoinMiter);

  view->visual = DefaultVisual(view->x->display, view->x->screen);
  view->colormap = DefaultColormap(view->x->display, view->x->screen);

  /* Initialize fonts */
  assert2(fontnames && fontnames[0], "No fonts to load");

  view->fonts = NULL;
  const char **it = fontnames;

  while(*it) {
    dfnt_t *new_font = load_xfont(view->x, *it, NULL);
    warn_if(!new_font, "Cannot load font: %s", *it);
    it += 1;

    /* Ignore invalid fonts */
    if(!new_font) continue;

    view->fonts = g_list_prepend(view->fonts, new_font);
  } /* while ... */

  /* Revert list of new fonts, to keep order of font names */
  assert2(view->fonts, "Cannot load any font.");
  view->fonts = g_list_reverse(view->fonts);

  /* Create styles */

	/* Always use first the first font for the styles */
	dfnt_t *font = (dfnt_t*)view->fonts->data;
	init_viewer_style(&view->menu.style_normal_even, view, colornames[dmenu_colorscheme_normal_even], 2, font);
	init_viewer_style(&view->menu.style_normal_odd,  view, colornames[dmenu_colorscheme_normal_odd],  2, font);
	init_viewer_style(&view->menu.style_select, view, colornames[dmenu_colorscheme_select], 2, font);
	init_viewer_style(&view->prompt.style, view, colornames[dmenu_colorscheme_prompt], 2, font);
	init_viewer_style(&view->input.style_good, view, colornames[dmenu_colorscheme_input_good], 2, font);
	init_viewer_style(&view->input.style_bad, view, colornames[dmenu_colorscheme_input_bad], 2, font);

  /* Create windows */
  setup_viewer(view);
}/*}}}*/

void draw_rect(dview_t *view, const XftColor color, int x, int y, int width, int height, int filled)
{/*{{{*/
  assert(view);
  debug("Draw rectangle: x=%i, y=%i, width=%i, height=%i, filled=%s", x, y, width, height, filled ? "yes" : "no");

	XSetForeground(view->x->display, view->gc, color.pixel);

  assert(filled ? 1 < width : 0 < width);
  assert(filled ? 1 < height : 0 < height);

	if (filled) { XFillRectangle(view->x->display, view->pixmap, view->gc, x, y, width, height); 
	} else { XDrawRectangle(view->x->display, view->pixmap, view->gc, x, y, width - 1, height - 1); 
	} /* if ... */
}/*}}}*/

void draw_ntext(dview_t *view, const dstyle_t *style, int x, int y, int width, int height, const char *text, size_t n)
{/*{{{*/
  assert(view);
  assert(style);
  assert(0 < width);
  assert(0 < height);
  debug("Draw text: x=%i, y=%i, width=%i, height=%i, text=`%s'", x, y, width, height, text);

  /* Render box behind text */
  draw_rect(view, style->background, x, y, width, height, 1);

  /* Check, if there's enough space for the text box */
  x += style->font->padding / 2;
  width -= style->font->padding;
  warn_if(0 >= width, "Text box is too small, even for a single letter.");

  if(0 >= width) return;

  /* Create rendering context and start rendering the text */
  XftDraw *draw = XftDrawCreate(view->x->display, view->pixmap, view->visual, view->colormap);

  /* Center text vertically in bounding box */
	const int text_y = y + (height - style->font->height) / 2 + style->font->xfont->ascent;
	XftDrawString8(draw, &style->foreground, style->font->xfont, x, text_y, (XftChar8*)text, n);

	XftDrawDestroy(draw);
}/*}}}*/

/* Draw text on ui using style at x/y. The bounding box is fixed to width and
 * height. If the text is wider than width, it is trimmed. */
void draw_text(dview_t *view, const dstyle_t *style, int x, int y, int width, int height, const char *text)
{/*{{{*/
  draw_ntext(view, style, x, y, width, height, text, strlen(text));
}/*}}}*/

void update_ui(dview_t *view, const xcmd_t *model)
{/*{{{*/
  assert(view);
  assert(model);
  debug("Update user interface.");

	//unsigned int curpos;
	//struct item *item;
	int x = view->menu.x;
	int y = view->menu.y;

	int max_item_width = view->menu.width;

	/* Menu background */
	draw_rect(view, view->menu.style_normal_even.background, view->menu.x, view->menu.y, view->menu.width, view->menu.height, 1);

	if(view->prompt.text) {
	  debug("Draw prompt to user interface: x=%i, y=%i, width=%i, height=%i", x, y, view->prompt.width, view->menu.line_height);
	  draw_text(view, &view->prompt.style, x, y,  view->prompt.width, view->menu.line_height, view->prompt.text);
	  max_item_width -= view->prompt.width;
	  x += view->prompt.width;
	} /* if ... */

	assert(0 < max_item_width);

  debug("Draw input to user interface: x=%i, y=%i, width=%i, height=%i", x, y, view->input.width, view->menu.line_height);

  if(model->matches.input) draw_text(view, &view->input.style_good, x, y,  view->input.width, view->menu.line_height, model->matches.input);

	assert2(0 < view->menu.lines, "No other method implemented");

  /* Render menu items */
	if(0 < view->menu.lines) {

    /* Identify page, where the selected item is placed on */
    const size_t block = model->matches.selected / view->menu.lines;
    const size_t idx_lo = block * view->menu.lines;
    const size_t idx_hi = min(idx_lo + view->menu.lines, model->matches.count);
    size_t i;
  
	  for(i = idx_lo; i < idx_hi; i += 1) {
	    const int even_row_number = (idx_lo - i) % 2;
	    const dstyle_t *item_style = even_row_number ? &view->menu.style_normal_even : &view->menu.style_normal_odd;
	    const dstyle_t *slct_style = &view->menu.style_select;
	    const char *item = model->matches.index[i];

	    y += view->menu.line_height;

	    /* Redering full text */
	    if(model->matches.selected == i) {
	      /* Render selected text */
	      draw_text(view, slct_style, x, y, max_item_width, view->menu.line_height, item);
	    } else {
	      /* Render text with alternating style */
	      draw_text(view, item_style, x, y, max_item_width, view->menu.line_height, item);
	    } /* if ... */

	  } /* for ... */

  } /* if ... */

	XCopyArea(view->x->display, view->pixmap, view->menu_hwnd, view->gc, view->menu.x, view->menu.y, view->menu.width, view->menu.height, 0, 0);
	XSync(view->x->display, False);

}/*}}}*/
