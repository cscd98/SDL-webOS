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

#ifdef SDL_PLATFORM_WEBOS

#include "SDL_webos_libs.h"
#include "SDL_webos_luna.h"

typedef struct SyncUserdata
{
    SDL_Mutex *mutex;
    SDL_Condition *cond;
    bool finished;
    char **output;
} SyncUserdata;

static int syncCallCallback(LSHandle *sh, LSMessage *reply, HContext *ctx)
{
    SyncUserdata *userdata = ctx->userdata;

    (void)sh;

    SDL_LockMutex(userdata->mutex);
    userdata->finished = true;
    if (userdata->output) {
        *userdata->output = SDL_strdup(HELPERS_HLunaServiceMessage(reply));
    }
    SDL_SignalCondition(userdata->cond);
    SDL_UnlockMutex(userdata->mutex);
    return 0;
}

static int justCallCallback(LSHandle *sh, LSMessage *reply, HContext *ctx)
{
    (void)sh;
    (void)reply;

    SDL_free(ctx);
    return 1;
}

bool SDL_webOSLunaServiceJustCall(const char *uri, const char *payload, int pub)
{
    HContext *response_context;

    if (!HELPERS_HLunaServiceCall) {
        return SDL_SetError("webOS libraries are not initialized");
    }
    response_context = SDL_calloc(1, sizeof(HContext));
    if (response_context == NULL) {
        return false;
    }
    response_context->multiple = 0;
    response_context->pub = pub;
    response_context->callback = justCallCallback;
    return HELPERS_HLunaServiceCall(uri, payload, response_context) == 0;
}

bool SDL_webOSLunaServiceCallSync(const char *uri, const char *payload, int pub, char **output)
{
    SyncUserdata userdata;
    HContext context;
    int callRet;

    if (!HELPERS_HLunaServiceCall) {
        return SDL_SetError("webOS libraries are not initialized");
    }
    if (SDL_GetHintBoolean("SDL_WEBOS_DISABLE_LUNA_CALLS", false)) {
        return SDL_SetError("Disabled by SDL_WEBOS_DISABLE_LUNA_CALLS");
    }

    SDL_zero(userdata);
    userdata.mutex = SDL_CreateMutex();
    userdata.cond = SDL_CreateCondition();
    userdata.output = output;

    SDL_zero(context);
    context.multiple = 0;
    context.pub = pub ? 1 : 0;
    context.callback = syncCallCallback;
    context.userdata = &userdata;

    if ((callRet = HELPERS_HLunaServiceCall(uri, payload, &context)) != 0) {
        SDL_DestroyMutex(userdata.mutex);
        SDL_DestroyCondition(userdata.cond);
        return SDL_SetError("Failed to call %s: (%d) %s", uri, callRet,
                            HELPERS_HGetError ? HELPERS_HGetError(callRet) : "unknown error");
    }
    SDL_LockMutex(userdata.mutex);
    while (!userdata.finished) {
        SDL_WaitCondition(userdata.cond, userdata.mutex);
    }
    SDL_UnlockMutex(userdata.mutex);

    SDL_DestroyMutex(userdata.mutex);
    SDL_DestroyCondition(userdata.cond);
    SDL_ClearError();
    return true;
}

#endif // SDL_PLATFORM_WEBOS
