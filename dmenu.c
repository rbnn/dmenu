/* See LICENSE file for copyright and license details. */
#include "inputbuffer.h"
#include "util.h"
#include "xcmd.h"
#include <ctype.h>
#include <glib.h>
#include <getopt.h>
// #include <libconfig.h>
#include <locale.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/Xft/Xft.h>

#ifdef DISABLED
/* macros */
#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
                             * MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))
#define LENGTH(X)             (sizeof X / sizeof X[0])
#define TEXTW(X)              (drw_fontset_getwidth(drw, (X)) + lrpad)

/* enums */
enum { SchemeNorm, SchemeSel, SchemeOut, SchemeLast }; /* color schemes */

struct item {
	char *text;
	struct item *left, *right;
	int out;
};

#define MATCH_ALGO_TOKEN    "token"
#define MATCH_ALGO_PREFIX   "prefix"
#define MATCH_ALGO_DEFAULT  MATCH_ALGO_TOKEN

#define AUTO_COMPLETE_ALGO_PREFIX   "prefix"
#define AUTO_COMPLETE_ALGO_DEFAULT  AUTO_COMPLETE_ALGO_PREFIX

static char text[BUFSIZ] = "";
static int bh, mw, mh;
static int sw, sh; /* X display screen geometry width, height */
static int inputw = 0, promptw;
static int lrpad; /* sum of left and right padding */
static size_t cursor;
static struct item *items = NULL;
static struct item *matches, *matchend;
static struct item *prev, *curr, *next, *sel;
static int mon = -1, screen;
static char *match_algo = MATCH_ALGO_DEFAULT;
static char *auto_complete_algo = AUTO_COMPLETE_ALGO_DEFAULT;
static void(*match_func)(void) = NULL;
static void(*auto_complete_func)(void) = NULL;
static config_t configuration;

static Atom clip, utf8;
static Display *dpy;
static Window root, win;
static XIC xic;

static Drw *drw;
static Clr *scheme[SchemeLast];
#endif /* DISABLED */

/* static char const optstr[] = "ac::hfil:p:m:vx::"; */
static char const optstr[] = "bhfil:p:m:v";
static struct option optlng[] =
{
  /* {"auto-complete",       0, NULL, 'a'}, */
  {"bottom",              0, NULL, 'b'},
  /* {"config",              2, NULL, 'c'}, */
  {"help",                0, NULL, 'h'},
  {"ignore-case",         0, NULL, 'i'},
  {"fast",                0, NULL, 'f'},
  {"lines",               1, NULL, 'l'},
  {"prompt",              1, NULL, 'p'},
  /* {"match",               2, NULL, 'x'}, */
  {"monitor",             1, NULL, 'm'},
  /* {"font",                1, NULL,  1 }, */
  /* {"normal-background",   1, NULL,  2 }, */
  /* {"normal-foreground",   1, NULL,  3 }, */
  /* {"selected-background", 1, NULL,  4 }, */
  /* {"selected-foreground", 1, NULL,  5 }, */
  {"version",             0, NULL, 'v'},
  {NULL,                  0, NULL,  0 }
};

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

#include "config.h"

#ifdef DISABLED

static void
calcoffsets(void)
{
	int i, n;

	if (lines > 0)
		n = lines * bh;
	else
		n = mw - (promptw + inputw + TEXTW("<") + TEXTW(">"));
	/* calculate which items will begin the next page and previous page */
	for (i = 0, next = curr; next; next = next->right)
		if ((i += (lines > 0) ? bh : MIN(TEXTW(next->text), n)) > n)
			break;
	for (i = 0, prev = curr; prev && prev->left; prev = prev->left)
		if ((i += (lines > 0) ? bh : MIN(TEXTW(prev->left->text), n)) > n)
			break;
}

static void
cleanup(void)
{
	size_t i;

	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	for (i = 0; i < SchemeLast; i++)
		free(scheme[i]);
	drw_free(drw);
	XSync(dpy, False);
	XCloseDisplay(dpy);
	config_destroy(&configuration);
}

static int
drawitem(struct item *item, int x, int y, int w)
{
	if (item == sel)
		drw_setscheme(drw, scheme[SchemeSel]);
	else if (item->out)
		drw_setscheme(drw, scheme[SchemeOut]);
	else
		drw_setscheme(drw, scheme[SchemeNorm]);

	return drw_text(drw, x, y, w, bh, lrpad / 2, item->text, 0);
}

static void
match_sub_pattern(void)
{
	static char **tokv = NULL;
	static int tokn = 0;

	char buf[sizeof text], *s;
	int i, tokc = 0;
	size_t len, textsize;
	struct item *item, *lprefix, *lsubstr, *prefixend, *substrend;

	strcpy(buf, text);
	/* separate input text into tokens to be matched individually */
	for (s = strtok(buf, " "); s; tokv[tokc - 1] = s, s = strtok(NULL, " "))
		if (++tokc > tokn && !(tokv = realloc(tokv, ++tokn * sizeof *tokv)))
			die("cannot realloc %u bytes:", tokn * sizeof *tokv);
	len = tokc ? strlen(tokv[0]) : 0;

	matches = lprefix = lsubstr = matchend = prefixend = substrend = NULL;
	textsize = strlen(text);
	for (item = items; item && item->text; item++) {
		for (i = 0; i < tokc; i++)
			if (!fstrstr(item->text, tokv[i]))
				break;
		if (i != tokc) /* not all tokens match */
			continue;
		/* exact matches go first, then prefixes, then substrings */
		if (!tokc || !fstrncmp(text, item->text, textsize))
			appenditem(item, &matches, &matchend);
		else if (!fstrncmp(tokv[0], item->text, len))
			appenditem(item, &lprefix, &prefixend);
		else
			appenditem(item, &lsubstr, &substrend);
	}
	if (lprefix) {
		if (matches) {
			matchend->right = lprefix;
			lprefix->left = matchend;
		} else
			matches = lprefix;
		matchend = prefixend;
	}
	if (lsubstr) {
		if (matches) {
			matchend->right = lsubstr;
			lsubstr->left = matchend;
		} else
			matches = lsubstr;
		matchend = substrend;
	}
	curr = sel = matches;
	calcoffsets();
}

static void
match_common_prefix(void)
{
  struct item *item;
  char const *tok = text;

  matches = matchend = NULL;

  while(isspace(*tok)) {
    /* Eat up all white space */
    tok += 1;
  } /* while ... */

  size_t const toksize = strlen(tok);
  for(item = items; item && item->text; item++) {
    /* Compare prefix and select only items with common prefix. */
    if(!toksize || !fstrncmp(tok, item->text, toksize)) {
      appenditem(item, &matches, &matchend);
    } /* if ... */
  } /* for ... */

  curr = sel = matches;
  calcoffsets();
}

static void
auto_complete_longest_common_prefix(void)
{
  struct item *item;
  size_t maxtextsize = 0;

  for(item = matches; item && item->text; item = item->right) {
    size_t const curtextsize = strlen(item->text);
    maxtextsize = (maxtextsize < curtextsize) ? curtextsize : maxtextsize;
    if(!maxtextsize) return;
  } /*  for ... */


  char *maxprefix = (char*)malloc(1 + maxtextsize);
  if(!maxprefix) die("cannot allocate %lu bytes.", 1 + maxtextsize);

  size_t maxprefixsize = strlen(matches->text);
  memcpy(maxprefix, matches->text, 1 + maxprefixsize);

  /* Find maximum prefix
   *
   * Algorithm starts with first selection and shortens this string until all
   * items compare greater or equal to it. Doing so the algorithm strips
   * leading white space characters. */
  for(item = matches->right; item && item->text && maxprefixsize; item = item->right) {
    while(maxprefixsize && fstrncmp(maxprefix, item->text, maxprefixsize)) {
      maxprefixsize -= 1;
      *(maxprefix + maxprefixsize) = '\0';
    }

    /* Shortcut, when no completion is found */
    if(!maxprefixsize) break;
  } /* for ... */

  /* Update input field and cursor */
  if(maxprefixsize) {
    strcpy(text, maxprefix);
    cursor = maxprefixsize;
  } /* if ... */

  /* Free temporary memory */
  free(maxprefix);
  calcoffsets();
}

static void
insert(const char *str, ssize_t n)
{
	if (strlen(text) + n > sizeof text - 1)
		return;
	/* move existing text out of the way, insert new text, and update cursor */
	memmove(&text[cursor + n], &text[cursor], sizeof text - cursor - MAX(n, 0));
	if (n > 0)
		memcpy(&text[cursor], str, n);
	cursor += n;
	match_func();
}

static size_t
nextrune(int inc)
{
	ssize_t n;

	/* return location of next utf8 rune in the given direction (+1 or -1) */
	for (n = cursor + inc; n + inc >= 0 && (text[n] & 0xc0) == 0x80; n += inc)
		;
	return n;
}

static void
keypress(XKeyEvent *ev)
{
	char buf[32];
	int len;
	KeySym ksym = NoSymbol;
	Status status;

	len = XmbLookupString(xic, ev, buf, sizeof buf, &ksym, &status);
	if (status == XBufferOverflow)
		return;
	if (ev->state & ControlMask)
		switch(ksym) {
		case XK_a: ksym = XK_Home;      break;
		case XK_b: ksym = XK_Left;      break;
		case XK_c: ksym = XK_Escape;    break;
		case XK_d: ksym = XK_Delete;    break;
		case XK_e: ksym = XK_End;       break;
		case XK_f: ksym = XK_Right;     break;
		case XK_g: ksym = XK_Escape;    break;
		case XK_h: ksym = XK_BackSpace; break;
		case XK_i: ksym = XK_Tab;       break;
		case XK_j: /* fallthrough */
		case XK_J: /* fallthrough */
		case XK_m: /* fallthrough */
		case XK_M: ksym = XK_Return; ev->state &= ~ControlMask; break;
		case XK_n: ksym = XK_Down;      break;
		case XK_p: ksym = XK_Up;        break;

		case XK_k: /* delete right */
			text[cursor] = '\0';
			match_func();
			break;
		case XK_u: /* delete left */
			insert(NULL, 0 - cursor);
			break;
		case XK_w: /* delete word */
			while (cursor > 0 && strchr(worddelimiters, text[nextrune(-1)]))
				insert(NULL, nextrune(-1) - cursor);
			while (cursor > 0 && !strchr(worddelimiters, text[nextrune(-1)]))
				insert(NULL, nextrune(-1) - cursor);
			break;
		case XK_y: /* paste selection */
		case XK_Y:
			XConvertSelection(dpy, (ev->state & ShiftMask) ? clip : XA_PRIMARY,
			                  utf8, utf8, win, CurrentTime);
			return;
		case XK_Return:
		case XK_KP_Enter:
			break;
		case XK_bracketleft:
			cleanup();
			exit(1);
		default:
			return;
		}
	else if (ev->state & Mod1Mask)
		switch(ksym) {
		case XK_g: ksym = XK_Home;  break;
		case XK_G: ksym = XK_End;   break;
		case XK_h: ksym = XK_Up;    break;
		case XK_j: ksym = XK_Next;  break;
		case XK_k: ksym = XK_Prior; break;
		case XK_l: ksym = XK_Down;  break;
		default:
			return;
		}
	switch(ksym) {
	default:
		if (!iscntrl(*buf))
			insert(buf, len);
		break;
	case XK_Delete:
		if (text[cursor] == '\0')
			return;
		cursor = nextrune(+1);
		/* fallthrough */
	case XK_BackSpace:
		if (cursor == 0)
			return;
		insert(NULL, nextrune(-1) - cursor);
		break;
	case XK_End:
		if (text[cursor] != '\0') {
			cursor = strlen(text);
			break;
		}
		if (next) {
			/* jump to end of list and position items in reverse */
			curr = matchend;
			calcoffsets();
			curr = prev;
			calcoffsets();
			while (next && (curr = curr->right))
				calcoffsets();
		}
		sel = matchend;
		break;
	case XK_Escape:
		cleanup();
		exit(1);
	case XK_Home:
		if (sel == matches) {
			cursor = 0;
			break;
		}
		sel = curr = matches;
		calcoffsets();
		break;
	case XK_Left:
		if (cursor > 0 && (!sel || !sel->left || lines > 0)) {
			cursor = nextrune(-1);
			break;
		}
		if (lines > 0)
			return;
		/* fallthrough */
	case XK_Up:
		if (sel && sel->left && (sel = sel->left)->right == curr) {
			curr = prev;
			calcoffsets();
		}
		break;
	case XK_Next:
		if (!next)
			return;
		sel = curr = next;
		calcoffsets();
		break;
	case XK_Prior:
		if (!prev)
			return;
		sel = curr = prev;
		calcoffsets();
		break;
	case XK_Return:
	case XK_KP_Enter:
		puts((sel && !(ev->state & ShiftMask)) ? sel->text : text);
		if (!(ev->state & ControlMask)) {
			cleanup();
			exit(0);
		}
		if (sel)
			sel->out = 1;
		break;
	case XK_Right:
		if (text[cursor] != '\0') {
			cursor = nextrune(+1);
			break;
		}
		if (lines > 0)
			return;
		/* fallthrough */
	case XK_Down:
		if (sel && sel->right && (sel = sel->right) == next) {
			curr = next;
			calcoffsets();
		}
		break;
	case XK_Tab:
		if (!sel)
			return;
		if(!auto_complete_func) {
  		strncpy(text, sel->text, sizeof text - 1);
	  	text[sizeof text - 1] = '\0';
		  cursor = strlen(text);
  		match_func();
  	} else {
  	  auto_complete_func();
  	} /* if ... */
		break;
	}
	drawmenu();
}

static void
paste(void)
{
	char *p, *q;
	int di;
	unsigned long dl;
	Atom da;

	/* we have been given the current selection, now insert it into input */
	XGetWindowProperty(dpy, win, utf8, 0, (sizeof text / 4) + 1, False,
	                   utf8, &da, &di, &dl, &dl, (unsigned char **)&p);
	insert(p, (q = strchr(p, '\n')) ? q - p : (ssize_t)strlen(p));
	XFree(p);
	drawmenu();
}

static void
run(void)
{
	XEvent ev;

	while (!XNextEvent(dpy, &ev)) {
		if (XFilterEvent(&ev, win))
			continue;
		switch(ev.type) {
		case Expose:
			if (ev.xexpose.count == 0)
				drw_map(drw, win, 0, 0, mw, mh);
			break;
		case KeyPress:
			keypress(&ev.xkey);
			break;
		case SelectionNotify:
			if (ev.xselection.property == utf8)
				paste();
			break;
		case VisibilityNotify:
			if (ev.xvisibility.state != VisibilityUnobscured)
				XRaiseWindow(dpy, win);
			break;
		}
	}
}

static void
read_configuration(char const *const path)
{
  if(!path) {
    read_configuration(DEFAULT_CONFIG);
    return;
  } /* if ... */

  wordexp_t we_file;
  if(0 != wordexp(path, &we_file, 0)) die("cannot expand `%s': %m", path);

  FILE *f_cfg = fopen(we_file.we_wordv[0], "r");
  if(!f_cfg) return;

  if(CONFIG_TRUE != config_read(&configuration, f_cfg)) {
    /* Fatal error. f_cfg will be closed automaticaly on exit. */
    die("cannot read `%s': %s (near line %i)", we_file.we_wordv[0], config_error_text(&configuration), config_error_line(&configuration));
  } /* if ... */
  wordfree(&we_file);

  /* Reading was successfull */
  /* Fast? */

  /* Lines */
  int tmp_lines;
  if(CONFIG_TRUE == config_lookup_int(&configuration, "lines", &tmp_lines)) {
    /* Set lines */
    lines = (0 < tmp_lines) ? tmp_lines : 0;
  } /* if ... */

  /* Prompt */
  config_lookup_string(&configuration, "prompt", (char const**)&prompt);

  /* Match function */
  config_lookup_string(&configuration, "match", (char const**)&match_algo);

  /* Auto complete functio */
  config_lookup_string(&configuration, "auto_complete", (char const**)&auto_complete_algo);

  /* Monitor */
  config_lookup_int(&configuration, "monitor", &mon);

  /* Font */
  config_lookup_string(&configuration, "font", &fonts[0]);

  /* Normal-Group */
  config_setting_t *normal = config_lookup(&configuration, "normal");
  if(normal) {
    /* Go down to normal colors */
    config_setting_lookup_string(normal, "foreground", &colors[SchemeNorm][ColFg]);
    config_setting_lookup_string(normal, "background", &colors[SchemeNorm][ColBg]);
  } /* if ... */

  /* Selected-Group */
  config_setting_t *selected = config_lookup(&configuration, "selected");
  if(selected) {
    /* Go down to selected colors */
    config_setting_lookup_string(selected, "foreground", &colors[SchemeSel][ColFg]);
    config_setting_lookup_string(selected, "background", &colors[SchemeSel][ColBg]);
  } /* if ... */

  fclose(f_cfg);
}
#endif /* DISABLED */

static void
usage(void)
{
	fputs("usage: dmenu [-c|--config=FILE] [-h|--help] [-f|--fast]\n"
	      "             [-l|--lines=N] [-p|--prompt=STR] [-x|--match=(sub|prefix)]\n"
	      "             [-a|--auto-complete] [-m|--monitor=n] [--font=FONT]\n"
	      "             [--normal-foreground=CLR] [--normal-background=CLR]\n"
	      "             [--selected-foreground=CLR] [--selected-background=CLR]\n"
	      "             [-v|--version]\n", stderr);
	exit(1);
}

typedef struct dmenu_viewer dview_t;
typedef struct dmenu_control dctrl_t;
typedef struct dmenu dmenu_t;
typedef struct dmenu_x11 dx11_t;
typedef struct dmenu_ui dui_t;
typedef struct dmenu_font dfnt_t;
typedef struct dmenu_style dstyle_t;

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

struct dmenu_style
{/*{{{*/
  dfnt_t *font;
  XftColor foreground;
  XftColor background;
};/*}}}*/

struct dmenu_viewer
{
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
};

struct dmenu_control
{/*{{{*/
  const dx11_t *x;
  Window hwnd;
  XIM xim;
  XIC xic;
  inpbuf_t input;
  int fast_startup: 1;  /* Perform fast start-up */
  int do_exit:      1;  /* Exit main loop */
};/*}}}*/

struct dmenu_ui
{/*{{{*/
  const dx11_t  *x;
  Window menu_hwnd;
  Pixmap pixmap;
  GC gc;
  XIM xim;
  XIC xic;
  Atom clip;
  Atom utf8;
  Visual *visual;
  Colormap colormap;
  GList *fonts;
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

struct dmenu
{/*{{{*/
  dx11_t x;
  dui_t  ui;

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

  struct
  {
    int show_at_bottom:     1;
    int fast_startup:       1;  /* Perform fast start-up */
  } flag;

};/*}}}*/

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

void dmenu_default_menu(dview_t *view)
{/*{{{*/
  assert(view);
  debug("Restore default menu options.");

  view->menu.line_height = -1;
  view->menu.height = -1;
  view->menu.width = -1;
  view->menu.x = -1;
  view->menu.y = -1;
  view->menu.lines = 0;

  memset(&view->menu.style_normal_even, 0, sizeof(dstyle_t));
  memset(&view->menu.style_normal_odd, 0, sizeof(dstyle_t));
  memset(&view->menu.style_select, 0, sizeof(dstyle_t));
}/*}}}*/

void dmenu_getopt(dx11_t *x, xcmd_t *model, dview_t *view, dctrl_t *control, int argc, char *argv[])
{/*{{{*/
  assert(x);
  assert(model);
  assert(view);
  assert(control);
  xcfg_t model_config;

  dmenu_default_menu(view);
  xcmd_config_default(&model_config);

  debug("Parse command line options.");

  x->monitor = -1;
  view->menu.lines = 5;
  view->prompt.text = "~>";
  view->show_at_bottom = 0;
  control->fast_startup = 0;

	int c, idx;
	opterr = 0;

	while(-1 != (c = getopt_long(argc, argv, optstr, optlng, &idx))) {
	  switch(c) {
	    case 'b':   /* Bottom bar */
	      view->show_at_bottom = 1;
	      break;

	    case 'h':   /* print help message */
	      usage();
	      exit(EXIT_SUCCESS);

	    case 'f':   /* fast mode */
	      control->fast_startup = 1;
	      break;

	    case 'i':   /* case-insensitive item matching */
	      model_config.case_insensitive = 1;
	      break;

	    case 'l':   /* number of lines in vertical list */
	      view->menu.lines = atoi(optarg);
	      break;

	    case 'p':   /* adds prompt to left of input field */
	      view->prompt.text = optarg;
	      break;

	    case 'm':   /* select monitor */
	      x->monitor = atoi(optarg);
	      break;

	    case 'v':   /* print version number */
	      puts("dmenu-"VERSION);
	      exit(0);

	    default:
	      usage();
	      exit(EXIT_FAILURE);

	  } /* switch ... */
	} /* while ... */

	/* Apply model configuration */
	xcmd_init(model, &model_config);
}/*}}}*/

void init_x11(dx11_t *x)
{/*{{{*/
  assert(x);
  debug("Initialize X window system.");

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

void init_control(dctrl_t *control, const dx11_t *x, const Window hwnd)
{/*{{{*/
  assert(x);
  assert(control);
  debug("Initialize user interface(control).");

	/* open input methods */
  control->x = x;
  control->hwnd = hwnd;
  inputbuffer_init(&control->input);
	control->xim = XOpenIM(control->x->display, NULL, NULL, NULL);
	control->xic = XCreateIC(control->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, hwnd, XNFocusWindow, hwnd, NULL);

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000  };
	int i;

	/* try to grab keyboard, we may have to wait for another process to ungrab */
	for (i = 0; i < 1000; i++) {
		const int ok = XGrabKeyboard(control->x->display, control->x->root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
		if(GrabSuccess == ok) return;

		nanosleep(&ts, NULL);
	} /* for ... */

	die("Cannot grab keyboard");
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

void control_on_keypress(dctrl_t *control, xcmd_t *model, XKeyEvent *ev)
{
  assert(ev);
  assert(model);
  assert(control);
  debug("Evaluate keypress event.");

	char buf[32];
	int len;
	KeySym ksym = NoSymbol;
	Status status;

	len = XmbLookupString(control->xic, ev, buf, sizeof(buf), &ksym, &status);
	warn_if(XBufferOverflow == status, "Detected buffer overflow.");
	if (XBufferOverflow == status) return;

	int has_changed = 0;

	if (ev->state & ControlMask) {
		switch(ksym) {
		  case XK_a: ksym = XK_Home; break;
		  case XK_b: ksym = XK_Left; break;
		  case XK_c: ksym = XK_Escape; break;
		  case XK_d: ksym = XK_Delete; break;
		  case XK_e: ksym = XK_End; break;
		  case XK_f: ksym = XK_Right; break;
		  case XK_g: ksym = XK_Escape; break;
		  case XK_h: ksym = XK_BackSpace; break;
		  case XK_i: ksym = XK_Tab; break;

		  case XK_j: /* fallthrough */
		  case XK_J: /* fallthrough */
		  case XK_m: /* fallthrough */
		  case XK_M:
		    ksym = XK_Return;
		    ev->state &= ~ControlMask;
		    break;

		  case XK_n: ksym = XK_Down; break;
		  case XK_p: ksym = XK_Up; break;

		  case XK_k: /* delete right */
		    inputbuffer_erase(&control->input, +1);
		    has_changed = 1;
		  	break;

		  case XK_u: /* delete left */
		    inputbuffer_erase(&control->input, -1);
		    has_changed = 1;
		  	break;

		  case XK_w: /* delete word */
		    debug("Delete word.");
		  	// while (cursor > 0 && strchr(worddelimiters, text[nextrune(-1)]))
		  	// 	insert(NULL, nextrune(-1) - cursor);
		  	// while (cursor > 0 && !strchr(worddelimiters, text[nextrune(-1)]))
		  	// 	insert(NULL, nextrune(-1) - cursor);
		  	break;

		  case XK_y: /* fallthrough */
		  case XK_Y:
		    debug("Paste selection");
		  	// XConvertSelection(dpy, (ev->state & ShiftMask) ? clip : XA_PRIMARY,
		  	//                   utf8, utf8, win, CurrentTime);
		  	// return;
		  	break;

		  case XK_Return:   /* fallthrough */
		  case XK_KP_Enter: break;

		  case XK_bracketleft:
		    debug("Exit main loop.");
		  	control->do_exit = 1;
		  	break;

		  default: return;
		} /* switch ... */

	} else if (ev->state & Mod1Mask) {
		switch(ksym) {
		  case XK_g: ksym = XK_Home; break;
		  case XK_G: ksym = XK_End; break;
		  case XK_h: ksym = XK_Up; break;
		  case XK_j: ksym = XK_Next; break;
		  case XK_k: ksym = XK_Prior; break;
		  case XK_l: ksym = XK_Down; break;
		  default: return;
		}
	} /* if ... */

	switch(ksym) {
	  case XK_Delete:
	    inputbuffer_erase(&control->input, +1);
	  	has_changed = 1;
	  	break;

	  case XK_BackSpace:
	  	inputbuffer_erase(&control->input, -1);
	  	has_changed = 1;
	  	break;

	  case XK_Home:
	  	xcmd_update_selected(model, 0, 0);
	  	break;

	  case XK_End:
	  	xcmd_update_selected(model, G_MAXLONG, 0);
	  	break;

	  case XK_Escape:
	  	control->do_exit = 1;
	  	break;

	  case XK_Left:
	    inputbuffer_move(&control->input, -1);
	    has_changed = 1;
	  	break;

	  case XK_Right:
	    inputbuffer_move(&control->input, +1);
	    has_changed = 1;
	  	break;

	  case XK_Up:   /* fallthrough */
	  case XK_Prior:
	  	xcmd_update_selected(model, -1, 1);
	  	break;

	  case XK_Down:   /* fallthrough */
	  case XK_Next:
	  	xcmd_update_selected(model, +1, 1);
	  	break;

	  case XK_Return:   /* fallthrough */
	  case XK_KP_Enter:
	  	control->do_exit = 1;
	  	break;

	  case XK_Tab:
	    if(xcmd_auto_complete(model)) {
	      inputbuffer_set(&control->input, model->matches.input);
	      has_changed = 1;
	    } /* if ... */
	  	break;

	  default:
	    if(!iscntrl(*buf)) {
        /* Make buf nul-terminated */
        *(buf + len) = '\0';
	      /* Add text to input buffer */
	      inputbuffer_insert(&control->input, buf);
	      has_changed = 1;
	    } /* if ... */
	  	break;

	}

  model->has_changed = has_changed;
	if(has_changed) xcmd_update_matching(model, inputbuffer_get_text(&control->input));
}

void run_control(dctrl_t *control, xcmd_t *model)
{
  assert(control);
  assert(model);
  debug("Enter main control loop.");

	XEvent ev;

  debug("Enter main event loop.");

	control->do_exit = 0;
	while (!control->do_exit && !XNextEvent(control->x->display, &ev)) {
		if (XFilterEvent(&ev, control->hwnd))
			continue;
		switch(ev.type) {
	  	case Expose:
	  	  if (!ev.xexpose.count) {
	  	    model->has_changed = 1;
	  	    xcmd_notify_observer(model);
	      } /* if ... */
	  		break;

		  case KeyPress:
		    control_on_keypress(control, model, &ev.xkey);
			  break;

		  // case SelectionNotify:
		  // 	if (dmenu.ui.utf8 == ev.xselection.property)
		  // 		paste();
		  // 	break;

		  case VisibilityNotify:
		  	if (VisibilityUnobscured != ev.xvisibility.state) {
		  	  debug("Receive visibility notification.")
		  		XRaiseWindow(control->x->display, control->hwnd);
		  	} /* if ... */
		  	break;
		}
	} /* while ... */
}

int main(int argc, char *argv[])
{
  dx11_t x = {0};
  xcmd_t model;   /* M */
  dview_t view;   /* V */
  dctrl_t ctrl;   /* C */

  /* Configure MVC */
 //  dmenu_getopt(&x, &dmenu, &model, argc, argv);
  dmenu_getopt(&x, &model, &view, &ctrl, argc, argv);

  /* Setup callback functions for the model */
	model.observer = (void(*)(void*,const xcmd_t*))update_ui;
	model.observer_data = &view;

  /* Initialize X window system */
  init_x11(&x);

  /* Setup the viewer */
	init_viewer(&view, &x, colors, fonts);

  /* Load data and initialize controller. Flag fast_startup is set inside of
   * dmenu_getopt */
	if(ctrl.fast_startup) {
	  debug("Perform fast start-up.");
	  init_control(&ctrl, &x, view.menu_hwnd);
	  xcmd_read_items(&model, stdin);

	} else {
	  debug("Perform normal start-up.");
	  xcmd_read_items(&model, stdin);
	  init_control(&ctrl, &x, view.menu_hwnd);

	} /* if ... */

	xcmd_finish_items(&model);

  /* Start event handling loop */
	debug("Configuration is complete now.");
	run_control(&ctrl, &model);

	// XUngrabKeyboard
	// XUngrabKey(dpy, AnyKey, AnyModifier, root);
	// for (i = 0; i < SchemeLast; i++)
	// 	free(scheme[i]);
	// drw_free(drw);
	XSync(x.display, False);
	XCloseDisplay(x.display);

	return 1; /* unreachable */
}
