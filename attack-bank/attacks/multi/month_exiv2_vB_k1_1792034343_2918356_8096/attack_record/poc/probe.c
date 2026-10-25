// Exiv2 public-API probe: opens an in-memory QuickTime/MOV file through
// ImageFactory::open(const byte*, size_t) and reads its metadata.
//
// The file carries an audio track ('soun' handler) whose stsd atom holds one
// sound sample description, which QuickTimeVideo::audioDescDecoder() walks in
// four-byte chunks. On a tree where that decoder's tag buffer is too small
// for one of the chunk reads, the copy performed by MemIo::read() writes past
// the end of the heap allocation and AddressSanitizer aborts the process.
// On a tree where the buffer covers the read, the probe parses the file and
// exits normally.
#include <exiv2/exiv2.hpp>

#include <cstdio>
#include <cstring>

namespace {

// Minimal MOV: ftyp + moov{trak{hdlr(soun), mdia{minf{stbl{stsd(1 entry)}}}}}.
// All integers are big-endian, as the QuickTime atom structure requires.
const unsigned char movFile[] = {
    // ftyp atom (20 bytes): major brand "qt  " so isQTimeType() recognises it.
    0x00, 0x00, 0x00, 0x14, 0x66, 0x74, 0x79, 0x70,  // size 20, "ftyp"
    0x71, 0x74, 0x20, 0x20,                          // major brand "qt  "
    0x00, 0x00, 0x02, 0x00,                          // minor version
    0x71, 0x74, 0x20, 0x20,                          // compatible brand "qt  "

    // moov atom (176 bytes).
    0x00, 0x00, 0x00, 0xb0, 0x6d, 0x6f, 0x6f, 0x76,  // size 176, "moov"

    // trak atom (168 bytes).
    0x00, 0x00, 0x00, 0xa8, 0x74, 0x72, 0x61, 0x6b,  // size 168, "trak"

    // hdlr atom (32 bytes): handler type "soun" marks this as an audio track,
    // so sampleDesc() dispatches entries to audioDescDecoder().
    0x00, 0x00, 0x00, 0x20, 0x68, 0x64, 0x6c, 0x72,  // size 32, "hdlr"
    0x00, 0x00, 0x00, 0x00,                          // version and flags
    0x00, 0x00, 0x00, 0x00,                          // predefined
    0x73, 0x6f, 0x75, 0x6e,                          // handler type "soun"
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // reserved
    0x00, 0x00, 0x00, 0x00,                          // empty handler name

    // mdia atom (128 bytes).
    0x00, 0x00, 0x00, 0x80, 0x6d, 0x64, 0x69, 0x61,  // size 128, "mdia"

    // minf atom (120 bytes).
    0x00, 0x00, 0x00, 0x78, 0x6d, 0x69, 0x6e, 0x66,  // size 120, "minf"

    // stbl atom (112 bytes).
    0x00, 0x00, 0x00, 0x70, 0x73, 0x74, 0x62, 0x6c,  // size 112, "stbl"

    // stsd atom (104 bytes): one sound sample description entry.
    0x00, 0x00, 0x00, 0x68, 0x73, 0x74, 0x73, 0x64,  // size 104, "stsd"
    0x00, 0x00, 0x00, 0x00,                          // version and flags
    0x00, 0x00, 0x00, 0x01,                          // entry count: 1

    // Sound sample description entry (88 bytes): the size dword followed by
    // the fields audioDescDecoder() walks in four-byte chunks.
    0x00, 0x00, 0x00, 0x58,                          // entry size (88)
    0x00, 0x00, 0x00, 0x00,                          // chunk 0: format
    0x00, 0x00, 0x00, 0x00,                          // reserved
    0x00, 0x00, 0x00, 0x00,                          // reserved
    0x00, 0x00, 0x00, 0x00,                          // data reference index
    0x00, 0x00, 0x00, 0x00,                          // chunk 4: vendor id
    0x00, 0x01, 0x00, 0x10,                          // chunk 5: channels/size
    0x00, 0x00, 0x00, 0x00,                          // packet info
    0x00, 0x0b, 0xbb, 0x80,                          // chunk 7: sample rate
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,  // chunk 8 onwards
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // remaining fields
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,                          // trailing bytes
};

}  // namespace

int main(void) {
  // Invalid files generate a lot of warnings, so switch off logging.
  Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);

  try {
    Exiv2::Image::UniquePtr image =
        Exiv2::ImageFactory::open(movFile, sizeof(movFile));
    if (!image) {
      std::fprintf(stderr, "probe: image type not recognised\n");
      return 1;
    }
    image->readMetadata();
    std::printf("probe: readMetadata completed, %zu xmp tags\n", image->xmpData().count());
  } catch (const std::exception& e) {
    // A clean library error is not a memory-safety failure.
    std::printf("probe: library returned an error: %s\n", e.what());
  }
  return 0;
}
