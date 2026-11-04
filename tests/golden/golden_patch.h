// The runner's patches of its own code (issue #27; Windows x64): what difftest's tests do to difftest.exe
// with PatchJump, for the ported tests that redirect our functions to their fakes or the CRT's clock.
// The oracle's side keeps difftest's own PatchJump (tools/difftest/difftest.h).

#pragma once

#if !GOLDEN_ORACLE

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace golden {

// The import slot of Function (by name) in the runner's executable, or nullptr.
inline void **FindImport(const char *Function)
{
	uint8 *Base = (uint8 *)GetModuleHandleA(nullptr);
	const IMAGE_DOS_HEADER *Dos = (const IMAGE_DOS_HEADER *)Base;
	const IMAGE_NT_HEADERS *Nt = (const IMAGE_NT_HEADERS *)(Base + Dos->e_lfanew);
	const IMAGE_DATA_DIRECTORY &Directory = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	for (const IMAGE_IMPORT_DESCRIPTOR *Import = (const IMAGE_IMPORT_DESCRIPTOR *)(Base + Directory.VirtualAddress);
		Import->Name != 0; Import++) {
		const IMAGE_THUNK_DATA *Names = (const IMAGE_THUNK_DATA *)(Base + Import->OriginalFirstThunk);
		IMAGE_THUNK_DATA *Slots = (IMAGE_THUNK_DATA *)(Base + Import->FirstThunk);
		for (; Names->u1.AddressOfData != 0; Names++, Slots++) {
			if (IMAGE_SNAP_BY_ORDINAL(Names->u1.Ordinal))
				continue;
			const IMAGE_IMPORT_BY_NAME *Name = (const IMAGE_IMPORT_BY_NAME *)(Base + Names->u1.AddressOfData);
			if (strcmp((const char *)Name->Name, Function) == 0)
				return (void **)&Slots->u1.Function;
		}
	}
	return nullptr;
}

inline void SetImport(void **Slot, void *Value)
{
	DWORD Protection;
	VirtualProtect(Slot, sizeof(void *), PAGE_READWRITE, &Protection);
	*Slot = Value;
	VirtualProtect(Slot, sizeof(void *), Protection, &Protection);
}

// Overwrites the start of one of our functions with a jump to Target (rel32: both are in the runner's
// image, so within reach), as difftest's tests patch difftest.exe's. The runner is linked without
// identical-code folding (tests/golden/CMakeLists.txt), so no other function shares the patched code.
inline void RedirectOwn(void *Function, void *Target)
{
	intptr_t Distance = (intptr_t)Target - ((intptr_t)Function + 5);
	if (Distance != (int32_t)Distance) {
		printf("golden: cannot redirect %p to %p: out of a rel32 jump's reach\n", Function, Target);
		exit(1);
	}
	uint8 Jump[5];
	Jump[0] = 0xe9;
	int32_t Offset = (int32_t)Distance;
	memcpy(Jump + 1, &Offset, 4);
	DWORD Protection;
	VirtualProtect(Function, sizeof(Jump), PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(Function, Jump, sizeof(Jump));
	VirtualProtect(Function, sizeof(Jump), Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Function, sizeof(Jump));
}

} // namespace golden

#endif
