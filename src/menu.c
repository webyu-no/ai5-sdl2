/* Copyright (C) 2023 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#include <string.h>

#include "nulib.h"

#include "audio.h"
#include "input.h"
#include "memory.h"
#include "menu.h"
#include "texthook.h"
#include "vm.h"
#include "web.h"

static bool menu_initialized = false;
static int menu_index = 0;

// table mapping menu numbers (defmenu argument) to address indices
static unsigned menu_no_to_index_table[MEMORY_MENU_ENTRY_MAX] = {0};

static unsigned count_entries(void)
{
	for (unsigned i = 0; i < MEMORY_MENU_ENTRY_MAX; i++) {
		if (!memory.menu_entry_addresses[i])
			return i;
	}
	return MEMORY_MENU_ENTRY_MAX;
}

#ifdef __EMSCRIPTEN__
static void web_settings_menu(void)
{
	static const unsigned setting_indices[WEB_MENU_SETTING_COUNT] = {0, 1, 2};
	uint32_t addresses[MEMORY_MENU_ENTRY_MAX], numbers[MEMORY_MENU_ENTRY_MAX];
	unsigned mapping[MEMORY_MENU_ENTRY_MAX];
	uint16_t variables[26];
	uint16_t old_flags = mem_get_sysvar16(mes_sysvar16_flags);
	uint16_t old_entries = mem_get_sysvar16(mes_sysvar16_nr_menu_entries);
	uint16_t old_surface = mem_get_sysvar16(mes_sysvar16_dst_surface);
	uint8_t *var4 = xmalloc(game->var4_size);
	memcpy(addresses, memory.menu_entry_addresses, sizeof(addresses));
	memcpy(numbers, memory.menu_entry_numbers, sizeof(numbers));
	memcpy(mapping, menu_no_to_index_table, sizeof(mapping));
	memcpy(var4, mem_var4(), game->var4_size);
	for (unsigned i = 0; i < 26; i++) variables[i] = mem_get_var16(i);
	int old_index = menu_index;

	memset(memory.menu_entry_addresses, 0, sizeof(memory.menu_entry_addresses));
	memset(memory.menu_entry_numbers, 0, sizeof(memory.menu_entry_numbers));
	memset(menu_no_to_index_table, 0xff, sizeof(menu_no_to_index_table));
	for (unsigned i = 0; i < WEB_MENU_SUBMENU_COUNT; i++) {
		memory.menu_entry_addresses[i] = WEB_MENU_FIRST_ADDRESS + i;
		memory.menu_entry_numbers[i] = i;
		menu_no_to_index_table[i] = i;
	}
	menu_index = WEB_MENU_SUBMENU_COUNT;
	menu_initialized = true;
	mem_set_var16(23, 26);
	mem_set_var16(24, 120);
	mem_set_var16(11, 1);
	mem_set_var16(10, 12);
	mem_set_var16(18, 0);
	// MENU.MES does this before every nested Settings/SP Disc menu. It tells
	// procedure 38 to save the already-rendered pixels beneath this overlay so
	// procedure 93 can restore them when Cancel is pressed.
	mem_set_var4(1501, 0);
	mem_set_var4(1003, 15); // never run the attract intro inside this submenu
	vm_flag_off(FLAG_MENU_RETURN);
	vm_flag_off(FLAG_RETURN);
	mem_set_sysvar16(mes_sysvar16_nr_menu_entries, count_entries());
	texthook_commit();
	texthook_set_buffered(false);
	vm_call_procedure(38);
	texthook_set_buffered(true);
	while (true) {
		vm_call_procedure(39);
		if (input_down(INPUT_ACTIVATE)) {
			unsigned selected = mem_get_var16(18);
			input_wait_until_up(INPUT_ACTIVATE);
			if (selected < WEB_MENU_SETTING_COUNT) {
				audio_se_play("se62.wav", 0);
				web_setting(setting_indices[selected], 1);
				vm_call_procedure(95);
				vm_call_procedure(36);
			} else if (selected == WEB_MENU_SETTING_COUNT) {
				audio_se_play("se62.wav", 0);
				web_export_saves();
			} else if (selected == WEB_MENU_SETTING_COUNT + 1) {
				audio_se_play("se62.wav", 0);
				web_import_saves();
			}
		} else if (input_down(INPUT_CANCEL)) {
			audio_se_play("se63.wav", 0);
			vm_call_procedure(33);
			input_wait_until_up(INPUT_CANCEL);
			break;
		} else if (input_down(INPUT_UP)) {
			vm_call_procedure(34);
			input_wait_until_up(INPUT_UP);
		} else if (input_down(INPUT_DOWN)) {
			vm_call_procedure(35);
			input_wait_until_up(INPUT_DOWN);
		} else if (input_down(INPUT_LEFT)) {
			vm_call_procedure(36);
			input_wait_until_up(INPUT_LEFT);
		} else if (input_down(INPUT_RIGHT)) {
			vm_call_procedure(37);
			input_wait_until_up(INPUT_RIGHT);
		} else {
			vm_delay(16);
		}
	}
	// Just like MENU.MES' real Settings submenu, Cancel closes this overlay.
	// Procedure 38 saved the pixels beneath it and 93 restores those pixels.
	vm_call_procedure(93);

	memcpy(memory.menu_entry_addresses, addresses, sizeof(addresses));
	memcpy(memory.menu_entry_numbers, numbers, sizeof(numbers));
	memcpy(menu_no_to_index_table, mapping, sizeof(mapping));
	memcpy(mem_var4(), var4, game->var4_size);
	free(var4);
	for (unsigned i = 0; i < 26; i++) mem_set_var16(i, variables[i]);
	mem_set_sysvar16(mes_sysvar16_flags, old_flags);
	mem_set_sysvar16(mes_sysvar16_nr_menu_entries, old_entries);
	mem_set_sysvar16(mes_sysvar16_dst_surface, old_surface);
	menu_index = old_index;
	menu_initialized = true;
	// MENU.MES returns from Settings through L_410/L_359, which runs the main
	// menu initializer again. Besides drawing, this rebuilds procedure 95's
	// hover-row scratch image and reloads the cursor hidden by procedure 33.
	texthook_commit();
	texthook_set_buffered(false);
	vm_call_procedure(38);
	texthook_set_buffered(true);
}
#endif

void menu_define(unsigned menu_no, bool empty)
{
	if (!menu_initialized) {
		memset(memory.menu_entry_addresses, 0, sizeof(memory.menu_entry_addresses));
		memset(menu_no_to_index_table, 0xff, sizeof(menu_no_to_index_table));
		menu_index = 0;
		menu_initialized = true;
	}

	// XXX: if menu entry is empty, nothing is written to menu_entry_numbers
	//      or menu_entry_addresses
	if (empty)
		return;

	if (menu_index >= MEMORY_MENU_ENTRY_MAX)
		VM_ERROR("Too many menu entries");

	// compute virtual address of current IP (will be farcall'd from bytecode)
	uint8_t *ip = vm.ip.code + vm.ip.ptr;
	assert(ip >= memory_raw && ip < memory_raw + sizeof(struct memory));

	// XXX: menu_nos are written to menu_entry_numbers sequentially, regardless
	//      of the current contents
	memory.menu_entry_numbers[menu_index++] = menu_no;

	// XXX: addresses are pushed to the first free slot in menu_entry_addresses
	unsigned i = count_entries();
	if (i >= MEMORY_MENU_ENTRY_MAX)
		VM_ERROR("Too many menu entries");
	memory.menu_entry_addresses[i] = ip - memory_raw;
#ifdef __EMSCRIPTEN__
	if (game->id == GAME_YUNO && menu_no == 10 && mem_get_var16(11) == 0)
		memory.menu_entry_addresses[i] = WEB_MENU_ROOT_ADDRESS;
#endif

	// keep track of which menu_no corresponds to which address index
	// (for menu_get_no)
	menu_no_to_index_table[menu_no] = i;
}

void menu_exec(void)
{
	for (int i = 32; i < 40; i++) {
		if (!vm.procedures[i].code)
			VM_ERROR("Procedure %d is undefined in menuexec", i);
	}

	mem_set_sysvar16(mes_sysvar16_nr_menu_entries, count_entries());

	// initialize menu
	texthook_commit();
	texthook_set_buffered(false);
	vm_call_procedure(38);
	texthook_set_buffered(true);
	while (true) {
		if (vm_flag_is_on(FLAG_MENU_RETURN) || vm_flag_is_on(FLAG_RETURN))
			break;
		// update menu
		vm_call_procedure(39);
		if (input_down(INPUT_ACTIVATE)) {
#ifdef __EMSCRIPTEN__
			unsigned selected = mem_get_var16(18);
			uint32_t address = selected < MEMORY_MENU_ENTRY_MAX
				? memory.menu_entry_addresses[selected] : 0;
			if (address == WEB_MENU_ROOT_ADDRESS) {
				input_wait_until_up(INPUT_ACTIVATE);
				audio_se_play("se63.wav", 0);
				web_settings_menu();
				continue;
			}
			if (address >= WEB_MENU_FIRST_ADDRESS
					&& address < WEB_MENU_FIRST_ADDRESS + WEB_MENU_ITEM_COUNT) {
				input_wait_until_up(INPUT_ACTIVATE);
				audio_se_play("se62.wav", 0);
				web_setting(address - WEB_MENU_FIRST_ADDRESS, 1);
				// Procedure 95 is MENU.MES' own selected-row renderer.  Keeping
				// the entry in this menu avoids changing palette/menu contexts.
				vm_call_procedure(95);
				// Match MENU.MES' normal input paths by resetting its attract timer.
				vm_call_procedure(36);
				continue;
			}
#endif
			vm_call_procedure(32);
			if (game->id != GAME_DOUKYUUSEI2 || mem_get_var4(2035) == 0)
				input_wait_until_up(INPUT_ACTIVATE);
		} else if (input_down(INPUT_CANCEL)) {
			if (game->id == GAME_DOUKYUUSEI2)
				input_wait_until_up(INPUT_CANCEL);
			vm_call_procedure(33);
			input_wait_until_up(INPUT_CANCEL);
		} else if (input_down(INPUT_UP)) {
			vm_call_procedure(34);
			input_wait_until_up(INPUT_UP);
		} else if (input_down(INPUT_DOWN)) {
			vm_call_procedure(35);
			input_wait_until_up(INPUT_DOWN);
		} else if (input_down(INPUT_LEFT)) {
			vm_call_procedure(36);
			input_wait_until_up(INPUT_LEFT);
		} else if (input_down(INPUT_RIGHT)) {
			vm_call_procedure(37);
			input_wait_until_up(INPUT_RIGHT);
		} else {
			vm_delay(16);
		}
	}
	menu_initialized = false;
}

/* Common code pattern ("selected" = var16[18]):
 *     System.get_menu_no(selected); // puts menu_no of selected index into System.var16[22]
 *     selected = System.var16[22] + 1; // set selected index to menu_no + 1
 */
void menu_get_no(unsigned index)
{
	for (int no = 0; no < MEMORY_MENU_ENTRY_MAX; no++) {
		if (menu_no_to_index_table[no] == index) {
			mem_set_sysvar16(mes_sysvar16_menu_no, no);
			return;
		}
	}
	mem_set_sysvar16(mes_sysvar16_menu_no, 200);
}
