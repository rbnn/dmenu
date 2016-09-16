#ifndef XCMD_H
#define XCMD_H
#include <stdio.h>

typedef struct xcmd xcmd_t;
typedef struct xcmd_config  xcfg_t;
typedef enum xcmd_match     xmatch_t;
typedef enum xcmd_complete  xcomplete_t;

struct xcmd
{   //{{{
  /* Items */
  struct
  {
    char **index;
    char *data;
    size_t count;
  } all_items;

  struct
  {
    char **index;
    char **shadow;
    size_t count;
    size_t selected;
  } matches;

  /* Callback functions for the model */
  int(*strncmp)(const char*, const char*,const size_t);
  void*(*match_init)(xcmd_t*,const char*);
  void(*match_free)(const xcmd_t*,void*);
  int(*match)(const xcmd_t*,const char*,const char*,const void*);
  void*(*complete_init)(const xcmd_t*);
  void (*complete_free)(const xcmd_t*,void*);
  int(*complete)(const xcmd_t*,char**,size_t*,void*);
  int    match_ok;    /* Set true by match_init(), if it was successfull */
  void  *match_data;  /* Transparent storage class for matching algorithm */
  void  *complete_data;

  /* Callback functions for MVC-pattern */
  void(*observer)(void*,const xcmd_t*);
  void *observer_data;
  int has_changed;

  /* Prompt */
  char *prompt;
};  //}}}

enum xcmd_match
{
  xcmd_match_prefix,
  xcmd_match_strip_prefix,
  xcmd_match_regex,
  xcmd_match_none
};

enum xcmd_complete
{
  xcmd_complete_none
};

struct xcmd_config
{   //{{{
  xmatch_t    match;
  xcomplete_t complete;
  int         case_insensitive;
  char       *prompt;
};  //}}}

void xcmd_init(xcmd_t *ptr, const xcfg_t *cfg);
void xcmd_destroy(xcmd_t *ptr);

int xcmd_read_items(xcmd_t *ptr, FILE *f);
int xcmd_add_item(xcmd_t *ptr, const char *s);
int xcmd_finish_items(xcmd_t *ptr);
int xcmd_update_matching(xcmd_t *ptr, const char *input);
int xcmd_update_selected(xcmd_t *ptr, const long offset, const int relative);
int xcmd_auto_complete(const xcmd_t *ptr, char **inputptr, size_t *n);

int xcmd_notify_observer(xcmd_t *ptr);

/* Match: Prefix */
int match_prefix(const xcmd_t *ptr, const char *input, const char *text, const void *data);
int match_strip_prefix(const xcmd_t *ptr, const char *input, const char *text, const void *data);
/* Match: Regex */
int   match_regex(const xcmd_t *ptr, const char *input, const char *text, const void *data);
void *match_regex_init_case(xcmd_t *ptr, const char *input);
void *match_regex_init_icase(xcmd_t *ptr, const char *input);
void  match_regex_free(const xcmd_t *ptr, void *data);

/* Configuration */
int xcmd_config_load(xcfg_t *ptr, FILE *f);
int xcmd_config_default(xcfg_t *ptr);
#endif /* XCMD_H */
