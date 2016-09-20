/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <getopt.h>
#include <libconfig.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <wordexp.h>
#include <glib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

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

static int (*fstrncmp)(const char *, const char *, size_t) = strncmp;
static char *(*fstrstr)(const char *, const char *) = strstr;

static void
appenditem(struct item *item, struct item **list, struct item **last)
{
	if (*last)
		(*last)->right = item;
	else
		*list = item;

	item->left = *last;
	item->right = NULL;
	*last = item;
}

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

static char *
cistrstr(const char *s, const char *sub)
{
	size_t len;

	for (len = strlen(sub); *s; s++)
		if (!strncasecmp(s, sub, len))
			return (char *)s;
	return NULL;
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
drawmenu(void)
{
	unsigned int curpos;
	struct item *item;
	int x = 0, y = 0, w;

	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_rect(drw, 0, 0, mw, mh, 1, 1);

	if (prompt && *prompt) {
		drw_setscheme(drw, scheme[SchemeSel]);
		x = drw_text(drw, x, 0, promptw, bh, lrpad / 2, prompt, 0);
	}
	/* draw input field */
	w = (lines > 0 || !matches) ? mw - x : inputw;
	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_text(drw, x, 0, w, bh, lrpad / 2, text, 0);

	drw_font_getexts(drw->fonts, text, cursor, &curpos, NULL);
	if ((curpos += lrpad / 2 - 1) < w) {
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, x + curpos, 2, 2, bh - 4, 1, 0);
	}

	if (lines > 0) {
		/* draw vertical list */
		for (item = curr; item != next; item = item->right)
			drawitem(item, x, y += bh, mw - x);
	} else if (matches) {
		/* draw horizontal list */
		x += inputw;
		w = TEXTW("<");
		if (curr->left) {
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_text(drw, x, 0, w, bh, lrpad / 2, "<", 0);
		}
		x += w;
		for (item = curr; item != next; item = item->right)
			x = drawitem(item, x, 0, MIN(TEXTW(item->text), mw - x - TEXTW(">")));
		if (next) {
			w = TEXTW(">");
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_text(drw, mw - w, 0, w, bh, lrpad / 2, ">", 0);
		}
	}
	drw_map(drw, win, 0, 0, mw, mh);
}

static void
grabkeyboard(void)
{
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000  };
	int i;

	/* try to grab keyboard, we may have to wait for another process to ungrab */
	for (i = 0; i < 1000; i++) {
		if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync,
		                  GrabModeAsync, CurrentTime) == GrabSuccess)
			return;
		nanosleep(&ts, NULL);
	}
	die("cannot grab keyboard");
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
readstdin(void)
{
	char buf[sizeof text], *p;
	size_t i, imax = 0, size = 0;
	unsigned int tmpmax = 0;

	/* read each line from stdin and add it to the item list */
	for (i = 0; fgets(buf, sizeof buf, stdin); i++) {
		if (i + 1 >= size / sizeof *items)
			if (!(items = realloc(items, (size += BUFSIZ))))
				die("cannot realloc %u bytes:", size);
		if ((p = strchr(buf, '\n')))
			*p = '\0';
		if (!(items[i].text = strdup(buf)))
			die("cannot strdup %u bytes:", strlen(buf) + 1);
		items[i].out = 0;
		drw_font_getexts(drw->fonts, buf, strlen(buf), &tmpmax, NULL);
		if (tmpmax > inputw) {
			inputw = tmpmax;
			imax = i;
		}
	}
	if (items)
		items[i].text = NULL;
	inputw = items ? TEXTW(items[imax].text) : 0;
	lines = MIN(lines, i);
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
setup(void)
{
	int x, y;
	XSetWindowAttributes swa;
	XIM xim;
#ifdef XINERAMA
	XineramaScreenInfo *info;
	Window w, pw, dw, *dws;
	XWindowAttributes wa;
	int a, j, di, n, i = 0, area = 0;
	unsigned int du;
#endif

	/* init appearance */
	scheme[SchemeNorm] = drw_scm_create(drw, colors[SchemeNorm], 2);
	scheme[SchemeSel] = drw_scm_create(drw, colors[SchemeSel], 2);
	scheme[SchemeOut] = drw_scm_create(drw, colors[SchemeOut], 2);

	clip = XInternAtom(dpy, "CLIPBOARD",   False);
	utf8 = XInternAtom(dpy, "UTF8_STRING", False);

	/* calculate menu geometry */
	bh = drw->fonts->h + 2;
	lines = MAX(lines, 0);
	mh = (lines + 1) * bh;
#ifdef XINERAMA
	if ((info = XineramaQueryScreens(dpy, &n))) {
		XGetInputFocus(dpy, &w, &di);
		if (mon >= 0 && mon < n)
			i = mon;
		else if (w != root && w != PointerRoot && w != None) {
			/* find top-level window containing current input focus */
			do {
				if (XQueryTree(dpy, (pw = w), &dw, &w, &dws, &du) && dws)
					XFree(dws);
			} while (w != root && w != pw);
			/* find xinerama screen with which the window intersects most */
			if (XGetWindowAttributes(dpy, pw, &wa))
				for (j = 0; j < n; j++)
					if ((a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j])) > area) {
						area = a;
						i = j;
					}
		}
		/* no focused window is on screen, so use pointer location instead */
		if (mon < 0 && !area && XQueryPointer(dpy, root, &dw, &dw, &x, &y, &di, &di, &du))
			for (i = 0; i < n; i++)
				if (INTERSECT(x, y, 1, 1, info[i]))
					break;

		x = info[i].x_org;
		y = info[i].y_org + (topbar ? 0 : info[i].height - mh);
		mw = info[i].width;
		XFree(info);
	} else
#endif
	{
		x = 0;
		y = topbar ? 0 : sh - mh;
		mw = sw;
	}
	promptw = (prompt && *prompt) ? TEXTW(prompt) - lrpad / 4 : 0;
	inputw = MIN(inputw, mw/3);
	match_func();

	/* create menu window */
	swa.override_redirect = True;
	swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
	swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
	win = XCreateWindow(dpy, root, x, y, mw, mh, 0,
	                    DefaultDepth(dpy, screen), CopyFromParent,
	                    DefaultVisual(dpy, screen),
	                    CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);

	/* open input methods */
	xim = XOpenIM(dpy, NULL, NULL, NULL);
	xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
	                XNClientWindow, win, XNFocusWindow, win, NULL);

	XMapRaised(dpy, win);
	drw_resize(drw, mw, mh);
	drawmenu();
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

typedef struct dmenu dmenu_t;
typedef struct dmenu_x11 dx11_t;
typedef struct dmenu_ui dui_t;
typedef struct dmenu_font dfnt_t;
typedef struct dmenu_style dstyle_t;
typedef struct dmenu_item ditem_t;
typedef int(*dstrncmp_t)(const char*,const char*,size_t);
typedef int(*dmatch_t)(const char*,const char*,const dmenu_t*,void*);
typedef int(*dcomplete_t)(char**,size_t*,const dmenu_t*,void*);

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

struct dmenu_item
{/*{{{*/
	char *text;
	int width;
	int out;
};/*}}}*/

struct dmenu
{/*{{{*/
  dstrncmp_t strncmp;
  dx11_t  x;
  dui_t   ui;
  GList *fonts;

  struct
  {
    ditem_t  *index;
    ditem_t **match;
    size_t index_count;
    size_t match_count;
    size_t selected_id;
  } items;

  struct
  {
    dmatch_t  exec;
    void     *data;
  } match;

  struct
  {
    dcomplete_t exec;
    void       *data; 
  } complete;

  struct
  {
    char    *text;
    int      width;
    dstyle_t style;
  } prompt;

  struct
  {
    char    *text;
    int      width;
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
    int case_insensitive:   1;  /* Ignore case when comparing strings */
    #ifdef DMENU_IGNORE_WHITE
    int str_ignore_white:   1;  /* Ignore leading white space when comparing strings */ 
    #endif /* DMENU_IGNORE_WHITE */
  } flag;


};/*}}}*/

void dmenu_default_items(dmenu_t *ptr)
{/*{{{*/
  assert(ptr);
  debug("Restore default item data.");

  ptr->items.index = NULL;
  ptr->items.match = NULL;
  ptr->items.index_count = 0;
  ptr->items.match_count = 0;
  ptr->items.selected_id = 0;
}/*}}}*/

void dmenu_default_menu(dmenu_t *ptr)
{/*{{{*/
  assert(ptr);
  debug("Restore default menu options.");

  ptr->menu.line_height = -1;
  ptr->menu.height = -1;
  ptr->menu.width = -1;
  ptr->menu.x = -1;
  ptr->menu.y = -1;
  ptr->menu.lines = 0;

  memset(&ptr->menu.style_normal_even, 0, sizeof(dstyle_t));
  memset(&ptr->menu.style_normal_odd, 0, sizeof(dstyle_t));
  memset(&ptr->menu.style_select, 0, sizeof(dstyle_t));
}/*}}}*/

void dmenu_getopt(dmenu_t *ptr, int argc, char *argv[])
{/*{{{*/
  assert(ptr);

  dmenu_default_items(ptr);
  dmenu_default_menu(ptr);

  debug("Parse command line options.");

  ptr->input.text = NULL;
  ptr->menu.lines = 5;
  ptr->prompt.text = "~>";
  ptr->x.monitor = -1;
  ptr->flag.show_at_bottom = 0;
  ptr->flag.fast_startup = 0;
  ptr->flag.case_insensitive = 0;

	int c, idx;
	opterr = 0;

	while(-1 != (c = getopt_long(argc, argv, optstr, optlng, &idx))) {
	  switch(c) {
	    case 'b':   /* Bottom bar */
	      ptr->flag.show_at_bottom = 1;
	      break;

	    case 'h':   /* print help message */
	      usage();
	      exit(EXIT_SUCCESS);

	    case 'f':   /* fast mode */
	      ptr->flag.fast_startup = 1;
	      break;

	    case 'i':   /* case-insensitive item matching */
	      ptr->flag.case_insensitive = 1;
	      break;

	    case 'l':   /* number of lines in vertical list */
	      ptr->menu.lines = atoi(optarg);
	      break;

	    case 'p':   /* adds prompt to left of input field */
	      ptr->prompt.text = optarg;
	      break;

	    case 'm':   /* select monitor */
	      ptr->x.monitor = atoi(optarg);
	      break;

	    case 'v':   /* print version number */
	      puts("dmenu-"VERSION);
	      exit(0);

	    default:
	      usage();
	      exit(EXIT_FAILURE);

	  } /* switch ... */
	} /* while ... */

	/* Apply string comparison */
	ptr->strncmp = ptr->flag.case_insensitive ? strncasecmp : strncmp;

	/* Apply match function */
	ptr->match.exec = NULL;
	ptr->match.data = NULL;

	/* Apply complete function */
	ptr->complete.exec = NULL;
	ptr->complete.data = NULL;
}/*}}}*/

void init_ui(dui_t *ui, const dx11_t *x)
{/*{{{*/
  assert(ui);
  assert(x);
  debug("Initialize user interface.");

  assert(0 <= x->depth);
  assert(0 <= x->width);
  assert(0 <= x->height);

  ui->x = x;
  ui->pixmap = XCreatePixmap(x->display, x->root, x->width, x->height, x->depth);
  ui->gc = XCreateGC(x->display, x->root, 0, NULL);
  XSetLineAttributes(x->display, ui->gc, 1, LineSolid, CapButt, JoinMiter);

  ui->visual = DefaultVisual(ui->x->display, ui->x->screen);
  ui->colormap = DefaultColormap(ui->x->display, ui->x->screen);

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

GList *init_fontset(GList *fonts, const dx11_t *x, const char *fontnames[])
{/*{{{*/
  assert(x);
  debug("Initialize fonts.");

  warn_if(!fontnames || !fontnames[0], "No fonts to load");
  if(!fontnames) return NULL;

  GList *new_fontlist = NULL;
  const char **it = fontnames;

  while(*it) {
    dfnt_t *new_font = load_xfont(x, *it, NULL);
    warn_if(!new_font, "Cannot load font: %s", *it);
    it += 1;

    /* Ignore invalid fonts */
    if(!new_font) continue;

    new_fontlist = g_list_prepend(new_fontlist, new_font);
  } /* while ... */

  /* Revert list of new fonts, to keep order of font names */
  warn_if(!new_fontlist, "Cannot load any font.");
  new_fontlist = g_list_reverse(new_fontlist);
  fonts = g_list_concat(fonts, new_fontlist);

  return fonts;
}/*}}}*/

void grab_keyboard(dx11_t *x)
{/*{{{*/
  assert(x);
  debug("Grab keyboard.");

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000  };
	int i;

	/* try to grab keyboard, we may have to wait for another process to ungrab */
	for (i = 0; i < 1000; i++) {
		const int ok = XGrabKeyboard(x->display, x->root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
		if(GrabSuccess == ok) return;
			return;
		nanosleep(&ts, NULL);
	} /* for ... */

	die("Cannot grab keyboard");
}/*}}}*/

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

void load_items(dmenu_t *ptr, FILE *in)
{/*{{{*/
  assert(in);
  assert(ptr);
  debug("Load items from stream.");

  const dstyle_t *style = &ptr->menu.style_normal_even;
  assert(style);

  char *line = NULL;
  size_t line_size = 0;
  ssize_t n_bytes;

  const size_t chunk_size = 64;
  ditem_t *tmp_index = NULL;
  size_t tmp_size = 0;
  size_t tmp_count = 0;

  /* Calculate max width of items */
  int max_width = 0;
  size_t lineno = 0;

  while(0 < (n_bytes = getline(&line, &line_size, in))) {
    /* Remove newline character */
    *(line + n_bytes - 1) = '\0';
    n_bytes -= 1;
    lineno += 1;

    warn_if(!n_bytes, "Skip empty line near row %lu", lineno);
    if(!n_bytes) continue;

    /* Resize item buffer */
    if(tmp_size <= tmp_count) {
      tmp_size += chunk_size;

      debug("Resize item buffer to %lu items.", tmp_size);
      tmp_index = (ditem_t*)xrealloc(tmp_index, tmp_size * sizeof(ditem_t));
  
    } /*  if ... */

    ditem_t *new_item = tmp_index + tmp_count;
    new_item->text = xstrdup(line);
    new_item->out = 0;

	  new_item->width = get_textwidth(style->font, new_item->text, n_bytes);
	  max_width = max(new_item->width, max_width);

    tmp_count += 1;
  } /* while .... */
  assert2(feof(in), "Cannot read all items: %m");

  free(line);
  line = NULL;

  ptr->items.index = tmp_index;
  ptr->items.index_count = tmp_count;

  /* Shrinkt number of lines to available items */
  ptr->menu.lines = min(ptr->menu.lines, tmp_count);

  /* Resize input field to hold widest item */
  ptr->input.width = max_width;
  
  /* Match all items by default */
  warn_if(!ptr->items.index_count, "No items were loaded.");

  ptr->items.match_count = ptr->items.index_count;
  ptr->items.match = (ditem_t**)xmalloc(ptr->items.match_count * sizeof(ditem_t*));
  size_t i;

  for(i = 0; i < ptr->items.match_count; i += 1) {
    /* Save positions of all items. This is easy, as all items are stored in a
     * consecutive array. The array of matches is also a consecutive array, so
     * only some offsets have to be calculated. */
    *(ptr->items.match + i) = ptr->items.index + i;
  } /* for ... */

}/*}}}*/

void init_style(dstyle_t *ptr, const dui_t *ui, const char *colornames[], size_t n, dfnt_t *font)
{/*{{{*/
  assert(ptr);
  assert(ui);
  debug("Initialize style.");

	/* need (at least) two colors for a scheme */
  assert2(2 <= n, "Cannot initialize style without foreground and background color.");
  warn_if(2 < n, "Style initialization only requires foreground and background color.");

  /* Store font for this style */
  ptr->font = font;  
  warn_if(!font, "Style is created without a corresponding font.");

  /* Allocate colors */
  assert2(XftColorAllocName(ui->x->display, ui->visual, ui->colormap, colornames[0], &ptr->foreground), "Cannot allocate color: `%s'", colornames[0]);
  assert2(XftColorAllocName(ui->x->display, ui->visual, ui->colormap, colornames[1], &ptr->background), "Cannot allocate color: `%s'", colornames[1]);
}/*}}}*/

void draw_rect(dui_t *ui, const XftColor color, int x, int y, int width, int height, int filled)
{/*{{{*/
  assert(ui);
  debug("Draw rectangle: x=%i, y=%i, width=%i, height=%i, filled=%s", x, y, width, height, filled ? "yes" : "no");

	XSetForeground(ui->x->display, ui->gc, color.pixel);

  assert(filled ? 1 < width : 0 < width);
  assert(filled ? 1 < height : 0 < height);

	if (filled) {
	  XFillRectangle(ui->x->display, ui->pixmap, ui->gc, x, y, width, height);

	} else {
	  XDrawRectangle(ui->x->display, ui->pixmap, ui->gc, x, y, width - 1, height - 1);
	} /* if ... */
}/*}}}*/

void draw_ntext(dui_t *ui, const dstyle_t *style, int x, int y, int width, int height, const char *text, size_t n)
{/*{{{*/
  assert(ui);
  assert(style);
  assert(0 < width);
  assert(0 < height);
  debug("Draw text: x=%i, y=%i, width=%i, height=%i, text=`%s'", x, y, width, height, text);

  /* Render box behind text */
  draw_rect(ui, style->background, x, y, width, height, 1);

  /* Check, if there's enough space for the text box */
  x += style->font->padding / 2;
  width -= style->font->padding;
  warn_if(0 >= width, "Text box is too small, even for a single letter.");

  if(0 >= width) return;

  /* Create rendering context and start rendering the text */
  XftDraw *draw = XftDrawCreate(ui->x->display, ui->pixmap, ui->visual, ui->colormap);

  /* Center text vertically in bounding box */
	const int text_y = y + (height - style->font->height) / 2 + style->font->xfont->ascent;
	XftDrawString8(draw, &style->foreground, style->font->xfont, x, text_y, (XftChar8*)text, n);

	XftDrawDestroy(draw);
}/*}}}*/

/* Draw text on ui using style at x/y. The bounding box is fixed to width and
 * height. If the text is wider than width, it is trimmed. */
void draw_text(dui_t *ui, const dstyle_t *style, int x, int y, int width, int height, const char *text)
{/*{{{*/
  draw_ntext(ui, style, x, y, width, height, text, strlen(text));
}/*}}}*/

void update_ui(dmenu_t *ptr)
{/*{{{*/
  assert(ptr);
  debug("Update user interface.");

  dui_t *ui = &ptr->ui;

	//unsigned int curpos;
	//struct item *item;
	int x = ptr->menu.x;
	int y = ptr->menu.y;

	int max_item_width = ptr->menu.width;

	/* Menu background */
	draw_rect(ui, ptr->menu.style_normal_even.background, ptr->menu.x, ptr->menu.y, ptr->menu.width, ptr->menu.height, 1);

	if(ptr->prompt.text) {
	  debug("Draw prompt to user interface: x=%i, y=%i, width=%i, height=%i", x, y, ptr->prompt.width, ptr->menu.line_height);
	  draw_text(ui, &ptr->prompt.style, x, y,  ptr->prompt.width, ptr->menu.line_height, ptr->prompt.text);
	  max_item_width -= ptr->prompt.width;
	  x += ptr->prompt.width;
	} /* if ... */

	assert(0 < max_item_width);

  if(ptr->input.text) {
    debug("Draw input to user interface: x=%i, y=%i, width=%i, height=%i", x, y, ptr->input.width, ptr->menu.line_height);
	  draw_text(ui, &ptr->input.style_good, x, y,  ptr->input.width, ptr->menu.line_height, ptr->input.text);
	} /* if ... */

	assert2(0 < ptr->menu.lines, "No other method implemented");

  /* Render menu items */
	if(0 < ptr->menu.lines) {

    /* Identify page, where the selected item is placed on */
    const size_t block = ptr->items.selected_id / ptr->menu.lines;
    const size_t idx_lo = block * ptr->menu.lines;
    const size_t idx_hi = min(idx_lo + ptr->menu.lines, ptr->items.match_count);
    size_t i;
  
	  for(i = idx_lo; i < idx_hi; i += 1) {
	    const dstyle_t *item_style = (i % 2) ? &ptr->menu.style_normal_even : &ptr->menu.style_normal_odd;
	    const dstyle_t *slct_style = &ptr->menu.style_select;
	    const ditem_t *item = ptr->items.match[i];

	    y += ptr->menu.line_height;

	    /* Redering full text */
	    if(ptr->items.selected_id == i) {
	      /* Render selected text */
	      draw_text(ui, slct_style, x, y, max_item_width, ptr->menu.line_height, item->text);
	    } else {
	      /* Render text with alternating style */
	      draw_text(ui, item_style, x, y, max_item_width, ptr->menu.line_height, item->text);
	    } /* if ... */

	  } /* for ... */

  } /* if ... */

	XCopyArea(ui->x->display, ui->pixmap, ui->menu_hwnd, ui->gc, ptr->menu.x, ptr->menu.y, ptr->menu.width, ptr->menu.height, 0, 0);
	XSync(ui->x->display, False);

}/*}}}*/

void init_all_styles(dmenu_t *ptr)
{/*{{{*/
  assert(ptr);
  debug("Initialize styles for user interface.");

	/* init appearance */
	assert2(ptr->fonts, "No fonts were loaded");
	const dui_t *ui = &ptr->ui;

	dfnt_t *font = (dfnt_t*)ptr->fonts->data;
	init_style(&ptr->menu.style_normal_even, ui, colors[dmenu_colorscheme_normal_even], 2, font);
	init_style(&ptr->menu.style_normal_odd,  ui, colors[dmenu_colorscheme_normal_odd],  2, font);
	init_style(&ptr->menu.style_select, ui, colors[dmenu_colorscheme_select], 2, font);
	init_style(&ptr->prompt.style, ui, colors[dmenu_colorscheme_prompt], 2, font);
	init_style(&ptr->input.style_good, ui, colors[dmenu_colorscheme_input_good], 2, font);
	init_style(&ptr->input.style_bad, ui, colors[dmenu_colorscheme_input_bad], 2, font);
}/*}}}*/

void setup_ui(dui_t *ui, dmenu_t *ptr)
{/*{{{*/
  assert(ui);
  assert(ptr);
  debug("Setup user interface.");

	ui->clip = XInternAtom(ui->x->display, "CLIPBOARD",   False);
	ui->utf8 = XInternAtom(ui->x->display, "UTF8_STRING", False);

	/* Calculate menu geometry */
	ptr->menu.line_height = ptr->menu.style_normal_even.font->height + 2;
	ptr->menu.lines = max(0, ptr->menu.lines);
	ptr->menu.height = (1 + ptr->menu.lines) * ptr->menu.line_height;

  #ifdef XINERAMA
  int num_screens;
  Window focus;
  XineramaScreenInfo *info;

	if ((info = XineramaQueryScreens(ui->x->display, &num_screens))) {
	  int di;

	  debug("Found %i screens.", num_screens);

		XGetInputFocus(ui->x->display, &focus, &di);

    if(!is_betweeen(ui->x->monitor, 0, num_screens) && (focus != ui->x->root) && (PointerRoot != focus) && (None != focus)) {
      Window parent;

      debug("Cannot select monitor %i.", ui->x->monitor);

			/* find top-level window containing current input focus */
			do {
			  Window root, *children;
        unsigned int num_children;

			  parent = focus;

			  XQueryTree(ui->x->display, parent, &root, &focus, &children, &num_children);
			  if(children) XFree(children);

			} while((focus != ui->x->root) && (focus != parent));

			/* find xinerama screen with which the window intersects most */
	    XWindowAttributes parent_attrs;
	    if(XGetWindowAttributes(ui->x->display, parent, &parent_attrs)) {
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
	          ptr->x.monitor = i;
	          area = tmp_area;
	        } /* if ... */
	      } /* for ... */
	    } /* if ... */

    } /* if ... */

		/* no focused window is on screen, so use pointer location instead */
		int pointer_x, pointer_y, win_x, win_y;
		unsigned int mask;
		if((0 > ui->x->monitor) && XQueryPointer(ui->x->display, ui->x->root, &focus, &focus, &pointer_x, &pointer_y, &win_x, &win_y, &mask)) {
		  int i;
		  debug("Select screen from pointer location.");

		  for(i = 0; i < num_screens; i += 1) {
		    const int in_x = is_betweeen(pointer_x, info[i].x_org, info[i].x_org + info[i].width);
		    const int in_y = is_betweeen(pointer_y, info[i].y_org, info[i].y_org + info[i].height);

		    if(!in_x || !in_y) continue;

		    ptr->x.monitor = i;
		    break;
		  }
		} /* if ... */

    warn_if((ui->x->monitor < 0) || (num_screens <= ui->x->monitor), "Cannot find monitor %i", ui->x->monitor);
    ptr->x.monitor = clip(ui->x->monitor, 0, num_screens - 1);

		ptr->menu.y = info[ptr->x.monitor].y_org;
		ptr->menu.y += (ptr->flag.show_at_bottom ? info[ptr->x.monitor].height - ptr->menu.height : 0);
		ptr->menu.x = info[ptr->x.monitor].x_org;
		ptr->menu.width = info[ptr->x.monitor].width;

		XFree(info);

	} else
  #endif /* XINERAMA */
	{
		ptr->menu.x = 0;
		ptr->menu.y = ptr->flag.show_at_bottom ? ui->x->height - ptr->menu.height : 0;
		ptr->menu.width = ui->x->width;
	} /* if ... */

  /* Calculate text width of prompt */
  if(ptr->prompt.text) {
    XGlyphInfo ext;

    const dfnt_t *font = ptr->prompt.style.font;
    const char *text = ptr->prompt.text;
    const size_t len = strlen(text);

	  XftTextExtentsUtf8(font->x->display, font->xfont, (XftChar8 *)text, len, &ext);

	  ptr->prompt.width = ext.xOff + font->padding;
  } /* if ... */

  // ptr->input.x = ptr->prompt.x + ptr->promt.width;

  warning("Maybe match all items?");
  //#warning Run match function
	// match_func();

	/* Create menu window */
	XSetWindowAttributes menu_attrs;
	menu_attrs.override_redirect = True;
	menu_attrs.background_pixel = ptr->menu.style_normal_even.background.pixel;
	menu_attrs.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

  assert(0 <= ptr->menu.height);
  assert(0 <= ptr->menu.width);
  const unsigned int border_width = 0;

	ui->menu_hwnd = XCreateWindow(ui->x->display, ui->x->root,
	    ptr->menu.x, ptr->menu.y, ptr->menu.width, ptr->menu.height, border_width,
	    ui->x->depth, CopyFromParent, ui->visual,
	    CWOverrideRedirect | CWBackPixel | CWEventMask, &menu_attrs);

	/* open input methods */
	ui->xim = XOpenIM(ui->x->display, NULL, NULL, NULL);
	ui->xic = XCreateIC(ui->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
	    XNClientWindow, ui->menu_hwnd, XNFocusWindow, ui->menu_hwnd, NULL);

	XMapRaised(ui->x->display, ui->menu_hwnd);

  update_ui(ptr);
}/*}}}*/

int main(int argc, char *argv[])
{
  dmenu_t dmenu;

  dmenu_getopt(&dmenu, argc, argv);

  warn_if(!setlocale(LC_CTYPE, "") || !XSupportsLocale(), "No locale support: %m");
	dmenu.x.display = XOpenDisplay(NULL);
	assert2(dmenu.x.display, "Cannot open display");
	dmenu.x.screen = DefaultScreen(dmenu.x.display);
	dmenu.x.root = RootWindow(dmenu.x.display, dmenu.x.screen);
	dmenu.x.height = DisplayHeight(dmenu.x.display, dmenu.x.screen);
	dmenu.x.width = DisplayWidth(dmenu.x.display, dmenu.x.screen);
	dmenu.x.depth = DefaultDepth(dmenu.x.display, dmenu.x.screen);

	init_ui(&dmenu.ui, &dmenu.x);
	dmenu.fonts = init_fontset(NULL, &dmenu.x, fonts);
	assert2(dmenu.fonts, "No fonts could be loaded");
  init_all_styles(&dmenu);

	/* Load data */
	if(dmenu.flag.fast_startup) {
	  debug("Perform fast start-up.");

	  grab_keyboard(&dmenu.x);
	  load_items(&dmenu, stdin);

	} else {
	  debug("Perform normal start-up.");

	  load_items(&dmenu, stdin);
	  grab_keyboard(&dmenu.x);
	} /* if ... */

	setup_ui(&dmenu.ui, &dmenu);

	XEvent ev;

	while (!XNextEvent(dmenu.x.display, &ev)) {
		if (XFilterEvent(&ev, dmenu.ui.menu_hwnd))
			continue;
		switch(ev.type) {
		case Expose:
			// if (ev.xexpose.count == 0)
			// 	drw_map(drw, win, 0, 0, mw, mh);
			break;
		case KeyPress:
		  exit(1);
			break;
		// case SelectionNotify:
		// 	if (ev.xselection.property == utf8)
		// 		paste();
		// 	break;
		// case VisibilityNotify:
		// 	if (ev.xvisibility.state != VisibilityUnobscured)
		// 		XRaiseWindow(dpy, win);
		// 	break;
		}
	} /* while ... */

	// XUngrabKeyboard
	// XUngrabKey(dpy, AnyKey, AnyModifier, root);
	// for (i = 0; i < SchemeLast; i++)
	// 	free(scheme[i]);
	// drw_free(drw);
	XSync(dmenu.x.display, False);
	XCloseDisplay(dmenu.x.display);

	// sw = DisplayWidth(dpy, screen);
	// sh = DisplayHeight(dpy, screen);
	// drw = drw_create(dpy, screen, root, sw, sh);
	// if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
	// 	die("no fonts could be loaded.");
	// lrpad = drw->fonts->h;

	// if (fast) {
	// 	grabkeyboard();
	// 	readstdin();
	// } else {
	// 	readstdin();
	// 	grabkeyboard();
	// }
	// setup();
	//run();

	return 1; /* unreachable */
}
