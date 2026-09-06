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

#ifndef SDL_webos_pbnjson_inlines_h_
#define SDL_webos_pbnjson_inlines_h_

#ifdef SDL_PLATFORM_WEBOS

// Convenience function to construct a jobject_key_value structure.
//
// The behaviour is undefined if the key is NULL or is not a string.
static SDL_INLINE jobject_key_value PBNJSON_jkeyval(jvalue_ref key, jvalue_ref value)
{
    SDL_assert(key != NULL);
    SDL_assert(PBNJSON_jis_string(key));
    return (jobject_key_value){ (key), (value) };
}

// Convenience function that casts a C string to the raw_buffer used for JSON
// strings.
static SDL_INLINE raw_buffer PBNJSON_j_cstr_to_buffer(const char *cstring)
{
    return (raw_buffer){ cstring, SDL_strlen(cstring) };
}

// Convenience function that converts an arbitrary string of a known length to
// the raw_buffer used for JSON strings.
static SDL_INLINE raw_buffer PBNJSON_j_str_to_buffer(const char *string, size_t length)
{
    return (raw_buffer){ (string), (length) };
}

// Convenience function that creates a JSON string from a C string.
//
// The string must outlive the resulting JSON value, so this is safest with
// string literals or constants that live for the life of the program.
static SDL_INLINE jvalue_ref PBNJSON_j_cstr_to_jval(const char *cstring)
{
    return PBNJSON_jstring_create_nocopy(PBNJSON_j_cstr_to_buffer(cstring));
}

// Convenience function that determines whether an object contains a key.
static SDL_INLINE int PBNJSON_jobject_containskey(jvalue_ref obj, raw_buffer key)
{
    return PBNJSON_jobject_get_exists(obj, key, NULL);
}

// The last argument to PBNJSON_jobject_create_var().
#define J_END_OBJ_DECL ((jobject_key_value){ NULL, NULL })

// Create a raw_buffer from a C string literal or char array (anything the
// compiler knows the size of). Do not use this with variables unless you
// understand the lifetime requirements for the string.
#define J_CSTR_TO_BUF(string) PBNJSON_j_str_to_buffer(string, sizeof(string) - 1)

// Convert a C string literal to a jvalue_ref. Same requirements as
// J_CSTR_TO_BUF().
#define J_CSTR_TO_JVAL(string) PBNJSON_jstring_create_nocopy(PBNJSON_j_str_to_buffer(string, sizeof(string) - 1))

#endif // SDL_PLATFORM_WEBOS

#endif // SDL_webos_pbnjson_inlines_h_
