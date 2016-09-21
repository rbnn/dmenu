#include "clip.h"
#include "xcmd.h"
#include "util.h"
#include <ctype.h>
#include <glib.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

static void *match_regex_initx(xcmd_t *ptr, const char *input, const int flags);

void xcmd_init(xcmd_t *ptr, const xcfg_t *cfg)
{
  assert(ptr);
  xcfg_t default_config;
  debug("Initialize xcmd-model.");

  /* Initialize items */
  ptr->items.index = NULL;
  ptr->items.data  = 0;
  ptr->items.count = 0;
  ptr->matches.index   = NULL;
  ptr->matches.shadow  = NULL;
  ptr->matches.count = 0;
  ptr->matches.selected = 0;
  ptr->matches.input = NULL;
  ptr->matches.complete = g_string_new(NULL);

  /* Select appropriate configuration */
  debug("Apply %s configuration.", cfg ? "default" : "user");

  if(!cfg) {
    xcmd_config_default(&default_config);
    cfg = &default_config;
  } /* if ... */

  /* String comparison */
  debug("String comparison: case-%ssensitive", cfg->case_insensitive ? "in" : "");
  if(cfg->case_insensitive) {
    /* Compare strings ignoring case */
    ptr->strncmp = &strncasecmp;
  } else {
    /* Compare string case sensitive */
    ptr->strncmp = &strncmp;
  } /* if ... */

  /* Auto complete */
  ptr->complete_data = NULL;
  switch(cfg->complete) {
    case xcmd_complete_none:
      debug("Disable auto-complete.");
      ptr->complete_init = NULL;
      ptr->complete_free = NULL;
      ptr->complete = NULL;
      break;
  } /* switch ... */

  /* Match functions */
  ptr->match_data = NULL;
  switch(cfg->match) {
    case xcmd_match_prefix:
      debug("Match items on common prefix.");
      ptr->match_init = NULL;
      ptr->match_free = NULL;
      ptr->match = match_prefix;
      break;

    case xcmd_match_strip_prefix:
      debug("Match items on common stripped prefix.");
      ptr->match_init = NULL;
      ptr->match_free = NULL;
      ptr->match = match_strip_prefix;
      break;

    case xcmd_match_regex:
      debug("Match items on regular expression.");
      ptr->match_init = cfg->case_insensitive ? match_regex_init_icase : match_regex_init_case;
      ptr->match_free = match_regex_free;
      ptr->match = match_regex;
      break;

    /* As the match-function is required, fail here */
    case xcmd_match_none:
      die("Invalid match-algorithm!");
  } /* if ... */

  /* MVC */
  ptr->observer = NULL;
  ptr->observer_data = NULL;
  ptr->has_changed = 0;
}

void xcmd_destroy(xcmd_t *ptr)
{
  /* Don't destroy NULL-pointer */
  if(!ptr) return;

  /* Free auto-complete data */
  if(ptr->complete_free) {
    ptr->complete_free(ptr, ptr->complete_data);
    ptr->complete_data = NULL;
  } /* if ... */

  /* Free items */
  free(ptr->items.index);
  free(ptr->items.data);
  free(ptr->matches.index);
  free(ptr->matches.shadow);
  ptr->items.index = NULL;
  ptr->items.data = NULL;
  ptr->matches.index  = NULL;
  ptr->matches.shadow = NULL;
  ptr->matches.count = 0;
  ptr->matches.selected = 0;
  g_string_free(ptr->matches.complete, TRUE);
  ptr->matches.complete = NULL;

  /* Model functions */
  ptr->strncmp = NULL;

  if(ptr->complete_free) ptr->complete_free(ptr, ptr->complete_data);
  ptr->complete_init = NULL;
  ptr->complete_free = NULL;
  ptr->complete_data = NULL;
  ptr->complete = NULL;

  if(ptr->match_free) ptr->match_free(ptr, ptr->match_data);
  ptr->match_init = NULL;
  ptr->match_free = NULL;
  ptr->match_data = NULL;
  ptr->match = NULL;

  /* MVC */
  ptr->observer = NULL;
  ptr->observer_data = NULL;
  ptr->has_changed = 0;

  /* Prompt */
  free(ptr->prompt);
  ptr->prompt = NULL;
}

int xcmd_read_items(xcmd_t *ptr, FILE *f)
{
  assert(ptr);
  assert(f);
  debug("Read items from input stream.");

  /* Dynamic buffer, that will be saved in ptr. */
  assert2(!ptr->items.data, "Items have already been initialized!");
  ptr->items.count = 0;
  const size_t block_size = 1024;
  size_t buffer_max_size = 0;
  size_t buffer_size = 0;

  char *line = NULL;
  size_t line_size = 0;
  ssize_t n_bytes = 0;

  while(0 < (n_bytes = getline(&line, &line_size, f))) {
    /* Remove trailing newline character */
    *(line + n_bytes - 1) = '\0';

    /* Grow buffer */
    const size_t old_buffer_size = buffer_size;
    buffer_size += n_bytes;

    if(buffer_max_size <= buffer_size) {
      const size_t n_blocks = 1 + buffer_size / block_size;
      buffer_max_size = n_blocks * block_size;
      ptr->items.data = (char*)xrealloc(ptr->items.data, buffer_max_size);
    } /* if ... */

    memcpy(ptr->items.data + old_buffer_size, line, n_bytes);
    ptr->items.count += 1;

  } /* while ... */
  assert2(feof(f), "Cannot read input: %m");

  free(line);
  line = NULL;

  return 0;
}

int xcmd_finish_items(xcmd_t *ptr)
{
  assert(ptr);
  debug("Finish list of items.");

  /* Allocate indexes */
  assert2(!ptr->items.index, "Items have already been finished!");
  assert2(0 < ptr->items.count, "No data!");
  ptr->items.index = (char**)xmalloc(ptr->items.count * sizeof(char*));
  ptr->matches.index  = (char**)xmalloc(ptr->items.count * sizeof(char*));
  ptr->matches.shadow = (char**)xmalloc(ptr->items.count * sizeof(char*));
  ptr->matches.count = ptr->items.count;
  ptr->matches.selected = 0;

  /* Fill indexes */
  char *x = ptr->items.data;
  char **it = ptr->items.index;
  char **const end = ptr->items.index + ptr->items.count;

  for(it = ptr->items.index; end != it; it += 1) {
    *it = x;
    /* Advance buffer by length of string and the NUL-byte */
    const size_t len = strlen(x);
    x += 1 + len;

    assert2(TRUE == g_utf8_validate(*it, len, NULL), "Found invalid UTF-8 string in element %lu!", it - ptr->items.index);
  } /* for ... */

  memcpy(ptr->matches.index, ptr->items.index, ptr->items.count * sizeof(char*));

  /* Update auto-complete data */
  if(ptr->complete_init) ptr->complete_data = ptr->complete_init(ptr);

  /* Notify observer, as the model has changed */
  ptr->has_changed = 1;
  xcmd_notify_observer(ptr);

  return 0;
}

int xcmd_update_matching(xcmd_t *ptr, const char *input)
{
  assert(ptr);
  debug("Update matching items using input ˋ%s'.", input);
  
  const size_t old_count = ptr->matches.count;

  if(!input || !strlen(input)) {
    /* Select all items, if input is empty */
    memcpy(ptr->matches.shadow, ptr->items.index, ptr->items.count * sizeof(char*));
    ptr->matches.count = ptr->items.count;

  } else {
    /* Select from matching items */
    ptr->match_ok = 0;  /* Reset */

    if(ptr->match_init) {
      /* Initialize matching data */
      ptr->match_data = ptr->match_init(ptr, input);
    } else {
      /* Without init-function, match_data is always usable. */
      ptr->match_ok = 1;
    } /* if ... */

    /* No changes will occur, if the data isn't usable */
    if(!ptr->match_ok) return -1;

    /* Require match-function to be set */
    assert(ptr->match);

    char **it;
    char **const end = ptr->items.index + ptr->items.count;
    ptr->matches.count = 0;

    /* Use double buffering-tchnique to calculate matches */
    for(it = ptr->items.index; end != it; it += 1) {
      /* If item doesn't match the input, go to the next one. */
      if(!ptr->match(ptr, input, *it, ptr->match_data)) continue;

      *(ptr->matches.shadow + ptr->matches.count) = *it;
      ptr->matches.count += 1;
    } /* for ... */

    if(ptr->match_free) {
      ptr->match_free(ptr, ptr->match_data);
      ptr->match_data = NULL;
    } /* if ... */
  } /* if ... */

  /* Select first matching item */
  ptr->matches.selected = 0;
  ptr->matches.input = input;

  /* Detect changes to double buffer */
  ptr->has_changed |= (old_count != ptr->matches.count) 
      || memcmp(ptr->matches.index, ptr->matches.shadow, ptr->matches.count * sizeof(char*));

  memcpy(ptr->matches.index, ptr->matches.shadow, ptr->matches.count * sizeof(char*));
  xcmd_notify_observer(ptr);

  return 0;
}

int xcmd_update_selected(xcmd_t *ptr, const long offset, const int relative)
{
  assert(ptr);
  debug("Update selected item.");

  /* An offset of 0 doesn't change anything */
  if(!offset) return 0;

  /* No data */
  if(!ptr->matches.count) return 0;

  const size_t old_selected_item = ptr->matches.selected;
  if(relative) {
    const long yz = (long)ptr->matches.count;
    const long dx = clip(offset, -yz, +yz);
    const long dn = (0 <= offset) ? 0 : ptr->matches.count;
    const long ds = dn + dx;
    assert(0 <= ds);

    ptr->matches.selected += ds;
    ptr->matches.selected %= ptr->matches.count;

  } else {
    /* Select absolute element */
    const size_t hi = ptr->matches.count;
    ptr->matches.selected = clip(offset, 0, hi - (hi ? 1 : 0));
  } /* if ... */

  /* Notify observer, as the model has changed */
  ptr->has_changed |= (old_selected_item != ptr->matches.selected);
  xcmd_notify_observer(ptr);

  return 0;
}

int xcmd_auto_complete(xcmd_t *ptr)
{
  assert(ptr);

  /* No data --> No auto-complete */
  if(!ptr->matches.count) return 0;

  // /* No auto-complete function */
  // if(!ptr->complete) return 0;

  /* On error ptr->complete() must not change inputpr. Return values are:
   * -1: Auto-complete failed
   *  0: Success, no changes made by auto-complete
   * +1: Success, changes made by auto-complete
   */
  debug("Run auto-complete.");


  char **it = ptr->matches.index;
  char **const end = ptr->matches.index + ptr->matches.count;
  GString *str = g_string_overwrite(ptr->matches.complete, 0, *it);
  it += 1;

  while(end != it) {
    while(str->len && strncmp(str->str, *it, str->len)) {
      debug("Complete? `%s' -- `%s'", str->str, *it);
      str = g_string_truncate(str, str->len - 1);
    } /* while ... */
    it += 1;
  }

  if(str->len) {
    ptr->matches.input = str->str;
    return 1;

  } else {
    return 0;

  } /* if ... */
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

/* Match: Prefix */
int match_prefix(const xcmd_t *ptr, const char *input, const char *text, const void *data)
{
  assert(ptr);
  assert(input);
  assert(text);
  assert(ptr->strncmp);
  debug("Match input ˋ%s' against text ˋ%s'.", input, text);

  /* Get minimum length of input and text */
  const size_t input_size = strlen(input);
  const size_t text_size = strlen(text);

  /* This is the case, when input is no longer a prefix of text */
  if(text_size < input_size) return 0;

  return !ptr->strncmp(input, text, input_size);
}

int match_strip_prefix(const xcmd_t *ptr, const char *input, const char *text, const void *data)
{
  assert(ptr);
  assert(input);
  assert(text);
  debug("Match stripped input ˋ%s' against text ˋ%s'.", input, text);

  /* Strip leading white space characters of input */
  while(('\0' != *input) && isspace(*input)) {
    input += 1;
  } /* while ... */

  /* Strip leading white space characters of text */
  while(('\0' != *text) && isspace(*text)) {
    text += 1;
  } /* while ... */

  return match_prefix(ptr, input, text, data);
}

/* Match: Regex */
int match_regex(const xcmd_t *ptr, const char *input, const char *text, const void *data)
{
  assert(ptr);
  assert(input);
  assert(text);

  /* Check for regular expression */
  if(!data) return 0;

  const regex_t *reg = (const regex_t*)data;
  return (REG_NOMATCH != regexec(reg, text, 0, NULL, 0));
}

void *match_regex_initx(xcmd_t *ptr, const char *input, const int flags)
{
  assert(ptr);
  assert(input);

  regex_t reg, *p_reg;
  if(regcomp(&reg, input, REG_EXTENDED | REG_NOSUB | flags)) {
    /* `regcomp(3)' failed */
    ptr->match_ok = 0;
    return NULL;
  } /* if ... */

  const size_t n = sizeof(regex_t);
  p_reg = (regex_t*)xmalloc(n);
  memcpy(p_reg, &reg, n);
  ptr->match_ok = 1;
  return p_reg;
}

void *match_regex_init_case(xcmd_t *ptr, const char *input)
{
  return match_regex_initx(ptr, input, 0);
}

void *match_regex_init_icase(xcmd_t *ptr, const char *input)
{
  return match_regex_initx(ptr, input, REG_ICASE);
}

void match_regex_free(const xcmd_t *ptr, void *data)
{
  assert(ptr);
  free(data);
}

/* Configuration */
int xcmd_config_load(xcfg_t *ptr, FILE *f){assert(0); return 0;}
       
int xcmd_config_default(xcfg_t *ptr)
{
  assert(ptr);

  ptr->match = xcmd_match_prefix;
  ptr->complete = xcmd_complete_none;
  ptr->case_insensitive = 0;

  return 0;
}
