# The ownership map (Patella-Modern issue #58)

Who creates every heap object of the client, who frees it, and which pointers to it own it. Written for M4's
`std::unique_ptr` step (notes/modernization-plan.md, section 7, M4 "Ownership"): an owning member or global becomes a
`std::unique_ptr<T>` where that changes nothing the comparisons see; everything else stays a raw pointer, and this
file says why. Measured on main c0a4fd8 (November 4, 2026); the second part of the issue (main 3f6042a, November 5,
2026) converted the rest that can be converted inside the frozen layout and lists, in section 7, every owner that stays
raw, with its reason and the milestone that unblocks it.

## 1. Method and totals

Every `new`, `new[]`, `new (std::nothrow)`, `malloc`, `MsvcStrdup` and every `delete`, `delete[]` and `free` of src/
(outside src/Extern, comments and string literals skipped) was listed with its function and target, then read where
it goes: into a member, a global, a parent window, a message of the event queue, a return value or a local. The
plan's "608 `new` against 193 `delete`" counted the words; the expressions are:

| | Sites | |
|---|---:|---|
| `new T` / `new (std::nothrow) T` | 412 + 11 | |
| `new T[n]` / `new (std::nothrow) char[n]` | 116 + 4 | |
| `malloc`, `MsvcStrdup` | 1 + 2 | (compat/msvc_portable.h's own malloc and strdup not counted) |
| **allocations** | **546** | of which 379 GUI components, 27 drawing lists, 67 other objects and object arrays, 73 buffers |
| `delete` | 73 | |
| `delete[]` | 114 | |
| `free` | 25 | 15 of them free memory that came from `new` (below) |
| **releases** | **212** | |

The difference is not a leak count. 379 of the allocations are window components, and one `delete Child` in
TWindow::DeleteChild (GUI.cpp) frees all of them; the event queue's blocks are freed by a switch in ProcessEvents or
ClearMessages; Container.h's destructors free every container. Section 5 lists the real leaks.

Kinds of owner used below:

- **child list**: the component is handed to a TWindow (InsertChild, AppendChild, InsertChildAfter, or a constructor
  that does it: TDialog's body, TBorder's content, TSubWindow's body) and from then on belongs to the window's list
  of children. `TWindow::~TWindow` deletes every child through DeleteChild (RemoveChild, then `delete Child`); a
  window that drops a child calls DeleteChild itself. Members that point at a child (`OkButton`, `ScrollBar`,
  `NameEdit`, ...) do not own it.
- **member**: the object's destructor (or a setter) deletes it; nothing else points at it as an owner.
- **conditional**: a member owns the object only while it is not a child; the destructor asks `IsChild` first.
- **global, init/exit**: an `Init*` or `Load*` function creates it, an `Exit*`, `Release*` or `Reload*` function frees
  it, in the order ExitControl (Control.cpp) calls them.
- **message**: the block travels as an `INT_PTR` parameter of AddMessage (Status.cpp); ProcessEvents (Control.cpp)
  frees it after use, by message type, and ClearMessages (Status.cpp) frees what is still queued, also by type.
- **caller**: a function returns it (`readString`, `GetString`, `GetOpenDialogs`, `GetFullscreenModes`); the caller
  frees it.
- **local**: freed in the same function.
- **until exit**: created once, never freed; the process ends with it.

## 2. Ownership by subsystem

### GUI core (GUI.cpp, GUI.h)

- `TComponent::DrawingList` (+0x04), a `TDrawing[DrawingListLength]`: **member**. Every subclass allocates it in its
  constructor (27 sites in 9 files; TPlayerStates' two are the branches of one constructor), each the first list of
  its object (TComponent's constructor leaves it null and no class allocates over a base's), and
  `TComponent::~TComponent` deletes it (`delete[]`, GUI.cpp). It is the base of every window object; the difftest
  and the golden window tests build, read and compare it by offset in our objects and in the original's
  (tests/golden/gui_tests.cpp, dialogelements_tests.cpp, ...). Converted in the second part (section 6).
- `TWindow::FirstChild` and every `TComponent::Next`: the **child list**, an intrusive singly linked list; the parent
  owns the children (above). Not a pointer that one `unique_ptr` could own: ownership moves between lists
  (RemoveChild without delete, TTabBar's three lists) and is decided by `IsChild`.
- `TTimer`s are created by their users (Dialogs.h), register themselves in the timer list (non-owning) and
  unregister in their destructor.

### Dialogs (Dialogs.cpp, Dialogs.h)

- 232 components: the `Create*Dialog` functions build a body and a `TDialog` around it (the body becomes the
  dialog's child) and return the dialog to Control.cpp, which stores it in `ModalWindow` (below). Each body's
  constructor creates its controls and adds them to itself: **child list**. Members that point at controls
  (`TLoginDialog::Account`, the option dialogs' check boxes, `TEditMarkDialog::TypeButtons[14]`, ...) are not owners.
- **member** (exclusive; every one deleted in the class's destructor, the timers also in their setters):
  `TWaitLabel::Timer`, `TReconnectWindow::Timer`, `TCharacterListDialog::Timer`, `TLoginDialog::FocusGroup`,
  `TRuleViolationDialog::FocusGroup`, `TOutfitDialog::PartGroup` and `ColorGroup` (deleted Color first),
  `TEditBuddyDialog::IconGroup`, `TEditMarkDialog::TypeGroup`, `TAdvancedGraphicsDialog::EngineGroup`,
  `TNewChannelDialog::ChannelIDs` (`int[]`), `TGraphicsOptionsDialog::Modes` (`TFullscreenMode[]`, allocated by the
  surface's GetFullscreenModes through an out parameter).
- **local**: TIgnoreListDialog's two name tables (`char *[]` and their strings, 1440/1505, freed at 1464-1469 and
  1529-1534), the text copies of TReportDialog (1943) and TRuleViolationDialog (2280).
- Takes over a message's block: `TEditMarkDialog`'s constructor deletes the `TCoordinate` and `TMark` of messages
  0x1d/0x1e (2511-2513).

### Dialog elements (DialogElements.cpp, DialogElements.h)

- 15 components, all **child list** of the element that makes them: TLink's label and button, TCheckBox's label,
  the scroll bars' buttons and ranges, TFormattedText's lines and icons, TEditText's scroll bar, TListBox's scroll
  bar.
- **member** buffers: `TLabel::Text`, `FullText`, `Line` (`char *[]`, the line starts: the table is the label's, its
  entries point into `Text`), `TFormattedText::Text` and `Line` (a scratch buffer as large as `Text`, not a pointer
  into it), `TLink::Destination`, `TMultiStateButton::ButtonUps`, `ButtonDowns`, `Callbacks`, `TEditLine::Text`,
  `TPasswordEditLine::Stars`; the list boxes' lines (`TListBox::LineText`, `TFontListBox::FontLines(i).Text`, entries
  of a Container.h `vector`, freed by SetLine, Clear and the destructors).
- **conditional**: `TListBox::EditLine` is a child while the list box edits a line (`Editing`), else the list box
  deletes it in its destructor.
- Not owners: `TEditLine::FocusGroup` (the dialog's), `TRadioButton`'s group, `TScrollBar`'s callbacks.

### Sub-windows and the side panel (DialogWindow.cpp, DialogWindow.h)

- `TSubWindow`'s `Body`, `Resizer1`, `Resizer2`, `UpButton`, `CloseButton`, `MinimizeButton`, `Icon`, `Title`:
  **conditional**. Each is a child while shown; the setters and the destructor delete it themselves when it is not
  (a hidden "up" button, the parts of a minimized window): `if (X != nullptr && !IsChild(X)) delete X;`.
- `TContentWindow`'s and the dialog window's children: **child list**; TDialogWindow's DeleteChild clears its drag
  pointer first and rearranges after.

### The console (ConsoleWindow.cpp, ConsoleWindow.h)

- **member**: `TTabBar::LeftTabs`, `RightTabs` (two `TWindow`s holding the tabs that do not fit; deleted Left
  first in `~TTabBar`, before TWindow deletes the bar's own children), `TConsoleWindow::History` (a `TTalkBuffer`,
  deleted first in `~TConsoleWindow`), `TTab::Caption` and `FullCaption` (`char[]`), `TTextWindow::SelectedText`
  (`char[]`, replaced by every GetSelectedText; the caller only reads it).
- **conditional**: `TConsoleWindow::VolumeButton`, `CloseButton`, `ServerMessagesButton`, `MoveRightButton`,
  `MoveLeftButton` (children only while they apply to the current channel; `~TConsoleWindow` deletes the ones
  that are not children).
- **child list**: the tabs (in the bar or one of its two windows; CloseChannel deletes the closed tab itself, 1755,
  after removing it), the input line, the bar, the console and its text window and scroll bar, IgnoreButton,
  NewChannelButton. `CurrentConsole` and `DefaultTab` point at children.
- Not owners: `TTab::TextBuffer` (Talk.cpp's buffers, below).

### Content windows (ContentWindows.cpp, ContentWindows.h)

- 86 components: the content windows, their bodies and controls (**child list**); `TTradeWindowBody`'s three parts,
  the trade buttons (`TTradeButtons::SetCancelButton` and `ShowAcceptButton` replace children through DeleteChild).
- **member**: `TCombatControls::AttackModes`, `ChaseModes` (`TRadioButtonGroup`, deleted Attack first),
  `TBuddylistWindowBody::MouseOverText` (`char[36]`, replaced on every hover, deleted in the destructor).
- **message**: `TMarkMenuData` (603), the automap's context menu data, travels in message 59 (0x3b) to
  `TMarkContextMenu`, whose constructor deletes it (ContextMenus.cpp 1284).

### Context menus (ContextMenus.cpp)

- Entries and separators: **child list**. `THelpBubble::Text`: child.
- **message**: the copies a menu hands to a dialog through the queue: `TTalkLine` (1077, 1143; message 0x1c),
  `TCoordinate` and `TMark` (1296, 1299; messages 0x1d/0x1e), names (`char[30]`, 668, 837, 901).
- The menu itself is `ModalWindow` (Control.cpp).

### The main window and the map (MainWindow.cpp, MapWindow.cpp)

- `MainWindow` (global, `TMainWindow`): **global, init/exit**: InitMainWindow creates it, ExitMainWindow deletes it
  (MainWindow.cpp 95). Its 23 parts (side panel, side bar, console, dialog window, map frame, ...) are its children.
- `TMapFrame::MapWindow`, `TMapWindow::HintButton`: **child list** (the hint button is added and deleted with
  DeleteChild as hints come and go).
- `TMainWindow::ChangeMode`'s list of open dialogs comes from GetOpenDialogs (`new int[]`) and is freed with `free`
  (446).

### Control and the event queue (Control.cpp, Status.cpp's messages)

- `ModalWindow` (global, MainWindow.cpp's): the one modal window, a dialog, a context menu or the help bubble.
  **global**, created by ProcessEvents (Create*Dialog, the context menus) and MouseMove (the help bubble), deleted
  wherever the modal window ends (Resize, LeaveGame, the mouse handlers, KeyPress, ProcessEvents: 10 sites) and by
  ExitMainWindow. Read by TASSERTs (`ModalWindow!=NULL`), by tools/client_state.py, the test mode's state dump and
  the difftest's Control fakes.
- **message**: every block of the queue (Communication.cpp, Network.cpp, Control.cpp, MainFrm.cpp, the windows):
  texts (`new char[]`), `INT_PTR` and `uint32` tables, the context menus' copies. ProcessEvents frees each after its
  dialog or menu is made (by type, in its switch), ClearMessages the ones still queued (types 1, 2, 5-7, 14-16, 18, 25, 26,
  28, 47-51).

### Communication and the network (Communication.cpp, Network.cpp)

- `PatchData` (global, `uint8[]`): **global**, created by ReceivePatch/ReceiveWholeFile, freed by
  ReceivePatchBlock at the end of the file and by CancelPatch.
- `Socket` (global, `TSocket`, an MFC CAsyncSocket): **global, init/exit**: CreateSocket, deleted by CreateSocket's
  failure paths, ConnectSocket's failure, Disconnect and ExitNetwork.
- `HostnameResolveBuffer` (global, `malloc`): InitNetwork / ExitNetwork (`free`).
- **message**: the received texts, channel lists, outfit and info tables (13 sites in Communication.cpp, 5 error texts
  of 1024 bytes in Network.cpp).
- **local**: ReceiveMessageOfTheDay's text (878).

### Talk (Talk.cpp, Talk.h)

The original compiled `delete` as the destructor and `free` (VC7.1's static CRT); the reconstruction writes some of
those as `~T()` plus `free`, which this map counts as frees.

- `DefaultTextBuffer` (global): InitTalk / ExitTalk (destructor, then `free`).
- The other text buffers (`CreateTextBuffer`, six classes): **caller** (the console's tabs point at them),
  freed by DeleteTextBuffer (destructor, then `free`), which also frees the buffer's console filter.
- Lines (`TTalkLine`, `new (std::nothrow)`): appended to a buffer, which owns them (`first` ... `last`); a line's text
  (`Line`, `new char[]`) is freed with `free` by TTalkBuffer::Append (when the buffer is disabled) and Remove.
- Filters (`TTalkFilter`, `new (std::nothrow)`) in the four filter lists (Container.h `vector<TTalkFilter *>`), freed
  with `free` by the Clear*, Add* and Remove* functions.

### Status and preferences (Status.cpp, Preferences.cpp)

- `Characters` (global, `TCharacter[]`): **global, init/exit**: SetAccountCharacters replaces it, ExitStatus frees it.
  Read by tools/client_state.py (`ptr`).
- `TaintedClipboardText` (global, `char[]`): replaced by CheckClipboardText, freed by ExitStatus **without being
  cleared** (a dangling pointer after ExitStatus, as in the original).
- `LinkRegexp`: regcomp's `malloc`, freed by ExitStatus with `free`.
- `MotD` (Preferences.cpp): replaced by SetMotD (`delete[]` / `new char[]`), freed by ExitPreferences with `free`.
- GetOpenDialogs, GetOpenChannels: **caller** (`new int[]`); the callers free with `free` (SavePreferences,
  ChangeMode) or `delete[]` (TConsoleWindow's constructor).

### Graphics and skins (graphics.cpp, Skins.cpp)

- **global, init/exit**: `Sprites` (`TSprite[]`, LoadSprites / ExitGraphics, ReloadSprites), `Pictures`
  (`TPicture[]`, each with its `Offsets` `int[]`; LoadPictures / ExitGraphics, ReloadPictures), `Fonts`
  (`TFontInfo[4]`; InitFonts / ExitGraphics, ReloadPictures), `MemoryCacheData` and `MemoryCache`
  (InitMemoryCache / ExitGraphics), `VideoCache` (InitVideoCache / ExitGraphics, ResetVideoCache), `Skins`
  (`TSkin[]`; InitSkins / ExitSkins). ExitGraphics frees in the order VideoCache, MemoryCacheData, MemoryCache,
  Fonts, the pictures' offsets, Pictures, Sprites. The init functions store a new table over the old one without
  freeing it (InitFonts, InitMemoryCache, LoadSprites, LoadPictures, InitVideoCache; InitVideoCache also clears
  `VideoCache` unfreed when there are no cache surfaces): leaks only where they are called twice (the golden tests),
  kept. Every table but the pictures' `Offsets` converted in the second part.

### Objects and the map (Objects.cpp, Map.cpp)

- `ObjectProperties`, `OutfitProperties`, `EffectProperties`, `MissileProperties` (globals, `array<TProperties>`):
  **global, init/exit**: LoadObjects / ReleaseObjects, which first frees each entry's `Sprites` (`uint16[]`, from
  LoadProperties) and then the array, table by table.
- The map's fields and automap blocks are Container.h objects inside globals (no heap pointer of their own).
- **local**: the automap marks' texts (Map.cpp 1165, from readString).

### Surfaces (Surface.cpp, SurfaceDX.cpp, surfaceDX9.cpp, SurfaceGL.cpp, vulkan/)

- `Surface` (global, the current engine's `TSurface`): GetSurface creates it on first use, SurfaceExitInstance and
  SurfaceInitInstance's failure paths delete it.
- GetFullscreenModes: **caller** (`TFullscreenMode[]`); the engines' own copies are **member**s freed by InitDisplay
  and their enumeration callbacks.
- **local**: the pixel blocks of SurfaceGL.cpp (496, 647, 874, 906) and surfaceDX9.cpp (826).
- The Vulkan engine's own state (`TRenderer`, the present windows) is already `std::unique_ptr` and standard library
  members (issue #19); `CreateSurfaceVulkan` returns a raw `new` like the other engines' creators.

### Application and frame (Player.cpp, MainFrm.cpp)

- `CMainFrame`: created by CPlayerApp::InitInstance, deleted there if Create fails, else MFC's own (the frame deletes
  itself in PostNcDestroy).
- **message**: MainFrm.cpp's error texts (317).

### Files, buffers and portable code (Script.cpp, Utils.cpp, Container.h, vlong.cpp, sysinc.cpp)

- `TReadBinaryFile::readString`, `TReadBuffer::GetString`: **caller** (`new char[]`).
- Container.h's `array`, `matrix`, `matrix3d`, `vector`: **member** `entry`, freed by their destructors; `vector`
  grows into a new block and frees the old one. A `vector<T *>` does not own what its entries point at.
- vlong: `vlong_value` shared by copies (a count), deleted by the last owner; `flex_unit::a` (`unsigned[]`) member.
- `ClipboardBuffer` (sysinc.cpp, global): InitSysinc (`new char[1024]`), grown by ResizeClipboardBuffer (`new` the
  new block, `free` the old), ExitSysinc (`free`).

### Test mode, settings, Vulkan (src/testmode, src/settings, src/vulkan)

- New code: standard containers and `std::unique_ptr` already. The one raw `new`, the test mode's clock state
  (testmode/TestModeWin32.cpp 402), lives **until exit** on purpose (the clocks are read during static destruction).

## 3. Owners that are not one pointer

These keep their raw pointers until M7 redesigns them; a `unique_ptr` would have to know what the code decides at run
time:

- **The child list** (379 components): owned through `TWindow::FirstChild`/`TComponent::Next`. A `unique_ptr` per
  `Next` would make the list's destruction recursive and its order different from DeleteChild's (first child first,
  each child's subtree before its next sibling).
- **Conditional members** (TSubWindow's eight parts, TConsoleWindow's five buttons, `TListBox::EditLine`): their owner
  is the child list or the member, decided by `IsChild`/`Editing`.
- **The event queue** (`INT_PTR` parameters): ownership travels with the message type.
- **Container.h's `vector<T *>`** of owned objects (Talk.cpp's filter lists, the list boxes' lines): the owner frees
  with `free` or `delete[]` by entry; `vector<std::unique_ptr<T>>` would change the container's element type and its
  copies (Container.h copies entries by assignment when it grows) and the golden layouts of the list boxes. Listed for
  M7's containers.

## 4. Candidates for std::unique_ptr, and why not the others

A `T *` member or global may become `std::unique_ptr<T>` (`<T[]>` for arrays) when it is the only owner, its pointee is
freed with the matching `delete`/`delete[]` and its destructor cannot throw, the original's free sites can be kept as
explicit `reset()`s in the same order, and nothing reads it in a way the type change breaks: a TASSERT/TERROR/TEND
argument whose text would need `.get()` (until M5 the text may not change), memcpy of the object, the tools' PDB reader
(client_state.py reads a `unique_ptr` with the default deleter as a pointer), the tests' raw assignments (changed to
`reset()`/`.get()` where needed). Section 6 records what issue #58 converted, section 7 every owner that stays raw.

The reasons an owner stays raw, in short (section 7 names each owner):

- **The pointee's destructor may throw.** Every `TComponent` (`virtual ~TComponent() noexcept(false)`, issue #34).
  `std::unique_ptr`'s `reset()` and destructor are `noexcept` whatever the deleter, so an exception from such a
  destructor would end in `std::terminate` instead of reaching the caller's handler. Issue #81 checked whether that is
  still so after #78 (TBEGIN/TEND gone) and #80: it is. `~TTab` asserts (`PlayerName!=NULL`, a player's report
  without a name), calls the Send functions and DeleteTextBuffer (its `Filter!=-1` assertion), and the golden
  ConsoleWindow test throws through it on purpose ("A player's report without a name: its tab's destructor
  asserts"); `~TConsoleWindow`, `~TContainerWindowBody`, `~TTradeWindowBody`, `~TAutomapWindow`, `~TSubWindow` and
  `~TMainWindow` call the preferences, the hotkeys and the Send functions too. So `TComponent`'s destructor stays
  `noexcept(false)` and the owners of components stay raw (section 7). `TTalkBuffer`'s destructor was
  `noexcept(false)` too until #81 made it `noexcept`: it cannot throw (Clear only removes the first line, which never
  reaches Remove's assertion).
- **A TASSERT argument names it in a way a `unique_ptr` does not compile in** (until #81). `TEditLine::Text` was
  `strlen(Text)` in two assertions; since #76 (outcome only) and #79 (PATELLA_ASSERT) the text is not compared, and
  #81 converted it with `.get()`. `TPasswordEditLine::Stars`, which swaps blocks with `Text` around Draw, converted
  with it (the swap is `Text.swap(Stars)`).
- **Mismatched release** (the original freed `new` memory with `free`, `new T` with `delete[]`, or an array with
  `delete`): a `unique_ptr` with the default deleter would call another function; a custom deleter is not allowed. M7
  (or the Linux client, M8) gives each its matching release (section 5).
- **`malloc`'s memory** (regcomp's, the resolver buffer): freed with `free`, not `delete`. M8 replaces both (the
  regexp behind an RAII wrapper, the resolver with `getaddrinfo`).
- **Freed without being cleared**: `TaintedClipboardText` (ExitStatus). A `unique_ptr` is null after its release; the
  original leaves the old address, and a CheckClipboardText after ExitStatus deletes it a second time.
- **Destroyed at exit when the process ends through `exit(1)`** (CPlayerApp::FatalError) with the pointer still set: a
  global `unique_ptr` runs its destructor during static destruction, the original leaves the object. Harmless for
  plain arrays (memory only), not for objects whose destructors act: `ModalWindow` and `MainWindow` (window trees,
  timers, Tibia.cfg's open dialogs), `Socket` (MFC's CAsyncSocket after MFC's module state), `Surface` (the engine's
  device and windows). M7 moves the globals into subsystems owned by one `TClient` with a defined end; M8 replaces
  CAsyncSocket.
- **Not one owner**: the child list, the conditional members, the event queue's blocks, the owned entries of
  Container.h `vector<T *>`s, Talk.cpp's lines (section 3), and `vlong_value`, which copies of a vlong share with a
  count (never a `unique_ptr`; no `std::shared_ptr` either, a settled decision). M7 redesigns the GUI's tree, the
  queue and the containers.
- **A pointer in each element of an owned array**: `TPicture::Offsets` (in `Pictures`) and `TProperties::Sprites`
  (in the four object tables). A `unique_ptr` element would give the element type a constructor that clears it (the
  original's `new TPicture[n]` and `array<TProperties>`'s `new T[d]` leave it indeterminate), and a destructor, so
  every `delete[]` of the table would run one per element; the owners free them in their own loops first. M7 (the
  tables as `std::vector`s of elements that own their arrays).
- **Owned by MFC or by nothing**: `CMainFrame` (MFC deletes it in PostNcDestroy; M8 removes MFC), the test mode's
  clock state (alive until exit on purpose).

## 5. Leaks and mismatches the original has (kept)

- Message 0x1c (a rule violation report from a context menu): ProcessEvents frees the `TTalkLine` copy with
  `delete[]` as a `char` block, so the line's own text (`TTalkLine::Line`) is never freed; ClearMessages does the same
  for a queued one.
- Messages 0x1d/0x1e (the automap's marks) still in the queue when ClearMessages runs: the `TCoordinate` and `TMark`
  are not freed (ClearMessages has no case for them).
- ExitStatus: `TaintedClipboardText` and `LinkRegexp` freed but not cleared; the queued messages not freed (comment
  at Status.cpp 636).
- The `free`/`new` and `delete`/`new[]` mismatches of section 4: harmless with the Microsoft CRT (its `operator
  delete` and `delete[]` call `free`), undefined behaviour in standard C++. M7 (or the Linux client, M8) gives each
  its matching release.
- SurfaceGL.cpp 496/647/874/906 free arrays with `delete` (oracle-x86 only; not compiled for x64).
- The test mode's clock state lives until exit on purpose (section 2).

## 6. What issue #58 converted

Each a `std::unique_ptr` with the default deleter at the original's offset (static_asserts on the class's size and
the member's offset under `PATELLA_ORIGINAL_LAYOUT`, and on `sizeof`/`alignof` of the `unique_ptr` against the raw
pointer), the original's `delete`/`delete[]` kept as explicit `reset()`s at the same places and in the same order, the
allocations as `reset(new ...)` (the same form of `new`: default-initialised arrays, no `make_unique`):

| Subsystem | Members and globals | Commit |
|---|---|---|
| Console (ConsoleWindow.h) | `TTextWindow::SelectedText`, `TTab::Caption`, `TTab::FullCaption` (`char[]`) | 91e9cd6 |
| Content windows (ContentWindows.h) | `TCombatControls::AttackModes`, `ChaseModes` (`TRadioButtonGroup`), `TBuddylistWindowBody::MouseOverText` (`char[]`) | b37760b |
| Dialogs (Dialogs.h) | `TWaitLabel::Timer`, `TReconnectWindow::Timer`, `TCharacterListDialog::Timer` (`TTimer`), `TLoginDialog::FocusGroup`, `TRuleViolationDialog::FocusGroup` (`TFocusGroup`), `TOutfitDialog::PartGroup`, `ColorGroup`, `TEditBuddyDialog::IconGroup`, `TEditMarkDialog::TypeGroup`, `TAdvancedGraphicsDialog::EngineGroup` (`TRadioButtonGroup`), `TNewChannelDialog::ChannelIDs` (`int[]`) | d1871b9 |
| Dialog elements (DialogElements.h) | `TLabel::FullText`, `TLink::Destination` (`char[]`), `TMultiStateButton::ButtonUps`, `ButtonDowns` (`int[]`), `Callbacks` (`TCallback[]`) | dadff06 |
| Status (Status.cpp) | `Characters` (`TCharacter[]`) | d3ab25e |
| Communication (Communication.cpp) | `PatchData` (`uint8[]`) | 42b1b6d |
| Objects (Objects.cpp, Objects.h) | `ObjectProperties`, `OutfitProperties`, `EffectProperties`, `MissileProperties` (`array<TProperties>`) | 52c1c63 |
| Skins (Skins.cpp, Skins.h; second part) | `Skins` (`TSkin[]`) | 094a20a |
| Graphics (graphics.cpp, graphics.h; second part) | `Sprites` (`TSprite[]`), `Pictures` (`TPicture[]`), `Fonts` (`TFontInfo[]`), `MemoryCacheData` (`uint8[]`), `MemoryCache` (`TMemoryCacheItem[]`), `VideoCache` (`TVideoCacheItem[]`) | 36bdbd7 |
| GUI core (GUI.h; second part) | `TComponent::DrawingList` (`TDrawing[]`, in every window object) | a03affd |
| Dialog elements (DialogElements.h; second part) | `TLabel::Text` (`char[]`), `TLabel::Line` (`char *[]`), `TFormattedText::Text`, `Line` (`char[]`) | 59f8650 |
| Dialogs (Dialogs.h; second part) | `TGraphicsOptionsDialog::Modes` (`TFullscreenMode[]`) | c966428 |
| Containers (Container.h; second part) | `array<T>::entry`, `matrix<T>::entry`, `matrix3d<T>::entry`, `vector<T>::entry` (`T[]`, every instance) | 209422c |
| vlong (vlong.h; second part) | `flex_unit::a` (`unsigned[]`) | 92fd511 |
| Console (ConsoleWindow.h; issue #81) | `TConsoleWindow::History` (`TTalkBuffer`) | issue-81 |
| Dialog elements (DialogElements.h; issue #81) | `TEditLine::Text`, `TPasswordEditLine::Stars` (`char[]`) | issue-81 |

28 owning pointers in the first part: 22 members of 17 classes and 6 globals. The second part: 7 globals (`Skins` and
the six graphics tables) and 11 members (`DrawingList` of every window class, the label's and the formatted text's four
buffers, the graphics options' mode list, the containers' entries in their four templates, vlong's units). Where the original cleared an owning global
without freeing it (InitCommunication's `PatchData`, InitObjects' four tables), the code calls `release()` and drops
the result, so a block still held is left as the original left it. Where it deleted the old object before making the
new one (the timers' SetTimer and SetTimeout), `reset()` comes first and `reset(new ...)` after, so the timer list sees
the same order. Each pointee's destructor is `noexcept` (arrays of scalars, `TCallback`, `TCharacter`; `TTimer`,
`TFocusGroup`, `TRadioButtonGroup`, Container.h's `array`).

The second part's rules where the original did more than delete and new: where it stored a new table over one it may
still hold (InitSkins, InitFonts, InitMemoryCache, LoadSprites, LoadPictures, InitVideoCache: only the golden tests call
them twice), `release()` drops the old one unfreed before the `reset(new ...)`, so the leak stays (core_skins_test.cpp
checks that the old table is still allocated); InitVideoCache's clear without a free is a `release()`. The original
passed `&Modes` to GetFullscreenModes; the dialog now passes a local and `reset()`s Modes with it, which changes
nothing because every engine's GetFullscreenModes writes the pointer first. vector<T>'s growth keeps "free the old
block, then store the new one" as `reset()`, `reset(help)`; flex_unit::reserve holds the new block in a `unique_ptr`
local and moves it in after the `reset()` (GCC's analyzer took a raw one handed to `reset()` for a leak, in Debug). TPlayerStates allocates
one of two lists by its branch, not one over the other (part 1's map said it replaced it).

Behaviour that differs only where the original's own behaviour ends the client: a constructor that throws after
allocating a converted member now frees it during the unwinding (the members are destroyed), where the original
leaked it (a TReconnectWindow's timer is then also unregistered); every such throw reaches FatalError. And a converted
global that is still set when FatalError calls `exit(1)` is freed by its destructor during static destruction (memory
only: the arrays and Container.h tables above have no destructors that act). In the second part: Skins and the graphics
tables are arrays of plain structures (a picture's `Offsets` stays raw, so `Pictures`' destructor frees only the
table); in a constructor of the dialog elements, the containers or flex_unit that throws, the converted members are
null or freed during the unwinding, where the original left an indeterminate pointer or leaked the block.

Issue #81 (M5, #58's M5 list) converted three more: `TConsoleWindow::History` once `~TTalkBuffer` was made `noexcept`
(the console's constructor `History.reset(new TTalkBuffer(0))`, its destructor `History.reset()` where the original
deleted it, before the buttons), `TEditLine::Text` (`reset(new char[...])` in both constructors, `reset()` in the
destructor; every reader takes `.get()`) and `TPasswordEditLine::Stars` (the same; Draw's two swaps through a local are
`Text.swap(Stars)`), each with its size and offset asserted under `PATELLA_ORIGINAL_LAYOUT` (TConsoleWindow 0x6c,
History +0x48; TEditLine 0x68, Text +0x2c; TPasswordEditLine 0x6c, Stars +0x68). The rest of the M5 list stays raw
(section 7): the owners of components, because `TComponent`'s destructor still throws.

## 7. The owners that stay raw, and the milestone that unblocks each

After the second part every owning member and global of src/ is a `std::unique_ptr` or one of these (the reasons in
section 4; locals, return values and the queue's parameters are not members or globals and keep their raw pointers
with the owners below):

| Owner | Why raw | Unblocked by |
|---|---|---|
| The component members that own a component: `TTabBar::LeftTabs`, `RightTabs` (`TWindow`); the conditional members (`TSubWindow`'s eight parts, `TConsoleWindow`'s five buttons, `TListBox::EditLine`) | the pointee's destructor throws (`TComponent`, `noexcept(false)`, kept by #81): the two windows hold the tabs that do not fit, and `~TTab` asserts on a nameless report, sends and calls DeleteTextBuffer's assertion (the golden ConsoleWindow test throws through it); as a `unique_ptr`, `reset()` would end in `std::terminate`, and a throw from `delete LeftTabs` would destroy `RightTabs` while unwinding (its tabs' sends) where the original left it. The conditional ones are also owned by the child list or the member, by `IsChild`/`Editing` | M7 (the GUI tree): the destructors' work that can fail (a tab leaving its channel, the console storing the channels and its height, a body closing its container or trade, the preferences) moves into an explicit close before the delete, so `TComponent`'s destructor can become `noexcept`; then the exclusive ones convert, and the conditional ones with the tree |
| The child list: `TWindow::FirstChild`, `TComponent::Next` (379 allocation sites) | not one owner: an intrusive list, ownership moves between lists and is decided by `IsChild`; its destruction order is DeleteChild's | M7 (the GUI tree) |
| `ModalWindow`, `MainWindow` (globals) | the pointee's destructor throws (`TComponent`, kept `noexcept(false)` by #81; the main window's tree holds the console and its tabs); a global's destructor would run the window destructors during static destruction after FatalError's `exit(1)` | M7 (owned by `TClient`, with the GUI tree's `noexcept` destructors) |
| `Socket` (`TSocket`, a CAsyncSocket) | its destructor acts at `exit(1)`, after MFC's module state | M8 (CAsyncSocket replaced) |
| `Surface` (the engine) | its destructor releases the device and the windows at `exit(1)` | M7 (owned by `TClient`) |
| `TPicture::Offsets`, `TProperties::Sprites` | a pointer in each element of an owned array: a `unique_ptr` would clear it in `new T[n]` and give the element a destructor | M7 (the tables as `std::vector`s) |
| The list boxes' lines (`TListBox::LineText`, `TFontListBox::FontLines(i).Text`), Talk.cpp's filter lists (Container.h `vector<T *>` entries) | owned entries of a container of pointers; the filters are also freed with `free` | M7 (the containers) |
| `DefaultTextBuffer`, the text buffers of CreateTextBuffer, a `TTalkBuffer`'s lines (`first` ... `last`, `TTalkLine::Line`) | mismatched release (`new (std::nothrow)`, then the destructor and `free`; `free` of `new` text); the buffers are also not one owner (a tab's `TextBuffer` and the console filter list's entry; DeleteTextBuffer frees both). `~TTalkBuffer` is `noexcept` since #81, so that reason is gone | M7 (matching releases, the filter list, the lines' list) |
| `MotD` (Preferences.cpp), `ClipboardBuffer` (sysinc.cpp) | mismatched release (`free` of `new char[]`) | M7 |
| `TaintedClipboardText` (Status.cpp) | freed without being cleared (ExitStatus); the second delete after it is the original's | M7 (a deliberate fix, recorded) |
| `LinkRegexp` (Status.cpp) | regcomp's `malloc`, freed with `free` and not cleared | M8 (the regexp behind an RAII wrapper) |
| `HostnameResolveBuffer` (Network.cpp) | `malloc`, freed with `free` | M8 (getaddrinfo) |
| The event queue's blocks (`INT_PTR` parameters of AddMessage) | ownership travels with the message type; some are freed as another type (0x1a, 0x1c) | M7 |
| `vlong_value` (vlong's shared value) | shared by copies, with a count | never a `unique_ptr` (and no `std::shared_ptr`, settled) |
| `CMainFrame` | MFC deletes it (PostNcDestroy) | M8 (MFC removed) |
| The DirectDraw, Direct3D 9 and OpenGL engines' mode lists and pixel blocks | oracle-x86 only | M6 (they leave with the oracle) |
| The test mode's clock state (TestModeWin32.cpp) | alive until exit on purpose (read during static destruction) | stays |

## Appendix: every allocation by file

Line numbers at c0a4fd8. GUI components are owned by the child list unless section 2 names them; the other kinds as
section 2 says.

- `src/Communication.cpp`, 14: objects and arrays of objects (1): 2460 MsvcStrdup. buffers (char, integer arrays) (13): 891 uint8[]; 919 uint8[]; 970 char[]; 977 char[]; 2017 uint32[]; 2023 char[]; 2034 uint32[]; 2180 int[]; 2183 char[]; 2199 char[]; 2209 char[]; 2285 char[]; 2437 int[]. Freed at lines 878, 967, 1024.
- `src/ConsoleWindow.cpp`, 23: GUI components (16): TConsole 1263; TEditLine 1259; TMultiStateButton 1265; TScrollBar 641; TSkinButton 1268, 1271, 1273, 1275, 1277; TTab 1683, 1717; TTabBar 1261; TTextWindow 640; TToggleButton 1269; TWindow 920, 921. drawing lists (2): 639 TDrawing[]; 1254 TDrawing[]. objects and arrays of objects (2): 1258 TTalkBuffer; 1919 TTalkLine. buffers (char, integer arrays) (3): 522 char[]; 743 char[]; 744 char[]. Freed at lines 511, 768, 770, 1291, 1304, 1306, 1308, 1310, 1312, 1314, 1755.
- `src/Container.h`, 7: objects and arrays of objects (7): 88 T[]; 138 T[]; 185 T[]; 241 T[]; 259 T[]; 270 T[]; 285 T[]. Freed at lines 72, 119, 161, 215, 276, 291.
- `src/ContentWindows.cpp`, 95: GUI components (86): TAutomapView 284; TBattleWindowBody 161; TBuddylistWindowBody 203; TCapacityLabel 99, 1049; TCombatControls 78, 93; TCompassRose 295; TContainerWindowBody 119; TContentWindow 73; TInventoryView 75; TLabel 125, 145, 165, 185, 207, 1579, 2665, 2670, 2722, 2795; TObjectIcon 124, 1578; TPlayerStates 105, 1055; TScrollBar 1459, 1783, 2112, 2473, 2922; TSeparator 2475, 2662, 2724; TSkillsWindowBody 141; TSkinButton 96, 121, 122, 123, 143, 144, 163, 164, 183, 184, 205, 206, 287, 289, 291, 293, 1046, 1249, 1253, 1257, 1261, 1265, 1275, 1277; TSkinIcon 146, 166, 186, 208; TSmallInventoryWindow 91; TSoulLabel 102, 1052; TSubWindow 120, 142, 162, 182, 204; TTextButton 239, 285, 1272, 1280, 1282, 2763, 2765, 2781; TTextToggleButton 230, 233, 236; TToggleButton 1269; TTradeButtons 2808; TTradeNames 2810; TTradeOffers 2809; TTradeWindowBody 181. drawing lists (5): 277 TDrawing[]; 735 TDrawing[]; 742 TDrawing[]; 980 TDrawing[]; 1383 TDrawing[]. objects and arrays of objects (3): 603 TMarkMenuData; 1246 TRadioButtonGroup; 1247 TRadioButtonGroup. buffers (char, integer arrays) (1): 3048 char[]. Freed at lines 3046.
- `src/ContextMenus.cpp`, 13: GUI components (3): TContextMenuEntry 417; TFormattedText 77; TSeparator 429. drawing lists (2): 76 TDrawing[]; 318 TDrawing[]. objects and arrays of objects (5): 1077 TTalkLine; 1143 TTalkLine; 1296 TCoordinate; 1299 TCoordinate; 1299 TCoordinate. buffers (char, integer arrays) (3): 668 char[]; 837 char[]; 901 char[]. Freed at lines 1284.
- `src/Control.cpp`, 13: GUI components (10): TBuddyContextMenu 1138; TChannelContextMenu 1146; TCreatureContextMenu 1134; TEditContextMenu 1154; THelpBubble 424; TMarkContextMenu 1158; TObjectContextMenu 1130; TRequestContextMenu 1150; TSkillContextMenu 1162; TTextContextMenu 1142. buffers (char, integer arrays) (3): 661 INT_PTR[]; 853 char[]; 858 char[]. Freed at lines 107, 382, 421, 494, 500, 530, 587, 593, 641, 928, 940, 944, 954, 960, 966, 990, 991, 992, 993, 998, 999, 1004, 1012, 1014, 1037, 1041, 1049, 1051, 1111, 1115, 1119, 1123, 1127.
- `src/Creatures.cpp`, 1: objects and arrays of objects (1): 1175 MsvcStrdup.
- `src/DialogElements.cpp`, 42: GUI components (15): TEditLine 2648; THorizontalScrollBarRange 2322; TLabel 617, 714, 1196, 1238; TScrollBar 1738, 2636; TScrollBarRange 2581; TSkinButton 2318, 2319, 2577, 2578; TSkinIcon 649; TTextButton 716. drawing lists (10): 78 TDrawing[]; 167 TDrawing[]; 191 TDrawing[]; 215 TDrawing[]; 1193 TDrawing[]; 1268 TDrawing[]; 1295 TDrawing[]; 2129 TDrawing[]; 2377 TDrawing[]; 2642 TDrawing[]. objects and arrays of objects (1): 1148 TCallback[]. buffers (char, integer arrays) (16): 386 char[]; 387 char[]; 395 char[]; 692 char[]; 693 char[]; 711 char[]; 1146 int[]; 1147 int[]; 1253 char[]; 1280 char[]; 1699 char[]; 2824 char[]; 2840 char[]; 2982 char[]; 2996 char[]; 3012 char[]. Freed at lines 373, 377, 381, 686, 688, 2838, 2879, 3010, 3025.
- `src/DialogElements.h`, 2: drawing lists (2): 47 TDrawing[]; 837 TDrawing[]. Freed at lines 127, 129, 131, 162, 164, 188, 415, 416, 417, 475, 557, 749, 751, 813.
- `src/Dialogs.cpp`, 239: GUI components (222): TAddBuddyDialog 389; TAdvancedGraphicsDialog 510; TBorder 190, 1228; TBugReportDialog 315; TCharacterListDialog 234; TCheckBox 1401, 1417, 1423, 1426, 1470, 1486, 1492, 2036, 2340, 2668, 2669, 2670, 2671, 2672, 2729, 2825, 2826, 2827, 3020, 3021, 3022, 3023, 3024, 3071, 3119; TColorButton 1258; TConsoleOptionsDialog 523; TDialog 80, 94, 107, 121, 136, 151, 165, 192, 208, 223, 235, 267, 279, 293, 305, 316, 328, 339, 350, 361, 379, 390, 401, 413, 425, 437, 449, 461, 473, 486, 499, 511, 524, 536, 548, 559; TEditBuddyDialog 400; TEditLine 1787, 1864, 1912, 1958, 2304, 2327, 2334, 2492, 3116, 3242; TEditListDialog 292; TEditMarkDialog 460; TEditText 1317, 1356, 1748, 2031, 3240; TErrorReportDialog 558; TFontListBox 1407, 1476, 3108; TFormattedText 91, 1018; TGeneralOptionsDialog 485; TGraphicsOptionsDialog 498; THelpDialog 424; THintsDialog 436; THorizontalScrollBar 1127, 2828; THotkeyDialog 547; TIconButton 2021, 2502; TIgnoreListDialog 360; TLabel 79, 180, 588, 845, 847, 930, 933, 979, 1124, 1265, 1326, 1364, 1404, 1420, 1473, 1489, 1751, 1782, 1788, 1865, 1913, 1959, 2005, 2011, 2028, 2086, 2139, 2145, 2151, 2157, 2163, 2169, 2237, 2301, 2312, 2318, 2324, 2331, 2489, 2497, 2566, 2572, 2578, 2584, 2590, 2596, 2732, 2733, 2835, 2850, 3102, 3105, 3239, 3241; TLink 181, 922; TListBox 982, 1781, 2240, 2315, 2321, 2730; TLoginDialog 222; TLogoutLabel 163; TMoveObjectsDialog 207; TNetworkOptionsDialog 535; TNewChannelDialog 327; TObjectIcon 1322; TOptionsDialog 472; TOutfitDialog 304; TOutfitIcon 1227; TPasswordEditLine 918, 920; TPrivateChannelDialog 338, 349; TProgressBar 846, 2089; TQuestionLabel 149; TReconnectWindow 135; TReportDialog 378; TRuleViolationDialog 448; TSeparator 183, 589, 2008, 2601; TTextButton 611, 633, 654, 1231, 1251, 1411, 1414, 1480, 1483, 2138, 2144, 2150, 2156, 2162, 2168, 2565, 2571, 2577, 2583, 2589, 2595, 2731, 2840, 2843, 2846, 2852, 2855, 2858; TTextDialog 266, 278; TUpdateDialog 412; TWaitLabel 106, 120; TWindow 178. drawing lists (2): 579 TDrawing[]; 1362 TDrawing[]. objects and arrays of objects (10): 806 TTimer; 843 TTimer; 917 TFocusGroup; 1107 TTimer; 1234 TRadioButtonGroup; 1256 TRadioButtonGroup; 2014 TRadioButtonGroup; 2300 TFocusGroup; 2500 TRadioButtonGroup; 2824 TRadioButtonGroup. buffers (char, integer arrays) (5): 1440 char[]; 1505 char[]; 1794 int[]; 1943 char[]; 2280 char[]. Freed at lines 805, 1070, 1106, 1464, 1469, 1529, 1534, 2511, 2513.
- `src/DialogWindow.cpp`, 4: GUI components (2): TResizer 610, 611. drawing lists (2): 114 TDrawing[]; 600 TDrawing[]. Freed at lines 73, 75, 77, 79, 81, 83, 85, 87, 820, 836, 852, 868, 884.
- `src/graphics.cpp`, 7: objects and arrays of objects (5): 126 TSprite[]; 205 TPicture[]; 325 TFontInfo[]; 341 TMemoryCacheItem[]; 505 TVideoCacheItem[]. buffers (char, integer arrays) (2): 213 int[]; 340 uint8[]. Freed at lines 94, 96, 98, 100, 103, 104, 106, 1290, 1303, 1315, 1318, 1319.
- `src/MainFrm.cpp`, 1: buffers (char, integer arrays) (1): 317 char[].
- `src/MainWindow.cpp`, 25: GUI components (23): TAutomapWindow 387, 407; TBorder 408, 669; TButtonsWindow 395, 401; TConsoleWindow 367; TDialogWindow 368, 369; TLabel 545; TMainWindow 790; TMapFrame 365; TObjectIcon 546; TSplitter 366; TStartWindow 467; TStatusWindow 391, 405; TTextButton 654, 657, 660, 663, 666; TWindow 652. drawing lists (1): 729 TDrawing[]. buffers (char, integer arrays) (1): 296 INT_PTR[]. Freed at lines 95, 98, 446.
- `src/MapWindow.cpp`, 4: GUI components (2): TBlinkingButton 763; TMapWindow 1621. drawing lists (1): 1624 TDrawing[]. buffers (char, integer arrays) (1): 331 char[].
- `src/Network.cpp`, 7: objects and arrays of objects (2): 507 TSocket; 729 malloc. buffers (char, integer arrays) (5): 480 char[]; 590 char[]; 626 char[]; 633 char[]; 663 char[]. Freed at lines 510, 527, 573, 737, 740.
- `src/Objects.cpp`, 5: objects and arrays of objects (4): 719 array<TProperties>; 720 array<TProperties>; 721 array<TProperties>; 722 array<TProperties>. buffers (char, integer arrays) (1): 695 uint16[]. Freed at lines 602, 603, 608, 609, 614, 615, 620, 621.
- `src/Player.cpp`, 1: objects and arrays of objects (1): 327 CMainFrame. Freed at lines 332.
- `src/Preferences.cpp`, 3: buffers (char, integer arrays) (3): 536 int[]; 547 int[]; 613 char[]. Freed at lines 611, 1113, 1132, 1175.
- `src/Script.cpp`, 1: buffers (char, integer arrays) (1): 523 char[].
- `src/Skins.cpp`, 1: objects and arrays of objects (1): 297 TSkin[]. Freed at lines 305.
- `src/Status.cpp`, 3: objects and arrays of objects (1): 362 TCharacter[]. buffers (char, integer arrays) (2): 823 char[]; 826 char[]. Freed at lines 359, 487, 496, 499, 500, 501, 502, 505, 506, 509, 511, 515, 517, 642, 645, 647, 822, 830.
- `src/Surface.cpp`, 1: objects and arrays of objects (1): 86 TSurfaceGL. Freed at lines 101, 201, 237, 248.
- `src/SurfaceDX.cpp`, 2: objects and arrays of objects (2): 677 TFullscreenMode[]; 726 TSurfaceDX. Freed at lines 681, 702.
- `src/surfaceDX9.cpp`, 3: objects and arrays of objects (2): 584 TFullscreenMode[]; 666 TSurfaceDX9. buffers (char, integer arrays) (1): 826 uint8[]. Freed at lines 589, 610, 877, 959.
- `src/SurfaceGL.cpp`, 5: objects and arrays of objects (1): 797 TFullscreenMode[]. buffers (char, integer arrays) (4): 496 uint8[]; 647 uint8[]; 874 uint8[]; 906 uint8[]. Freed at lines 508, 609, 650, 891, 949.
- `src/sysinc.cpp`, 2: buffers (char, integer arrays) (2): 134 char[]; 150 char[]. Freed at lines 142, 153.
- `src/Talk.cpp`, 15: objects and arrays of objects (11): 499 TTalkFilter; 746 TTalkLine; 748 TTalkLine; 851 TTalkBuffer; 854 TChannelTalkBuffer; 856 TPrivateChannel; 858 TPrivateTalkBuffer; 860 TRuleViolationTalkBuffer; 866 TRuleViolationChannel; 920 TTalkLine; 1195 TTalkBuffer. buffers (char, integer arrays) (4): 544 char[]; 633 char[]; 1146 char[]; 1167 char[]. Freed at lines 561, 573, 594, 606, 618, 658, 670, 704, 880, 884, 931, 932, 978, 979, 1212.
- `src/testmode/TestModeWin32.cpp`, 1: objects and arrays of objects (1): 402 TState.
- `src/Utils.cpp`, 1: buffers (char, integer arrays) (1): 261 char[].
- `src/vlong.cpp`, 3: objects and arrays of objects (3): 20 unsigned[]; 234 vlong_value; 252 vlong_value. Freed at lines 23, 228, 264, 320.
- `src/vulkan/SurfaceVulkan.cpp`, 2: objects and arrays of objects (2): 79 TSurfaceVulkan; 640 TFullscreenMode[]. Freed at lines 928.
- `src/ConsoleWindow.h`, 0 allocations. Freed at lines 97, 226, 227.
- `src/ContentWindows.h`, 0 allocations. Freed at lines 181, 182, 410.
- `src/Dialogs.h`, 0 allocations. Freed at lines 180, 200, 232, 252, 307, 308, 438, 496, 562, 591, 638, 661.
- `src/GUI.cpp`, 0 allocations. Freed at lines 64, 1818.
- `src/Map.cpp`, 0 allocations. Freed at lines 1165.
- `src/vulkan/DeviceSettings.cpp`, 0 allocations. Freed at lines 41.
- `src/vulkan/Renderer.cpp`, 0 allocations. Freed at lines 128.
