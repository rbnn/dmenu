/* See LICENSE file for copyright and license details. */
#include "util.h"
#include "xcmd.h"
#include <glib.h>
#include <getopt.h>
// #include <libconfig.h>

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

#include "x.h"
#include "viewer.h"
#include "controller.h"

void dmenu_getopt(dx11_t *x, xcmd_t *model, dview_t *view, dctrl_t *control, int argc, char *argv[])
{/*{{{*/
  assert(x);
  assert(model);
  assert(view);
  assert(control);
  xcfg_t model_config;

  xcmd_config_default(&model_config);

  debug("Parse command line options.");

  x->monitor = -1;
  view->menu.lines = 5;
  view->prompt.text = "~>";
  view->show_at_bottom = 0;
  view->single_column = 0;
  control->fast_startup = 0;
  // char *config_file = NULL;

  const GOptionEntry options[] = 
  {
    {"bottom",      'b', 0, G_OPTION_ARG_NONE,    &view->show_at_bottom,          "Place window at the bottom of the screen", NULL  },
    /* {"config",      'c', 0, G_OPTION_ARG_STRING,  &config_file,                   "Load configuration from FILE",             "FILE"}, */
    /* {"exec",        'e', 0, G_OPTION_ARG_STRING,  &control->exec,                 "Execute PROG using selection",             "PROG"}, */
    {"ignore-case", 'i', 0, G_OPTION_ARG_NONE,    &model_config.case_insensitive, "Compare strings ignoring case",            NULL  },
    {"fast",        'f', 0, G_OPTION_ARG_NONE,    &control->fast_startup,         "Read input after grabbing the keyboard",   NULL  },
    {"lines",       'l', 0, G_OPTION_ARG_INT,     &view->menu.lines,              "Display input using N lines",              "N"   },
    {"prompt",      'p', 0, G_OPTION_ARG_STRING,  &view->prompt.text,             "Use STR as prompt message",                "STR" },
    {"monitor",     'm', 0, G_OPTION_ARG_INT,     &x->monitor,                    "Place window on screen ID",                "ID"  },
    {"single-column",0,  0, G_OPTION_ARG_NONE,    &view->single_column,           "Render items as single column view",       NULL  },
    {NULL,           0 , 0, 0,                    NULL,                           NULL,                                       NULL  }
  };

  GError *error = NULL;
  GOptionContext *context = g_option_context_new("- menu for the X Window System");
  g_option_context_add_main_entries(context, options, NULL);

  die_if(!g_option_context_parse(context, &argc, &argv, &error), "Option parsing failed: %s", error->message);
  g_option_context_free(context);

	/* Apply model configuration */
	xcmd_init(model, &model_config);

}/*}}}*/

int main(int argc, char *argv[])
{/*{{{*/
  dx11_t x = {0};
  xcmd_t model;   /* M */
  dview_t view;   /* V */
  dctrl_t ctrl;   /* C */

  /* Configure MVC */
 //  dmenu_getopt(&x, &dmenu, &model, argc, argv);
  dmenu_getopt(&x, &model, &view, &ctrl, argc, argv);

  /* Setup callback functions for the model */
	model.observer = (void(*)(void*,const xcmd_t*))viewer_update;
	model.observer_data = &view;

  /* Initialize X window system */
  init_x11(&x);

  /* Setup the viewer */
	viewer_init(&view, &x, colors, fonts);

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

	if(ctrl.result) {
	  printf("%s\n", ctrl.result);
	} /* if ... */

	return 1; /* unreachable */
}/*}}}*/
