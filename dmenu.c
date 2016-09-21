/* See LICENSE file for copyright and license details. */
#include "util.h"
#include "xcmd.h"
#include <glib.h>
#include <getopt.h>
// #include <libconfig.h>

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

#include "config.h"

#ifdef DISABLED

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

#include "x.h"
#include "viewer.h"
#include "controller.h"

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
