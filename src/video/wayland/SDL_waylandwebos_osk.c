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

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#include "SDL_waylandwebos_osk.h"

#include "SDL_waylandevents_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_keysym_to_scancode_c.h"

#include "text-client-protocol.h"

// Contributed by Mariotaku <mariotaku.lee@gmail.com>

static void osk_commit_string(void *data, struct text_model *text_model, uint32_t serial, const char *text);
static void osk_preedit_string(void *data, struct text_model *text_model, uint32_t serial, const char *text, const char *commit);
static void osk_delete_surrounding_text(void *data, struct text_model *text_model, uint32_t serial, int32_t index, uint32_t length);
static void osk_cursor_position(void *data, struct text_model *text_model, uint32_t serial, int32_t index, int32_t anchor);
static void osk_preedit_styling(void *data, struct text_model *text_model, uint32_t serial, uint32_t index, uint32_t length, uint32_t style);
static void osk_preedit_cursor(void *data, struct text_model *text_model, uint32_t serial, int32_t index);
static void osk_modifiers_map(void *data, struct text_model *text_model, struct wl_array *map);
static void osk_keysym(void *data, struct text_model *text_model, uint32_t serial, uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers);
static void osk_enter(void *data, struct text_model *text_model, struct wl_surface *surface);
static void osk_leave(void *data, struct text_model *text_model);
static void osk_input_panel_state(void *data, struct text_model *text_model, uint32_t state);
static void osk_input_panel_rect(void *data, struct text_model *text_model, int32_t x, int32_t y, uint32_t width, uint32_t height);

static const struct text_model_listener osk_text_model_listener = {
    osk_commit_string,
    osk_preedit_string,
    osk_delete_surrounding_text,
    osk_cursor_position,
    osk_preedit_styling,
    osk_preedit_cursor,
    osk_modifiers_map,
    osk_keysym,
    osk_enter,
    osk_leave,
    osk_input_panel_state,
    osk_input_panel_rect
};

void WaylandWebOS_DisplayInitTextModelFactory(SDL_VideoData *display, Uint32 id)
{
    display->webos_text_input.factory = wl_registry_bind(display->registry, id, &text_model_factory_interface, 1);
}

static void WaylandWebOS_SendCursorRectangle(SDL_VideoData *display)
{
    SDL_WindowData *wind = display->webos_text_input.window;
    SDL_Window *window;

    if (!wind) {
        return;
    }

    window = wind->sdlwindow;
    if (SDL_RectEmpty(&window->text_input_rect) ||
        (SDL_RectsEqual(&window->text_input_rect, &display->webos_text_input.cursor_rect) &&
         window->text_input_cursor == display->webos_text_input.cursor_offset)) {
        return;
    }

    SDL_copyp(&display->webos_text_input.cursor_rect, &window->text_input_rect);
    display->webos_text_input.cursor_offset = window->text_input_cursor;

    // Clamp the x value so it doesn't run too far past the end of the text input area.
    text_model_set_cursor_rectangle(display->webos_text_input.model,
                                    SDL_min(window->text_input_rect.x + window->text_input_cursor,
                                            window->text_input_rect.x + window->text_input_rect.w),
                                    window->text_input_rect.y,
                                    1,
                                    window->text_input_rect.h);
}

static void WaylandWebOS_Deactivate(SDL_VideoData *display)
{
    if (!display->webos_text_input.model) {
        return;
    }

    /* Deactivating destroys the model on the compositor side, so the next
     * activation needs a new one. */
    if (display->webos_text_input.seat) {
        text_model_deactivate(display->webos_text_input.model, display->webos_text_input.seat->wl_seat);
    }
    text_model_destroy(display->webos_text_input.model);
    WAYLAND_wl_display_flush(display->display);

    display->webos_text_input.model = NULL;
    display->webos_text_input.seat = NULL;
    display->webos_text_input.window = NULL;
    display->webos_text_input.preedit_cursor = 0;
    display->webos_text_input.has_preedit = false;
    SDL_zero(display->webos_text_input.cursor_rect);
    display->webos_text_input.cursor_offset = 0;
}

static void WaylandWebOS_Activate(SDL_VideoData *display, SDL_WaylandSeat *seat, SDL_WindowData *wind)
{
    struct text_model *model = text_model_factory_create_text_model(display->webos_text_input.factory);

    if (!model) {
        return;
    }
    text_model_add_listener(model, &osk_text_model_listener, display);

    display->webos_text_input.model = model;
    display->webos_text_input.seat = seat;
    display->webos_text_input.window = wind;

    /* LG's compositor raises the input panel on activate; show_input_panel is
     * never sent by its own clients and does not raise it. */
    text_model_activate(display->webos_text_input.model, ++display->webos_text_input.serial,
                        seat->wl_seat, wind->surface);
    text_model_set_content_type(display->webos_text_input.model, wind->text_input_props.hint,
                                wind->text_input_props.purpose);
    text_model_set_enter_key_type(display->webos_text_input.model, wind->text_input_props.enter_key_type);
    WaylandWebOS_SendCursorRectangle(display);
    WAYLAND_wl_display_flush(display->display);
}

void WaylandWebOS_UpdateTextInput(SDL_VideoData *display)
{
    SDL_WaylandSeat *seat;
    SDL_WaylandSeat *focus_seat = NULL;
    SDL_WindowData *focus = NULL;

    if (!display->webos_text_input.factory) {
        return;
    }

    /* The panel is a single system-wide object driven by one text model, and
     * every seat takes keyboard focus on the same surface, so hold on to the
     * seat that got there first rather than lowering and raising the panel for
     * each of the others. */
    seat = display->webos_text_input.seat;
    focus = display->webos_text_input.window;
    if (seat && focus && focus->text_input_props.active && seat->keyboard.focus == focus) {
        return;
    }
    focus = NULL;

    wl_list_for_each (seat, &display->seat_list, link) {
        if (seat->keyboard.focus && seat->keyboard.focus->text_input_props.active) {
            focus_seat = seat;
            focus = seat->keyboard.focus;
            break;
        }
    }

    WaylandWebOS_Deactivate(display);
    if (focus_seat) {
        WaylandWebOS_Activate(display, focus_seat, focus);
    }
}

void WaylandWebOS_WindowDestroyed(SDL_VideoData *display, SDL_WindowData *wind)
{
    if (display->webos_text_input.window == wind) {
        WaylandWebOS_Deactivate(display);
    }
}

void WaylandWebOS_SeatDestroyed(SDL_VideoData *display, SDL_WaylandSeat *seat)
{
    if (display->webos_text_input.seat == seat) {
        // The seat is going away, so the deactivate request can't be sent.
        display->webos_text_input.seat = NULL;
        WaylandWebOS_Deactivate(display);
    }
}

void WaylandWebOS_QuitTextInput(SDL_VideoData *display)
{
    WaylandWebOS_Deactivate(display);

    if (display->webos_text_input.factory) {
        text_model_factory_destroy(display->webos_text_input.factory);
        display->webos_text_input.factory = NULL;
    }
}

bool WaylandWebOS_StartTextInput(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props)
{
    SDL_VideoData *display = _this->internal;
    SDL_WindowData *wind = window->internal;

    wind->text_input_props.hint = TEXT_MODEL_CONTENT_HINT_NONE;

    switch (SDL_GetTextInputType(props)) {
    default:
    case SDL_TEXTINPUT_TYPE_TEXT:
        wind->text_input_props.purpose = TEXT_MODEL_CONTENT_PURPOSE_NORMAL;
        break;
    case SDL_TEXTINPUT_TYPE_TEXT_NAME:
        wind->text_input_props.purpose = TEXT_MODEL_CONTENT_PURPOSE_NAME;
        break;
    case SDL_TEXTINPUT_TYPE_TEXT_EMAIL:
        wind->text_input_props.purpose = TEXT_MODEL_CONTENT_PURPOSE_EMAIL;
        break;
    case SDL_TEXTINPUT_TYPE_TEXT_USERNAME:
        wind->text_input_props.purpose = TEXT_MODEL_CONTENT_PURPOSE_NORMAL;
        wind->text_input_props.hint |= TEXT_MODEL_CONTENT_HINT_SENSITIVE_DATA;
        break;
    case SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN:
        wind->text_input_props.purpose = TEXT_MODEL_CONTENT_PURPOSE_PASSWORD;
        wind->text_input_props.hint |= (TEXT_MODEL_CONTENT_HINT_HIDDEN_TEXT | TEXT_MODEL_CONTENT_HINT_SENSITIVE_DATA);
        break;
    case SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_VISIBLE:
        wind->text_input_props.purpose = TEXT_MODEL_CONTENT_PURPOSE_PASSWORD;
        wind->text_input_props.hint |= TEXT_MODEL_CONTENT_HINT_SENSITIVE_DATA;
        break;
    case SDL_TEXTINPUT_TYPE_NUMBER:
        wind->text_input_props.purpose = TEXT_MODEL_CONTENT_PURPOSE_NUMBER;
        break;
    /* This protocol predates the 'pin' purpose; digits plus the password hints
     * is the closest the panel understands. */
    case SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_HIDDEN:
        wind->text_input_props.purpose = TEXT_MODEL_CONTENT_PURPOSE_DIGITS;
        wind->text_input_props.hint |= (TEXT_MODEL_CONTENT_HINT_HIDDEN_TEXT | TEXT_MODEL_CONTENT_HINT_SENSITIVE_DATA);
        break;
    case SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_VISIBLE:
        wind->text_input_props.purpose = TEXT_MODEL_CONTENT_PURPOSE_DIGITS;
        wind->text_input_props.hint |= TEXT_MODEL_CONTENT_HINT_SENSITIVE_DATA;
        break;
    }

    switch (SDL_GetTextInputCapitalization(props)) {
    default:
    case SDL_CAPITALIZE_NONE:
        break;
    case SDL_CAPITALIZE_LETTERS:
        wind->text_input_props.hint |= TEXT_MODEL_CONTENT_HINT_UPPERCASE;
        break;
    case SDL_CAPITALIZE_WORDS:
        wind->text_input_props.hint |= TEXT_MODEL_CONTENT_HINT_TITLECASE;
        break;
    case SDL_CAPITALIZE_SENTENCES:
        wind->text_input_props.hint |= TEXT_MODEL_CONTENT_HINT_AUTO_CAPITALIZATION;
        break;
    }

    if (SDL_GetTextInputAutocorrect(props)) {
        wind->text_input_props.hint |= (TEXT_MODEL_CONTENT_HINT_AUTO_COMPLETION | TEXT_MODEL_CONTENT_HINT_AUTO_CORRECTION);
    }
    if (SDL_GetTextInputMultiline(props)) {
        wind->text_input_props.hint |= TEXT_MODEL_CONTENT_HINT_MULTILINE;
        wind->text_input_props.enter_key_type = TEXT_MODEL_ENTER_KEY_TYPE_RETURN;
    } else {
        wind->text_input_props.enter_key_type = TEXT_MODEL_ENTER_KEY_TYPE_DONE;
    }

    wind->text_input_props.active = true;

    /* The model can only be activated once a seat has keyboard focus, which on
     * webOS arrives about a second after the first buffer is committed. Record
     * the request and let the focus event do the work. */
    WaylandWebOS_UpdateTextInput(display);

    return true;
}

bool WaylandWebOS_StopTextInput(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *display = _this->internal;

    window->internal->text_input_props.active = false;
    WaylandWebOS_UpdateTextInput(display);

    return true;
}

bool WaylandWebOS_UpdateTextInputArea(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *display = _this->internal;

    if (display->webos_text_input.model && display->webos_text_input.window == window->internal) {
        WaylandWebOS_SendCursorRectangle(display);
        WAYLAND_wl_display_flush(display->display);
    }

    return true;
}

bool WaylandWebOS_ClearComposition(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *display = _this->internal;

    if (display->webos_text_input.model && display->webos_text_input.window == window->internal) {
        text_model_reset(display->webos_text_input.model, ++display->webos_text_input.serial);
        WAYLAND_wl_display_flush(display->display);
    }

    return true;
}

void WaylandWebOS_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props)
{
    /* The panel is raised by activating the text model, so there is nothing to
     * show that starting text input does not already do. */
    WaylandWebOS_StartTextInput(_this, window, props);
}

void WaylandWebOS_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    WaylandWebOS_StopTextInput(_this, window);
}

static void osk_commit_string(void *data, struct text_model *text_model, uint32_t serial, const char *text)
{
    SDL_VideoData *display = (SDL_VideoData *)data;

    if (text_model != display->webos_text_input.model) {
        return;
    }

    if (display->webos_text_input.has_preedit) {
        SDL_SendEditingText("", 0, 0);
        display->webos_text_input.has_preedit = false;
    }
    SDL_SendKeyboardText(text);
}

static void osk_preedit_string(void *data, struct text_model *text_model, uint32_t serial, const char *text, const char *commit)
{
    SDL_VideoData *display = (SDL_VideoData *)data;
    int cursor;

    if (text_model != display->webos_text_input.model) {
        return;
    }

    cursor = display->webos_text_input.preedit_cursor;
    display->webos_text_input.preedit_cursor = 0;

    if (text && *text) {
        SDL_SendEditingText(text, cursor >= 0 ? (int)SDL_utf8strnlen(text, cursor) : -1, -1);
        display->webos_text_input.has_preedit = true;
    } else if (display->webos_text_input.has_preedit) {
        SDL_SendEditingText("", 0, 0);
        display->webos_text_input.has_preedit = false;
    }
}

static void osk_delete_surrounding_text(void *data, struct text_model *text_model, uint32_t serial, int32_t index, uint32_t length)
{
    // FIXME: Do we care about this event?
}

static void osk_cursor_position(void *data, struct text_model *text_model, uint32_t serial, int32_t index, int32_t anchor)
{
    // No-op
}

static void osk_preedit_styling(void *data, struct text_model *text_model, uint32_t serial, uint32_t index, uint32_t length, uint32_t style)
{
    // No-op
}

static void osk_preedit_cursor(void *data, struct text_model *text_model, uint32_t serial, int32_t index)
{
    SDL_VideoData *display = (SDL_VideoData *)data;

    if (text_model == display->webos_text_input.model) {
        display->webos_text_input.preedit_cursor = index;
    }
}

static void osk_modifiers_map(void *data, struct text_model *text_model, struct wl_array *map)
{
    // No-op
}

static void osk_keysym(void *data, struct text_model *text_model, uint32_t serial, uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers)
{
    SDL_VideoData *display = (SDL_VideoData *)data;
    SDL_Scancode scancode;

    if (text_model != display->webos_text_input.model || !display->webos_text_input.seat) {
        return;
    }

    scancode = SDL_GetScancodeFromKeySym(sym, 0);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }

    if (scancode == SDL_SCANCODE_RETURN && SDL_GetHintBoolean(SDL_HINT_RETURN_KEY_HIDES_IME, false)) {
        SDL_StopTextInput(display->webos_text_input.window->sdlwindow);
        return;
    }

    SDL_SendKeyboardKey(0, display->webos_text_input.seat->keyboard.sdl_id, 0, scancode, state != 0);
}

static void osk_enter(void *data, struct text_model *text_model, struct wl_surface *surface)
{
    // No-op
}

static void osk_leave(void *data, struct text_model *text_model)
{
    SDL_VideoData *display = (SDL_VideoData *)data;
    SDL_WindowData *wind = display->webos_text_input.window;

    if (text_model != display->webos_text_input.model || !wind) {
        return;
    }

    /* The panel is dismissed without an input_panel_state event when the
     * compositor drops it, which it does after a few idle seconds. */
    display->webos_text_input.seat = NULL;
    WaylandWebOS_Deactivate(display);
    SDL_SendScreenKeyboardHidden();
    SDL_StopTextInput(wind->sdlwindow);
}

static void osk_input_panel_state(void *data, struct text_model *text_model, uint32_t state)
{
    SDL_VideoData *display = (SDL_VideoData *)data;

    if (text_model != display->webos_text_input.model) {
        return;
    }

    if (state) {
        SDL_SendScreenKeyboardShown();
    } else {
        SDL_SendScreenKeyboardHidden();

        /* Dismissing the panel is the only way to end text input from the
         * remote, so treat it as such and let the deactivate go out. */
        if (display->webos_text_input.window) {
            SDL_StopTextInput(display->webos_text_input.window->sdlwindow);
        }
    }
}

static void osk_input_panel_rect(void *data, struct text_model *text_model, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    /* LG reports the size of the panel with no origin -- y is 0 even though the
     * panel is docked to the bottom of the screen -- and SDL has no way to hand
     * the panel geometry to an application, so there is nothing to do here. */
}

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS
