#ifndef XCMD_H
#define XCMD_H
#include <stdio.h>

typedef struct xcmd xcmd_t;
typedef struct xcmd_config  xcfg_t;
typedef enum xcmd_match     xmatch_t;
typedef enum xcmd_complete  xcomplete_t;

/** \brief Model container
 *
 * The \c xcmd structure represents the model for a MVC-pattern. Therefore it
 * not only contains the whole state but also a callback function and a
 * callback argument for updating the observer,
 */
struct xcmd
{
  /** \brief Container for all items */
  struct
  {
    /** \brief List of items
     *
     * This list contains the start addresses of all strings in \c data.
     */
    char **index;
    /** \brief Contingous string of all items
     *
     * The string contains all items in a contigous way. Single items are
     * separated by a single NUL-byte.
     */
    char *data;
    /** \brief Number of items stored */
    size_t count;
  } all_items;

  /** \brief Container for a subset of items */
  struct
  {
    /** \brief Subset of items */
    char **index;
    /** \brief Subset of items
     *
     * This is for internal use only. While updating the selection, new items
     * are written into \c shadow and are finally copied to \c index. This is
     * used to detect changes made by the selection. */
    char **shadow;
    /** \brief Number of items stored */
    size_t count;
    /** \brief Currently selectet item in subset */
    size_t selected;
  } matches;

  /** \brief String comparison function
   *
   * This function pointer controls, how strings are compared. In order to
   * transparently allow e.g. case sensitive and case insensitive string
   * comparison, this variable can point to \c strncmp or \c strncasecmp.
   */
  int(*strncmp)(const char*, const char*,const size_t);
  /** \brief Initializer callback for match-data
   *
   * Some \c match functions might require to retain state information between
   * successive calls. Therefore on any call to \c xcmd_update_matching, the
   * function pointed to by this variable will be called with first and second
   * arguments being the current container \c ptr and the \c input given to \c
   * xcmd_update_matching. \c NULL disables this behaviour.
   */
  void*(*match_init)(xcmd_t*,const char*);
  /** \brief Finalizer callback for match-data
   *
   * If some match-functions  make use of \c match_init on calls to \c
   * xcmd_update_matching they should also specify \c match_free. The function
   * pointed to by this variable will be called after completing \c
   * xcmd_update_matching in order to clean-up the state information used by
   * some \c match functions. \c NULL disables this behaviour. In all cases the
   * variable \c match_data will be set to \c NULL on exit from \c
   * xcmd_update_matching.
   */
  void(*match_free)(const xcmd_t*,void*);
  /** \brief Match items against input
   *
   * The function pointed to by this variable will be called for every item
   * inside of \c xcmd_update_matching. This variable is required to point to
   * an appropriate function and must not be \c NULL. For every item in \c
   * all_items the output of the function is checked if it evaluates non-zero
   * and is inserted into \c matches. The parameters to \c match are as
   * follows:
   * -# \c ptr passed to \c xcmd_update_matching
   * -# \c input passed to \c xcmd_update_matching
   * -# An item from \c all_items
   * -# Value of \c match_data
   */
  int(*match)(const xcmd_t*,const char*,const char*,const void*);
  void*(*complete_init)(const xcmd_t*);
  void (*complete_free)(const xcmd_t*,void*);
  int(*complete)(const xcmd_t*,char**,size_t*,void*);
  /** \brief State of \c match_init
   *
   * If \c match_init points to an appropriate function every call to \c
   * xcmd_update_matching will cause \c match_init to initialize the variable
   * \c match_data. If this initialization succeded, \c match_init shall update
   * this variable. If \c match_init points to \c NULL, this variable will \em
   * automatically set to \c 1 (one).
   */
  int    match_ok;    /* Set true by match_init(), if it was successfull */
  void  *match_data;  /* Transparent storage class for matching algorithm */
  void  *complete_data;

  /** \brief Callback function to update the observer
   *
   * This container follows the Model-View-Controller design pattern. To allow
   * observers to be notified on changes to this container, \c observer shall
   * point to an appropriate function that carries out this update. The
   * function will receive the value of \c observer_data as first and the
   * address of the changed container as second argument. If this variable
   * points to \c NULL, no observer will be notified.
   */
  void(*observer)(void*,const xcmd_t*);
  /** \brief Data passed to \c observer
   *
   * This variable contains the data, that will be passed to \c observer as
   * first argument to distinguish different observer states.
   */
  void *observer_data;
  /** \brief Change indicator
   *
   * The variable contains the information, whether the container was changed
   * or not. If it is set to non-zero the container is meant to been changed
   * and a call to \c xcmd_notify_observer will actually notify the observer by
   * calling \c observer. Otherwise this call is suppressed, as the container
   * is not identified of being changed since the last call.
   */
  int has_changed;

  /* Prompt */
  //deprecated
  char *prompt;
};

/** \brief Match algorithms */
enum xcmd_match
{
  /** \brief Match items on a common prefix
   *
   * The \c match function will evaluate non-zero if \c input is a prefix of
   * the current \c item.
   */
  xcmd_match_prefix,
  /** \brief Match items on a common prefix without leading whitespace
   *
   * Similar to \c xcmd_match_prefix except that leading whitespace will be
   * ignored on both \c input and \c item on comparison.
   */
  xcmd_match_strip_prefix,
  /** \brief Match items on a regular expression
   *
   * The \c input is taken as extended regular expression and evaluates to
   * non-zero if \c regexec succeeds matching \c item.
   */
  xcmd_match_regex,
  /** \brief Failure state
   *
   * Invalid matching function. This will result in an error.
   */
  xcmd_match_none
};

/** \brief Auto-complete algorithms */
enum xcmd_complete
{
  /** \brief Disable auto-complete */
  xcmd_complete_none
};

/** \brief Configuration
 *
 * Contains configuration used for \c xcmd_init.
 */
struct xcmd_config
{
  /** \brief Select match algorithm
   *
   * This selects the match algorithm used by \c xcmd. The pointers \c match,
   * \c match_init and \c match_free in structure \c xcmd will be configured
   * appropriately. If this variable equals to \c xcmd_match_none, then \c
   * xcmd_init will fail.
   */
  xmatch_t    match;
  /** \brief Select auto-complete algorithm
   *
   * This selects the auto-complete algorithm used by \c xcmd. The pointers \c
   * complete, \c complete_init and \c complete_free in structure \c xcmd will
   * be configured appropriately.
   */
  xcomplete_t complete;
  /** \brief String comparison
   *
   * If set non-zero, \c xcmd_init will install \c strncasecmp. Otherwise it
   * installs \c strncmp.
   */
  int         case_insensitive;
  // deprecated
  char       *prompt;
};

/** \brief Initialize instance
 *
 * The function initializes a new instance of \c xcmd_t using the configuration
 * \c cfg. If no configuration is passed, i.e. \c cfg is \c NULL, the default
 * configuration is used.
 */
void xcmd_init(xcmd_t *ptr, const xcfg_t *cfg);
/** \brief Destroy instance
 *
 * Destroy an instance of \c xcmd and frees all memory allocated by the
 * instance \c ptr. All pointers provided by previous calls to the instance are
 * invalid after calling \c xcmd_destroy.
 */
void xcmd_destroy(xcmd_t *ptr);

/** \brief Fill list of items
 *
 * The function reads items line by line from stream \c f and stores them in \c
 * all_items of the instance \c ptr. As mltiple calls to \c xcmd_read_items
 * would override previous results, it will result in an error. On success this
 * function returns zero, otherwise a non-zero value is returned.
 */
int xcmd_read_items(xcmd_t *ptr, FILE *f);

/** \brief Complete list of items
 *
 * After complete reading or inserting items, this function will calculate the
 * item \c index of the instance \c ptr. If \c complete_init points to an
 * appropriate function, it is called to initialize \c complete_data. On
 * success this function returns zero, otherwise a non-zero value is returned.
 */
int xcmd_finish_items(xcmd_t *ptr);

/** \brief Update the \c index of \c matches
 *
 * The function will select all items into \c matches, where the function \c
 * match evaluates to non-zero for \c input. On success the function returns
 * zero, i.e. an optional call to \c match_init was successfull. Otherwise a
 * non-zero value is returned. On success the function also tries to notify its
 * observers about the changes made.
 */
int xcmd_update_matching(xcmd_t *ptr, const char *input);

/** \brief Update the selected item
 *
 * The function changes the currently selected item. If \c relative is set
 * non-zero, the selection pointer is moved relatively by \c offset elements.
 * Otherwise \c offset is used as absolute position. On success the function
 * returns zero and tries to notify its observers about the changes made.
 * Otherwise a non-zero value is returned.
 */
int xcmd_update_selected(xcmd_t *ptr, const long offset, const int relative);

/** \brief Complete input
 *
 * The function tries to complete the pointed to by \c inputptr using the
 * installed completion-function. When resizing the input text, \c n is
 * updated.
 */
int xcmd_auto_complete(const xcmd_t *ptr, char **inputptr, size_t *n);

/** \brief Notify observer
 *
 * The function itries to notify the observer of \c ptr by calling \c observer.
 * The observer will be notified, if the \c has_changed is set non-zero,
 * otherwise the notification is suppressed. On success the function returns
 * zero, i.e. \c observer points to an appropriate function. Especially the
 * value of \c has_changed does not influence the success of the function. If
 * \c observer points to \c NULL, the function returns a non-zero value.
 */
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
