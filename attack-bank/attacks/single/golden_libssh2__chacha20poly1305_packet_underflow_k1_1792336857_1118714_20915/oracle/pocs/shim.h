#ifndef POC_SHIM_H
#define POC_SHIM_H

/* Minimal stand-ins for the basic C types the public-domain chacha20 and
   poly1305 reference sources need, so the malicious server in residual.c can
   use byte-identical crypto to the library it talks to. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#endif
