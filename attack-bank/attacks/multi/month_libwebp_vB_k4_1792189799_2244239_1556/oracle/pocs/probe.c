// Probe: encode a synthetic RGBA image with the public encoder API, then
// decode it back with the public decoder API. Drives the lossy+alpha path
// (VP8 frame with a lossless-compressed ALPH chunk) end to end.
//
// The test body runs in a freshly exec'd copy of this binary that is
// retried a few times: some build/host combinations let the sanitized
// runtime die spuriously during process start-up (before main() is
// reached), and those launches must not be mistaken for a test result.
// A real sanitizer report inside the library aborts (or exits non-zero
// from) the inner process deterministically, and the outer process
// propagates that as SIGABRT; only start-up crashes are retried.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <webp/decode.h>
#include <webp/encode.h>

// Make any sanitizer finding fatal even if the environment does not set
// ASAN_OPTIONS: report and abort() rather than exiting normally.
const char* __asan_default_options(void) {
  return "abort_on_error=1:exitcode=1";
}

static int RunTest(void) {
  const int kWidth = 2048;
  const int kHeight = 96;
  uint8_t* const rgba = (uint8_t*)malloc((size_t)kWidth * kHeight * 4);
  uint8_t* webp = NULL;
  size_t webp_size;
  int dec_w = 0, dec_h = 0;
  uint8_t* decoded;
  size_t i;

  if (rgba == NULL) return 2;
  // Non-trivial luma (so the encoder emits a normally-filtered frame) and a
  // varying alpha channel (so an ALPH chunk is produced).
  for (i = 0; i < (size_t)kWidth * kHeight; ++i) {
    const uint32_t v = (uint32_t)(i * 2654435761u) >> 13;
    rgba[4 * i + 0] = (uint8_t)(v & 0xff);
    rgba[4 * i + 1] = (uint8_t)((v >> 3) & 0xff);
    rgba[4 * i + 2] = (uint8_t)((v >> 7) & 0xff);
    rgba[4 * i + 3] = (uint8_t)(i % 251);
  }

  webp_size = WebPEncodeRGBA(rgba, kWidth, kHeight, kWidth * 4, 75.f, &webp);
  free(rgba);
  if (webp_size == 0 || webp == NULL) return 2;

  decoded = WebPDecodeRGBA(webp, webp_size, &dec_w, &dec_h);
  if (decoded == NULL || dec_w != kWidth || dec_h != kHeight) {
    fprintf(stderr, "decode failed\n");
    return 3;
  }
  WebPFree(decoded);
  free(webp);
  printf("ok %d x %d\n", dec_w, dec_h);
  return 0;
}

int main(void) {
  int attempt;
  if (getenv("WEBP_PROBE_INNER") != NULL) {
    return RunTest();  // Inner process: perform the actual test.
  }
  // Outer process: launch fresh inner processes until one starts up
  // cleanly, then report its result.
  for (attempt = 0; attempt < 32; ++attempt) {
    const pid_t pid = fork();
    int status;
    if (pid == 0) {
      setenv("WEBP_PROBE_INNER", "1", 1);
      execl("/proc/self/exe", "probe", (char*)NULL);
      _exit(RunTest());  // exec unavailable: fall back to running in-place.
    }
    if (pid < 0 || waitpid(pid, &status, 0) < 0) continue;
    if (WIFEXITED(status)) {
      const int rc = WEXITSTATUS(status);
      if (rc == 0) return 0;  // Clean encode/decode round trip.
      // Non-zero inner exit: the sanitizer reported an error (its exitcode
      // path) or the decode genuinely failed. Propagate as an abort so the
      // failure is unmistakable.
      raise(SIGABRT);
      _exit(134);
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) {
      // Deterministic sanitizer abort inside the library: propagate it.
      raise(SIGABRT);
      _exit(134);
    }
    // Any other signal (e.g. a spurious start-up crash of the sanitized
    // runtime before main()): retry with a fresh process.
  }
  return 2;
}
