/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifndef SDL_webos_json_h_
#define SDL_webos_json_h_

#ifdef SDL_PLATFORM_WEBOS

#include "SDL_webos_libs.h"

// Parse a JSON document. On success the caller owns *parser and must release
// it with PBNJSON_jdomparser_release() once it is done with the returned value,
// which belongs to the parser. Returns NULL on failure.
extern jvalue_ref SDL_webOSJsonParse(const char *json, jdomparser_ref *parser, bool failOnNegative);

extern const char *SDL_webOSJsonStringify(jvalue_ref value);

extern jvalue_ref PBNJSON_jobject_get_nested(jvalue_ref obj, ...);

#endif // SDL_PLATFORM_WEBOS

#endif // SDL_webos_json_h_
