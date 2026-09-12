/* Minimal debugger hooks for the YU-NO-only browser release. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "debug.h"
#include "vm.h"

bool debug_on_error = false;
bool debug_on_F12 = false;

void dbg_repl(void) {}
void dbg_invalidate(uint32_t addr, size_t size) {}
void dbg_load_file(const char *name, uint32_t addr, size_t size) {}

uint8_t dbg_handle_breakpoint(uint32_t addr)
{
	VM_ERROR("Debugger breakpoint reached in release build at 0x%08x", addr);
}
