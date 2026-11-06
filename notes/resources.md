# Resources (issue #17)

Tibia.exe's `.rsrc` section (RVA 0x323000, 0x4520 bytes) holds 55 resource entries in 9 types.
`src/res/Patella.rc` (with `src/res/resource.h`) rebuilds all of them with the original types,
IDs and languages, and the compiled `.res` is byte-identical to the original entry for entry.

## Building and checking

The CMake presets do all three steps (issue #10; before, `tools\build-resources.cmd [path\to\Tibia.exe]`):

```
python -I tools/build.py windows-x64
```

1. At configure time `python -I tools/extract_resources.py Tibia.exe build/<preset>/res` (cmake/Resources.cmake)
   writes the pictures from the local Tibia.exe (the cache variable PATELLA_TIBIA_EXE, default the root's) to
   `build/<preset>/res` (gitignored): `cursor_<id>.cur` (RT_GROUP_CURSOR + its RT_CURSOR
   images), `icon_128.ico` (RT_GROUP_ICON + RT_ICON), `bitmap_<id>.bmp` (RT_BITMAP with a
   BITMAPFILEHEADER) and `resources.tsv` (every entry with type, name, language, code page, size
   and RVA). Any other binary type would go to `<type>_<id>.bin`; Tibia.exe has none.
   `--list` prints the table only.
2. rc.exe of the Windows SDK (10.0.26100.0 x86 here) compiles `src\res\Patella.rc` with that folder on the
   include path (`/l0x0409`), as part of Patella.exe.
3. Until Patella-Modern issue #94, after the link of the 32-bit oracle's Patella.exe, `tools/compare_resources.py`
   (a step of that build; a difference failed it) compared the set of (type, name, language) and the data of every
   entry with Tibia.exe's. It left with the oracle; the x64 build's resources were never compared (plan section 3,
   tier C).

Result (November 2, 2026): 55 entries in the original, 55 in Patella.res, 0 differences, on the
first compile; no padding or format difference of rc.exe had to be worked around. A negative
check (one string, the file version and one cursor changed or removed) gave the expected
DIFFERENT and MISSING lines.

Not compared, since a `.res` does not have them or the linker makes them: the directory's time
stamp (0 in Tibia.exe) and version, the code page of the data entries (0 in Tibia.exe and from
cvtres), the placement and order of the data, the `.res` memory flags.

The pictures are not committed: the two cursors and the icon are CipSoft's, the other cursors
and the bitmaps Microsoft's (MFC). The text resources (version information, strings, the
dialog) are written out in the script.

## Entries

Languages: 1031 German (Germany), 1033 English (United States). Code page 0 for all. The RT_CURSOR
and RT_ICON images are numbered by rc.exe with one counter in script order; Patella.rc keeps the
original order (its comments say so), which is also the order of the data in Tibia.exe.

| Type | ID | Lang | Size | Contents | Used by |
|---|---|---|---|---|---|
| RT_CURSOR | 1 | 1031 | 308 | image of 132: 32x32 1 bpp, hot spot (15,14) | through group 132 |
| RT_GROUP_CURSOR | 132 (0x84) | 1031 | 20 | `IDC_CROSSHAIR`: four triangles pointing inwards | MainFrm.cpp `CMainFrame::CMainFrame` (0x4a8520): `LoadCursor(AfxGetInstanceHandle(), 0x84)` → `DefaultCursor`, the cursor of every client mouse cursor except 1, 2, 3 and 6 (`UpdateMouseCursor`) |
| RT_CURSOR | 2 | 1031 | 308 | image of 137: 32x32 1 bpp | through group 137 |
| RT_GROUP_CURSOR | 137 (0x89) | 1031 | 20 | `IDC_MAGNIFIER`: a magnifying glass | MainFrm.cpp `CMainFrame::CMainFrame`: `LoadCursor(AfxGetInstanceHandle(), 0x89)` → `HandCursor`, client mouse cursor 6 (the reconstruction's name; the picture is a magnifier, not a hand) |
| RT_ICON | 3 | 1033 | 744 | 32x32, 16 colours | through group 128 |
| RT_ICON | 4 | 1033 | 296 | 16x16, 16 colours | through group 128 |
| RT_GROUP_ICON | 128 | 1033 | 34 | `IDR_MAINFRAME`: the Tibia character icon | Player.cpp `CPlayerApp::InitInstance` (0x4da3e0): `LoadIcon(AfxGetResourceHandle(), 128)` for the window class it registers, and again for `WM_SETICON` (`ICON_BIG`) |
| RT_VERSION | 1 | 1033 | 1224 | `VS_VERSION_INFO`: 7.7.2.0, StringFileInfo block 000004b0 (neutral, Unicode), Translation 0x0000/1200 | not read by the code; Windows (file properties) |
| RT_CURSOR | 5, 6 | 1033 | 308, 180 | images of 30977 (32x32 and 16x16) | through group 30977 |
| RT_GROUP_CURSOR | 30977 | 1033 | 34 | MFC AFX_IDC_CONTEXTHELP | nothing: no reference to 0x7901 in the image's code (MFC's context help code is not linked) |
| RT_CURSOR | 7-20 | 1033 | 308 each | images of the wheel cursors, in afxres.rc's order: 7 N, 8 S, 9 E, 10 W, 11 NE, 12 NW, 13 SE, 14 SW, 15 pan horz, 16 pan vert, 17 pan HV, 18 origin horz, 19 origin vert, 20 origin HV | through groups 30998-31011 |
| RT_GROUP_CURSOR | 30998-31011 | 1033 | 20 each | MFC AFX_IDC_MOUSE_PAN_NW, \_N, \_NE, \_W, \_HV, \_E, \_SW, \_S, \_SE, \_HORZ, \_VERT, AFX_IDC_MOUSE_ORG_HORZ, \_VERT, \_HV (names from the pictures and the order of afxres.h) | nothing: no reference in the image's code (`CScrollView` is not linked) |
| RT_DIALOG | 30721 | 1033 | 232 | MFC AFX_IDD_NEWTYPEDLG, the "New" document type dialog (DLGTEMPLATE, MS Shell Dlg 8) | nothing: no reference to 0x7801 in the image's code (`CDocManager` is not linked) |
| RT_BITMAP | 30994 | 1033 | 184 | MFC AFX_IDB_MINIFRAME_MENU, 12x10 4 bpp | nothing: no reference in the image's code (`CMiniFrameWnd` is not linked) |
| RT_BITMAP | 30996 | 1033 | 324 | MFC AFX_IDB_CHECKLISTBOX_95, 33x11 4 bpp | nothing: no reference in the image's code (`CCheckListBox` is not linked) |
| RT_STRING | 3841 | 1033 | 130 | 0xF000-0xF00F: AFX_IDS_OPENFILE ... AFX_IDS_UNNAMED_FILE | MFC file dialogs and documents (not linked); 0xF006 by `GetErrorMessage` |
| RT_STRING | 3842 | 1033 | 42 | 0xF011 AFX_IDS_HIDE | MFC control bars (not linked) |
| RT_STRING | 3843 | 1033 | 402 | 0xF020-0xF025: exception texts (AFX_IDP_NO_ERROR_AVAILABLE, AFX_IDS_NOT_SUPPORTED/RESOURCE/MEMORY/USER/INVALID_ARG_EXCEPTION) | MFC: `CException::ReportError` (0x5279eb) uses 0xF020; 0xF021-0xF023 and 0xF025 are pushed at 0x52e300-0x52ea6a (the static MFC exception objects and `AfxThrowResourceException`), read by `CSimpleException::InitString` (0x52301f) |
| RT_STRING | 3857, 3858 | 1033 | 1250, 794 | 0xF100-0xF11F: AFX_IDP_* document, registry, DLL and DDX parse messages | `CWinApp::WinHelp`/`HtmlHelp` (0x526932, 0x5269ac) use 0xF107, MFC code at 0x52721c 0xF108; the rest belongs to MFC code that is not linked (documents, DDX/DDV) |
| RT_STRING | 3859 | 1033 | 732 | 0xF120-0xF12F: AFX_IDP_FAILED_* file messages | MFC documents (not linked) |
| RT_STRING | 3860 | 1033 | 138 | 0xF130-0xF131: AFX_IDP_SCRIPT_ERROR, \_DISPATCH_EXCEPTION | MFC OLE automation (not linked) |
| RT_STRING | 3865 | 1033 | 172 | 0xF18C-0xF18D: AFX_IDP_GET/SET_NOT_SUPPORTED | MFC OLE automation (not linked) |
| RT_STRING | 3866 | 1033 | 222 | 0xF190-0xF192: MAPI messages | MFC `CDocument::OnFileSendMail` |
| RT_STRING | 3867 | 1033 | 1220 | 0xF1A0-0xF1AE: CFileException texts (AFX_IDP_FILE_*) | MFC `CFileException::GetErrorMessage` (ID = 0xF1A0 + cause; no immediate 0xF1A0 found, so possibly not linked) |
| RT_STRING | 3868 | 1033 | 612 | 0xF1B0-0xF1B7: CArchiveException texts (AFX_IDP_ARCH_*) | MFC `GetErrorMessage` (0x529b52: base 0xF1B0, and 0xF006 for an unnamed file) |
| RT_STRING | 3869 | 1033 | 44 | 0xF1C0 AFX_IDS_OCC_SCALEUNITS_PIXELS | MFC OLE control containment (not linked) |
| RT_STRING | 3887 | 1033 | 66 | 0xF2E1-0xF2E3: AFX_IDS_CHECKLISTBOX_UNCHECK/CHECK/MIXED | MFC `CCheckListBox` accessibility (not linked) |

"Not linked": the binary's code has no reference to the ID (searched as an immediate in the raw
pseudocode and as a 32-bit value or `push imm` in `.text`; hits in Tibia's own functions and
catch blocks are other constants); the resource only comes along with afxres.rc. Such
entries are kept anyway, so that Patella's resource set equals the original's.

The MFC part is the standard `afxres.rc` that an AppWizard project includes, with
`_AFX_NO_SPLITTER_RESOURCES`, `_AFX_NO_OLE_RESOURCES`, `_AFX_NO_TRACKER_RESOURCES` and
`_AFX_NO_PROPERTY_RESOURCES` defined and without `afxprint.rc`: only the context help cursor,
the wheel cursors, the "New" dialog, two bitmaps and the core string tables are left. The AFX
names are from MFC's afxres.h as remembered (MFC is not installed here to check them); the
values are the binary's, and the wheel cursors' names agree with their pictures.

## Resources the code asks for

Found with a search of the raw pseudocode for every resource API (`LoadCursor`, `LoadIcon`,
`LoadString`/`AfxLoadString`/`CString::LoadString`, `LoadBitmap`, `LoadMenu`,
`LoadAccelerators`, `FindResource`/`LoadResource`, `LoadImage`, dialog creation) and of the
binary's code for the IDs.

Tibia's own code (all present, with the right type):

| ID | Type | Where |
|---|---|---|
| 128 | icon | Player.cpp `CPlayerApp::InitInstance` (0x4da3e0), twice |
| 0x84 (132) | cursor | MainFrm.cpp `CMainFrame::CMainFrame` (0x4a8520) |
| 0x89 (137) | cursor | MainFrm.cpp `CMainFrame::CMainFrame` |

The other cursors of CMainFrame (`IDC_ARROW`, `IDC_SIZENS`, `IDC_WAIT`) are system cursors
(`LoadCursor(NULL, ...)`). No Tibia function loads a string, menu, accelerator table, bitmap or
dialog resource; the client draws all its dialogs itself (Dialogs.cpp).

The MFC library code in the image (names.tsv kind stl and the unnamed MFC functions):

| Function | Asks for | In the original |
|---|---|---|
| `CWinApp::SetCurrentHandles` (0x52edd9) | string 0xE000 AFX_IDS_APP_TITLE | **absent**: MFC falls back to the module name (`m_pszExeName`, "Tibia"); Patella must not add it either. The app name is only used by MFC's own message boxes and profile functions, which Tibia does not call (`CPlayerApp` passes NULL to `CWinApp`). |
| `AfxEndDeferRegisterClass` (0x524928) → `_AfxRegisterWithIcon` (0x5248e7) | icons 0x7A01 AFX_IDI_STD_MDIFRAME, 0x7A02 AFX_IDI_STD_FRAME | **absent**: falls back to `IDI_APPLICATION`. Only for MFC's own frame classes; Tibia registers its own class with icon 128 (`PreCreateWindow`). |
| `CSimpleException::InitString` (0x52301f), `GetErrorMessage` (0x529b52) | the exception strings 0xF020-0xF025, 0xF1A0-, 0xF1B0- | present |
| `AfxMessageBox(UINT)` (0x529fb9), `AfxFormatStrings` (0x52a2d5), `CString::LoadString` (0x510055/0x510115), `CheckImplicitLoad` (0x510c32), `ATL::AtlGetStringResourceImage` (0x50fc15) | strings by ID from their callers | the same string tables as the original |
| `CFrameWnd::LoadFrame` (0x52c2ac), `GetIconWndClass` (0x52ab3a), `LoadAccelTable` (0x52a7e5), `CFrameWnd::Create` (0x52c8a7, menu) | the frame's resource ID (string, menu, accelerators, icon) | never called with an ID: Tibia creates its frame with `CFrameWnd::Create(NULL, title, ...)` and no menu |
| `CFrameWnd::OnToolTipText` (0x52bfcb), `CFrameWnd::OnChevronPushed` (0x52c3a3) | strings of command IDs | no tool bars; nothing to load |
| `_AfxLoadDotBitmap` (0x52d7bc), `AUX_DATA::AUX_DATA` (0x52eb2f) | system bitmap OBM_CHECK, system cursors | not resources of the image |
| `AfxLoadLangResourceDLL` (0x52d978) | satellite resource DLLs (`EnumResourceLanguages`) | none exists; the image's resources are used |

So every resource the code asks for is in Patella.rc with the original type, ID and language,
and the ones it asks for but the original lacks are also absent from Patella.rc, so the
fallbacks are the same.

## For #16 (linking Patella.exe)

- Run `python -I tools/extract_resources.py Tibia.exe build/res` before the build (or call
  `tools\build-resources.cmd`, which also checks the result), compile `src\res\Patella.rc` with
  `/I build\res` and give the `.res` to link.exe (it runs cvtres itself). In a .vcxproj: a
  `ResourceCompile` item for Patella.rc with `AdditionalIncludeDirectories` build\res and a
  pre-build event for the extractor.
- Do not include MFC's `afxres.rc` (or `afxprint.rc`, ...): Patella.rc already has MFC 7.1's
  resources, and the installed MFC's would add entries and change texts. With static MFC
  (`/MT`, no `_AFXDLL`) nothing else brings MFC resources in.
- Link with `/MANIFEST:NO` (or without `/MANIFEST:EMBED`): Tibia.exe has no RT_MANIFEST, and an
  embedded manifest would be a 56th entry (and could switch on visual styles or DPI awareness).
- Then `python -I tools/compare_resources.py Tibia.exe Patella.exe` must report 0 differences;
  that is the final check of #17.
- The exe's name decides MFC's app name (no AFX_IDS_APP_TITLE, as in the original): "Patella"
  for Patella.exe. Tibia's code does not use it.

Done in #16 (November 2, 2026): Patella.vcxproj compiles Patella.rc with build\res on the include path
(its ExtractResources target runs the extractor when build\res\resources.tsv is missing), without
afxres.rc, and links with /MANIFEST:NO. `python -I tools/compare_resources.py Tibia.exe
build\Release\Patella.exe` (tools\build-patella.cmd runs it after the build): "55 entries in the original,
55 in Patella.exe, 0 differences", for the Release and the Debug build.
