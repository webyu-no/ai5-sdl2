#ifndef AI5_WEB_H
#define AI5_WEB_H
#ifdef __EMSCRIPTEN__
#include <SDL.h>
#include <emscripten.h>
#include <stdint.h>
#define WEB_MENU_ROOT_ADDRESS 0xfffffff0u
#define WEB_MENU_FIRST_ADDRESS 0xfffffff1u
#define WEB_MENU_FIRST_NO 190u
#define WEB_MENU_SETTING_COUNT 3u
#define WEB_MENU_ITEM_COUNT 5u
#define WEB_MENU_SUBMENU_COUNT 5u
void web_present(SDL_Renderer *renderer);
void web_delay(uint32_t ms);
void web_yield_if_due(void);
void web_script_progress(const char *name, uint32_t offset);
void web_player_wait(const char *name, uint32_t offset);
SDL_Cursor *web_color_cursor(SDL_Surface *s, int x, int y);
SDL_Cursor *web_mono_cursor(const uint8_t *data, const uint8_t *mask, int w, int h, int x, int y);
SDL_Cursor *web_system_cursor(SDL_SystemCursor id);
void web_set_cursor(SDL_Cursor *cursor);
void web_free_cursor(SDL_Cursor *cursor);
int web_show_cursor(int toggle);
void web_cursor_pos(unsigned *x, unsigned *y);
void web_cursor_warp(unsigned x, unsigned y);
void web_sync_saves(void);
void web_scene_enter(const char *name);
void web_reflector_open(unsigned slot);
void web_hotspot_table_loaded(const char *scene, const char *name, unsigned offset);
void web_hotspot_hover(const char *scene, unsigned offset, unsigned id);
int web_setting(int setting, int advance);
int web_menu_text(uint32_t address);
void web_restart(void);
void web_export_saves(void);
void web_import_saves(void);
#endif
#endif
