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

// Long enough that a busy bus is not cut off - a reply normally lands in a
// couple of milliseconds - and short enough that a service which never answers
// ends as an error instead of a hung process.
#define LUNA_CALL_TIMEOUT_MS 5000

typedef struct SyncCall
{
    HContext context;
    SDL_Mutex *mutex;
    SDL_Condition *cond;
    bool finished;
    bool abandoned;
    char *output;
} SyncCall;

static void destroySyncCall(SyncCall *call)
{
    SDL_DestroyCondition(call->cond);
    SDL_DestroyMutex(call->mutex);
    SDL_free(call->output);
    SDL_free(call);
}

static int syncCallCallback(LSHandle *sh, LSMessage *reply, HContext *ctx)
{
    SyncCall *call = ctx->userdata;

    (void)sh;

    SDL_LockMutex(call->mutex);
    if (call->abandoned) {
        // The caller gave up waiting, so this side owns the call now.
        SDL_UnlockMutex(call->mutex);
        destroySyncCall(call);
        return 0;
    }
    call->output = SDL_strdup(HELPERS_HLunaServiceMessage(reply));
    call->finished = true;
    SDL_SignalCondition(call->cond);
    SDL_UnlockMutex(call->mutex);
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
    if (HELPERS_HLunaServiceCall(uri, payload, response_context) != 0) {
        SDL_free(response_context);
        return false;
    }
    return true;
}

bool SDL_webOSLunaServiceCallSync(const char *uri, const char *payload, int pub, char **output)
{
    SyncCall *call;
    Uint64 deadline;
    int callRet;

    if (!HELPERS_HLunaServiceCall) {
        return SDL_SetError("webOS libraries are not initialized");
    }
    if (SDL_GetHintBoolean("SDL_WEBOS_DISABLE_LUNA_CALLS", false)) {
        return SDL_SetError("Disabled by SDL_WEBOS_DISABLE_LUNA_CALLS");
    }

    // The reply arrives on a thread of libhelpers' own, and may arrive after
    // this call has timed out, so the state it writes to cannot live on our
    // stack.
    call = SDL_calloc(1, sizeof(SyncCall));
    if (call == NULL) {
        return false;
    }
    call->mutex = SDL_CreateMutex();
    call->cond = SDL_CreateCondition();
    if (call->mutex == NULL || call->cond == NULL) {
        destroySyncCall(call);
        return false;
    }
    call->context.multiple = 0;
    call->context.pub = pub ? 1 : 0;
    call->context.callback = syncCallCallback;
    call->context.userdata = call;

    if ((callRet = HELPERS_HLunaServiceCall(uri, payload, &call->context)) != 0) {
        destroySyncCall(call);
        return SDL_SetError("Failed to call %s: (%d) %s", uri, callRet,
                            HELPERS_HGetError ? HELPERS_HGetError(callRet) : "unknown error");
    }

    deadline = SDL_GetTicks() + LUNA_CALL_TIMEOUT_MS;
    SDL_LockMutex(call->mutex);
    while (!call->finished) {
        const Uint64 now = SDL_GetTicks();
        if (now >= deadline) {
            call->abandoned = true;
            SDL_UnlockMutex(call->mutex);
            return SDL_SetError("Timed out waiting for a reply from %s", uri);
        }
        SDL_WaitConditionTimeout(call->cond, call->mutex, (Sint32)(deadline - now));
    }
    if (output != NULL) {
        *output = call->output;
        call->output = NULL;
    }
    SDL_UnlockMutex(call->mutex);

    destroySyncCall(call);
    SDL_ClearError();
    return true;
}

#endif // SDL_PLATFORM_WEBOS
