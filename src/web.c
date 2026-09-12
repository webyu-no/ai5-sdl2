#include "web.h"
#ifdef __EMSCRIPTEN__
#include <stdlib.h>
#include <stdio.h>
#include "nulib.h"
#include "game.h"
#include "gfx_private.h"
#include "vm.h"

EM_JS(void, web_capture, (), { Module.webYuno.capture(); });
void web_scene_enter(const char *name) {
    EM_ASM({ Module.webYuno.sceneEnter(UTF8ToString($0)); }, name);
}
void web_script_progress(const char *name, uint32_t offset) {
    EM_ASM({ Module.webYuno.scriptProgress(UTF8ToString($0), $1); }, name, offset);
}
void web_player_wait(const char *name, uint32_t offset) {
    EM_ASM({ Module.webYuno.playerWait(UTF8ToString($0), $1); }, name, offset);
}
void web_reflector_open(unsigned slot) {
    EM_ASM({ Module.webYuno.reflectorOpen($0); }, slot);
}
void web_hotspot_table_loaded(const char *scene, const char *name, unsigned offset) {
    EM_ASM({ Module.webYuno.hotspotTableLoaded(UTF8ToString($0), UTF8ToString($1), $2); },
            scene, name, offset);
}
void web_hotspot_hover(const char *scene, unsigned offset, unsigned id) {
    EM_ASM({ Module.webYuno.hotspotHover(UTF8ToString($0), $1, $2); }, scene, offset, id);
}
void web_present(SDL_Renderer *renderer) {
    SDL_RenderPresent(renderer);
    web_capture();
}
static double last_yield;
EM_ASYNC_JS(void, web_cooperate, (), {
    const state = Module.webYuno;
    if (!state.cooperateChannel) {
        state.cooperateQueue = [];
        state.cooperateChannel = new MessageChannel();
        state.cooperateChannel.port1.onmessage = () => state.cooperateQueue.shift()?.();
    }
    await new Promise(resolve => {
        state.cooperateQueue.push(resolve);
        state.cooperateChannel.port2.postMessage(0);
    });
});
void web_delay(uint32_t ms) {
    if (!ms) {
        // emscripten_sleep(0) is a chained setTimeout(0), which browsers clamp
        // to about 4ms. At a 16ms engine cadence that made all web animation
        // roughly 20% slow. MessageChannel yields a task without timer clamping.
        web_cooperate();
        last_yield = emscripten_get_now();
        return;
    }
    // Repeated MessageChannel tasks can win task-source selection over a
    // rendering opportunity in Firefox. Let a timer yield the main thread and
    // request it one millisecond early to cover callback/Asyncify overhead.
    // At most one message task absorbs reduced-clock quantization; never form
    // a self-refilling task queue while a transition is trying to paint.
    double started = emscripten_get_now();
    double deadline = started + ms;
    emscripten_sleep(ms > 1 ? ms - 1 : 0);
    if (emscripten_get_now() < deadline)
        web_cooperate();
    last_yield = emscripten_get_now();
}
void web_yield_if_due(void) {
    double before = emscripten_get_now();
    if (before - last_yield < 16.0)
        return;

    web_cooperate();
    double after = emscripten_get_now();
    double elapsed = after - last_yield;
    // This is a cooperative VM yield, not an authored delay. Preserve its
    // 16ms cadence so browser task-resume latency cannot accumulate once per
    // yield. Skip missed cadence boundaries, and discard only a long suspended
    // tab interval.
    if (elapsed > 100.0)
        last_yield = after;
    else
        last_yield += (unsigned)(elapsed / 16.0) * 16.0;
}

EM_JS(int, cursor_create, (const uint8_t *pixels, int w, int h, int hx, int hy), {
    return Module.webYuno.createCursor(HEAPU8.slice(pixels, pixels + w*h*4), w, h, hx, hy);
});
SDL_Cursor *web_color_cursor(SDL_Surface *s, int x, int y) {
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0);
    if (!rgba) return NULL;
    int id = cursor_create(rgba->pixels, rgba->w, rgba->h, x, y);
    SDL_FreeSurface(rgba);
    return (SDL_Cursor *)(uintptr_t)id;
}
SDL_Cursor *web_mono_cursor(const uint8_t *data, const uint8_t *mask, int w, int h, int x, int y) {
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) return NULL;
    for (int row = 0; row < h; row++) for (int col = 0; col < w; col++) {
        int bit = 128 >> (col % 8), i = row * ((w+7)/8) + col/8;
        uint8_t *p = (uint8_t *)s->pixels + row*s->pitch + col*4;
        p[0] = p[1] = p[2] = data[i] & bit ? 0 : 255;
        p[3] = mask[i] & bit ? 255 : 0;
    }
    SDL_Cursor *c = web_color_cursor(s, x, y);
    SDL_FreeSurface(s);
    return c;
}
SDL_Cursor *web_system_cursor(SDL_SystemCursor id) {
    const uint8_t data[16] = {128,192,224,240,248,252,254,255,248,216,140,12,6,6,0,0};
    const uint8_t mask[16] = {128,192,224,240,248,252,254,255,248,216,140,12,6,6,0,0};
    return web_mono_cursor(data, mask, 8, 16, 0, 0);
}
EM_JS(void, web_set_cursor, (SDL_Cursor *cursor), { Module.webYuno.setCursor(cursor); });
EM_JS(void, web_free_cursor, (SDL_Cursor *cursor), { Module.webYuno.freeCursor(cursor); });
EM_JS(int, web_show_cursor, (int toggle), {
    const previous = Module.webYuno.cursor.visible ? 1 : 0;
    if (toggle >= 0) Module.webYuno.showCursor(Boolean(toggle));
    return previous;
});
EM_JS(void, web_cursor_pos, (unsigned *x, unsigned *y), {
    HEAPU32[x >> 2] = Module.webYuno.cursor.x;
    HEAPU32[y >> 2] = Module.webYuno.cursor.y;
});
EM_JS(void, web_cursor_warp, (unsigned x, unsigned y), { Module.webYuno.warp(x, y); });
EM_ASYNC_JS(void, web_sync_saves, (), { await Module.webYuno.syncSaves(); });
EM_JS(void, web_restart, (), { Module.webYuno.restart(); });
EM_ASYNC_JS(void, web_export_saves, (), {
    try {
        await Module.webYuno.exportSaves();
    } catch (error) {
        if (error.name !== 'AbortError') { console.error(error); alert(error.message); }
    }
});
EM_ASYNC_JS(void, web_import_saves, (), {
    try {
        await Module.webYuno.importSaves();
    } catch (error) {
        if (error.name !== 'AbortError') { console.error(error); alert(error.message); }
    }
});
EM_JS(int, web_setting, (int setting, int advance), { return Module.webYuno.setting(setting, advance); });

int web_menu_text(uint32_t address) {
	if (address < WEB_MENU_ROOT_ADDRESS ||
			address >= WEB_MENU_FIRST_ADDRESS + WEB_MENU_SUBMENU_COUNT) return 0;
	char label[96];
	if (address == WEB_MENU_ROOT_ADDRESS) {
		game->draw_text_zen("  Web YU-NO settings");
		return 1;
	}
	switch (address - WEB_MENU_FIRST_ADDRESS) {
	case 0:
		snprintf(label, sizeof(label), "Pointer: %s", web_setting(0, 0) ? "System" : "Internal");
		break;
	case 1:
		snprintf(label, sizeof(label), "Pointer speed: %d", (web_setting(1, 0) - 25) / 25 + 1);
		break;
	case 2: {
		// The original Settings menu allows sixteen Latin characters.
		const char *modes[] = {"Integer", "Nearest", "SharpBL", "Original"};
		snprintf(label, sizeof(label), "Scaling: %s", modes[web_setting(2, 0)]);
		break;
	}
	case 3: snprintf(label, sizeof(label), "Export saves"); break;
	case 4: snprintf(label, sizeof(label), "Import saves"); break;
	}
    game->draw_text_zen(label);
    return 1;
}
#endif
