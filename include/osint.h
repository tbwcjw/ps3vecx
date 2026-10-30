#ifndef __OSINT_H
#define __OSINT_H

#define STBI_ONLY_PNG        /* smaller code, PNG is all we need */
#define STBI_NO_HDR          /* avoids needing libm */
#define STBI_NO_LINEAR
#include "stb_image.h"

void osint_render (void);

#endif
