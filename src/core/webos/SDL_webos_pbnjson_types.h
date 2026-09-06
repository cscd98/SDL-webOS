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

#ifndef SDL_webos_pbnjson_types_h_
#define SDL_webos_pbnjson_types_h_

#ifdef SDL_PLATFORM_WEBOS

// *INDENT-OFF* // clang-format off

typedef struct jvalue *jvalue_ref;
typedef struct jdomparser *jdomparser_ref;
typedef struct jschema *jschema_ref;

// A string and its length. This allows encodings other than UTF-8, but more
// importantly allows some no-copy optimizations.
typedef struct
{
    // Pointer to the string.
    const char *m_str;
    // Number of characters in m_str, not including any terminating null.
    size_t m_len;
} raw_buffer;

// A key/value pair in a JSON object.
typedef struct
{
    // String containing the name of the key.
    jvalue_ref key;
    // The JSON value.
    jvalue_ref value;
} jobject_key_value;

// Opaque reference to the parser context.
typedef struct __JSAXContext *JSAXContextRef;

// Invoked when there is an error while parsing the input. The callback should
// return non-zero if the error is fixed and parsing can be continued.
typedef int (*jerror_parsing)(void *ctxt, JSAXContextRef parseCtxt);

// Invoked when the JSON does not match a schema.
typedef int (*jerror_schema)(void *ctxt, JSAXContextRef parseCtxt);

// Invoked when a general error occurs.
typedef int (*jerror_misc)(void *ctxt, JSAXContextRef parseCtxt);

// Set of callbacks invoked if errors occur during JSON processing. All fields
// are optional; a callback that is NULL is not called.
typedef struct JErrorCallbacks
{
    // There was an error parsing the input.
    jerror_parsing m_parser;
    // There was an error validating the input against the schema.
    jerror_schema m_schema;
    // Some other error occurred while parsing.
    jerror_misc m_unknown;
    // User-specified data, passed to the callback function.
    void *m_ctxt;
} *JErrorCallbacksRef;

typedef struct JSchemaResolver *JSchemaResolverRef;

// JSON schema wrapper. Contains the schema, resolver and error callbacks, and
// is used while validating JSON against a schema.
typedef struct JSchemaInfo
{
    // The schema to use when parsing.
    jschema_ref m_schema;
    // The error handlers to use when parsing fails.
    JErrorCallbacksRef m_errHandler;
    // The pbnjson schema resolver to invoke when an external JSON reference is
    // encountered. The resolver provides the part of the schema referenced.
    JSchemaResolverRef m_resolver;
    // Padding for future binary compatibility.
    void *m_padding[2];
} JSchemaInfo, *JSchemaInfoRef;

// A bit-wise combination of JDOMOptimization values.
typedef unsigned int JDOMOptimizationFlags;

// A set of conversion result flags.
typedef unsigned int ConversionResultFlags;

// *INDENT-ON* // clang-format on

#endif // SDL_PLATFORM_WEBOS

#endif // SDL_webos_pbnjson_types_h_
