// Reproducer: parse a crafted header blob through the public
// Imf::Header::readFrom stream API.
//
// The blob holds a single custom attribute "foo" of type "bytes"
// whose value consists of a zero-length type hint followed by an
// eight byte payload (attribute size field = 12), followed by the
// end-of-header marker and some trailing slack bytes.

#include <ImfHeader.h>
#include <ImfIO.h>

#include <cstring>

namespace {

class BlobStream : public Imf::IStream
{
public:
    BlobStream (const unsigned char* data, unsigned long size)
        : IStream ("blob"), _data (data), _size (size), _pos (0)
    {}

    bool
    read (char c[/*n*/], int n) override
    {
        if (static_cast<unsigned long> (n) > _size - _pos)
            throw IEX_NAMESPACE::InputExc ("Unexpected end of file.");
        std::memcpy (c, _data + _pos, static_cast<size_t> (n));
        _pos += static_cast<unsigned long> (n);
        return _pos < _size;
    }

    uint64_t tellg () override { return _pos; }
    void     seekg (uint64_t pos) override { _pos = pos; }

private:
    const unsigned char* _data;
    unsigned long        _size;
    uint64_t             _pos;
};

} // namespace

int
main ()
{
    static const unsigned char blob[] = {
        'f', 'o', 'o', 0,             // attribute name
        'b', 'y', 't', 'e', 's', 0,   // attribute type
        12, 0, 0, 0,                  // attribute value size
        0, 0, 0, 0,                   // type hint length
        1, 2, 3, 4, 5, 6, 7, 8,       // payload bytes
        0,                            // end of header
        0, 0, 0, 0, 0, 0, 0, 0        // slack
    };

    BlobStream is (blob, sizeof (blob));
    Imf::Header hdr;
    int         version = 2;
    hdr.readFrom (is, version);

    return 0;
}
