#include "inputbuffer.h"
#include <string.h>


void inputbuffer_init(inpbuf_t *in)
{
  assert(in);
  debug("Initialize input buffer.");

  in->text = g_string_new(NULL);
  in->cursor.ptr = 0;
  in->cursor.pos = 0;
}

void inputbuffer_destroy(inpbuf_t *in)
{
  if(!in) return;

  g_string_free(in->text, TRUE);
  in->text = NULL;
  in->cursor.pos = 0;
  in->cursor.ptr = 0;
}

gint inputbuffer_set(inpbuf_t *in, const gchar *str)
{
  assert(in);
  g_string_overwrite(in->text, 0, str);
  in->cursor.pos = g_utf8_strlen(in->text->str, -1);
  in->cursor.ptr = in->text->len;

  inputbuffer_debug(in);
  return in->cursor.pos;
}

gint inputbuffer_insert(inpbuf_t *in, const gchar *str)
{
  assert(in);
  debug("Insert text `%s' into input buffer at position %lu.", str, in->cursor.pos);

  /* Check, wheter x contains a valid UTF-8 string */
  const gint str_ok = g_utf8_validate(str, -1, NULL);
  warn_if(!str_ok, "Ignore inserting invalid UTF-8 string.");
  if(!str_ok) return -1;

  const gsize n_bytes = strlen(str);
  const gsize n_glyph = g_utf8_strlen(str, -1);

  /* Resize string to contain new input */
  const gsize old_text_len = in->text->len;
  in->text = g_string_set_size(in->text, in->text->len + n_bytes);

  /* Make room for x */
  char *const mv_dest = in->text->str + in->cursor.ptr + n_bytes;
  char *const mv_src = in->text->str + in->cursor.ptr;

  memmove(mv_dest, mv_src, old_text_len - in->cursor.ptr);
  memcpy(mv_src, str, n_bytes);

  /* Update cursor positions */
  in->cursor.ptr += n_bytes;
  in->cursor.pos += n_glyph;

  inputbuffer_debug(in);
  return n_glyph;
}

void inputbuffer_erase(inpbuf_t *in, glong n)
{
  assert(in);

  /* Don't do anything, if nothing shall be erased or if the input buffer is
   * already empty. */
  if(!n || !in->text->len) return;

  debug("Erase %li glyphs from input buffer.", n);

  /* Save initial state and move cursor before/after deleted part. */
  const inpbuf_t old_in = *in;
  inputbuffer_move(in, n);

  /* Calculate memory range to erase */
  const glong ptr_lo = min(old_in.cursor.ptr, in->cursor.ptr);
  const glong ptr_hi = max(old_in.cursor.ptr, in->cursor.ptr);

  /* Update cursor */
  in->text = g_string_erase(in->text, ptr_lo, ptr_hi - ptr_lo);
  in->cursor.pos = min(old_in.cursor.pos, in->cursor.pos);
  in->cursor.ptr = ptr_lo;

  inputbuffer_debug(in);
}

void inputbuffer_move(inpbuf_t *in, glong off)
{
  assert(in);
  debug("Move cursor by %li glyphs.", off);

  /* Input buffer must contain a valid UTF-8 string */
  const gchar *ptr = in->text->str + in->cursor.ptr;

  /* Move cursor to the left */
  for(; (0 > off) && (0 < in->cursor.ptr); off += 1) {
    ptr = g_utf8_prev_char(ptr);
    in->cursor.ptr  = ptr - in->text->str;
    in->cursor.pos -= 1;
  } /* for ... */

  /* Move cursor to the right */
  for(; (0 < off) && (in->cursor.ptr < in->text->len); off -= 1) {
    ptr = g_utf8_next_char(ptr);
    in->cursor.ptr  = ptr - in->text->str;
    in->cursor.pos += 1;
  } /* for ... */

  inputbuffer_debug(in);
}

void inputbuffer_set_cursor(inpbuf_t *in, glong pos)
{
  assert(in);
  assert(0 <= pos);
  inputbuffer_move(in, pos - in->cursor.pos);
}

void inputbuffer_move_front(inpbuf_t *in)
{
  inputbuffer_set_cursor(in, 0);
}

void inputbuffer_move_back(inpbuf_t *in)
{
  inputbuffer_set_cursor(in, G_MAXLONG);
}

const gchar *inputbuffer_get_text(const inpbuf_t *in)
{
  assert(in);
  return in->text->str;
}

#ifdef inputbuffer_TEST
int main()
{
  inpbuf_t in;
  inputbuffer_init(&in);
  inputbuffer_insert(&in, "€");
  inputbuffer_insert(&in, "ä");
  inputbuffer_insert(&in, " Ronny");
  inputbuffer_insert(&in, " Biemann");
  inputbuffer_move(&in, -8);
  inputbuffer_insert(&in, " ∂");
  inputbuffer_erase(&in, 3);
  inputbuffer_erase(&in, -1);
  inputbuffer_insert(&in, " Bi");
  inputbuffer_move_front(&in);
  inputbuffer_insert(&in, "prefix → ");
  inputbuffer_move_back(&in);
  inputbuffer_insert(&in, " ← suffix");

  inputbuffer_destroy(&in);
  return 0;
}
#endif /* inputbuffer_TEST */
