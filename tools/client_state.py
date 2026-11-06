r"""Read a running client's game state and window layout from its memory (issue #19).

Tibia.exe only (Patella-Modern issues #92, #94): the tools read the x64 client (Patella.exe, PatellaTest.exe of
windows-x64) through its test mode, never its memory (tools/testmode_client.py: the control channel's probe;
notes/testmode.md), and Client refuses a 64-bit process. The reference recordings' record step and the live
comparisons' Tibia.exe runs read Tibia.exe with it (tools/reference_record.py, tools/session_client.py).
`--layout` on an x64 image still lists its PDB's entries (no process), and tools/patch_client.py finds the
port patch's Servers through Pdb.

Works on the original Tibia.exe (fixed addresses and field offsets, from the comments in src/). Until issue #94 it
read the 32-bit oracle's Patella.exe as well: its globals' addresses, field offsets, structure sizes and array
lengths from its PDB through dbghelp (SymGetTypeInfo; Patella-Modern issue #6), the addresses relative to the
module base (Patella.exe is relocatable); the oracle had the original layout (static_asserts in src/), so its PDB
gave exactly Tibia.exe's constants. That path (Client with a pdb_exe, Layout.from_pdb, the RTTI class names) is
kept for `--layout`; no running client uses it since the oracle left the fork. Every offset the tools use is named
in LAYOUT by its type and member names; Tibia.exe's values are the constants there.

state(): the client state the server's packets lead to, as a JSON-able dict:
  - connection state, player ID and statistics (Creatures.cpp), the skills;
  - the map centre and every map point's objects (Map.cpp Fields: type, data, data2 of the
    ObjectCount objects; creatures are type 99 with the creature ID);
  - the creature table (ID, name, position, direction, outfit, light, health, speed, skull,
    shield, visible) of the used slots;
  - the body slots, the open containers (object, name, capacity, objects) and both trade
    offers (Inventory.cpp);
  - the default channel's lines (mode and text without the time stamp: Talk.cpp).
  Left out on purpose: times and animation state (step and mark times, effects), which
  depend on the clock, not on the packets.

gui(): the window tree under MainWindow and ModalWindow (TComponent: Parent, Next, OffsetX/Y,
Width/Height; TWindow: FirstChild) as a list of (depth, class, x, y, width, height) with client-area
coordinates. A component is a window when its vtable slot of TComponent::GetFirstChild (+0x20 in the
original) is TWindow's `return FirstChild;`: in a Patella.exe the function the PDB names
TWindow::GetFirstChild (after any `jmp rel32` thunk: incremental linking), in every image the code
`mov eax, [ecx+24h]; ret` (8b 41 24 c3) of the original and the x86 Release build, `mov rax,
[rcx+XXh]; ret` (48 8b 41 XX c3) of an x64 Release build. A Debug build (/Od) compiles it otherwise
(this through the stack, /RTC and /JMC code), so only its address identifies it there (issue #88);
TElement's throws "not possible". Class names come from Patella's RTTI (MSVC's complete object locator before the vtable:
absolute pointers on x86, image-relative offsets on x64). Tibia.exe has none for these classes: there
ORIGINAL_VTABLES names the few the session needs, the vtable address stands in for the rest.

Raw numeric offsets (skin_rects/skin_int with an int, or address arithmetic in a caller) are only valid
on a layout that equals the original (Tibia.exe; the 32-bit oracle's build until issue #94): Layout.frozen.
"""

import ctypes
import ctypes.wintypes as wt
import os
import re
import struct
import sys

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi")
kernel32.OpenProcess.restype = wt.HANDLE
kernel32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
kernel32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
	ctypes.POINTER(ctypes.c_size_t)]
kernel32.CloseHandle.argtypes = [wt.HANDLE]
kernel32.IsWow64Process.argtypes = [wt.HANDLE, ctypes.POINTER(wt.BOOL)]
kernel32.GetCurrentProcess.restype = wt.HANDLE
kernel32.LocalFree.argtypes = [ctypes.c_void_p]
psapi.EnumProcessModulesEx.argtypes = [wt.HANDLE, ctypes.POINTER(ctypes.c_void_p), wt.DWORD,
	ctypes.POINTER(wt.DWORD), wt.DWORD]

PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
LIST_MODULES_32BIT = 1
LIST_MODULES_64BIT = 2

IMAGE_FILE_MACHINE_I386 = 0x014c
IMAGE_FILE_MACHINE_AMD64 = 0x8664

# Tibia.exe: the globals' addresses (comments in src/*.cpp) and their types as the PDB names them
# (kinds: see LAYOUT).
ORIGINAL_GLOBALS = [
	("ConnectionState", 0x71c588, "int"),
	("ModalWindow", 0x5d16ac, "ptr"),
	("MainWindow", 0x5d16b0, "ptr"),
	("SkinProperties", 0x5ccfc8, "TSkinProperties"),
	("PlayerState", 0x5c67d8, "int"),
	("PlayerSkillProgress", 0x5c67dc, "int[7]"),
	("PlayerSkillValue", 0x5c67f8, "int[7]"),
	("FollowID", 0x5c6818, "int"),
	("AttackID", 0x5c681c, "int"),
	("PlayerCapacity", 0x5c6820, "int"),
	("PlayerSoul", 0x5c6824, "int"),
	("PlayerMaxMana", 0x5c6828, "int"),
	("PlayerMana", 0x5c682c, "int"),
	("PlayerMagicLevelProgress", 0x5c6830, "int"),
	("PlayerLevelProgress", 0x5c6834, "int"),
	("PlayerMagicLevel", 0x5c6838, "int"),
	("PlayerLevel", 0x5c683c, "int"),
	("PlayerExperience", 0x5c6840, "int"),
	("PlayerMaxHealth", 0x5c6844, "int"),
	("PlayerHealth", 0x5c6848, "int"),
	("PlayerID", 0x5c684c, "int"),
	("Creatures", 0x5c68b0, "TCreature[150]"),
	("Body", 0x5ced60, "TObject[10]"),
	("Containers", 0x5cedd8, "TContainer[16]"),
	("OwnOffer", 0x5d0c98, "TOffer"),
	("CounterOffer", 0x5d1170, "TOffer"),
	("MapZ", 0x5d16e8, "int"),
	("MapY", 0x5d16ec, "int"),
	("MapX", 0x5d16f0, "int"),
	("Fields", 0x5d4c18, "array<TMapPoint>"),
	("DefaultTextBuffer", 0x71ef68, "ptr"),
	("Characters", 0x71c54c, "ptr"),
	("AccountCharacters", 0x71c550, "int"),
]
ORIGINAL = {name: address for name, address, kind in ORIGINAL_GLOBALS}

# Every field offset the tools read (this module, tools/session_client.py, tools/compare_files.py and
# tools/compare_rendering.py), as Type.Member[.Member]
# with the original's offset and the member's kind: "int" (any 4-byte integer or enum), "bool", "ptr"
# (a pointer: 4 or 8 bytes), "char[N]", a type name, or "Type[N]".
LAYOUT_FIELDS = [
	# Container.h: array<TMapPoint> Fields (Map.cpp)
	("array<TMapPoint>.min", 0x00, "int"),
	("array<TMapPoint>.d", 0x04, "int"),
	("array<TMapPoint>.entry", 0x08, "ptr"),
	# Map.h, Objects.h
	("TMapPoint.ObjectCount", 0x00, "int"),
	("TMapPoint.Objects", 0x04, "TObject[10]"),
	("TObject.Type", 0x00, "int"),
	("TObject.Data", 0x04, "int"),
	("TObject.Data2", 0x08, "int"),
	# Creatures.h
	("TCreature.CreatureID", 0x00, "int"),
	("TCreature.Name", 0x04, "char[32]"),
	("TCreature.RealX", 0x24, "int"),
	("TCreature.RealY", 0x28, "int"),
	("TCreature.RealZ", 0x2c, "int"),
	("TCreature.Direction", 0x50, "int"),
	("TCreature.OutfitType", 0x60, "int"),
	("TCreature.Colors", 0x64, "int[4]"),
	("TCreature.LightStrength", 0x74, "int"),
	("TCreature.LightColor", 0x78, "int"),
	("TCreature.Mark", 0x7c, "int"),
	("TCreature.Health", 0x84, "int"),
	("TCreature.Speed", 0x88, "int"),
	("TCreature.Visible", 0x8c, "bool"),
	("TCreature.PKFlag", 0x90, "int"),
	("TCreature.PARTYFlag", 0x94, "int"),
	# Inventory.h
	("TContainer.IsOpen", 0x00, "bool"),
	("TContainer.Object", 0x04, "TObject"),
	("TContainer.Name", 0x10, "char[30]"),
	("TContainer.Capacity", 0x30, "int"),
	("TContainer.HasParent", 0x34, "bool"),
	("TContainer.Count", 0x38, "int"),
	("TContainer.Objects", 0x3c, "TObject[36]"),
	("TOffer.Name", 0x00, "char[30]"),
	("TOffer.Count", 0x20, "int"),
	("TOffer.Objects", 0x24, "TObject[100]"),
	# Talk.h
	("TTalkBuffer.first", 0x10, "ptr"),
	("TTalkLine.Mode", 0x08, "int"),
	("TTalkLine.Next", 0x10, "ptr"),
	("TTalkLine.WithoutTime", 0x34, "ptr"),
	# Status.h
	("TCharacter.Name", 0x00, "char[30]"),
	# GUI.h, DialogElements.h, ContextMenus.h
	("TComponent.Parent", 0x0c, "ptr"),
	("TComponent.Next", 0x10, "ptr"),
	("TComponent.OffsetX", 0x14, "int"),
	("TComponent.OffsetY", 0x18, "int"),
	("TComponent.Width", 0x1c, "int"),
	("TComponent.Height", 0x20, "int"),
	("TWindow.FirstChild", 0x24, "ptr"),
	("TLabel.FullText", 0x28, "ptr"),
	("TContextMenuEntry.Text", 0x24, "char[50]"),
	("TTextButton.Text", 0x44, "char[50]"),
	# graphics.h, GUI.h, MapWindow.h
	("TRect.x", 0x00, "int"),
	("TRect.y", 0x04, "int"),
	("TRect.width", 0x08, "int"),
	("TRect.height", 0x0c, "int"),
	("TCreatureName.CreatureID", 0x00, "int"),
	("TCreatureName.x", 0x04, "int"),
	("TCreatureName.y", 0x08, "int"),
	# tools/session_client.py: the skin's slot areas, the map window's names, the trade and container
	# windows' buttons
	("TSkinProperties.Style288.Data60", 0x288 + 0x60, "TRect[10]"),
	("TSkinProperties.Data62C", 0x62c, "int"),
	("TSkinProperties.Data630.x", 0x630, "int"),
	("TSkinProperties.Data630.y", 0x634, "int"),
	("TSkinProperties.Data638.width", 0x638 + 8, "int"),
	("TSkinProperties.Data638.height", 0x638 + 12, "int"),
	("TSkinProperties.Data648", 0x648, "int"),
	("TSkinProperties.Data64C", 0x64c, "int"),
	("TSkinProperties.Style6FC.Data68", 0x6fc + 0x68, "TRect"),
	("TSkinProperties.Style6FC.Data78", 0x6fc + 0x78, "TRect"),
	("TSkinProperties.Style6FC.Data9C", 0x6fc + 0x9c, "int"),
	("TSkinProperties.Style6FC.DataA0", 0x6fc + 0xa0, "int"),
	("TSkinProperties.Style6FC.DataA4", 0x6fc + 0xa4, "int"),
	("TMapWindow.NumberOfNames", 0x2c, "int"),
	("TMapWindow.Names", 0x30, "TCreatureName[150]"),
	("TTradeButtons.CancelButton", 0x34, "ptr"),
	("TSubWindow.CloseButton", 0x54, "ptr"),
	# tools/compare_files.py, tools/compare_rendering.py (Patella-Modern issue #15): the option dialogs'
	# controls (Dialogs.h, DialogElements.h), the mark dialog's symbol buttons, the automap's zoom buttons
	# (ContentWindows.h), the side bar's button places and the frame rate text (GUI.h)
	("TCheckBox.Checked", 0x30, "bool"),
	("TGeneralOptionsDialog.TextualEffects", 0x2c, "ptr"),
	("TGeneralOptionsDialog.CreatureNames", 0x30, "ptr"),
	("TGeneralOptionsDialog.ShowHints", 0x34, "ptr"),
	("TGeneralOptionsDialog.ClassicControl", 0x38, "ptr"),
	("TGeneralOptionsDialog.AutoChaseOff", 0x3c, "ptr"),
	("TConsoleOptionsDialog.InfoMessages", 0x2c, "ptr"),
	("TConsoleOptionsDialog.EventMessages", 0x30, "ptr"),
	("TConsoleOptionsDialog.StatusMessages", 0x34, "ptr"),
	("TConsoleOptionsDialog.TimeStamps", 0x38, "ptr"),
	("TConsoleOptionsDialog.PrivateMessages", 0x3c, "ptr"),
	("TNetworkOptionsDialog.TestServerLogin", 0x2c, "ptr"),
	("TGraphicsOptionsDialog.Fullscreen", 0x2c, "ptr"),
	("TAdvancedGraphicsDialog.LightEffects", 0x30, "ptr"),
	("TAdvancedGraphicsDialog.NoStretching", 0x34, "ptr"),
	("TAdvancedGraphicsDialog.WindowsMousepointer", 0x38, "ptr"),
	("TAdvancedGraphicsDialog.Framerate", 0x3c, "ptr"),
	("TAdvancedGraphicsDialog.OpenGLButton", 0x4c, "ptr"),
	("THorizontalScrollBar.LeftButton", 0x30, "ptr"),
	("THotkeyDialog.AutoSendCheckBox", 0x2458, "ptr"),
	("THotkeyDialog.Selected", 0x245c, "int"),
	("TEditMarkDialog.TypeButtons", 0xb0, "ptr[14]"),
	("TAutomapWindow.ZoomInButton", 0x54, "ptr"),
	("TAutomapWindow.ZoomOutButton", 0x58, "ptr"),
	("TSkinProperties.Data60C", 0x60c, "TRect"),
	("TSkinProperties.Data61C", 0x61c, "TRect"),
	("TSkinProperties.Style1394.Data00", 0x1394 + 0x00, "int"),  # the advanced options' width (issue #31)
	("TSkinProperties.Style1394.Data08.x", 0x1394 + 0x08, "int"),
	("TSkinProperties.Style1394.Data08.y", 0x1394 + 0x0c, "int"),
	("TSkinProperties.Style1394.Data10.y", 0x1394 + 0x14, "int"),
	("TSkinProperties.Style1394.Data70.x", 0x1394 + 0x70, "int"),
	("TSkinProperties.Style1394.Data70.y", 0x1394 + 0x74, "int"),
]

# The sizes the tools use as strides.
LAYOUT_SIZES = [
	("TCreature", 0x9c),
	("TContainer", 0x1ec),
	("TMapPoint", 0xac),
	("TObject", 0x0c),
	("TCharacter", 0x54),
	("TRect", 0x10),
	("TCreatureName", 0x0c),
]

# Virtual functions by their offset in the vtable.
LAYOUT_VSLOTS = [
	("TComponent::GetFirstChild", 0x20),
]

# Functions whose address the tools compare with a vtable slot's (Patella.exe's PDB only): the window
# test of gui() (issue #88: a Debug build's code matches no fixed pattern).
LAYOUT_FUNCTIONS = [
	"TWindow::GetFirstChild",
]

# Tibia.exe has no RTTI for its windows: the vtables of the classes the session driver looks
# for (comments in src/*.h, or the vtable store in the constructor).
ORIGINAL_VTABLES = {
	0x56cf38: "TMapWindow",            # MapWindow.h
	0x55ed38: "TInventoryView",        # constructor 0x42a750
	0x55eed0: "TContainerWindowBody",  # constructor 0x438500
	0x55f138: "TTradeOffers",          # constructor 0x430dc0
	0x55f2a0: "TTradeButtons",         # constructor 0x432f00
	0x55f370: "TTradeWindowBody",      # constructor 0x43baf0
	0x55f730: "TContextMenu",          # ContextMenus.h
	0x55f6a8: "TContextMenuEntry",     # ContextMenus.h
	0x560bb8: "TLabel",                # DialogElements.h
	0x561c40: "TTextButton",           # DialogElements.h
	0x563778: "TDialog",               # Dialogs.h
	# issue #21 (tools/compare_files.py): the option and mark dialogs, their controls, the automap
	# and the console's tabs
	0x56c5c0: "TStartWindow",          # MainWindow.h
	0x563258: "TOptionsDialog",        # Dialogs.h
	0x563360: "TGeneralOptionsDialog", # Dialogs.h
	0x5624b8: "TGraphicsOptionsDialog",  # Dialogs.h
	0x562738: "TAdvancedGraphicsDialog",  # Dialogs.h
	0x563598: "TConsoleOptionsDialog",  # Dialogs.h
	0x563650: "TNetworkOptionsDialog",  # Dialogs.h
	0x563a90: "THotkeyDialog",         # Dialogs.h
	0x562698: "TEditMarkDialog",       # Dialogs.h
	0x563b30: "TErrorReportDialog",    # Dialogs.h
	0x561de8: "TCheckBox",             # DialogElements.h
	0x55e688: "TAutomapView",          # ContentWindows.h (notes/decisions.md, ContentWindows.cpp)
	0x55ea08: "TAutomapWindow",        # ContentWindows.h
	0x55e098: "TTab",                  # ConsoleWindow.h
}

MAP_POINTS = 18 * 14 * 8  # MAPSIZE_X * MAPSIZE_Y * MAPSIZE_Z (Map.h): Fields' number of entries


class ReadError(Exception):
	pass


def parse_kind(kind):
	"""(element kind, count) of "T[N]", else (kind, None)."""
	match = re.fullmatch(r"(.+)\[(\d+)\]", kind)
	return (match.group(1), int(match.group(2))) if match else (kind, None)


def pe_machine(exe_path):
	"""The machine field of a PE image's file header."""
	with open(exe_path, "rb") as f:
		head = f.read(0x400)
	if head[:2] != b"MZ":
		raise ReadError("%s: not a PE image" % exe_path)
	pe = struct.unpack_from("<I", head, 0x3c)[0]
	if head[pe:pe + 4] != b"PE\0\0":
		raise ReadError("%s: not a PE image" % exe_path)
	return struct.unpack_from("<H", head, pe + 4)[0]


class Layout:
	"""Where the tools find things in one binary: globals (address, or RVA for a PDB), field offsets,
	sizes, array lengths, vtable slots, the pointer size. Layout.original() is Tibia.exe's,
	Layout.from_pdb(exe) Patella.exe's from its PDB."""

	def __init__(self, source, pointer_size, globals_, relative, fields, sizes, vslots, kinds, functions=None):
		self.source = source
		self.pointer_size = pointer_size
		self.globals = globals_    # name -> address (relative: RVA)
		self.relative = relative
		self.fields = fields       # path -> offset
		self.sizes = sizes         # type -> size
		self.vslots = vslots       # Type::Function -> offset in the vtable
		self.kinds = kinds         # path or global -> kind
		self.functions = functions or {}  # Type::Function -> RVA (a PDB's; absent when inlined everywhere)
		self.frozen = (pointer_size == 4 and all(fields.get(path) == offset for path, offset, kind in LAYOUT_FIELDS)
			and all(sizes.get(name) == size for name, size in LAYOUT_SIZES)
			and all(vslots.get(name) == slot for name, slot in LAYOUT_VSLOTS))

	@classmethod
	def original(cls):
		return cls("Tibia.exe (constants)", 4, dict(ORIGINAL), False,
			{path: offset for path, offset, kind in LAYOUT_FIELDS}, dict(LAYOUT_SIZES), dict(LAYOUT_VSLOTS),
			dict([(path, kind) for path, offset, kind in LAYOUT_FIELDS]
				+ [(name, kind) for name, address, kind in ORIGINAL_GLOBALS]))

	@classmethod
	def from_pdb(cls, exe_path):
		layout, problems = cls.resolve_pdb(exe_path)
		if problems:
			raise ReadError("%s: the PDB does not match what the tools read:\n  %s" % (exe_path,
				"\n  ".join(problems)))
		return layout

	@classmethod
	def resolve_pdb(cls, exe_path):
		"""(Layout, problems): every entry of the tables resolved in the PDB of exe_path; a problem is an
		entry that is missing or whose kind differs (a field read as int that is no longer 4 bytes, ...)."""
		machine = pe_machine(exe_path)
		if machine not in (IMAGE_FILE_MACHINE_I386, IMAGE_FILE_MACHINE_AMD64):
			raise ReadError("%s: machine 0x%x, neither x86 nor x64" % (exe_path, machine))
		pointer_size = 4 if machine == IMAGE_FILE_MACHINE_I386 else 8
		problems = []
		globals_, fields, sizes, vslots, kinds, functions = {}, {}, {}, {}, {}, {}
		with Pdb(exe_path) as pdb:
			if pdb.pointer_size not in (None, pointer_size):
				problems.append("pointer size %d in the PDB, %d by the image's machine" % (pdb.pointer_size,
					pointer_size))
			for name, address, kind in ORIGINAL_GLOBALS:
				try:
					globals_[name], type_id = pdb.global_symbol(name)
					kinds[name] = pdb.kind(type_id)
					if not kind_matches(kind, kinds[name], pdb, type_id, pointer_size):
						problems.append("global %s: %s, expected %s" % (name, kinds[name], kind))
				except ReadError as e:
					problems.append(str(e))
			for path, offset, kind in LAYOUT_FIELDS:
				try:
					fields[path], type_id = pdb.member_path(path)
					kinds[path] = pdb.kind(type_id)
					if not kind_matches(kind, kinds[path], pdb, type_id, pointer_size):
						problems.append("field %s: %s, expected %s" % (path, kinds[path], kind))
				except ReadError as e:
					problems.append(str(e))
			for name, size in LAYOUT_SIZES:
				try:
					sizes[name] = pdb.length(pdb.type_named(name))
				except ReadError as e:
					problems.append(str(e))
			for name, slot in LAYOUT_VSLOTS:
				try:
					vslots[name] = pdb.vtable_slot(name)
				except ReadError as e:
					problems.append(str(e))
			for name in LAYOUT_FUNCTIONS:
				try:
					found = pdb.function_symbol(name)
					if found:
						functions[name] = found[0]
				except ReadError as e:
					problems.append(str(e))
		if problems:
			return None, problems
		return cls(exe_path, pointer_size, globals_, True, fields, sizes, vslots, kinds, functions), []

	def off(self, path):
		return self.fields[path]

	def size(self, name):
		return self.sizes[name]

	def count(self, path):
		"""The number of elements of an array field or global."""
		count = parse_kind(self.kinds[path])[1]
		if count is None:
			raise ReadError("%s is not an array (%s)" % (path, self.kinds[path]))
		return count


def kind_matches(expected, actual, pdb, type_id, pointer_size):
	if expected != actual:
		return False
	element, count = parse_kind(expected)
	if element == "ptr":  # a pointer or an array of pointers: pointer_size bytes each
		return pdb.length(type_id) == (1 if count is None else count) * pointer_size
	return True


class Pdb:
	"""Symbols and type information of an image's PDB through dbghelp (System32's: the PDB format of
	MSVC 14.5x reads with it, and it is on every Windows; a Windows SDK Debuggers copy is not needed)."""

	TI_GET_SYMTAG, TI_GET_SYMNAME, TI_GET_LENGTH, TI_GET_TYPEID, TI_GET_BASETYPE = 0, 1, 2, 4, 5
	TI_FINDCHILDREN, TI_GET_OFFSET, TI_GET_COUNT, TI_GET_CHILDRENCOUNT = 7, 10, 12, 13
	TI_GET_VIRTUALBASEOFFSET = 29
	SYMTAG_FUNCTION, SYMTAG_DATA, SYMTAG_UDT, SYMTAG_ENUM = 5, 7, 11, 12
	SYMTAG_POINTER, SYMTAG_ARRAY, SYMTAG_BASETYPE, SYMTAG_TYPEDEF, SYMTAG_BASECLASS = 14, 15, 16, 17, 18
	BT_CHAR, BT_INT, BT_UINT, BT_BOOL, BT_LONG, BT_ULONG = 2, 6, 7, 10, 13, 14

	class SYMBOL_INFO(ctypes.Structure):
		_fields_ = [("SizeOfStruct", wt.ULONG), ("TypeIndex", wt.ULONG), ("Reserved", ctypes.c_uint64 * 2),
			("Index", wt.ULONG), ("Size", wt.ULONG), ("ModBase", ctypes.c_uint64), ("Flags", wt.ULONG),
			("Value", ctypes.c_uint64), ("Address", ctypes.c_uint64), ("Register", wt.ULONG),
			("Scope", wt.ULONG), ("Tag", wt.ULONG), ("NameLen", wt.ULONG), ("MaxNameLen", wt.ULONG),
			("Name", ctypes.c_char * 256)]

	ENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, ctypes.POINTER(SYMBOL_INFO), wt.ULONG, ctypes.c_void_p)

	def __init__(self, exe_path):
		self.exe_path = os.path.abspath(exe_path)
		self.dbghelp = d = ctypes.WinDLL(os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32",
			"dbghelp.dll"))
		d.SymInitialize.argtypes = [wt.HANDLE, ctypes.c_char_p, wt.BOOL]
		d.SymLoadModuleEx.restype = ctypes.c_uint64
		d.SymLoadModuleEx.argtypes = [wt.HANDLE, wt.HANDLE, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint64,
			wt.DWORD, ctypes.c_void_p, wt.DWORD]
		d.SymEnumSymbols.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.c_char_p, self.ENUMPROC, ctypes.c_void_p]
		d.SymGetTypeFromName.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.c_char_p,
			ctypes.POINTER(self.SYMBOL_INFO)]
		d.SymGetTypeInfo.argtypes = [wt.HANDLE, ctypes.c_uint64, wt.ULONG, ctypes.c_int, ctypes.c_void_p]
		d.SymCleanup.argtypes = [wt.HANDLE]
		self.handle = wt.HANDLE(0x7919)
		d.SymSetOptions(0x2 | 0x4)  # SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS
		# search path: the exe's folder (the PDB next to it), then dbghelp's own (the path in the debug
		# directory); a PDB whose signature does not match the image is not loaded
		if not d.SymInitialize(self.handle, os.path.dirname(self.exe_path).encode(), False):
			raise ReadError("SymInitialize failed")
		self.base = d.SymLoadModuleEx(self.handle, None, self.exe_path.encode(), None, 0x400000, 0, None, 0)
		if not self.base:
			d.SymCleanup(self.handle)
			raise ReadError("SymLoadModuleEx failed for %s" % self.exe_path)
		self.pointer_size = None
		self.members_cache = {}

	def close(self):
		if self.handle is not None:
			self.dbghelp.SymCleanup(self.handle)
			self.handle = None

	def __enter__(self):
		return self

	def __exit__(self, *exception):
		self.close()

	def info(self, type_id, what, ctype=wt.DWORD):
		value = ctype()
		if not self.dbghelp.SymGetTypeInfo(self.handle, self.base, type_id, what, ctypes.byref(value)):
			return None
		return value.value

	def name(self, type_id):
		pointer = ctypes.c_void_p()
		if not self.dbghelp.SymGetTypeInfo(self.handle, self.base, type_id, self.TI_GET_SYMNAME,
				ctypes.byref(pointer)) or not pointer.value:
			return None
		try:
			return ctypes.wstring_at(pointer.value)
		finally:
			kernel32.LocalFree(pointer)

	def children(self, type_id):
		count = self.info(type_id, self.TI_GET_CHILDRENCOUNT) or 0
		if not count:
			return []

		class FINDCHILDREN(ctypes.Structure):
			_fields_ = [("Count", wt.ULONG), ("Start", wt.ULONG), ("ChildId", wt.ULONG * count)]

		params = FINDCHILDREN(count, 0)
		if not self.dbghelp.SymGetTypeInfo(self.handle, self.base, type_id, self.TI_FINDCHILDREN,
				ctypes.byref(params)):
			raise ReadError("%s: TI_FINDCHILDREN failed for type %d" % (self.exe_path, type_id))
		return list(params.ChildId)

	def tag(self, type_id):
		return self.info(type_id, self.TI_GET_SYMTAG)

	def length(self, type_id):
		value = self.info(type_id, self.TI_GET_LENGTH, ctypes.c_uint64)
		if value is None and self.tag(type_id) == self.SYMTAG_TYPEDEF:
			return self.length(self.info(type_id, self.TI_GET_TYPEID))
		if value is None:
			raise ReadError("%s: no length for type %d" % (self.exe_path, type_id))
		return value

	def kind(self, type_id):
		"""A type in LAYOUT's terms: int, bool, char, ptr, a structure's name, "T[N]"."""
		tag = self.tag(type_id)
		if tag == self.SYMTAG_TYPEDEF:
			return self.kind(self.info(type_id, self.TI_GET_TYPEID))
		if tag == self.SYMTAG_POINTER:
			if self.pointer_size is None:
				self.pointer_size = self.length(type_id)
			return "ptr"
		if tag == self.SYMTAG_ARRAY:
			return "%s[%d]" % (self.kind(self.info(type_id, self.TI_GET_TYPEID)), self.info(type_id, self.TI_GET_COUNT))
		if tag == self.SYMTAG_UDT:
			name = self.name(type_id)
			# TFixedString<N> (src/types/FixedString.h, issue #57) is exactly char[N]: its only member is the array
			fixed = re.fullmatch(r"TFixedString<(\d+)>", name or "")
			if fixed and self.length(type_id) == int(fixed.group(1)):
				return "char[%s]" % fixed.group(1)
			# std::array<T, N> (issue #60) is exactly T[N]: its only data member is the array _Elems (MSVC's
			# library); read as that array
			if (name or "").startswith("std::array<"):
				members = self.members(type_id)
				if len(members) == 1 and "_Elems" in members:
					offset, element = members["_Elems"]
					if offset == 0 and self.length(element) == self.length(type_id):
						return self.kind(element)
			# std::unique_ptr<T> with the default deleter (issue #58) is exactly a T *: an owning member is read as
			# the pointer it holds (kind_matches checks its length against the image's pointer size)
			if re.fullmatch(r"std::unique_ptr<.*,std::default_delete<.*> >", name or ""):
				return "ptr"
			return name
		if tag == self.SYMTAG_ENUM:
			return "int" if self.length(type_id) == 4 else "enum%d" % self.length(type_id)
		if tag == self.SYMTAG_BASETYPE:
			base, length = self.info(type_id, self.TI_GET_BASETYPE), self.length(type_id)
			if base in (self.BT_INT, self.BT_UINT, self.BT_LONG, self.BT_ULONG) and length == 4:
				return "int"
			if base == self.BT_BOOL and length == 1:
				return "bool"
			if base == self.BT_CHAR and length == 1:
				return "char"
			return "base%d:%d" % (base, length)
		return "tag%s" % tag

	def type_named(self, name):
		info = self.SYMBOL_INFO()
		info.SizeOfStruct = 88  # sizeof(SYMBOL_INFO) in C (Name[1] at +84, padded to 8)
		info.MaxNameLen = 256
		if not self.dbghelp.SymGetTypeFromName(self.handle, self.base, name.encode(), ctypes.byref(info)):
			raise ReadError("%s: no type %s in the PDB" % (self.exe_path, name))
		return info.TypeIndex

	def members(self, type_id):
		"""name -> (offset, type) of the data members of a structure, those of its base classes included."""
		if type_id in self.members_cache:
			return self.members_cache[type_id]
		result = {}
		for child in self.children(type_id):
			tag = self.tag(child)
			if tag == self.SYMTAG_DATA:
				offset = self.info(child, self.TI_GET_OFFSET)
				if offset is not None:  # static members have none
					result.setdefault(self.name(child), (offset, self.info(child, self.TI_GET_TYPEID)))
			elif tag == self.SYMTAG_BASECLASS:
				base_offset = self.info(child, self.TI_GET_OFFSET) or 0
				for name, (offset, member_type) in self.members(self.info(child, self.TI_GET_TYPEID)).items():
					result.setdefault(name, (base_offset + offset, member_type))
		self.members_cache[type_id] = result
		return result

	def member_path(self, path):
		"""(offset, type) of Type.Member[.Member...]."""
		type_name, *names = path.split(".")
		type_id = self.type_named(type_name)
		offset = 0
		for name in names:
			while self.tag(type_id) == self.SYMTAG_TYPEDEF:
				type_id = self.info(type_id, self.TI_GET_TYPEID)
			members = self.members(type_id)
			if name not in members:
				raise ReadError("%s: no member %s in %s" % (self.exe_path, name, path))
			member_offset, type_id = members[name]
			offset += member_offset
		return offset, type_id

	def vtable_slot(self, name):
		"""The offset in the vtable of the virtual function Type::Function."""
		type_name, function = name.split("::")
		for child in self.children(self.type_named(type_name)):
			if self.tag(child) == self.SYMTAG_FUNCTION and (self.name(child) or "").split("::")[-1] == function:
				slot = self.info(child, self.TI_GET_VIRTUALBASEOFFSET)
				if slot is not None:
					return slot
		raise ReadError("%s: no virtual function %s" % (self.exe_path, name))

	def global_symbol(self, name):
		"""(RVA, type) of the global `name`; exactly one symbol may have that name."""
		hits = []

		def callback(info, size, context):
			if info.contents.Name.decode() == name:
				hits.append((info.contents.Address, info.contents.TypeIndex))
			return True

		self.dbghelp.SymEnumSymbols(self.handle, self.base, name.encode(), self.ENUMPROC(callback), None)
		addresses = set(address for address, type_id in hits)
		if len(addresses) != 1:
			raise ReadError("%s: %d symbols named %s" % (self.exe_path, len(addresses), name))
		return hits[0][0] - self.base, hits[0][1]

	def function_symbol(self, name):
		"""(RVA, size in bytes) of the function `name`, or None when the PDB has none (inlined everywhere);
		more than one function of that name is an error."""
		hits = []

		def callback(info, size, context):
			if info.contents.Name.decode() == name and info.contents.Tag == self.SYMTAG_FUNCTION:
				hits.append((info.contents.Address - self.base, info.contents.Size))
			return True

		self.dbghelp.SymEnumSymbols(self.handle, self.base, name.encode(), self.ENUMPROC(callback), None)
		hits = sorted(set(hits))
		if len(hits) > 1:
			raise ReadError("%s: %d functions named %s" % (self.exe_path, len(hits), name))
		return hits[0] if hits else None


def pdb_symbols(exe_path, names):
	"""RVA of each name in the PDB of exe_path (dbghelp finds it next to the exe or through the debug
	directory)."""
	with Pdb(exe_path) as pdb:
		return {name: pdb.global_symbol(name)[0] for name in names}


def process_is_64bit(handle):
	"""True for a 64-bit process (on 64-bit Windows a 32-bit one runs under WOW64)."""
	wow64 = wt.BOOL()
	if not kernel32.IsWow64Process(handle, ctypes.byref(wow64)):
		raise ReadError("IsWow64Process failed (error %d)" % ctypes.get_last_error())
	if wow64.value:
		return False
	return struct.calcsize("P") == 8  # not WOW64: as wide as the OS, which a 64-bit Python implies


# Issue #92: the x64 client is read through its test mode only.
X64_REFUSED = ("%s is a 64-bit client: client_state.py reads Tibia.exe "
	"only; the x64 client is read through its test mode (PatellaTest.exe --test-control, "
	"tools/testmode_client.py; Patella-Modern issue #92)")

# The kinds of the members the session drivers follow (member(), member_index()).
FIELD_KINDS = {path: kind for path, offset, kind in LAYOUT_FIELDS}


class Client:
	"""A running client's memory. pdb_exe: None for Tibia.exe, else the exe whose PDB to use (Patella.exe,
	x86 or x64; the process must have the same bitness)."""

	def __init__(self, pid, pdb_exe=None):
		self.handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
		if not self.handle:
			raise ReadError("OpenProcess %d failed (error %d)" % (pid, ctypes.get_last_error()))
		if process_is_64bit(self.handle):
			self.close()
			raise ReadError(X64_REFUSED % ("PID %d" % pid))
		try:
			self.layout = Layout.original() if pdb_exe is None else Layout.from_pdb(pdb_exe)
			self.attach()
		except BaseException:
			self.close()
			raise
		self.rtti = pdb_exe is not None

	def attach(self):
		"""Bitness, module base and the globals' addresses."""
		L = self.layout
		self.is64 = process_is_64bit(self.handle)
		if (8 if self.is64 else 4) != L.pointer_size:
			raise ReadError("the process is %d-bit, the layout (%s) %d-bit" % (64 if self.is64 else 32, L.source,
				8 * L.pointer_size))
		if self.is64 and struct.calcsize("P") != 8:
			raise ReadError("a 64-bit process needs a 64-bit Python")
		self.pointer_size = L.pointer_size
		self.ptr_format = "<I" if L.pointer_size == 4 else "<Q"
		self.base = self.module_base()
		self.addr = {name: (self.base + value if L.relative else value) for name, value in L.globals.items()}
		F = L.fields
		self.window_getfirstchild = window_getfirstchild_code(L.pointer_size, F["TWindow.FirstChild"])
		rva = L.functions.get("TWindow::GetFirstChild")
		self.window_getfirstchild_address = self.base + rva if rva is not None else None

	def close(self):
		if self.handle:
			kernel32.CloseHandle(self.handle)
			self.handle = None

	def module_base(self):
		modules = (ctypes.c_void_p * 256)()
		need = wt.DWORD()
		if not psapi.EnumProcessModulesEx(self.handle, modules, ctypes.sizeof(modules), ctypes.byref(need),
				LIST_MODULES_64BIT if self.is64 else LIST_MODULES_32BIT):
			raise ReadError("EnumProcessModulesEx failed")
		return modules[0]

	def read(self, address, count):
		buffer = ctypes.create_string_buffer(count)
		got = ctypes.c_size_t()
		if not kernel32.ReadProcessMemory(self.handle, address, buffer, count, ctypes.byref(got)) \
				or got.value != count:
			raise ReadError("ReadProcessMemory 0x%x (%d bytes) failed" % (address, count))
		return buffer.raw

	def u32(self, address):
		return struct.unpack("<I", self.read(address, 4))[0]

	def i32(self, address):
		return struct.unpack("<i", self.read(address, 4))[0]

	def ptr(self, address):
		"""A pointer: 4 bytes in a 32-bit client, 8 in a 64-bit one."""
		return struct.unpack(self.ptr_format, self.read(address, self.pointer_size))[0]

	def cstr(self, address, limit=256):
		# up to the end of the page first: a short string near the end of the last committed
		# page of a heap region cannot be read with `limit` bytes
		first = min(limit, 0x1000 - (address & 0xfff))
		data = self.read(address, first)
		if b"\0" not in data and first < limit:
			data += self.read(address + first, limit - first)
		return data.split(b"\0", 1)[0].decode("latin-1")

	def var(self, name):
		return self.i32(self.addr[name])

	def var_ptr(self, name):
		return self.ptr(self.addr[name])

	def off(self, path):
		return self.layout.off(path)

	def field_i32(self, data, base, path):
		return struct.unpack_from("<i", data, base + self.layout.off(path))[0]

	def field_u32(self, data, base, path):
		return struct.unpack_from("<I", data, base + self.layout.off(path))[0]

	def field_ptr(self, data, base, path):
		return struct.unpack_from(self.ptr_format, data, base + self.layout.off(path))[0]

	def field_str(self, data, base, path):
		"""A char[N] member as text up to its first NUL."""
		start = base + self.layout.off(path)
		return data[start:start + self.layout.count(path)].split(b"\0", 1)[0].decode("latin-1")

	def object_at(self, data, offset):
		"""A TObject (Type, Data, Data2) in data at offset."""
		return [self.field_i32(data, offset, "TObject.Type"), self.field_i32(data, offset, "TObject.Data"),
			self.field_i32(data, offset, "TObject.Data2")]

	def objects_at(self, data, offset, count):
		size = self.layout.size("TObject")
		return [self.object_at(data, offset + size * i) for i in range(count)]

	# --- state ---------------------------------------------------------------------------

	def state(self):
		L = self.layout
		s = {}
		s["connection"] = self.var("ConnectionState")
		s["player"] = {name: self.var(name) for name in ("PlayerID", "PlayerHealth", "PlayerMaxHealth",
			"PlayerExperience", "PlayerLevel", "PlayerLevelProgress", "PlayerMagicLevel",
			"PlayerMagicLevelProgress", "PlayerMana", "PlayerMaxMana", "PlayerSoul", "PlayerCapacity",
			"PlayerState", "AttackID", "FollowID")}
		skills = L.count("PlayerSkillValue")
		s["skills"] = list(struct.unpack("<%di" % skills, self.read(self.addr["PlayerSkillValue"], 4 * skills)))
		skills = L.count("PlayerSkillProgress")
		s["skill_progress"] = list(struct.unpack("<%di" % skills,
			self.read(self.addr["PlayerSkillProgress"], 4 * skills)))
		s["map_centre"] = [self.var("MapX"), self.var("MapY"), self.var("MapZ")]
		s["map"] = self.map_points()
		s["creatures"] = self.creatures()
		positions = L.count("Body")
		s["body"] = self.objects_at(self.read(self.addr["Body"], L.size("TObject") * positions), 0, positions)
		s["containers"] = self.containers()
		s["trade"] = [self.offer(self.addr["OwnOffer"]), self.offer(self.addr["CounterOffer"])]
		s["console"] = self.console()
		return s

	def map_points(self):
		L = self.layout
		head = self.read(self.addr["Fields"], L.off("array<TMapPoint>.entry") + self.pointer_size)
		minimum = self.field_i32(head, 0, "array<TMapPoint>.min")
		count = self.field_i32(head, 0, "array<TMapPoint>.d")
		entry = self.field_ptr(head, 0, "array<TMapPoint>.entry")
		if count != MAP_POINTS or entry == 0:
			raise ReadError("Fields: min %d, %d entries at 0x%x" % (minimum, count, entry))
		size = L.size("TMapPoint")
		data = self.read(entry, MAP_POINTS * size)
		limit = L.count("TMapPoint.Objects")
		points = []
		for i in range(MAP_POINTS):
			offset = i * size
			objects = self.field_i32(data, offset, "TMapPoint.ObjectCount")
			if objects < 0 or objects > limit:
				raise ReadError("map point %d: %d objects" % (i, objects))
			points.append(self.objects_at(data, offset + L.off("TMapPoint.Objects"), objects))
		return points

	def creatures(self):
		L = self.layout
		size = L.size("TCreature")
		count = L.count("Creatures")
		data = self.read(self.addr["Creatures"], count * size)
		out = []
		for i in range(count):
			c = i * size
			creature_id = self.field_u32(data, c, "TCreature.CreatureID")
			if creature_id == 0:
				continue
			out.append({"slot": i, "id": creature_id,
				"name": self.field_str(data, c, "TCreature.Name"),
				"pos": [self.field_i32(data, c, "TCreature.RealX"), self.field_i32(data, c, "TCreature.RealY"),
					self.field_i32(data, c, "TCreature.RealZ")],
				"direction": self.field_i32(data, c, "TCreature.Direction"),
				"outfit": [self.field_i32(data, c, "TCreature.OutfitType")] + [
					struct.unpack_from("<i", data, c + L.off("TCreature.Colors") + 4 * j)[0]
					for j in range(L.count("TCreature.Colors"))],
				"light": [self.field_i32(data, c, "TCreature.LightStrength"),
					self.field_i32(data, c, "TCreature.LightColor")],
				"mark": self.field_i32(data, c, "TCreature.Mark"),
				"health": self.field_i32(data, c, "TCreature.Health"),
				"speed": self.field_i32(data, c, "TCreature.Speed"),
				"visible": data[c + L.off("TCreature.Visible")],
				"skull": self.field_i32(data, c, "TCreature.PKFlag"),
				"shield": self.field_i32(data, c, "TCreature.PARTYFlag")})
		return out

	def containers(self):
		L = self.layout
		size = L.size("TContainer")
		count = L.count("Containers")
		data = self.read(self.addr["Containers"], count * size)
		out = []
		for i in range(count):
			c = i * size
			if not data[c + L.off("TContainer.IsOpen")]:
				continue
			objects = self.field_i32(data, c, "TContainer.Count")
			out.append({"index": i, "object": self.object_at(data, c + L.off("TContainer.Object")),
				"name": self.field_str(data, c, "TContainer.Name"),
				"capacity": self.field_i32(data, c, "TContainer.Capacity"),
				"has_parent": data[c + L.off("TContainer.HasParent")],
				"objects": self.objects_at(data, c + L.off("TContainer.Objects"),
					max(0, min(objects, L.count("TContainer.Objects"))))})
		return out

	def offer(self, address):
		L = self.layout
		first = L.off("TOffer.Objects")
		head = self.read(address, first)
		count = self.field_i32(head, 0, "TOffer.Count")
		count = max(0, min(count, L.count("TOffer.Objects")))
		objects = self.read(address + first, L.size("TObject") * count) if count else b""
		return {"name": self.field_str(head, 0, "TOffer.Name"), "count": count,
			"objects": self.objects_at(objects, 0, count)}

	def console(self):
		buffer = self.var_ptr("DefaultTextBuffer")
		if buffer == 0:
			return []
		line = self.ptr(buffer + self.off("TTalkBuffer.first"))
		out = []
		while line and len(out) < 2000:
			mode = self.i32(line + self.off("TTalkLine.Mode"))
			text = self.ptr(line + self.off("TTalkLine.WithoutTime"))
			out.append([mode, self.cstr(text, 512) if text else None])
			line = self.ptr(line + self.off("TTalkLine.Next"))
		return out

	def characters(self):
		"""The names in the character list (Status.cpp: TCharacter, 0x54 bytes in the original)."""
		count = self.var("AccountCharacters")
		array = self.var_ptr("Characters")
		if count <= 0 or array == 0:
			return []
		size = self.layout.size("TCharacter")
		name = self.off("TCharacter.Name")
		return [self.cstr(array + size * i + name, self.layout.count("TCharacter.Name")) for i in range(count)]

	# --- window tree ------------------------------------------------------------------------

	def class_name(self, vtable):
		if self.rtti:
			try:
				# MSVC RTTI: the complete object locator's address is in the slot before the vtable;
				# its pTypeDescriptor (+12) is an address on x86, an image-relative offset on x64; the
				# TypeDescriptor's name follows its pVFTable and spare pointers
				locator = self.ptr(vtable - self.pointer_size)
				descriptor = self.u32(locator + 12)
				if self.is64:
					descriptor += self.base
				name = self.cstr(descriptor + 2 * self.pointer_size, 128)
				if name.startswith(".?AV") and name.endswith("@@"):
					return name[4:-2]
			except ReadError:
				pass
		original = vtable - self.base + 0x400000
		return ORIGINAL_VTABLES.get(original, "vt_%x" % original) if not self.rtti else "vt_%x" % original

	def resolve_thunks(self, function, limit=4):
		"""function after up to `limit` `jmp rel32` thunks (e9 disp32: an incremental link's ILT entries)."""
		for _ in range(limit):
			code = self.read(function, 5)
			if code[0] != 0xe9:
				break
			function += 5 + struct.unpack("<i", code[1:5])[0]
			if self.pointer_size == 4:
				function &= 0xffffffff
		return function

	def is_window(self, vtable):
		function = self.ptr(vtable + self.layout.vslots["TComponent::GetFirstChild"])
		if self.window_getfirstchild_address is not None and 				self.resolve_thunks(function) == self.window_getfirstchild_address:
			return True
		return self.read(function, len(self.window_getfirstchild)) == self.window_getfirstchild

	def gui(self):
		L = self.layout
		nodes = []
		# OffsetX to Height in one read, as before
		first = L.off("TComponent.OffsetX")
		place = L.off("TComponent.Height") + 4 - first

		def walk(component, depth, x, y):
			count = 0
			while component and count < 500:
				vtable = self.ptr(component)
				data = self.read(component + first, place)
				ox, oy = self.field_i32(data, -first, "TComponent.OffsetX"), self.field_i32(data, -first, "TComponent.OffsetY")
				width, height = (self.field_i32(data, -first, "TComponent.Width"),
					self.field_i32(data, -first, "TComponent.Height"))
				node = {"depth": depth, "class": self.class_name(vtable), "x": x + ox, "y": y + oy,
					"w": width, "h": height, "ptr": component, "vtable": vtable}
				if node["class"] == "TLabel":
					text = self.ptr(component + L.off("TLabel.FullText"))
					node["text"] = self.cstr(text, 256) if text else None
				elif node["class"] == "TContextMenuEntry":
					node["text"] = self.cstr(component + L.off("TContextMenuEntry.Text"),
						L.count("TContextMenuEntry.Text"))
				elif node["class"] == "TTextButton":  # Text[50] (DialogElements.h)
					node["text"] = self.cstr(component + L.off("TTextButton.Text"), L.count("TTextButton.Text"))
				nodes.append(node)
				if self.is_window(vtable):
					walk(self.ptr(component + L.off("TWindow.FirstChild")), depth + 1, x + ox, y + oy)
				component = self.ptr(component + L.off("TComponent.Next"))
				count += 1

		main = self.var_ptr("MainWindow")
		if main:
			walk(main, 0, 0, 0)
		modal = self.var_ptr("ModalWindow")
		if modal:
			nodes.append({"depth": 0, "class": "(modal)", "x": 0, "y": 0, "w": 0, "h": 0, "ptr": 0, "vtable": 0})
			walk(modal, 1, 0, 0)
		return nodes

	def skin_offset(self, member):
		"""The offset in SkinProperties of a member path ("Style288.Data60"), or a raw offset (an int),
		which only a layout equal to the original allows."""
		if isinstance(member, str):
			return self.layout.off("TSkinProperties." + member)
		if not self.layout.frozen:
			raise ReadError("raw SkinProperties offset 0x%x on a layout that differs from the original (%s): "
				"name the member in client_state.LAYOUT_FIELDS" % (member, self.layout.source))
		return member

	def skin_rects(self, member, count):
		"""count TRects (x, y, width, height) at SkinProperties' member (see skin_offset)."""
		L = self.layout
		size = L.size("TRect")
		data = self.read(self.addr["SkinProperties"] + self.skin_offset(member), size * count)
		return [tuple(self.field_i32(data, size * i, "TRect." + name) for name in ("x", "y", "width", "height"))
			for i in range(count)]

	def skin_int(self, member):
		return self.i32(self.addr["SkinProperties"] + self.skin_offset(member))

	# --- what the session drivers read besides state() and gui() (tools/testmode_client.py's
	# TestClient gives the same from the test client's probe, issue #92) ----------------------

	def member(self, component, path):
		"""The member `path` (a LAYOUT_FIELDS name) of the component at `component`: a pointer (4 or 8
		bytes), an int, or a bool's byte, by its kind."""
		kind, count = parse_kind(FIELD_KINDS[path])
		if count is not None:
			raise ReadError("%s is an array: member_index" % path)
		address = component + self.off(path)
		if kind == "ptr":
			return self.ptr(address)
		if kind == "bool":
			return self.read(address, 1)[0]
		if kind == "int":
			return self.i32(address)
		raise ReadError("%s: a %s, not a pointer, int or bool" % (path, kind))

	def member_index(self, component, path, index):
		"""Element `index` of the pointer array `path` of the component at `component`."""
		kind, count = parse_kind(FIELD_KINDS[path])
		if kind != "ptr" or count is None or not 0 <= index < count:
			raise ReadError("%s[%d]: not an element of a pointer array (%s)" % (path, index, FIELD_KINDS[path]))
		return self.ptr(component + self.off(path) + self.pointer_size * index)

	def checked(self, box):
		"""TCheckBox's Checked (+0x30 in the original, after the label pointer)."""
		return self.member(box, "TCheckBox.Checked") != 0

	def player_id(self):
		return self.var("PlayerID")

	def map_names(self, map_window):
		"""[(creature ID, x, y)] of the map window's names (TMapWindow.Names, NumberOfNames of them)."""
		L = self.layout
		size = L.size("TCreatureName")
		count = self.i32(map_window + self.off("TMapWindow.NumberOfNames"))
		names = []
		for i in range(max(0, min(count, L.count("TMapWindow.Names")))):
			name = self.read(map_window + self.off("TMapWindow.Names") + size * i, size)
			names.append((self.field_u32(name, 0, "TCreatureName.CreatureID"), self.field_i32(name, 0, "TCreatureName.x"),
				self.field_i32(name, 0, "TCreatureName.y")))
		return names

	def snapshot(self):
		"""state() and gui(), read one after the other (TestClient's: one probe)."""
		return self.state(), self.gui()


def window_getfirstchild_code(pointer_size, first_child):
	"""The code of TWindow::GetFirstChild, `return FirstChild;`, as MSVC compiles it (thiscall: this in
	ecx; x64: this in rcx), for a disp8 offset."""
	if not 0 <= first_child < 0x80:
		raise ReadError("TWindow.FirstChild at 0x%x: not a disp8, the code pattern does not apply" % first_child)
	if pointer_size == 4:
		return bytes([0x8b, 0x41, first_child, 0xc3])        # mov eax, [ecx+disp8]; ret
	return bytes([0x48, 0x8b, 0x41, first_child, 0xc3])      # mov rax, [rcx+disp8]; ret


# --- checks without a client ------------------------------------------------------------------

def check_layout(exe_path, compare=None, out=print):
	"""Resolves every LAYOUT entry in the PDB of exe_path and lists it. compare (default: for an x86
	image) compares each value with the original's constant. Returns the number of problems
	(missing entries, kinds that differ, and with compare the values that differ)."""
	layout, problems = Layout.resolve_pdb(exe_path)
	if problems:
		for problem in problems:
			out("PROBLEM " + problem)
		return len(problems)
	if compare is None:
		compare = layout.pointer_size == 4
	original = Layout.original()
	out("%s: %d-bit, pointer size %d; values %s" % (exe_path, 8 * layout.pointer_size, layout.pointer_size,
		"compared with the original's constants" if compare else "listed (not compared: the layout may differ)"))
	mismatches = 0
	counts = {"global": 0, "field": 0, "size": 0, "vslot": 0}

	def row(what, name, ours, theirs, kind=""):
		nonlocal mismatches
		counts[what] += 1
		ok = not compare or ours == theirs
		if not ok:
			mismatches += 1
		out("%-6s %-38s %-20s %10s %10s  %s" % (what, name, kind, "0x%x" % theirs, "0x%x" % ours,
			("ok" if ok else "MISMATCH") if compare else ""))

	out("%-6s %-38s %-20s %10s %10s" % ("what", "name", "kind", "original", "PDB"))
	for name, address, kind in ORIGINAL_GLOBALS:
		# a global's address is Tibia.exe's absolute one, Patella.exe's an RVA: only its kind is compared
		counts["global"] += 1
		out("%-6s %-38s %-20s %10s %10s  %s" % ("global", name, layout.kinds[name], "0x%x" % address,
			"rva 0x%x" % layout.globals[name], "kind ok"))
	for path, offset, kind in LAYOUT_FIELDS:
		row("field", path, layout.fields[path], offset, layout.kinds[path])
	for name, size in LAYOUT_SIZES:
		row("size", name, layout.sizes[name], size)
	for name, slot in LAYOUT_VSLOTS:
		row("vslot", name, layout.vslots[name], slot)
	code = window_getfirstchild_code(layout.pointer_size, layout.fields["TWindow.FirstChild"])
	original_code = window_getfirstchild_code(4, original.fields["TWindow.FirstChild"])
	out("code   TWindow::GetFirstChild %s (original %s)" % (code.hex(" "), original_code.hex(" ")))
	for name in LAYOUT_FUNCTIONS:
		rva = layout.functions.get(name)
		out("func   %-38s %s" % (name, "rva 0x%x" % rva if rva is not None else "none (inlined everywhere)"))
	if compare and code != original_code:
		mismatches += 1
	out("checked: %d globals (kind), %d fields, %d sizes, %d vtable slots, 1 code pattern; %s" % (
		counts["global"], counts["field"], counts["size"], counts["vslot"],
		"%d mismatches; frozen (equal to the original): %s" % (mismatches, layout.frozen) if compare
		else "0 problems"))
	return mismatches


def self_test(out=print):
	"""The process-side paths without a client, against this Python process (64-bit with a 64-bit Python):
	bitness, module base, pointer reads, the kind of a C string. Returns the number of failures."""
	failures = 0

	def expect(what, ok, detail=""):
		nonlocal failures
		out("%-50s %s %s" % (what, "ok" if ok else "FAILED", detail))
		if not ok:
			failures += 1

	kernel32.GetModuleHandleW.restype = ctypes.c_void_p
	kernel32.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
	client = Client.__new__(Client)
	client.handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, os.getpid())
	client.rtti = False
	try:
		is64 = struct.calcsize("P") == 8
		client.layout = Layout("self test", 8 if is64 else 4, {}, True, {"TWindow.FirstChild": 0x48 if is64 else 0x24},
			{}, {"TComponent::GetFirstChild": 0x20}, {})
		client.attach()
		expect("bitness of this process", client.is64 == is64, "%d-bit" % (64 if client.is64 else 32))
		base = kernel32.GetModuleHandleW(None)
		expect("module base = GetModuleHandle(NULL)", client.base == base, "0x%x" % client.base)
		expect("MZ at the module base", client.read(client.base, 2) == b"MZ")
		target = ctypes.c_void_p(0x1122334455667788 if is64 else 0x11223344)
		expect("ptr() reads a pointer-sized value", client.ptr(ctypes.addressof(target)) == target.value,
			"0x%x" % client.ptr(ctypes.addressof(target)))
		text = ctypes.create_string_buffer(b"Patella\0junk")
		expect("cstr()", client.cstr(ctypes.addressof(text), 30) == "Patella")
		expect("GetFirstChild pattern (x64, FirstChild 0x48)", client.window_getfirstchild ==
			(bytes.fromhex("488b4148c3") if is64 else bytes.fromhex("8b4124c3")), client.window_getfirstchild.hex(" "))
		expect("GetFirstChild pattern (x86, original)", window_getfirstchild_code(4, 0x24) == bytes.fromhex("8b4124c3"))
		self_test_is_window(client, expect)
		if is64:
			# Issue #92: a 64-bit process (this one) is refused; the x64 client is read through its test mode.
			try:
				Client(os.getpid()).close()
				expect("a 64-bit process is refused (issue #92)", False)
			except ReadError as e:
				expect("a 64-bit process is refused (issue #92)", "64-bit client" in str(e), str(e)[:60])
	finally:
		client.close()
	out("self test: %d failures" % failures)
	return failures


def self_test_is_window(client, expect):
	"""is_window() on made-up vtables in this process: a Release body (the pattern), a Debug body (issue #88:
	only the PDB's address identifies it, directly or through a `jmp rel32` thunk), TElement's body."""
	size = client.pointer_size
	first_child = client.layout.fields["TWindow.FirstChild"]
	# windows-x64-debug's body (/Od): this through the stack, `mov [rsp+8], rcx; push rdi; mov rax, [rsp+10h];
	# mov rax, [rax+disp8]; pop rdi; ret`
	debug_body = bytes.fromhex("48894c2408" "57" "488b442410" "488b40%02x" "5f" "c3" % first_child)
	code = ctypes.create_string_buffer(0x100)
	release = window_getfirstchild_code(size, first_child)
	ctypes.memmove(ctypes.addressof(code), release, len(release))
	ctypes.memmove(ctypes.addressof(code) + 0x20, debug_body, len(debug_body))
	ctypes.memmove(ctypes.addressof(code) + 0x60, bytes.fromhex("cccccccc"), 4)  # TElement's: not the pattern
	thunk = 0x80
	ctypes.memmove(ctypes.addressof(code) + thunk, bytes([0xe9]) + struct.pack("<i", 0x20 - (thunk + 5)), 5)
	vtables = (ctypes.c_size_t * 16)()

	def vtable_with(function):
		vtables[0x20 // size] = function
		return ctypes.addressof(vtables)

	base = ctypes.addressof(code)
	saved = client.window_getfirstchild_address
	try:
		client.window_getfirstchild_address = None
		ok = [client.is_window(vtable_with(base)), client.is_window(vtable_with(base + 0x20))]
		expect("is_window: Release body, pattern only", ok == [True, False], str(ok))
		client.window_getfirstchild_address = base + 0x20
		ok = [client.is_window(vtable_with(base + 0x20)), client.is_window(vtable_with(base + thunk)),
			client.is_window(vtable_with(base + 0x60)), client.is_window(vtable_with(base))]
		expect("is_window: Debug body by address, thunk", ok == [True, True, False, True], str(ok))
	finally:
		client.window_getfirstchild_address = saved


def main(argv):
	import argparse
	parser = argparse.ArgumentParser(description="Checks of client_state.py that need no running client.")
	parser.add_argument("--layout", metavar="EXE", help="resolve every LAYOUT entry in EXE's PDB and list it")
	parser.add_argument("--compare", choices=("auto", "yes", "no"), default="auto",
		help="compare with the original's constants (auto: for an x86 image)")
	parser.add_argument("--self-test", action="store_true", help="the process-side paths against this process")
	args = parser.parse_args(argv)
	if not args.layout and not args.self_test:
		parser.error("nothing to do: --layout EXE and/or --self-test")
	failures = 0
	if args.layout:
		failures += check_layout(args.layout, {"auto": None, "yes": True, "no": False}[args.compare])
	if args.self_test:
		failures += self_test()
	return 1 if failures else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
