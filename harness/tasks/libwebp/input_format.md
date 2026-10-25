Untrusted input = a crafted WebP image (bytes). Realistic entry: `WebPDecodeRGBA`/`WebPDecode` (or the
incremental `WebPIDecoder`) from <webp/decode.h> on crafted bytes; or produce the bytes via
`WebPEncode` (<webp/encode.h>) and decode them back in one probe. Fire = ASan abort inside libwebp
(dec/enc/dsp) while handling the crafted image.
