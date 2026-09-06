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

#include "SDL_webos_json.h"
#include "SDL_webos_luna.h"

bool SDL_webOSGetPanelResolution(int *width, int *height)
{
    char *response = NULL;
    bool result = false;

    if (SDL_webOSLunaServiceCallSync("luna://com.webos.service.panelcontroller/getPanelResolution", "{}", 1,
                                     &response) &&
        response != NULL) {
        jdomparser_ref parser = NULL;
        jvalue_ref parsed;
        if ((parsed = SDL_webOSJsonParse(response, &parser, true)) != NULL) {
            PBNJSON_jnumber_get_i32(PBNJSON_jobject_get(parsed, J_CSTR_TO_BUF("width")), width);
            PBNJSON_jnumber_get_i32(PBNJSON_jobject_get(parsed, J_CSTR_TO_BUF("height")), height);
            result = true;
            PBNJSON_jdomparser_release(&parser);
        }
        SDL_free(response);
        response = NULL;
    }
    if (!result && SDL_webOSLunaServiceCallSync("luna://com.webos.service.tv.systemproperty/getSystemInfo",
                                                "{\"keys\": [\"UHD\"]}", 1, &response) &&
        response != NULL) {
        jdomparser_ref parser = NULL;
        jvalue_ref parsed;
        if ((parsed = SDL_webOSJsonParse(response, &parser, true)) != NULL) {
            jvalue_ref uhd = PBNJSON_jobject_get(parsed, J_CSTR_TO_BUF("UHD"));
            int is_uhd = 0;
            if (PBNJSON_jis_string(uhd)) {
                raw_buffer uhd_buf = PBNJSON_jstring_get_fast(uhd);
                is_uhd = uhd_buf.m_str != NULL && SDL_strncmp(uhd_buf.m_str, "true", uhd_buf.m_len) == 0;
            } else {
                PBNJSON_jboolean_get(uhd, &is_uhd);
            }
            if (is_uhd) {
                *width = 3840;
                *height = 2160;
            } else {
                *width = 1920;
                *height = 1080;
            }
            result = true;
            PBNJSON_jdomparser_release(&parser);
        }
        SDL_free(response);
    }
    return result;
}

bool SDL_webOSGetRefreshRate(int *rate)
{
    const char *uri = "luna://com.webos.service.config/getConfigs";
    const char *payload = "{\"configNames\":[\"tv.hw.SoCOutputFrameRate\",\"tv.hw.supportFrc\"]}";
    char *response = NULL;
    bool result = false;

    if (SDL_webOSLunaServiceCallSync(uri, payload, 1, &response) && response != NULL) {
        jdomparser_ref parser = NULL;
        jvalue_ref parsed;
        if ((parsed = SDL_webOSJsonParse(response, &parser, true)) != NULL) {
            jvalue_ref configs = PBNJSON_jobject_get(parsed, J_CSTR_TO_BUF("configs"));
            if (PBNJSON_jis_object(configs)) {
                const char *keys[] = { "tv.hw.SoCOutputFrameRate", "tv.hw.supportFrc", NULL };
                for (int i = 0; keys[i] != NULL && !result; i++) {
                    jvalue_ref config = PBNJSON_jobject_get(configs, PBNJSON_j_cstr_to_buffer(keys[i]));
                    switch (i) {
                    case 0:
                    {
                        char value[16];
                        int value_num;
                        raw_buffer config_buf = PBNJSON_jstring_get_fast(config);
                        if (config_buf.m_str == NULL) {
                            continue;
                        }
                        SDL_zeroa(value);
                        SDL_memcpy(value, config_buf.m_str, SDL_min(config_buf.m_len, 15));
                        value_num = (int)SDL_strtol(value, NULL, 10);
                        if (value_num > 0) {
                            *rate = value_num;
                        }
                        result = true;
                        break;
                    }
                    case 1:
                    {
                        int support_frc = 0;
                        PBNJSON_jboolean_get(config, &support_frc);
                        if (support_frc) {
                            *rate = 120;
                        } else {
                            *rate = 60;
                        }
                        result = true;
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
            PBNJSON_jdomparser_release(&parser);
        }
        SDL_free(response);
    }
    return result;
}

#endif // SDL_PLATFORM_WEBOS
