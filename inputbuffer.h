#ifndef INPUT_BUFFER_H
#define INPUT_BUFFER_H
#include "util.h"
#include <glib.h>

typedef struct inputbuffer inpbuf_t;

struct inputbuffer
{
  /** \brief Input string buffer */
  GString *text;

  /** \brief Cursor positions */
  struct
  {
    /** \brief Glyph-cursor position */
    gsize pos;
    /** \brief Byte-cursor position 
     *
     * Cursor position as byte offset relative to \c text. This must always
     * point to valid UTF-8 string. */
    gsize ptr;
  } cursor;
};

void inputbuffer_init(inpbuf_t *in);
void inputbuffer_destroy(inpbuf_t *in);
gint inputbuffer_set(inpbuf_t *in, const gchar *str);
gint inputbuffer_insert(inpbuf_t *in, const gchar *str);
void inputbuffer_erase(inpbuf_t *in, glong n);
void inputbuffer_move(inpbuf_t *in, glong off);
void inputbuffer_set_cursor(inpbuf_t *in, glong pos);
void inputbuffer_move_front(inpbuf_t *in);
void inputbuffer_move_back(inpbuf_t *in);
const gchar *inputbuffer_get_text(const inpbuf_t *in);

#define inputbuffer_debug(in) \
  { \
    assert(in); \
    debug("State: size=%lu, len=%lu, glyphs=%li, valid=%i", \
        in->text->allocated_len, \
        in->text->len, \
        g_utf8_strlen(in->text->str, -1), \
        g_utf8_validate(in->text->str, -1, NULL)); \
    debug("Text:  `%s'", in->text->str); \
    debug("Cursor: %*s", (int)in->cursor.pos, "^"); \
  }
#endif /* INPUT_BUFFER_H */
