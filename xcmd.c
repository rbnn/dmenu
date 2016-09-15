#include "xcmd.h"
#include "utils.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void xcmd_init(xcmd_t *ptr, const xcfg_t *cfg)
{
  assert(ptr);
  xcfg_t default_config;
  debug("Initialize xcmd-model.");

  /* Initialize items */
  ptr->all_items = NULL;
  ptr->matching_items = NULL;
  ptr->matching_count = 0;
  ptr->selected_item = 0;

  /* Select appropriate configuration */
  debug("Apply %s configuration.", cfg ? "default" : "user");

  if(!cfg) {
    xcmd_config_default(&default_config);
    cfg = &default_config;
  } /* if ... */

  /* String comparison */
  if(cfg->case_insensitive) { ptr->strncmp = &strncasecmp;
  } else { ptr->strncmp = &strncmp;
  } /* if ... */

  /* Auto complete */
  switch(cfg->complete) {
    case xcmd_complete_none:
      ptr->complete = NULL;
      break;
  } /* switch ... */

  /* Match functions */
  switch(cfg->match) {
    case xcmd_match_prefix:
      ptr->match = &match_prefix;
      break;

    case xcmd_match_strip_prefix:
      ptr->match = &match_strip_prefix;
      break;

    /* As the match-function is required, fail here */
    case xcmd_match_none: die("Invalid match-algorithm!");
  } /* if ... */

  /* Prompt */
  ptr->prompt = cfg->prompt;

  /* MVC */
  ptr->observer = NULL;
  ptr->observer_data = NULL;
  ptr->has_changed = 0;
}

void xcmd_destroy(xcmd_t *ptr)
{
  /* Don't destroy NULL-pointer */
  if(!ptr) return;

  /* Free items */
  ptr->matching_count = 0;
  ptr->selected_item = 0;
  g_list_free(ptr->matching_items);
  g_list_free_full(ptr->all_items, (GDestroyNotify)free);

  /* Model functions */
  ptr->strncmp = NULL;
  ptr->complete = NULL;
  ptr->match = NULL;

  /* MVC */
  ptr->observer = NULL;
  ptr->observer_data = NULL;
  ptr->has_changed = 0;
}

int xcmd_read_items(xcmd_t *ptr, FILE *f)
{
  assert(ptr);
  assert(f);
  debug("Read items from input stream.");

  char *line = NULL;
  size_t line_size = 0;
  ssize_t n_bytes = 0;

  while(0 < (n_bytes = getline(&line, &line_size, f))) {
    /* Remove trailing newline character */
    *(line + n_bytes - 1) = '\0';

    assert2(!xcmd_add_item(ptr, line), "Cannot add item ˋ%s'!", line);
  } /* while ... */
  assert2(feof(f), "Cannot read input: %m");

  free(line);

  return 0;
}

int xcmd_add_item(xcmd_t *ptr, const char *s)
{
  assert(ptr);
  warn_if(!s, "Ignore empty item.");
  if(!s) return 0;

  debug("Add item ˋ%s'.", s);
  ptr->all_items = g_list_prepend(ptr->all_items, xstrdup(s));
  return 0;
}

int xcmd_finish_items(xcmd_t *ptr)
{
  assert(ptr);
  debug("Finish list of items.");

  ptr->all_items = g_list_reverse(ptr->all_items);
  ptr->matching_items = g_list_copy(ptr->all_items);
  ptr->matching_count = g_list_length(ptr->matching_items);
  ptr->selected_item = 0;

  /* Notify observer, as the model has changed */
  ptr->has_changed = 1;
  xcmd_notify_observer(ptr);

  return 0;
}

int xcmd_update_matching(xcmd_t *ptr, const char *input)
{
  assert(ptr);
  debug("Update matching items using input ˋ%s'.", input);

  if(!input || !strlen(input)) {
    /* Select all items, if input is empty */
    g_list_free(ptr->matching_items);
    ptr->matching_items = g_list_copy(ptr->all_items);

  } else {
    /* Select from matching items */
    GList *new_matching_items = NULL;
    GList *it;

    /* Require match-function to be set */
    assert(ptr->match);

    for(it = ptr->matching_items; it; it = g_list_next(it)) {
      char *item = (char*)it->data;
      debug("Matching item ˋ%s' and input ˋ%s'.", item, input);

      /* If item doesn't match the input, go to the next one. */
      if((*ptr->match)(ptr, input, item)) continue;

      new_matching_items = g_list_prepend(new_matching_items, item);
    } /* for ... */

    g_list_free(ptr->matching_items);
    ptr->matching_items = g_list_reverse(new_matching_items);
  } /* if ... */

  /* Select first matching item */
  ptr->matching_count = g_list_length(ptr->matching_items);
  ptr->selected_item = 0;

  /* Notify observer, as the model has changed */
  ptr->has_changed = 1;
  xcmd_notify_observer(ptr);

  return 0;
}

int xcmd_update_selected(xcmd_t *ptr, const int offset)
{
  assert(ptr);
  debug("Update selected item.");

  /* An offset of 0 doesn't change anything */
  if(!offset) return 0;

  const size_t old_selected_item = ptr->selected_item;
  ptr->selected_item += offset;
  ptr->selected_item %= ptr->matching_count;
  // if(0 < offset) {
  //   /* Don't go beyond last element */
  //   ptr->selected_item += ((ptr->selected_item + 1) < ptr->matching_count) ? 1 : 0;
  // } else {
  //   /* Don't go beyond first element */
  //   ptr->selected_item -= ptr->selected_item ? 1 : 0;
  // } /* if ... */

  assert(!ptr->matching_count || (ptr->selected_item < ptr->matching_count));

  /* Notify observer, as the model has changed */
  ptr->has_changed |= (old_selected_item != ptr->selected_item);
  xcmd_notify_observer(ptr);

  return 0;
}

int xcmd_auto_complete(const xcmd_t *ptr, char **inputptr, size_t *n)
{
  assert(ptr);
  assert(inputptr);
  assert(n);

  /* No data --> No auto-complete */
  if(!ptr->matching_items) return 0;

  /* No auto-complete function */
  if(!ptr->complete) return 0;

  /* On error ptr->complete() must not change inputpr. Return values are:
   * -1: Auto-complete failed
   *  0: Success, no changes made by auto-complete
   * +1: Success, changes made by auto-complete
   */
  debug("Run auto-complete.");
  return (*ptr->complete)(ptr, inputptr, n);
}

int xcmd_notify_observer(xcmd_t *ptr)
{
  assert(ptr);
  
  /* No observer to be notified */
  if(!ptr->observer) return -1;

  /* No need to inform the observer */
  if(!ptr->has_changed) return 0;

  /* Notify observer and reset changes */
  (*ptr->observer)(ptr->observer_data, ptr);
  ptr->has_changed = 0;

  return 0;
}

/* Match functions */
int match_prefix(const xcmd_t *ptr, const char *input, const char *text)
{
  assert(ptr);
  assert(input);
  assert(text);
  assert(ptr->strncmp);

  /* Get minimum length of input and text */
  const size_t input_size = strlen(input);
  const size_t text_size = strlen(text);

  /* This is the case, when input is no longer a prefix of text */
  if(text_size < input_size) return +1;

  return (*ptr->strncmp)(input, text, input_size);
}

int match_strip_prefix(const xcmd_t *ptr, const char *input, const char *text)
{
  assert(ptr);
  assert(input);
  assert(text);

  /* Strip leading white space characters of input */
  while(('\0' != *input) && isspace(*input)) {
    input += 1;
  } /* while ... */

  /* Strip leading white space characters of text */
  while(('\0' != *text) && isspace(*text)) {
    text += 1;
  } /* while ... */

  return match_prefix(ptr, input, text);
}

/* Configuration */
int xcmd_config_load(xcfg_t *ptr, FILE *f){assert(0); return 0;}
       
int xcmd_config_default(xcfg_t *ptr)
{
  assert(ptr);

  ptr->match = xcmd_match_prefix;
  ptr->complete = xcmd_complete_none;
  ptr->case_insensitive = 1;
  ptr->prompt = xstrdup("~>");

  return 0;
}
