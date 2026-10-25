Untrusted input = a crafted OpenEXR file or metadata blob (bytes). Realistic entry: the Imf readers,
e.g. `Imf::RgbaInputFile`/`Imf::InputFile` on a crafted .exr, or a parser constructor fed crafted bytes
(e.g. `Imf::IDManifest(const char* data, const char* end)`) from the Imf* public headers. Fire = ASan
abort inside OpenEXR/Imath while parsing the crafted input. (C++ probe is fine: int main(), public Imf API.)
