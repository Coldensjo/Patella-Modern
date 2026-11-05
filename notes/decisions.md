# Decisions made without asking

Judgement calls made during the reconstruction instead of stopping to ask,
with the evidence, for review. Confidence: high, medium, low.

## Script.cpp

- **Class and member names from CipSoft's game server** (medium). The client
  binary has no names for the script and binary-file classes. Its tokenizer is
  the same state machine as the game server's `TReadScriptFile::nextToken`
  (same states, the same special characters 'L', 'G', 'N', 'I' for `<=`, `>=`,
  `<>`, `->`, the same token kinds), and the client's TERROR names the state
  variable `State`, as the server does. So the classes are `TReadScriptFile`,
  `TWriteScriptFile`, `TReadBinaryFile`, `TWriteBinaryFile` and the methods use
  the server's lower-case names (`open`, `nextToken`, `getIdentifier`,
  `readNumber`, `writeLn`, ...), not the TitleCase that notes/reconstruction.md
  asks for invented names: these are taken, not invented. Fields (`Token`,
  `File`, `Filename`, `Line`, `String`, `Bytes`, `Number`, `CoordX/Y/Z`,
  `Special`) and the `TOKEN` enumerators likewise. In names.tsv members are
  `Class_method`.
- **MAX_IDENT_LENGTH 20 and MAX_STRING_LENGTH 1000** (high for the values,
  low for the names): from the checks in nextToken and the 1000-byte buffer.
- **Patch functions named ApplyPatch, ApplyFullPatch, InstallPatch** (low).
  0x4e3100 applies a difference patch received from the login server,
  0x4e3500 writes a complete file, 0x4e3610 (lines 879-883) replaces the file
  by the patched copy. Names are ours.
- **Inline header functions go to Script.h** (high). The constructors,
  destructors and `close()` functions at 0x4df550-0x4df6c9 sit in the
  Preferences/Script gap, before Script.cpp's first function. They are COMDAT
  copies from Preferences.obj (the Tibia.cfg loader and saver are the first
  users in link order), and Script.cpp's own ApplyPatch calls 0x4df6b0 from its
  catch block. Which one of `close()` and the destructor each copy is follows
  the loader (it calls 0x4df5d0 then 0x4df5b0 at the end of the file) and
  ApplyPatch (0x4df6b0 in the catch handler, which closes, and again in the
  continuation, which destroys). Identical bodies were folded by the linker:
  0x4df690 is both binary-file constructors, 0x4df6b0 both `close()`s and
  destructors, 0x4df660 both `close()` and the destructor of TWriteScriptFile.
- **Source order**: line order; TReadBinaryFile::readString (line 644, placed
  by the compiler after InstallPatch at 0x4e3a10) is put between readQuad and
  readBytes. The four `read*` helpers of TReadScriptFile (0x4e3920-0x4e39d0) have
  no line evidence and stay at the end, where the binary has them.
- **error()'s "(unknown)"** is written as a local array because the binary
  copies the text to the stack.
- **nextToken's "string too long" check** is written at the end of the loop
  body; the binary jumps straight into the switch on the first pass. The
  error calls are noreturn for the compiler, so the code after them does not
  show the original loop shape.
  Reviewed in #24: confirmed. Every state's break jumps to 0x4e0203 `cmp [esp+0x10],0x3e7; jne 0x4dfcad`
  (the switch), and the identifier state compares `cmp ebx,0x14` (0x4dfde1) right after its getc, as
  written. Only the first pass differs from a check at the loop head, where Size is 0, so the shape cannot
  change behaviour. scripttest.cpp's TestLongTokens now reads the two limits from those instructions,
  compares identifiers of 18-19 characters and strings of 996-998 stored characters (with escapes) on both
  sides and checks that one character more raises our error (our side only; the image cannot throw in the
  child). Mutations: limit MAX_STRING_LENGTH -> 100 failures; identifier limit - 1 -> our side raises.
  High.

## Talk.cpp

Integration naming audit, November 1, 2026, by ChatGPT gpt-6.1-sol. No
original function spellings are present in these entries' assertion/catch
strings, so the TitleCase names below are inferred from behavior and callers.
Confidence is **medium** for the chosen name/role and **low** for its exact
original spelling; the field offsets, flags, arguments and vtable slots are
**high** confidence from instructions and differential tests. Assertion/catch
identifiers (Speaker, TargetBuffer, ReceiveTalkMode, Addressee, etc.) are kept.

| Address | Chosen name | Confidence | Behavior |
|---|---|---|---|
| 0x4fd4c0 | SetTalkChanged | medium; spelling low | Sets the talkbox/status redraw latch |
| 0x4fd540 | SetTalkThrottle | medium; spelling low | Restarts the ignored-talk throttle for 1000 milliseconds |
| 0x4fd590 | SetConsoleChanged | medium; spelling low | Sets the console redraw latch |
| 0x5052e0 | AddTalkbox | medium; spelling low | Adds or merges positioned speech/private/broadcast text with its speaker prefix |
| 0x505ac0 | AddMessageTalkbox | medium; spelling low | Adds queued event/admin text or replaces the information talkbox |
| 0x4ff130 | UpdateTalkboxes | medium; spelling low | Expires talkboxes using elapsed milliseconds since the previous update |
| 0x4ff500 | SetStatusMessage | medium; spelling low | Copies or clears the status message and sets its permanent/50-tick lifetime |
| 0x4ff7d0 | UpdateStatusMessage | medium; spelling low | Decrements the status-message tick counter and requests redraw on expiration |
| 0x500060 | IsTalkIgnored | medium; spelling low | Tests the ignore filters after temporary and buddy-name exemptions |
| 0x500c60 | AddTemporaryIgnore | medium; spelling low | Replaces the temporary name exemption used when buddy ignoring is enabled |
| 0x500e40 | RemoveTemporaryIgnore | medium; spelling low | Removes the temporary name exemption |
| 0x501400 | ClearIgnoreList | medium; spelling low | Frees every ignore filter and clears its vector entry; called by SetDefaultPreferences |
| 0x5076e0 | ShowPermanentIgnoreWarning | medium; spelling low | Appends the permanent-ignore warning to the default console when named permanent filters exist |
| 0x505f10 | AddTextBufferLine | medium; spelling low | Constructs a statement with optional addressee and appends it to the supplied text buffer |
| 0x5061f0 | AddConsoleText | medium; spelling low | Routes statements through console filters or broadcasts messages according to preferences |
| 0x5025c0 | GetTextBuffer | medium; spelling low | Looks up the console text buffer matching receive mode, addressee and channel |
| 0x504d70 | ResetTalk | medium; spelling low | Clears all talkboxes, status text and the default text buffer and resets talk order |
| 0x504f30 | InitTalk | medium; spelling low | Initializes ignore flags, resets talk and allocates the default text buffer |
| 0x507a00 | ExitTalk | medium; spelling low | Clears ignore/exemption lists and destroys the default text buffer |
| 0x507bc0 | CreateTextBuffer | medium; spelling low | Creates the matching concrete buffer and registers its filter; send mode is an integer |
| 0x503920 | TTalkBuffer::GetNext | medium; spelling low | Advances the non-null statement cursor and returns the advanced cursor |


- **Leading sysinc/Talk gap** (high). 0x4fd440-0x4fd59f is Talk.cpp:
  fourteen helpers use the same status message, ten talkboxes, console-change
  flag and ignore flags at 0x71dbd0-0x71ef6c as asserted Talk functions. None
  uses sysinc's clock storage at 0x71dbcc. 0x55a130 initializes that sysinc
  clock; the next four initializers, 0x55a140/160/180/1a0, construct Talk's
  filter vectors at 0x71ef70/90/b0/d0. No function in this gap goes to sysinc.cpp.
- **Header assignments** (high for ownership/layout, low for invented member
  names). 0x4fd5a0-0x4fd6df contains the inline buffer constructor, vtable
  accessors and TTalkLine::GetText, not standalone sysinc routines. The
  0x573738 vtable and its five derived vtables identify the slots. Ghidra
  missed most tiny accessors; names.tsv records them nevertheless. The
  addressee getter 0x506eb0, channel-ID getter 0x507060, write-enabled
  setter/getter 0x507260/270, private-channel predicate 0x5079f0, and
  always-throwing 0x5075c0 (Talk.h line 321) likewise belong to Talk.h.
  Identical true/false/null/-1 accessors folded into earlier objects stay
  inline, not additional C++ translation-unit functions.
- **Container.h** (high). 0x4fd6e0 is the initialized four-byte pointer
  vector constructor, with Container.h lines 355/356, space>0 and block>=0.
  Its existing vector<T> implementation is reused and its instantiation
  documented in Container.h. operator() folds with vector<int> at 0x452ac0.
  The static initializers pass (0,9,10,NULL) for the first three lists and
  (0,4,5,NULL) for ConsoleFilterList. No STL implementation is reconstructed.
- **Trailing Talk/TMyException region** (high). Talk's final catch at
  0x5081b5 ends at exclusive 0x508261. Through 0x50826f there is only INT3
  alignment. 0x508270 is VC7.1 std::string::find_last_of, already marked
  stl and owned by TMyException.cpp; 0x508350 is its existing STL substring
  constructor. Nothing additional is assigned to Talk.cpp or sysinc.cpp.
- **Ghidra function splits** (high). 0x502f70 is a branch target inside
  Remove(TTalkLine*) at 0x502f00; 0x5036c0 is a loop block of GetLine at
  0x503670. Neither has an entry prologue, independent stack frame, or callable
  signature. They have names.tsv labels but no duplicate reimplementation.
  ehinfo's scan can also mistake the following function's prologue for the
  frame of the four-byte getter at 0x507060 and the tail-call destructor at
  0x506920; disassembly establishes that both have no frame of their own.
- **Names and source types** (low for original spelling; high for tested
  layouts). TTalkBuffer, TTalkLine, TTalkFilter, the five derived class names,
  flag names and helpers not present in assertion strings are ours. Talk.h
  now owns the full types, the shared TALK_MODE enum and all Talk declarations.
  TTextBuffer/TCommunicationChannel/TCommunicationStatement views and all Talk
  declarations were removed from external.h. It includes Talk.h only for its
  unfinished ConsoleWindow declarations. TRGBColor stays in external.h;
  Talk.h forward-declares it for its pointer-only color interfaces.
- **One definition per entry** (high). CreateTextBuffer is the only definition
  of 0x507bc0, and TTalkBuffer::GetNext the only definition of 0x503920.
  Both compatibility shims and all Talk-to-TCommunicationChannel casts are
  removed. Every names.tsv address is unique; reviewed Talk rows are edited
  in place and now describe each function's behavior.
- **Factory's second parameter** (high). 0x507bc0 takes integer SendTalkMode,
  not bool. ReceiveRuleViolationChannel pushes 1 at 0x4115d6 and calls the
  factory at 0x4115dd. Its source now uses (6,1,NULL,ChannelID), preserving
  the binary request, including the fallback lookup and event 0x32. Equal
  modes and full integer send modes remain differentially tested.
- **Old channel-view correction** (high). First +0x10, Cursor +0x18 and the
  statement Name +0x14 exactly match TTalkBuffer::first/cursor and
  TTalkLine::Addressee; static assertions enforce them in Communication.cpp.
  The old view's comment calling slot +0x10 close was wrong: that slot is
  SetWriteEnabled(bool), not Close (slot +0x08). ReceiveCancelRuleViolation,
  ReceiveLockRuleViolation and ReceiveCloseChannel all pass false to +0x10;
  those calls now use SetWriteEnabled(false) without changing Closed.
  Removal remains virtual Remove(TTalkLine*) at +0x20, and event 0x31's
  Unknown40(1) becomes SetSelected(true), the same +0x40 slot. See the full
  caller/instruction audit and regression coverage in notes/Talk.md.
- **Source order** (high where lines exist). The compiler placed the free
  slot finder (line 300), speech/message insertion (352-474), permanent-ignore
  warning (1144-1149), routing (1179-1279), factory/deletion (1361-1383), and
  buffer/line constructors late in the binary. They are placed by source
  lines, not address. Helpers without line evidence stay with their binary
  neighbours; header COMDATs are in the header.
- **Exact oddities retained** (high). Remove(int)'s loop never increments i
  (0x503330-0x50333d), so only index zero can succeed on a nonempty list;
  nonzero indices run to the Search!=NULL assertion. GetNext returns the
  advanced cursor. Base/private/channel constructors leave Closed untouched;
  only the rule-violation constructors clear it. Zero/negative line limits
  are not normalized. Named-line insertion uses its time offset; insertion
  without an addressee discards the caller's offset. Ignore filters never
  match modes 9/10/11/12. IgnoreEnabled gates exceptions, not the final ignore
  list itself. HasUnreadConsoleText is an inferred, imperfect name: the
  actual predicate is selected AND IsPrivateChannel(), not selected AND
  locked; differential testing caught and corrected that distinction.
- **Allocation and destruction** (high for binary behavior, low for original
  source syntax). Talk uses nothrow allocations to preserve VC7's null-return
  new branches. A null filter allocation is stored as a null list entry; a
  null line allocation still reaches virtual Append. Unchecked string
  allocations remain unchecked. Objects allocated with new are deliberately
  released with free, as in the image. The buffer destructor is nonvirtual,
  resets to the base vtable and clears through the base destructor; its
  noexcept(false) is a modern-language adaptation so Clear exceptions can
  still reach the caller's TEND. Limits, padding and uninitialized fields
  are not repaired.
  Reviewed in #24: confirmed: the low part is only the source syntax; the behaviour (null-return new,
  free, the destructor) is what talktest compares.
- **ABI and field types** (high for widths/offsets, low for original source
  declarations). Colors use an explicit output pointer and return it, which
  reproduces the hidden-return-buffer x86 ABI without guessing the original
  struct-by-value syntax. TTalkLine::Time is a signed 32-bit time_t, not
  modern time_t. Flags at +0x24..+0x29 have inferred semantic names; their
  byte values, vtable access and six class sizes are checked. Write's original
  catch calls its FILE* stack slot bytes; an INT_PTR alias retains that name.
  Reviewed in #24: confirmed: TTalkLine::Time is only set from _time32 and passed to _localtime32 (never
  compared), so its signedness cannot show; the colour output pointer is the hidden-return ABI (talktest).
- **Differential harness** (high). The Preferences test's old Talk-reset
  counter is replaced with the real implementation on both sides; its
  original ignore vector is initialized before SetDefaultPreferences.
  PatchImage is reused; Talk additionally redirects 32-bit time/localtime,
  sprintf and fprintf. The test executable's own _time32 is temporarily
  redirected to the same fixed clock, restored afterwards, to avoid
  second-boundary races. Only explicitly invoked mapped functions execute;
  the original executable's loader, imports and entry point never run.
  Error/exception and allocation-failure paths are not invoked in the image.
- **Metadata correction** (high). The old automatic names extraction
  mistook the 0x502f70 explanatory comment for a function and appended
  BuddyIgnoreList. The erroneous duplicate is removed; the single remaining
  TTalkBuffer_Remove_line_block row identifies an internal block of 0x502f00,
  not a function or global. No separate source definition is added.

See notes/Talk.md for exception-table checks, switch tables, all remaining
callcheck differences, caller integration and differential coverage.

## Objects.cpp

- **The whole Objects/Player gap (0x4d7cd0-0x4d913e) is Player.obj** (high).
  It starts at 0x4d7cd0 (not found by Ghidra), `mov eax, 0x56dde4; ret`:
  the GetMessageMap of BEGIN_MESSAGE_MAP, slot 10 of the vtable 0x56de08
  whose overrides 0x4d94c0, 0x4da3e0 and 0x4dae60 are Player.cpp functions.
  The application object it belongs to is at 0x719dc0, among Player.cpp's
  data (0x719db0-0x719e6c), and its static initialiser (0x55a030) comes
  right after Network.cpp's (0x55a010). Everything after it is either the
  class's constructor (0x4d7cf0), compiler-generated destructors, the MFC
  inline AfxGetMainWnd, or <fstream>/<string> code (filebuf, codecvt,
  getline, string::compare) called only from Player.cpp (0x4d9700,
  0x4d9bd0, 0x4da140); FatalError (0x4d7d80) is called only from Player.cpp.
  Nothing in the gap belongs to Objects.cpp. 0x4d9110 is TMyException.h's
  GetOrigin, kept in Player.obj. The STL/MFC functions got names.tsv rows of
  kind stl; the three Player.cpp functions (0x4d7cd0, 0x4d7cf0, 0x4d7d80) got
  game rows and are left for Player.cpp.
- **Application class name CPlayerApp, FatalError** (low). MFC's wizard names
  the class after the project; the project folder is c:\mud\tibiaplayer and
  the file Player.cpp. FatalError is ours: it shuts the connection down,
  leaves fullscreen, hides the main window, shows a message box and calls
  exit(1), each step in its own try/catch(...).
- **NoObject at 0x719da0 is Objects.cpp's** (low). A 12-byte TObject in .bss
  that is never written and is read by Communication, Inventory and Map as
  the empty object. It lies between Objects.cpp's last global (0x719d9c) and
  Player.cpp's first (0x719db0); the object system is the likelier owner.
  The name is ours.
  Reviewed in #24: confirmed for the behaviour: a .bss datum with no static initialiser in the __xc table
  0x5c002c-0x5c00c4 and no atexit, only read; its owner decides the file, not the code. Owner stays low.
- **Container.h array<T>** (class name low, members high). The four tables
  are 12-byte objects {min, d, entry} with a constructor at line 152
  ("d>0") and operator() at line 178 ("index out of range (original i: ...,
  i: ..., min: ..., d: ...)"); "ObjectProperties->low()" and "->high()"
  name the bounds. Written as operator() returning T&, like vector<T>
  (Container.h already has it so). The destructor `delete[] entry` is
  inferred from ReleaseObjects, which frees entry right before the array.
  Put before vector<T> in Container.h, as in the original line order.
  Reviewed in #24: confirmed: the atexit thunk 0x55a2c0 of Map.obj's array<TMapPoint> (0x5d4c18) deletes
  its entry (0x5d4c20), the same destructor ReleaseObjects inlines.
- **TProperties and field names** (low for the struct name, high for Width,
  Height, Layers, PatternWidth, PatternHeight, PatternDepth, Phases and
  Flags, which are in the assertions). ExactSize, Sprites, Waypoints,
  MaxLength, Brightness, LightColor, ShiftX/Y, Elevation, AutomapColor and
  LensHelp are ours; Waypoints and MaxLength follow the game server.
- **Flag names** (medium for 1-20, low for 22, 23, 26, 27). BANK,
  CUMULATIVE, LIQUIDCONTAINER, LIQUIDPOOL, HANG, LIGHT, SHIFT, HEIGHT,
  AUTOMAP and LENSHELP are in the assertions; their bit numbers come from
  the loader's switch and the accessors. The others follow the game server
  and the usual 7.x .dat flag order. CREATURE (99) is from MapWindow.cpp.
- **Function names** (medium): GetObject*/GetOutfit*/GetEffect*/GetMissile*
  after GetObjectFlag (in the assertions); the tables follow the loader's
  order (objects from 100, outfits, effects, missiles), checked by which
  global each accessor passes in ECX. GetEntry* are static: only this file
  calls them. ReleaseObjects (low), LoadObjects, InitObjects, ExitObjects and
  ReloadObjects (from their callers in Control.cpp: InitControl,
  ExitControl, event 0x45 after patching Tibia.dat).
- **Parameter passing** (low). Entry is a reference ("Entry.Flags"); the
  pattern functions take `TObject &Obj` (the callers pass an address, and
  "GetObjectFlag(Obj,CUMULATIVE)" needs an overload for objects, written as
  an inline in Objects.h); LoadProperties takes pointers. The outfit
  sprite's 5th parameter is called Direction (PatternX = Direction + 1).
  Reviewed in #24: confirmed: a reference and a pointer are passed alike (the callers push an address);
  objectstest compares every function.
- **Obj.Data2 holds the hook flag** (medium): GetHangPattern compares the
  third TObject field with 18 and 19 (HOOKSOUTH, HOOKEAST).
  Reviewed in #24: confirmed: GetHangPattern reads [Obj+8] (0x4d6f0f) and compares it with 0x12 and 0x13.
  High.
- **Source order**: line order. LoadProperties (line 939) is placed by the
  compiler after GetEntrySpriteNumber, and ExitObjects (1023) before
  ReloadObjects (1012). The line gaps 70-91 and 281-303 fit one more accessor
  each (probably of MaxLength, the only field without one); they are not in
  the binary (removed by the linker) and not reconstructed.
- **TReadBinaryFile::readWord returns a 16-bit type in the original** (high
  for the observation). Every caller in Objects.cpp zero-extends AX
  (`movzx`), so the original returns uint16; Script.h declares `int`. The
  value is the same (at most 0xffff). Resolved at the merge: Script.h's
  readWord now returns uint16.
- **InitObjects' catch (TMyException &)**: the binary's handler has a catch
  object, so the original named the reference without using it; it is
  unnamed here because modern MSVC warns (C4101). PROGRAM_ERROR 1 has no
  enumerator (name unknown); common.h is unchanged.
- **TObject stays in external.h** (Status.h uses it); Objects.h includes
  external.h for it.

## Preferences/Script gap (Tibia.cfg loader and saver)

- **0x4ddc00-0x4df53f is Preferences.cpp** (high): the loader, saver and
  0x4df520 use only Preferences data (0x719e70-0x71c44f and 0x5c2558), plus
  the flag 0x71c319 that only they touch; Script.cpp has no globals. They
  follow SetSkillBar (line 850) in the binary and have no line evidence, so
  they end the file in binary order.
- **Names InitPreferences, SavePreferences, ExitPreferences, PreferencesLoaded**
  (medium): InitControl calls the loader where the other modules are
  initialised, ExitControl calls 0x4df520 where the others exit, and the
  Init/Exit pairs of the other files (InitObjects/ExitObjects) are named so.
  The flag is set after loading whether or not that failed, and the saver
  writes only when it is set: "loaded" rather than "save requested".
- **TWriteScriptFile's destructor** (high, corrects the Script.cpp entry):
  the saver calls close() (0x4df660) and then 0x4df6b0, so the destructor is
  only `if (File) fclose(File); File = NULL;`, folded with the binary files'
  close(), not close() itself. Script.h fixed.
- **No yes/no helper**: the comparison with "yes" and the "yes"/"no" choice
  are written out at every use. The binary's code differs per site (the
  buddy's Notify keeps the full strcmp result), which a shared helper would
  not produce.
- **The OpenDialogs duplicate check reads the vector's fields** (`min`,
  `max`, `entry[i]`) directly: the binary loops from min to max-1 and indexes
  entry without the start offset, which operator() would subtract. Written
  that way.
- **Buffer sizes**: the loader's identifier copy is 20 bytes (CopyString
  limit 0x14); the saver's hotkey buffer is written as HOTKEY_LENGTH+1 (the
  `_snprintf` limit 0x101; the stack slot is 264 bytes with padding).

## Player.cpp

- **Member functions of CPlayerApp** (medium): FatalError, HandleProgramError,
  HandleException and ReadErrorFile are written as members. HandleProgramError
  and HandleException read `this` (+0xa0, +0x1c); FatalError gets `this` in
  ECX from both callers but does not use it; ReadErrorFile is called with
  ECX still holding `this` (low: it could be a free function).
  Reviewed in #24: confirmed for the behaviour: ReadErrorFile (0x4da140) writes ECX before any read and
  FatalError (0x4d7d80) only pushes it as a stack slot, so member or free function compile to the same
  behaviour.
- **Names** (low unless noted): Terminating (+0xa0) and RunPatch (+0xa1, Run
  starts patch.exe; set elsewhere through the application pointer),
  GetWindowTitle/GetWindowClassName (the decoded "Tibia   " and
  "TibiaClient"), CountTibiaWindows (the EnumWindows callback at 0x4d9420,
  missed by Ghidra), NumberOfTibiaWindows, LastMousePosition,
  LastMouseCursor. OutputControlErrorInformations (0x445900) and
  CMainFrame::OutputErrorInformations (0x4a8360) are named after Player.cpp's
  catch texts (high).
- **The XOR code tables at 0x5733a4/0x5733c4** are in another object's
  .rdata (the key tables at 0x56dd98/0x56ddb8 are Player's, next to its
  vtable); issue #12 gave them to sysinc.cpp (see "Unknown-owner data").
- **The critical-error text pointer 0x5c249c** is an entry of Player.obj's
  copy of the text table (0x5c22cc-0x5c2517): TextCriticalError in external.h
  (issue #12, see "Unknown-owner data").
- **Source order**: no line evidence for the message map, constructor,
  FatalError, CountTibiaWindows, the error handlers and ReadErrorFile; they
  are in binary order around the decoders (62, 75); InitInstance (255-347),
  Run (383-408) and OnIdle (456) end the file in line order (the compiler put
  OnIdle at 0x4d94c0).
- **The top-level handlers of InitInstance and Run** are written out (not a
  macro): PROGRAM_ERROR shows its code, a TMyException is wrapped ("exception
  occurred"), std::bad_alloc, std::out_of_range, exception, const char* and
  ... become TERRORs, and an outer catch of TMyException writes error.txt.
  Their catch types are from the exception tables.
- **std::exception** stands for VC7.1's global `exception` (the binary's type
  name is .?AVexception@@).
- **Odd behaviour kept**: "host" and "port" on the command line are parsed and
  dropped; InitInstance returns TRUE after an error; the 30-character
  service pack buffer may lack its terminator; error.txt's last dashes have
  no line break; HandleProgramError/HandleException set Terminating only when
  it is already set.
- **Tests**: only the two decoders are pure. Player.cpp is linked into the
  difftest against stubs for MFC and its callees (playertest.cpp).

## ContextMenus/Control gap (0x445900-0x445aeb)

- **Both functions are Control.cpp** (high): 0x445900 prints Control.cpp's
  LastPacketTypes ring (0x5c4510/0x5c4514) and is named by Player.cpp's
  catch text "OutputControlErrorInformations"; the catch blocks of 0x4459b0
  start Control.cpp's assigned range (0x445aec) and name line 93 with
  Width and Height.
- **Resize** (low): our name; MainFrm.cpp calls it with the new client size
  (and with 0,0 after switching to DirectX 9 fullscreen).
- Not differentially tested: OutputControlErrorInformations writes through
  the original's iostream code, and Resize needs the window objects.

## start/Communication gap (0x401000-0x40434f)

- **0x401000 is Communication.cpp's InitCommunication** (high for the file,
  low for the name): it clears PatchData (0x5c4344), which only
  Communication.cpp uses, and InitControl calls it among the other Init
  functions. It is the first function of the image, before the STL code.
- **0x401010-0x404341 is STL** (high): string, stringstream, streambuf,
  locale, ctype, codecvt and num_put code with no game data or game calls
  (checked by scanning every function for game globals, game callees and
  strings); Communication.obj, the first object in link order, is the first
  to instantiate it. Rows of kind stl with generic names std_<address>
  (low): naming each template member was not needed for the reconstruction.

## Status/SurfaceDX gap: Surface.cpp (0x4eaa80-0x4eae6f) and SurfaceDX.cpp

- **A file Surface.cpp** (name low, split high): 0x4eaa80-0x4eae6f has its
  own data (0x71d0c0-0x71d0cc: the main window, curEngine, the available
  engines, the surface object), defines TSurface (vtable 0x56ef78, mostly
  _purecall slots) and creates the engine surfaces; "Surface" sorts between
  Status and SurfaceDX in the link order. Issue #9 revises the data split:
  engine-shared data extends from 0x71d0bc through 0x71d0f4, while private
  SurfaceDX data starts at 0x71d0f8 (see SurfaceDX.cpp below).
- **0x4eae70-0x4ed19f is SurfaceDX.cpp** (high): TSurfaceDX's vtable
  (0x56f038) follows TSurface's, its destructor 0x4eae90 sets it and calls
  ~TSurface, and the DirectX 5 factory 0x4eca00 is what GetSurface calls for
  engine 0. Recorded in file-ranges.tsv; issue #9 reconstructs this object.
- **TSurface moved from external.h to Surface.h** with the binary's shape:
  pure slots where the vtable has _purecall, empty default bodies for +0x04,
  +0x08, +0x64, +0x80, +0x98, a forwarder at +0x50 (to +0x54), TestEngine
  (+0xbc) storing the main window. Slot parameter lists are known only from
  the default bodies' RET n (int placeholders). The data members are offsets
  with placeholder names except Window (+0x08). TSurfaceGL is declared in
  external.h (constructor 0x4fb670, 0x498 bytes) because GetSurface creates
  it with new.
  Reviewed in #24: superseded: issues #9-#11 typed every slot from the engines' code, and their tests call
  them on both sides.
- **Names** (low): GetCurrentEngine, GetSurface, GetAvailableEngines,
  SurfaceInitInstance (after the binary's SurfaceExitInstance),
  SurfaceWindow, AvailableEngines.
- **The removed try block in SurfaceInitInstance**: the unwind map has two
  states around ChoosePixelFormat/DescribePixelFormat without a try-block
  entry; the compiler drops handlers that only guard C functions. The catch
  body is unknown; it is written like the first one (PROGRAM_ERROR 12), low.
  Reviewed in #24: confirmed: our /EHsc compile of SurfaceInitInstance has the binary's 4 unwind states
  and one try block (states 0-0); the second catch (...) is eliminated by our compiler too (C functions
  only), so its guessed body is never emitted. (difftest builds with /EHa, which keeps it; no test reaches
  it.)
- **0x5c2564** (the "engine N" value) is Surface.cpp's CommandLineEngine
  (issue #12, see "Unknown-owner data"): in .data after Status.cpp's 0x5c2560
  and before CacheSurfaceWidth, written by Player.cpp and read here.

## sysinc.cpp and the SurfaceGL/sysinc gap

- **The whole gap 0x4fcb90-0x4fce4f is sysinc.cpp** (high): the functions use
  only sysinc.cpp's data (the clipboard buffer 0x71dbc4/0x71dbc8 that the
  asserted functions grow and fill, the start time 0x71dbcc with its static
  initialiser 0x55a130). 0x4fcd50-0x4fce4f is ATL header code (the code
  page lookup _AtlGetConversionACP with its InterlockedExchange init, AtlThrow
  for E_OUTOFMEMORY, a block-chain free) kept in sysinc.obj: rows of kind
  stl, generic names (low).
- **Names** (low unless noted): FileExists, IsClipboardEmpty, GetErrorText,
  GetErrorHint (medium), InitSysinc, ExitSysinc, ResizeClipboardBuffer,
  GetClipboardText, ClipboardBuffer, ClipboardBufferSize, StartTime. textdata
  and help are from the assertions; textdata is the locked clipboard memory,
  so the parameter of SetClipboardText is Text (the earlier external.h
  declaration had called it textdata).
- **SetClipboardText returns bool** (high: it returns 1/0); the earlier
  declaration was void.
- **0x4fd02c is not a function**: Ghidra split GetClipboardText at the branch
  after OpenClipboard.
- **Odd behaviour kept**: the clipboard buffer comes from new and is
  released with free; removing a CR moves one byte past the data.
- **Tests**: GetErrorText/GetErrorHint run in a separate sysinctest.exe,
  because difftest.exe gives Status.cpp recorders for GetMilliseconds,
  RemoveFile and SetClipboardText. The rest needs the clipboard or the
  original's CRT (srand).

## Help.cpp and the gaps on both sides

- **0x4a2a00-0x4a2b88 is Help.cpp** (high): the functions use only Help.cpp's
  data: 0x5ce520-0x5ce538 next to the EventHelps vector 0x5ce53c (static
  initialiser 0x559e80, right after GUI.cpp's at 0x559e60; GUI.cpp's data ends
  at 0x5ce51c) and the asserted AddEventHelp uses the same count. 0x4a29e0 is
  a compiler-generated deleting destructor of a GUI.cpp class (stl row).
- **The gap after Help.cpp**: 0x4a4440 (GetCurrentHelpText) is Help.cpp's;
  0x4a4460-0x4a4546 use Inventory.cpp's data and are recorded as
  Inventory.cpp, not reconstructed yet.
- **Names** (medium/low): the help-area and handle functions, GetEventHelp,
  PopEventHelp, AddEventHelp (the "hints" are Preferences' KnownHints),
  CheckEventHelpName, CheckEventHelpPosition, GetEventHelpTitle/Text,
  GetCurrentHelpText, the data names. EVENTHELPS = 14 from the assertions.
- **CheckEventHelpPosition** is written as independent tests with the
  binary's hex bounds; the binary merged tests with a common x and returns
  early where no later test can match. Verified on every bound and its
  neighbours on floors 4-9 (difftest).
- **GetHelpText** is one switch; the binary splits it into >1000, ==1000 and
  the 10-23 table. Texts were copied from the decompiled strings and are
  compared byte for byte.
- **The vector<int> destructor** (atexit 0x55a2b0 frees entry) is not in
  Container.h yet; behaviour at process exit only.
  Reviewed in #24: superseded by issue #13: Container.h's vector destructor frees entry, and our
  EventHelps registers it at exit like the binary's atexit thunk 0x55a2b0 (delete of 0x5ce558).

## Inventory.cpp and the Inventory/MainFrm gap

- **Struct and field names** (low unless from the assertions): TContainer
  (IsOpen and Count are in the assertions), Object, Name, Capacity,
  HasParent, Objects; TOffer (OwnOffer.Count and CounterOffer.Count are in
  the assertions) with Name and Objects. CONTAINER_OBJECTS = 36 is our name
  (the binary's limit 0x24); BODY_POSITIONS, OPEN_CONTAINERS and
  OBJECTS_PER_OFFER are from the assertions. Layouts from the offsets
  (container 0x1ec bytes; offer 0x4d4, CounterOffer is 0x4d8 after OwnOffer).
  Reviewed in #24: the padding is confirmed harmless: no instruction or data word refers to
  0x5d116c-0x5d116f (only OwnOffer 0x5d0c98, its Count/Objects 0x5d0cb8-0x5d0cc4, CounterOffer 0x5d1170
  and 0x5d1190-0x5d119c), nothing copies a whole TOffer, and CounterOffer ends at PacketHeader 0x5d1644.
  TOffer is 0x4d4 bytes (Inventory.h said 0x4d8; corrected).
- **Function names** (medium/low): ours, after their callers in
  Communication.cpp; GetContainerObject is the container itself,
  GetContainerContent an object in it.
- **The gap after Help.cpp (0x4a4460-0x4a4546) is Inventory.cpp** (high):
  only Inventory data. Placed first, in binary order.
- **The Inventory/MainFrm gap (0x4a8210-0x4a8646) is MainFrm.cpp** (high):
  CMainFrame members (the key translation reads the frame at +0x200; the
  constructor and OutputErrorInformations are called on the main window by
  Player.cpp). Not reconstructed yet.
- **Callers** now pass TObject (Type, Data1, Data2) where Communication.cpp
  built TCommunicationObject literals for Inventory functions; the layout
  is the same.

## MainFrm.cpp and the MainFrm/MainWindow gap

- **The message map** is decoded from the AFX_MSGMAP_ENTRY table at
  0x56c030 (24 entries, in the binary's order). OnKeyDown/OnSysKeyDown and
  OnKeyUp/OnSysKeyUp share one address each (identical tail calls to
  KeyDown/KeyUp, folded). WM_DISPLAYCHANGE and WM_USER+1 are ON_MESSAGE
  entries; WM_USER+1 is the WSAAsyncGetHostByName reply (OnHostResolved,
  our name).
- **Member layout** (offsets from the reads): +0xd0 the mouse cursor at
  construction, +0xd4 a WINDOWPLACEMENT (0x5239d8/0x5239ef are
  CWnd::Get/SetWindowPlacement; its rcNormalPosition is the +0xf0 rectangle
  OnDestroy saves), +0x100 64 ints cleared and never used, +0x200 the held
  modifiers (KeyFlags). Names ours.
- **PacketHeader (0x5d1644) is MainFrm.cpp's** (medium): the constructor
  clears it and OutputErrorInformations prints it; Network.cpp writes it.
  The other data 0x5d1654-0x5d169c (the frame pointer, cursors, button
  flags, the class name buffer, the wheel remainder) are MainFrm's too.
  Reviewed in #24: confirmed for the behaviour (.bss, no initialiser, no atexit; the constructor clears
  it). The same review found MainFrm.obj's unused TCallback 0x5d1674 (initialiser 0x559f20) missing: added
  as MainFrm.cpp's static NoCallback.
- **Free functions** ExitProgram, CloseProgram, SetDisplayMode, OpenURL and
  UpdateMouseCursor (names low; OpenURL and SetDisplayMode after
  Control.cpp's callback parameter names) use the global MainFrame and are
  passed as callbacks to InitWindow and the main window.
- **Eliminated try blocks**: ExitProgram, AddFrameSize and KeyUp have the
  unwind states of a TBEGIN/TEND but no try entry; written with TBEGIN/TEND.
  Reviewed in #24: confirmed: our /EHsc compile of ExitProgram, AddFrameSize and KeyUp has the binary's 11
  unwind states and no try block, so the guessed catch bodies are never emitted.
- **Control.cpp's RightButtonDown** now takes (x, y, Flags) like the other
  button functions: MainFrm passes them; the reconstruction had declared it
  without parameters.
- **OnMouseWheel's catch** names point.x/point.y as in the binary.
- **The gap after MainFrm.cpp (0x4ac010-0x4ac20d) is MainWindow.cpp**
  (high): a window class destructor and Draw plus 0x4ac090, which deletes
  MainWindow and ModalWindow.
- **Tests**: TranslateKey is compared for all keys, flags and modifiers; the
  rest needs MFC and the window system. MainFrm.cpp is linked into the
  difftest against stubs. (Since issue #37 every function is compared with
  MFC faked at its calls: "Network.cpp and MainFrm.cpp (issue #37)" below.)

## Order of the GUI files

- **GUI.cpp before MainWindow.cpp** (judgement): the backlog says smallest
  first, but MainWindow.cpp and the other window files derive from GUI.cpp's
  TWindow and its subclasses, whose layouts and vtables are only partly known
  in external.h. Reconstructing the base first avoids writing the derived
  classes against guessed layouts.

## Creatures.cpp

Reconstructed by Claude Opus 5.5, November 1, 2026.

- **Leading gap 0x44b740-0x44bde0 is Creatures.cpp** (high). Every function
  uses Creatures data (VIP list 0x5c4560-0x5c67d7, player data
  0x5c67d8-0x5c68a8, creature table 0x5c68b0-0x5cc417) or a TCreature member
  offset, and nothing of Control.cpp (its last global is 0x5c4554). No code
  outside 0x44b740-0x4524df touches 0x5c4558-0x5cc417, so every global is
  static. 0x44bc70 is the line-142 function; its PROGRAM_ERROR catch 0x44bdcd
  was the last gap entry. Nothing in either gap goes to Control.cpp.
- **Trailing gap 0x4524e0-0x45399a is DialogElements.obj** (high for the
  object, low for the split between DialogElements.cpp and a header). Its
  functions use vtables 0x560bb8-0x5611e0 and are called from DialogElements.cpp
  (0x45af40-0x46b950) and later objects, never from Creatures.cpp; 0x453930
  has DialogElements.cpp line-42 catch blocks. The no-EH functions at the
  start of each object (as in Creatures.obj) suggest these are DialogElements.cpp's
  own functions; they are left unnamed for that file's reconstruction, except
  the Container.h instances: vector<int>::operator() 0x452ac0 and the
  vector<int>(..., init) constructor 0x453300 (named before), the vector<int>
  and vector<TDialogText> destructors 0x452c50/0x452c60, and
  vector<TDialogText> 0x452eb0/0x453600 (0x18-byte element: a pointer, then
  4 and the colour at 0x5c1230 and 0; element constructor 0x452c70, not found
  by Ghidra). The element type name is ours (low). The template's destructor
  (`delete[] entry`) is documented in Container.h but not added: adding it
  changes the exit behaviour of Status/Preferences/Talk statics (correct per
  the binary's atexit handlers, but outside this file); open question.
  Reviewed in #24: the open question was settled by issue #13 (vector<T> has its destructor; the statics
  free their entries at exit as the binary's atexit thunks do).
- **Static initialisers** (low, not used as evidence): 0x559df0 constructs an
  8-byte (0,0) object at 0x5c4558, just before Creatures data, and
  0x559e00-0x559e20 three at 0x5cc418-0x5cc42f just after it (0x5cc420 is used
  by Dialogs.cpp). The same constructor (0x496130) builds such objects in
  several objects. Issue #12 settled them: 0x5c4558 is Control.obj's
  NoCallback, 0x5cc418 Creatures.obj's (see "Unknown-owner data").
  Reviewed in #24: confirmed: the __xc table 0x5c002c-0x5c00c4 has one TCallback(NULL, 0) (0x496130, no
  atexit) for ConsoleWindow, ContentWindows, ContextMenus, Control, Creatures, Dialogs, DialogWindow,
  MainFrm (0x5d1674), MainWindow and MapWindow; ours are a static NoCallback in each of those files
  (MainFrm.cpp's added now), without a destructor.
- **Source order** (medium for the rule, low for the gap functions' place).
  The binary order is the line order except that a function calling a
  function defined further down comes after the last of them: lines 278-592,
  InitCreatures (1366, calls ResetCreatures at 1414) and SetPosition (108,
  calls CheckMonsterHint at 573) follow ResetCreatures. The leaf functions
  without exception frame (0x44b740-0x44bc6f) precede line 142, so they are
  in the source before it; they keep the binary order, with SetPosition
  (line 108) put before the three TCreature accessors that come last
  (GetDeltaX, GetDeltaY, GetDrawDirection). IsPlayerInParty reads the
  shield itself instead of calling GetPlayerPartyFlag, which comes after it.
  Reviewed in #24: no behaviour: the order of definitions does not change the code; IsPlayerInParty's
  inlined read is compared by creaturestest.
- **Names from the binary** (high): GetCreature, GetPlayerID,
  GetPlayerPartyFlag (assertions in ContextMenus.cpp/ConsoleWindow.cpp),
  CreatureID (the member catch blocks read `*this`), NumberOfPlayerSteps,
  CurrentPlayerStep, NumberOfBuddies, BUDDIES, SKILLS, Opponent and the
  parameter names. All other function, class (TCreature, TBuddyEntry),
  member and global names are ours (medium where the behaviour is clear,
  low for IsPlayer* state bits, HighlightID, UpdateSkulls, NotifySkull,
  HasPlayerSkull, IsPlayerInParty, GetBuddyListNew, CheckMonsterHint and the
  path flags Exact/Diagonal, whose meaning in the path finder is unclear).
  TCreature keeps the old view's field names (RealX..., DeltaX, Colors...).
  The VIP list accessors by position are GetBuddyList* because Preferences.h
  already has GetBuddyIcon/GetBuddyDescription/GetBuddyNotify by character;
  0x44bb80 is GetBuddyPosition because Preferences.h has FindBuddy(int) (an
  overload named FindBuddy was silently chosen over it by EditBuddy; found
  by the difftest). In names.tsv members are TCreature_Method.
- **Help and map callees keep FUN_ names** (high): FUN_004a32e0 (hint
  numbers 7, 8, 9, 10, 13, 14), FUN_004a3460 (monster name), and the Map.cpp
  functions FUN_004b08c0, FUN_004b08f0, FUN_004b0960, FUN_004b6140,
  FUN_004be130, FUN_004bdf30 and the path finder FUN_004bee00 (new
  declarations in external.h under "added for Creatures.cpp"; the Map.cpp
  agent may rename them).
- **Dropped try blocks** (medium): AddCreature (16 EH states, no try block)
  and UpdateSkulls (11 states, none) are written with TBEGIN/TEND: their
  states match an assertion plus a TEND, and the compiler removes a try block
  whose body cannot throw. Their catch lines and names are unknown; written
  as plain TEND.
  Reviewed in #24: confirmed: our /EHsc compile has the binary's 16 and 11 unwind states and no try block,
  so the guessed catch bodies are never emitted.
- **"[bug0000038]" / "[bug0000289]"** in two assertion texts: written with a
  local TASSERT1_BUG/TASSERT2_BUG macro in Creatures.cpp that appends the
  bug number to the stringized expression (common.h unchanged).
- **Odd behaviour kept** (high, from the instructions): AddCreature
  overwrites slot 149 when no slot matches and keeps a reused slot's name,
  position and battle list links; AddBuddy does not decrement the online
  count when an online entry is updated as offline, so GetNumberOfBuddies
  (online count with offline buddies hidden) can exceed the list length;
  SetPlayerDestination adds the unconfirmed step twice in its second
  comparison; HasPlayerSkull, IsPlayerInParty and GetPlayerPartyFlag do not
  check for a missing player creature; SetPlayerSkill does not check
  SkillNumber; SortBuddy moves an entry past equal ones; CheckMonsterHint
  has no z check.
- **Types** (medium): CreatureID and CharacterID are uint32 where the binary
  compares unsigned (0x40000000); SkullTime is `__time32_t` and the clock
  `_time32` (VC7.1's 32-bit time_t, as in Talk.cpp); GetHealthColor returns
  TRGBColor by value (hidden pointer, `ret 4`). The IsPlayer* state bits are
  declared bool (the binary returns `(State >> n) & 1` in EAX).
  Reviewed in #24: confirmed: every comparison with 0x40000000 is unsigned (0x44b910 sbb, 0x44d8a1,
  0x451fb4, 0x451ff2 jb), UpdateSkulls compares SkullTime signed (0x44c88d jl, 0x44c894 jle: a 32-bit
  time_t), the bits are 0/1 in either type, GetHealthColor ends with ret 4 (creaturestest). High.
- **Switches checked against the jump tables** (high): GetSkillName
  (0x44d6a8, cases 0-6), DoPlayerStep (0x4510c8, step directions 1-8),
  FindPlayerPath (0x4513f0, results -4..-1); CompareBuddies' sort-method
  switch is a sub/dec chain.
- **Call sites fixed** (high): Communication.cpp's ReadCreature called
  0x452320 as a free function; it is TCreature::SetPosition on the creature
  (`mov ecx,esi` at 0x40cb9a). The TCommunicationCreature view's offsets all
  match TCreature (static_asserts now on TCreature); its comments were
  right. Control.cpp passes HideCreature to FUN_004bf620.
- **Difftest** (high): creaturestest.cpp compares every function of the
  file on random operation sequences, with the creature table and VIP list
  compared byte for byte, against fakes for the map, path finder and help
  functions on both sides; assertion-failure paths are not exercised. A test
  that indexes the VIP list must use the full length, see the online-count
  behaviour above.
## Map.cpp

Reconstructed by Claude Opus 5.5 (delegate), November 1, 2026.

- **MainWindow/Map gap (0x4b08a0-0x4b0b10) is Map.cpp** (high). The nine
  functions use only Map.cpp data: the centre position 0x5d16e8-0x5d16f0
  (also written by ShiftMap and InitMap), the automap flag 0x5d16cc (set by
  AddAutomapMark, UpdateAutomap, InitMap), the light 0x5d16c4/c8 and
  0x7152d0-0x7152e4; line 1784 asserts AmbienteActualBrightness (0x5d16c4).
  Map.cpp's data is 0x5d16c0-0x7152e7: its static initialisers 0x559f40,
  0x559f60, 0x559f90 construct 0x5d4c18, 0x5d4c24 and 0x5d4c40, and the next
  one, 0x559fc0, constructs 0x7152e8 (MapWindow). 0x5d16b4 (an 8-byte object
  initialised by 0x559f30 and never used) lies before Map's first variable
  0x5d16c0 and is left to MainWindow.cpp (low). 0x5d16f4 is unused.
  Ghidra missed four functions of the object: 0x4b0880 (TPathNode's inline
  constructor, called only by matrix<TPathNode>'s constructor; it starts the
  object, before the gap), 0x4b0b30 (TAutomapBlock's destructor, from the
  atexit iterator 0x55a2e0), 0x4b0b40 (TMapPoint's constructor, passed to the
  eh vector constructor iterator by array<TMapPoint>) and 0x4b1410
  (TAutomapBlock's constructor, initialiser 0x559f90). 0x4b0b20 is
  matrix<TPathNode>'s destructor (Container.h), called only by FindPath's
  unwind funclet. The gap functions have no line evidence and open the file
  in binary order.
  Reviewed in #24: confirmed: 0x559f30 sits between MainFrm.obj's 0x559f20 and Map.obj's 0x559f40 in the
  __xc table 0x5c002c-0x5c00c4. High.
- **Map/MapWindow gap (0x4bf860-0x4bfb0b) is MapWindow.cpp** (high): the
  destructor, scalar deleting destructor and constructor of the class with
  vtable 0x56cf38, whose constructor 0x4bfa00 owns the catch blocks of
  MapWindow.cpp line 130; 0x4bf8d0/0x4bf8e0 (missed by Ghidra) are its vtable
  slots +0x3c/+0x40 returning 240 and 176; 0x4bf8f0 and 0x4bf930 work on
  member arrays of 18x14 fields. Only names.tsv rows (the class name
  TMapWindow and the member names are ours, low).
- **Container.h matrix<T> and matrix3d<T>** (high for the code, low for the
  class names). Lines 217-253 and 282-328, between array (152-178) and
  vector (353 on); xmin, ymin, zmin, dx, dy, dz, x, y, z are in the
  assertions and messages. matrix3d's error has "[bug0000013]" as its
  expression, matrix's has none. Both have destructors: matrix's out-of-line
  copy is 0x4b0b20, and the atexit thunks 0x55a2c0/0x55a2d0 free the entries
  of the static array and matrix3d.
- **vector<T> gets its destructor** (high): delete[] entry, inlined in the
  atexit thunks of all static vectors (Talk's 0x55a390-0x55a3c0, those of
  Preferences and Status at 0x55a320-0x55a380) and in TAutomapBlock's
  destructor 0x4b0b30, which frees Marks.entry (+0x200a0 = +0x20018 + 0x88).
  Container.h had no destructor; the earlier static vectors now free their
  entries at exit like the original.
- **TObject's default constructor clears it** (medium). TMapPoint's
  constructor 0x4b0b40 zeroes exactly the ten TObjects and nothing else,
  which is what a compiler-generated constructor does when TObject has a
  constructor that clears its fields. Written as default member initializers
  in external.h, so TObject stays an aggregate (brace initialisation still
  works). There is no static initialiser for the TObject globals (NoObject,
  the objects of Status.cpp): the original compiler apparently dropped
  constructors that only store zeros into .bss (low for that explanation).
  Reviewed in #24: confirmed, and the explanation corrected: Inventory.obj does run zeroing initialisers
  for TObjects in .bss (0x559ea0: TContainer.Object inline and Objects[] through the eh vector constructor
  iterator with 0x4a4450, which clears three words; 0x559ee0/0x559f00: OwnOffer/CounterOffer.Objects), so
  the compiler did not drop them; only the plain TObject globals and arrays (NoObject, Body, Status.cpp's)
  have none. Either way the data are zero before main; ours (default member initialisers) have the same
  three Inventory initialisers. High for the constructor.
- **TRGBColor constructors** (medium for TRGBColor(int Color), low for the
  others). 0x425950 (between ConsoleWindow.cpp and ContentWindows.cpp) takes
  ECX and a palette index, stores R, B, G and returns this: a constructor.
  GetAutomapColor calls it on its return slot; SetAmbientLight has it inlined
  (same store order). Defined inline in external.h's TRGBColor (the header
  it belongs to is not reconstructed); a default constructor and a
  three-value constructor (GetAutomapColor's black, and Control.cpp's
  "= {200, 200, 200}") were added so that the type stays usable as before.
  Reviewed in #24: confirmed: the constructors only store their values (the order of independent stores
  cannot show); the default one zeroes, as the binary does (graphics.cpp's entry); maptest and
  graphicstest compare the colours.
- **Colours by value** (medium). GetAmbientColor and GetAutomapColor return
  TRGBColor by value (hidden return pointer, the same ABI as Talk's explicit
  output pointer); the constructor call on the return slot shows a returned
  temporary.
  Reviewed in #24: confirmed: by value with a hidden return pointer is the ABI the binary shows (maptest).
- **Callers switched from TCommunicationObject to TObject** (high): the
  layout is the same (Type, Data, Extra = Type, Data1, Data2, 12 bytes).
  Communication.cpp's map code (ReadField, ReceiveAddObject,
  ReceiveChangeObject, ReceiveDeleteObject, ReceiveMoveCreature) now uses
  TObject; TCommunicationObject remains only for the Inventory.cpp calls, so
  its external.h comment (shared with TCommunicationCreature) was left as it
  is.
- **Names** (low unless noted). From the binary: MAPSIZE_X/Y/Z,
  OBJECTS_PER_POINT, EFFECTS, Point, Effect(s), Block, Coordinate,
  EffectNumber, EffectIteratorField/Count, AmbienteActualBrightness, the
  fields ObjectCount, EffectNumber, Status, Type, Next, Previous, bx, by, bz,
  x, y, and FindPath with its parameters (StartX ... ForceDiagonal, from its
  error message; high). AmbienteBrightness, AmbienteColor and
  AmbienteActualColor follow the binary's "Ambiente" spelling. Ours:
  TMapPoint (the assertions say "Point"), TEffect, TMark, TCoordinate,
  TAutomapBlock, TPathNode, the globals Fields and FieldNumber, and all
  function names. GetObject is called GetMapObject because windows.h defines
  GetObject as a macro. The four GetTop*Object names guess the purpose from
  the flags they test (UNMOVE: move; FORCEUSE, LIQUIDPOOL: use); MapWindow.cpp
  will tell.
- **Static functions**: the functions without callers outside Map.obj
  (SortObjects, InsertObject, GetEffectIteratorNumber, NewEffect,
  DeleteEffect, the six ShiftMap* helpers, GetFieldAutomapInfo, the automap
  file and cache functions, IsAutomapBlocked, GetAutomapSpeed) are static,
  as Objects.cpp's GetEntry* are.
- **Parameter forms** (medium): the GetTop*Object functions take int
  &Position (the catch prints "Position" with the value read through the
  pointer); AddAutomapMark takes TCoordinate &Coordinate ("Coordinate.x");
  DeleteAutomapMark reads only x and y of its first parameter (the callers
  pass an x/y pair and the level separately; written as TCoordinate &, low);
  GetAutomapMarks takes vector<TMark> * and int * (low).
  Reviewed in #24: confirmed: references and pointers are passed alike; DeleteAutomapMark reads only x and
  y either way (maptest).
- **Assertions with bug numbers**: expression texts such as
  "In(rx,0,MAPSIZE_X-1) [bug0000013]" are written with local TASSERTB macros
  that append the tag to the stringised expression. All assertion texts of
  the file were compared with the binary's strings.
- **Try blocks from the exception tables**: InsertObject's try starts after
  its five assertions (states 25-25), although the point lookup precedes it;
  SaveAutomapBlock's starts before its early return and its assertions
  (states 0-38); the others follow the usual pattern. LoadAutomapBlock and
  SaveAutomapBlock have inner try blocks whose catch (TMyException &)
  handlers wrap the error ("failed to write colors" in both, sic, "failed to
  load flags", "failed to write flag count", "failed to write flag" with i)
  and an outer one that reports a damaged file. FindPath has a single try
  with only a TMyException & handler (no TEND trio).
- **Locals that the catch blocks print**: GetAutomapBlock keeps Position in
  bx's stack slot (written with Position declared before TBEGIN); ClearField
  sets i to -1 after the object loop (kept); ProcessEffects,
  ProcessMissileEffects, ExitMap and DeleteAutomapMark declare i before
  TBEGIN so that TEND1(i) can print it (the binary's slot is uninitialised
  before the loop, ours is 0).
  Reviewed in #24: no behaviour outside the error paths: only the value TEND1(i) prints differs, where the
  original prints an uninitialised slot (cannot be reproduced).
- **Exact oddities kept**: AddMissileEffect's duration is 150 ms times
  (int)sqrt(sqrt(dx*dx + dy*dy)) (two fsqrt; dx*dx in double, dy*dy in int);
  only missiles on the centre's level are added; effect, text and missile
  data overlap (union); DeleteObject returns true when 9 objects remain;
  AddObject silently drops an object on a full point while AppendObject
  raises an error; ShiftMapLevel clears levels 5-7 after moving up; an
  automap file without marks is accepted (eof); GetAutomapMarks takes block
  coordinates in parameters named x and y; FindPath's error message has no
  opening parenthesis, and its open-list removal assumes the node is present
  (a missing node is cut after the last one; an empty list would crash);
  IsFieldPassable treats piles of more than two HEIGHT objects as passable.
  Reviewed in #24: the union is confirmed: its offsets are the accessors', maptest compares every effect
  kind after random operations, and MSVC does no type-based alias analysis, so a union cannot change the
  code.
- **Source order**: line order, the gap functions first (binary order);
  GetNextObject between lines 171 and 197 and ClearMap between 913 and 945,
  as in the binary. GetAmbientBrightness (1784) stays in line order near the
  end, apart from the other light functions at the start.
- **callcheck** (45 of 75 functions differ, all explained): the Container.h
  operator() calls (Fields(...), FieldNumber(...), Nodes(...), Marks(...))
  and the TRGBColor constructor are not lexical calls of names; the binary
  inlines GetMapLevel, GetNextObject, AbsoluteToRelative and
  RelativeToAbsolute in some callers; Ghidra split LoadAutomapBlock,
  SaveAutomapBlock and ShiftMapLevel, so their file calls and ClearField are
  counted under 0x4b3242, 0x4b3988 and 0x4bdbe0; the file member calls
  (open, readQuad, ...) are not resolved because several classes have
  methods of these names; AddMissileEffect's _ftol is a cast; the two
  DeleteEffect calls of ProcessEffects share one call site in the binary.
- **Tests** (maptest.cpp): the image's static constructors are called
  explicitly (its initialisers never run); each side works in its own
  directory of generated automap files; missing and damaged files are not
  tested (the original would raise an exception), so deleted marks (at 0,0,
  block 0,0) are never looked up. FindPath reached every result except -4
  (no path).

## graphics.cpp

Reconstructed by Claude Opus 5.5 (delegate; a first delegate's unreviewed
WIP, checked and finished by a second one), November 1, 2026.

- **DialogWindow/graphics gap (0x48b380-0x48b547)** (high for the split).
  DialogWindow.cpp ends with the catch block 0x48b287 (line 297, to 0x48b334).
  0x48b380 and 0x48b3b0 are TRGBColor's operator== and operator!= (`ret 0xc`,
  ECX = this, the other colour by value; operator!= negates the inlined ==):
  out-of-line copies of inline members, kept in graphics.obj because it is
  the first object in link order that calls them (FindVideoMaskedSprite
  0x48e09e, GetVideoPicture 0x48f1df; the only other caller is SurfaceGL.cpp
  0x4fbe47). file-ranges.tsv gives them to graphics.h (low: inferred because
  TRGBColor, TRect and TFont are the types of graphics.cpp's interface; GUI.cpp
  put them in GUI.h until graphics.cpp was merged, and they now are in
  graphics.h, which GUI.h includes). The rest, 0x48b3f0-0x48b54f,
  is graphics.cpp: 0x48b3f0 is TVideoCacheItem's constructor (missed by
  Ghidra; pushed to the eh vector constructor iterator by InitVideoCache at
  0x48db0b); 0x48b430, 0x48b460 and 0x48b4a0 use only graphics.cpp data
  (Fonts, NumberOfFonts, the memory cache and its hash table, the tables).
  GetFontLineHeight is called only from other objects, but its first caller
  (0x41a070) precedes graphics.obj in link order, so it is not an inline copy
  but graphics.cpp's own function. The three have no exception frame and no
  line evidence and open the file in binary order (as in Creatures.cpp).
  Reviewed in #24: no behaviour: the header only decides where the identical inline copies are emitted.
- **Graphics/GUI gap**: not reconstructed (the GUI.cpp delegate settles it).
  No graphics.cpp function was found in it: 0x495480-0x495700 construct
  objects with TFonts whose colour comes from GUI.obj's copy at 0x5c1954, and
  0x496150 clears 0x5ccf98, which only GUI.cpp code uses. graphics.cpp's
  data ends at 0x5ccf97 and its code at 0x49547f (padding after the last
  catch block 0x4953c3).
- **Names** (from the binary, high): FileName, PATH_MAX, SpriteNumber,
  NumberOfSprites, MaskNumber, PictureNumber, NumberOfPictures, TileX, TileY,
  Picture.TilesX/TilesY, Buffer, Position, MemoryCacheSize, VideoCacheSize,
  Item.Type, EMPTY_ITEM, HashValue, HASH_TABLE_SIZE, FontNumber,
  NumberOfFonts, FontName, CharsPerLine, CharWidth, CharHeight, Surface,
  Width, Height, Text, Length, Alignment, LEFT_ALIGN, CENTER_ALIGN,
  RIGHT_ALIGN, Format.Alignment (so TFont's third field is Alignment, renamed
  from Unknown10), MemoryCacheBytes. Ours: every function name (medium where
  the behaviour is clear, low for the cache helpers Touch/Find/Remove/Insert
  and for GetFittingTextLength, GetTextPosition and GetFontLineHeight), the
  types TSprite, TPicture, TFontInfo, TMemoryCacheItem, TVideoCacheItem, the
  other fields and globals, SPRITE_ITEM and PICTURE_ITEM. PrintText* rather
  than DrawText* because windows.h defines DrawText. The values PATH_MAX 1024
  and HASH_TABLE_SIZE 100 are from the checks (`< 0x400`, `% 100`).
- **Surface slots** (medium for the parameters, low for the names): the
  calls give the parameters of TSurface +0x28 WriteImage, +0x2c
  WriteMaskedImage, +0x34 Blit, +0x3c BlitClipped, +0x48 StretchBlit, +0x90
  GetCacheSurfaces and +0x94 GetSurfaceRect; ToSurface and FromSurface are
  SurfaceGL.cpp's assertion names. +0x50 (Unknown50, whose default body calls
  +0x54) takes a surface and a colour key: every caller copies a TRGBColor to
  the stack (`sub esp,0xc`), so it was changed from four ints to
  (int Surface, TRGBColor ColorKey), and +0x54 likewise (same ABI). Only
  engines 1 and 2 (OpenGL and DirectX 9) get the call.
  Reviewed in #24: confirmed: (int, TRGBColor) has the stack layout of the callers' copies; graphicstest
  records the calls on both sides.
  Settled in #29: Blit, BlitClipped and StretchBlit take their source (and StretchBlit its target) as a
  TRect FromRect/ToRect: every call builds a 16-byte block (see "TRect's members (issue #29)").
- **TVideoCacheItem's overlapping fields** (medium): masked sprites keep the
  mask number and four colours at +0x14/+0x18, pictures and font characters
  TileX, TileY and the text colour at +0x14/+0x18/+0x1c (the Find and Get
  functions read them so). Written as unions. Its constructor 0x48b3f0 clears
  the four colours and then the text colour, in that order; written as a
  user constructor with these stores. A C++03 union cannot hold a TRGBColor
  with a constructor, so the original declaration was probably different
  (low); the stores are what the binary shows.
  Reviewed in #24: confirmed: 0x48b3f0 stores +0x18..+0x47 and then +0x1c..+0x27, as ours; graphicstest
  memcmp's every item (0x5c bytes) after each operation, so layout, union bytes and the constructor are
  compared. The original declaration stays low.
- **TRGBColor's default constructor zeroes** (as GUI.cpp found): the binary
  zeroes default-constructed colours, also here (DrawOutfit's local array, the
  `new TSprite[]` and `new TPicture[]` loops, which leave Offset and the tile
  sizes alone). So TSprite and TPicture have no constructors of their own and
  DrawOutfit just declares its array; the WIP's explicit zeroing (written
  against the old non-zeroing TRGBColor()) was removed in the merge with
  GUI.cpp. TFont's third field is Alignment (was Unknown10 in GUI.h).
  Reviewed in #24: confirmed: the SkinProperties constructor test (guitest, byte for byte) shows the
  zeroing; DrawOutfit sets all four colours before using them, so its array's initial value cannot show.
- **Dropped code with exception states left** (medium): FindMemorySprite,
  FindMemoryPicture, the four FindVideo* functions (11 states, no try block),
  GetTextHeight and GetFittingTextLength (21) and GetTextPartWidth (26) are
  written with TBEGIN/TEND whose try blocks the compiler removed (nothing in
  them throws). DrawPicturePart has 26 states and a try at 15-15, that is
  three assertions before TBEGIN, but only the PictureNumber one is in the
  code: it returns early when Width or Height is not positive, and the two
  assertions `Width>0` and `Height>0` after that return (as in
  DrawPicturePartClipped, lines 1192-1193) were removed as always true. Their
  line numbers are unknown.
  Reviewed in #24: confirmed: our /EHsc compile has the binary's unwind states and no try block for the
  eight Find/text functions (11, 21, 26 states), and DrawPicturePart's 26 states with the try at 15-15:
  the two always-true assertions emit nothing in either.
- **Try blocks from the exception tables**: the four file functions have an
  inner try (states 11-12; the TReadBinaryFile is state 12) whose
  catch (TMyException &) throws PROGRAM_ERROR 2 (LoadSprites), 4
  (LoadSprite), 3 (LoadPictures) or 5 (LoadPictureTile); LoadPictureTile's
  tile assertions (lines 289-290) are inside it (states 11-22), so a bad tile
  becomes PROGRAM_ERROR 5 too. DrawMaskedSprite and DrawMaskedSpriteScaled
  have their assertions inside the try (states 0-10). DrawSprite,
  GetFontHeight and the Touch/Remove/Insert functions have no try block. No
  function has a switch (no jump table in 0x48b380-0x49547f).
- **Exact oddities kept**: the files are closed twice (close() and the
  destructor); LoadSprite and LoadPictureTile open the file for every sprite
  or tile and write past the buffer on malformed runs; GetFontLineHeight
  maps numbers below 0 and above NumberOfFonts to font 3 but reads before the
  table for 0; RemoveVideoItem ignores position 0, so a reused slot 0 stays in
  its old hash chain (the video Find functions give up after
  VideoCacheSize+2 steps); FindVideoPicture matches font characters of the
  picture (no colour test) and FindVideoFontChar matches a plain tile for
  colour 0,0,0; GetVideoPicture tests the colour key of Pictures[PictureNumber]
  (the next picture; past the end for the last one) against 1,1,1 and does
  the same in both branches; a blank advances text by the width of character
  index 12 (','); InitFont never looks at column 0 or row 0 and lets a blank
  character take part in the Height maximum with -1; DrawSpriteClipped skips
  negative sprite numbers, the other DrawSprite* only 0; DrawObject's count
  buffer holds three digits; DrawPicturePartClipped fails its assertions on
  an empty part where DrawPicturePart returns.
- **Data not static** (judgement): only SpriteSignature and
  PictureSignature are used by another file (Communication.cpp), but all
  graphics.cpp globals are declared in graphics.h so that graphicstest.cpp
  can compare the caches, hash tables and LRU lists directly (Objects.h
  exports its property tables the same way). They are in .bss order with
  their addresses; 0x5cc75c is unused.
  Reviewed in #24: no behaviour: linkage only.
- **Header structure**: graphics.h holds TRect, TRGBColor (with the two
  operators), DefaultFontColor and TFont, moved from GUI.h, and includes only
  common.h (TObject, a parameter of DrawObject, is forward-declared). GUI.h
  includes graphics.h; external.h includes GUI.h and declares no graphics.cpp
  function; Communication.cpp, Control.cpp and Player.cpp include graphics.h
  (the WIP's `#include "graphics.h"` inside external.h made the two headers
  include each other). GUI.cpp calls DrawPicturePart and
  DrawPicturePartClipped by name now; the latter's clip is a TRect, which the
  binary's DrawSkinClipped builds on the stack (four stores after
  `sub esp,0x10`), so the call passes TRect(ClipX, ClipY, ClipWidth,
  ClipHeight). guitest.cpp keeps its picture recorders: our two functions
  jump to them while TestGUI runs (and are restored after it), as talktest.cpp
  does with the CRT clock.
  Reviewed in #24: confirmed: a 16-byte struct argument has the stack layout of the binary's four stores
  (guitest).
- **Source order**: line order; the three gap functions first, the Find
  functions (no lines) between TouchMemoryItem (466) and RemoveMemoryItem
  (522) and between TouchVideoItem (644) and RemoveVideoItem (762), and
  GetTextPosition (no frame) after GetFittingTextLength (1410), where the
  binary has them. The compiler placed InitFont and InitFonts (348-403) after
  ReloadSprites, GetTextWidth (1373) before ReloadPictures, and DrawObject
  and DrawOutfit (1100, 1137) at the end.
- **callcheck** (10 of 56 functions differ, all explained): the four file
  functions' TReadBinaryFile constructor, open and close calls are not
  resolved (several classes have methods of these names; 0x4df6b0 is both
  close() and the destructor, as in Map.cpp); InitVideoCache's eh vector
  constructor iterator 0x4175e0 is compiler-generated; the TRGBColor
  operators in FindVideoMaskedSprite and GetVideoPicture are not lexical
  calls; the Unknown50 calls (0x4eabb0) are virtual calls in the binary.
- **Tests** (graphicstest.cpp): the image's malloc/free and stdio are
  redirected (PatchScript); both sides load the real Tibia.spr and Tibia.pic,
  and every sprite and every picture tile is decoded on both sides and
  compared. The surface is a recording TSurfaceGL shared by both sides (the
  image's surface pointer 0x71d0cc is set to it); after every operation the
  logs of surface calls (with the tiles written), the memory and video
  caches, hash tables, LRU lists and font tables are compared. Small caches
  make items be replaced all the time, so the slot-0 behaviour is reached.
  Objects and outfits use the real Tibia.dat. Every function is tested
  except the assertion-failure paths; InitGraphics, ReloadSprites and
  ReloadPictures run in the directory of the data files.
## GUI.cpp

Reconstructed by Claude Opus 5.5 (two delegates: the first stopped on a Claude
API rate limit, WIP f6161d4; the second checked all of it, corrected it and
finished it), November 1, 2026.

- **The graphics/GUI gap 0x495480-0x4979ff is GUI.cpp and Container.h**
  (high); nothing in it goes to graphics.cpp. Evidence: GUI.cpp's static
  initialisers are 0x559e30 (Timers 0x5ccfa4, vector<TTimer*>(0,0,2,NULL)
  0x4973f0), 0x559e50 (SkinProperties 0x5ccfc8, constructor 0x496800) and
  0x559e60 (Hotkeys 0x5ce4f0, vector<THotkey>(0,50,10) 0x4976f0, missed by
  Ghidra); its data is 0x5ccf98-0x5ce51c (graphics.cpp's ends at 0x5ccf94,
  Help.cpp's starts at 0x5ce520). The gap functions are the compiler-generated
  constructors of the SkinProperties blocks (called only by 0x496800), the
  TWindow/TElement constructors whose vtables 0x564528/0x5645c8 hold the
  asserted GUI.cpp functions, ~TComponent and the deleting destructors of
  vtables 0x5644a0/0x5645c8, the TTimer getters, TCallback's constructor,
  InitHotkeys (clears 0x5ccf98) and the empty 0x496160. Container.h:
  vector<THotkey>::operator() 0x496570 and the two vector constructors.
  0x4a29e0 (in the Help.cpp range) is TWindow's deleting destructor
  (vtable 0x564528 slot 0); its names.tsv row was renamed from
  GUI_scalar_deleting_dtor.
- **Classes and vtables** (high for the layouts, medium for the class
  names). TComponent (vtable 0x5644a0, 0x24 bytes) is abstract: slots
  +0x4c-+0x68 and +0x78-+0x80 are _purecall (0x512a0d) and pure here.
  TElement (0x5645c8, no children, 0x24 bytes) and TWindow (0x564528, child
  list FirstChild/Next and Capture, 0x2c bytes, six more slots +0x84-+0x9c)
  derive from it; TTimer (0x5646e8, 0x14 bytes) has only its destructor
  virtual. Every slot of the three vtables is declared at its offset and was
  compared with the binary. The inline slots whose only copies are in
  earlier objects (GetNext 0x417680, GetParent, TWindow::GetFirstChild,
  GetCapture, Fit, GetMinimumWidth/Height 0x417a20, the TElement mouse
  answers 0x4259f0/0x425a00/0x41acb0/0x417a30/0x417a40, and
  TComponent::GetFirstChild/GetLastChild, which throw "not possible" at
  GUI.h lines 681/682) are defined in GUI.h and have names.tsv rows.
  TComponent's seven child functions throw "not possible" (lines 993-1049;
  line 1033 has no function, the linker dropped it).
- **FitToChildren and FitToChild take a TRect by value** (high for the
  16-byte block, medium for the type): Fit (0x4176c0) and every caller
  (0x441f4b, 0x442f58, 0x47fb59; FitToChild at 0x4882c0) build the margins
  with sub esp,0x10 and four stores, which MSVC does for a struct argument,
  not for four ints (the WIP had four ints; the stack layout is the same, so
  the difftest could not tell). SetChildArea and SetChildPosition get plain
  pushes at all their call sites (wrong for SetChildArea, see below). The only known 16-byte struct is TRect;
  its x, y, Width, Height are the left, top, right, bottom margins. The
  SkinProperties members passed this way (+0x0a0, Style194+0x50) became
  TRects.
  Settled in #29: TRect's members are x, y, width, height (the margins are Area.width/Area.height now).
  Settled in #27: SetChildArea gets a 16-byte block at all its 136 call sites too and takes TRect Area
  now; SetChildPosition does get three plain pushes.
  Reviewed in #24: confirmed: the 16-byte block is the binary's; which struct type it is cannot change the
  code.
- **Names from the binary**: SkinProperties and its member paths
  (PlayerStates.NumberOfIcons, BuddylistWindow.Logo, Buttons.Texture,
  CheckBox.Item, StatusWindow.HitpointsBar/ManaBar), DrawingList,
  DrawingListLength, NumberOfSkins, Parent, FirstChild, Capture, Component,
  getWidth/getHeight, the parameter and local names in the assertions and
  catch blocks (_OffsetX, _Width, Child, Child1/2, Search, Search1/2,
  Before1/2, LeastX/Y, skin, srcx, srcy, skinoffsetx/y, Surface, elapsed,
  Key, i). OffsetX/OffsetY and Width/Height follow from _OffsetX and _Width.
  Ours (medium where the behaviour is clear, low otherwise): TComponent,
  TElement, TTimer, TCallback, THotkey, TDrawing, TGUIPoint, the
  SkinProperties block types and their DataXXX members, DefaultFontColor and
  every function name (SetChildArea, FitToChild, FitToChildren, ClipChildren,
  PositionChanged, HandleEvent, Send, AddTimer, UpdateTimers, AddHotkey,
  CheckHotkey, DrawSkinClipped, InitSkinProperties, ...). The old external.h
  names GUIStyle and TGUIStyle are replaced by SkinProperties/TSkinProperties.
  PlayerStates' block constructor was inlined, so its boundaries are unknown
  and NumberOfIcons stays the direct member Data490 (low).
  Reviewed in #24: no behaviour: the member keeps its offset (guitest compares SkinProperties byte for
  byte).
- **Graphics types in GUI.h** (judgement): TRect, TRGBColor and TFont belong
  to graphics.cpp's header, but GUI.h needs them complete, so they moved from
  external.h into GUI.h until graphics.cpp is merged. (Done in the merge with graphics.cpp: they are in graphics.h now,
  which GUI.h includes; see "graphics.cpp".) Their default
  constructors zero them (the SkinProperties constructors zero every such
  member); TFont() is font 4 with colour 224,224,224, read from a copy that
  every object constructing a TFont has (GUI 0x5c1954, DialogElements
  0x5c1230, an earlier object 0x5c066c), so DefaultFontColor is a header
  constant. Main's TRGBColor(int Color) palette constructor (Map.cpp) was
  merged into GUI.h's TRGBColor.
  Reviewed in #24: confirmed: the three copies 0x5c1954, 0x5c1230 and 0x5c066c hold 224, 224, 224 (one per
  object, as a header constant gives).
- **The empty function 0x496160** (low for the names): ExitControl calls it
  five times; following InitControl's order backwards they are ExitHotkeys
  (InitHotkeys is called last), ExitCommunication, ExitHelp, ExitCreatures
  and ExitInventory, each an empty function in its own file;
  TComponent::Arrange/PositionChanged and TElement::StopDragging fold into
  the same copy. KeyPress and HandleEvent fold into 0x4eac00.
- **Try blocks from the exception tables** (high): the assertions come before
  TBEGIN where the try starts above state 0 (DrawSkin 35, DrawSkinClipped 15,
  GetPrevious 5, RemoveChild 10, InsertChildAfter 10, SwapChildren 25,
  SetCapture/ReleaseCapture 5, SetChildArea/SetChildPosition/FitToChild 15);
  SetSize, InsertChild and AppendChild have assertions and no try; the seven
  "not possible" functions share one 5-state table without try;
  InitSkinProperties has 11 states and no try block (TBEGIN/TEND around a
  body that cannot throw, as in Creatures.cpp); UpdateTimers' extra state
  is its static LastTime. No function has a switch (no jump tables).
- **Odd behaviour kept** (high): SwapChildren of two children that are not
  neighbours removes them and inserts each after its own predecessor, so
  nothing changes; AppendChild does not clear Next when the list was empty;
  RemoveChild of the capturing child stops its drag and releases the capture
  through the child (with NULL); TWindow::StopDragging does not release the
  capture of the parents; TElement::SetCapture ignores its argument;
  MouseHover and HelpMouseMove ignore the capture, MouseHover goes only to
  the first child under the point; TTimer::Update sends the callback on every
  update, not only at the end; a timer of time 0 is never registered;
  CheckHotkey drops flag 0x200 and folds lower-case letters; RemoveHotkey
  moves the last entry into the gap; ClipChildren compares the right and
  bottom edges with the old position; FitToChildren does nothing without
  children and takes all margins as absolute values.
- **Error texts**: DrawSkinClipped's "some assertions failed (surface: ...)"
  and TComponent::Draw's "dialog origin outside main window (surface: ...)"
  are built with a stringstream as in the binary; the stream pieces were
  compared with the binary's strings (" x: ", " y: ", " i: ", "/", ")").
- **callcheck** (23 of 63 functions differ, all explained): virtual calls
  are lexical names in our source but indirect calls in the decompile
  (RemoveChild, InsertChild, InsertChildAfter, StopDragging, DeleteChild,
  GetParent; Child->MouseMove etc. also resolve to Control.cpp's free
  functions of the same names, GetMinimumWidth/Height to a MapWindow row);
  Timers(i) and Hotkeys(i) are Container.h operator() calls (0x452ac0,
  0x496570); IsExpired is inlined in UpdateTimers; the qualified calls
  TComponent::SetSize and TComponent::Draw are not resolved by the script;
  the stringstream constructor 0x403a10; Ghidra split the six mouse
  functions' child loops (rows *_block, not functions).
- **Difftest** (guitest.cpp, 4,823,996 checks): SkinProperties' constructor
  on 0xcc-filled memory and InitSkinProperties, byte for byte; DrawSkin,
  DrawSkinClipped and TComponent::Draw against recorders of the two
  graphics.cpp picture functions; two mirrored window trees (original
  objects made by the image's constructors, ours) under 30 kinds of random
  operations (each run at least 230 times), comparing trees, captures,
  return values, call logs of probe children and the help state; timers and
  hotkeys with probe receivers. Assertion failures are not exercised.
- **Left for graphics.cpp**: the two picture functions keep their FUN_ names
  (FUN_00490c60, FUN_004910e0); FUN_004910e0's declaration was added to
  external.h in its own block so the graphics.cpp lines stay untouched.

## MapWindow.cpp

Reconstructed by Claude Opus 5.5 (issue #1), November 1, 2026.

- **Ranges** (high). The Map/MapWindow gap 0x4bf860-0x4bf9ff holds the
  out-of-line copies of TMapWindow's inline members (destructor, the two
  minimum sizes, IsFieldVisible, SetLightColor, the deleting destructor):
  they open the object before the constructor (line 130), as the inline
  copies of the other files do. The MapWindow/Network gap 0x4ca0e0-0x4ca2ff
  is Network.cpp (file-ranges.tsv): InitServerList and the setters use only
  Network data (0x7152f0 on), 0x4ca2d0/0x4ca2f0 are TSocket's deleting and
  implicit destructor (a jmp to CAsyncSocket::~CAsyncSocket), 0x4ca2c0 is
  MFC's inline CObject::operator delete (PASCAL, ret 4), called only by
  unwind funclets of new-expressions. MapWindow.obj's data: the TCallback
  0x7152e8 (static initialiser 0x559fc0, TCallback(NULL, 0), never used;
  written as a static of ours, NoCallback) and, in .data before Network's
  0x5c22c4, its copy of the text header's table (the strings up to "Report
  Rule Violation" at 0x5c22ac, then DefaultFontColor's copy 0x5c22b4). Every
  object that includes the text header has such a copy (Control.obj's is
  external.h's 0x5c0c80 table); the two texts MapWindow uses are declared in
  external.h with this object's addresses (TextReceiveHint 0x5c2130,
  TextSelectPlayerToTrade 0x5c2258).
  Reviewed in #24: confirmed: 0x559fc0 builds TCallback(NULL, 0) at 0x7152e8 with no atexit, as our static
  does.
- **Two classes** (high for the layouts, low for the names). TMapWindow
  (vtable 0x56cf38, 0x92d8 bytes) is the map view; TMapFrame (vtable
  0x56d0b8, 0x40 bytes, created by MainWindow.cpp with operator new(0x40))
  holds it at +0x2c, keeps it at 15:11 and draws the border. The names
  TMapWindow (the file's name for the view) and TMapFrame are ours. Every
  vtable slot was compared with TWindow's: TMapWindow overrides SetSize, the
  minimum sizes, Draw, Arrange, LeftButtonDown/Up, RightButtonUp,
  HelpMouseMove and HandleEvent; TMapFrame SetSize, the minimum sizes and
  Arrange. TMapFrame's destructor is the implicit one (deleting destructor
  0x4adc40 in MainWindow.obj, which calls the folded 0x443e10, a jmp to
  ~TWindow).
- **Member layout** (high for offsets and types, low for the names except
  MAP_WIDTH/MAP_HEIGHT): the creature names (150 entries of ID and position),
  the textual effects to draw (200 entries of number, position and field),
  Light[18][14] (TRGBColor: the constructor clears it with the inline
  TRGBColor constructor before the try block), the map position, MaxLevel,
  CoverLevel[18][14] (the highest level with a FULLBANK ground), the scroll
  offset (the player's walk offset + 32), the 16x12 tile cache (TCacheField,
  0x8c bytes) and the hint button.
- **TObject converts to int** (medium). Draw's TEND1(Obj) prints "Obj" with
  the value of Obj.Type, and the assertions "SrcObj!=0", "TradeObject!=0" and
  "Obj!=CREATURE" compare an object with a number while the code tests its
  Type. external.h's TObject got `operator int() const { return Type; }`.
  Reviewed in #24: confirmed: the conversion only enables the binary's TEND1(Obj) and comparisons; every
  TObject user is compiled and differentially tested.
- **Status.cpp's object slots take the index first** (high). LeftButtonDown
  calls SetDragObject(Obj, Position, x, y, z), and the getters' first output
  goes to the Position argument of SendMoveObject, SendLook and the other
  messages. The parameters of Set/GetDragObject, Set/GetTradeObject and
  Set/GetUseObject and the variables behind them were renamed by address
  (DragX at 0x71c5c8 is DragIndex now, and so on); the code is unchanged.
- **TCreature's PKFlag and PARTYFlag** (high): the errors "unknown creature PK
  flag" (Creature->PKFlag, +0x90) and "unknown creature party flag"
  (Creature->PARTYFlag, +0x94) name them; Creatures.h's Skull and Shield were
  renamed (Communication.cpp, Creatures.cpp, creaturestest.cpp).
- **SkinProperties +0x60-+0x9b are five colours** (high for the types, low for
  the names): the values InitSkinProperties stores there are (248,248,248),
  (224,64,64), (64,224,64), (248,164,164), (180,248,180), and DrawField reads
  them as the frames of the creature under the mouse, attacked, followed, and
  attacked or followed under the mouse. GUI.h had a TRGBColor and three TRects
  (same size); InitSkinProperties' stores were rewritten member by member.
- **TDrawing constructors** (medium): `new TDrawing[8]` in TMapFrame's
  constructor zeroes the entries in a loop (an inline default constructor),
  and Arrange assigns whole entries built from seven values.
  Reviewed in #24: confirmed: the binary zeroes the new entries (0x4c1ab4) and the window tests compare
  the drawing lists.
- **Surface slots** (high for the arguments, low for the names): +0x20
  DrawFrameClipped(Surface, TRect, LineWidth, TRGBColor, TRect Clip), +0x30
  BlitRect(ToSurface, x, y, FromSurface, TRect Source), +0x40
  StretchBlitRect(ToSurface, TRect, FromSurface, TRect), +0x4c
  ShadeRectangles(Surface, Count, TRect *, TRGBColor *) with four corner
  colours per rectangle, +0xa4 CanShadeRectangles(). +0x9c(2) returning true
  makes Draw forget the tile cache (the surface lost its contents, low).
  Reviewed in #24: no behaviour: only the explanation of +0x9c(2) is low; the code does what the binary
  does (mapwindowtest).
- **FULLBANK = 30** (low for the name): the Tibia.dat flag after LENSHELP;
  Draw takes the highest level whose first object has it as the field's
  cover level. **EVENTHELPS** moved from Help.cpp to Help.h
  ("In(Hint,1,EVENTHELPS)").
- **Names of Map.cpp's GetTop*Object** (medium now): LeftButtonDown drags the
  "move" object, RightButtonUp makes the "look" object the trade (look) object
  and the "use" object the use object, LeftButtonUp in action state 7 uses
  the use object with the "multi-use" object, and HelpMouseMove shows the
  look object's help.
- **Limit** (low): a clamp between two bounds in either order appears inlined
  about thirty times (with constant bounds only one branch is left); the
  binary has no copy. Written as a static inline function of MapWindow.cpp.
  **TERROR1B** (local macro): the party flag error has "[bug0001106]" as its
  expression, like Map.cpp's TASSERTB.
  Reviewed in #24: confirmed: the inline clamp is compared wherever it is used (window tests).
- **Odd behaviour kept** (high): the light of a graphical effect is added
  with the field's level as its radius and its brightness as its level (the
  arguments are swapped; the missile light has them right); Draw reads
  Light[][] of a creature's field without checking it is on the map;
  HelpMouseMove asks GetObjectFlag(Type, LENSHELP) even when the look object
  is empty (type 0 fails the assertion); CacheSprite and FlushField draw the
  matching sprites of a tile again over it without clearing it; a party
  shield of type 1 is drawn only while the player is in no party; the classic
  control attacks only when the use object is not FORCEUSE; only creatures
  on the player's level get names; HandleEvent(27) without a hint button
  fails the assertion of RemoveChild.
- **Arithmetic**: TMapFrame::SetSize's 15:11 height is floor(width *
  0.73333335f) (0x512b50 is floor: its error path reports operation 11,
  _FpCodeFloor; the product is a float multiplication, fild/fmul m32). x87
  and SSE give the same floor here: the float constant is slightly above
  11/15, so the product is either a whole number plus a tiny excess or at
  least 1/15 away from one. AddLightSource's distance is
  (int)sqrt((double)dx * dx + dy * dy), as in Map.cpp.
- **Try blocks from the exception tables** (high): Draw (0-136) has the inner
  blocks of lines 477, 495, 527 (TEND1(Obj) each, with their own TObject),
  550, 757 (with 599 inside the creature-name loop, one try per iteration),
  776, 815 (with 794 inside the loop) and 830; DrawField (0-76) those of lines
  874, 884, 927, 987 (TEND2(i, Creatures[i]->OutfitType), i declared before
  the try), 1054 (TEND1(EffectNumber)) and 1060. AddLightSource's inner try
  has a single TMyException & handler that wraps the error with a
  stringstream text. The assertions of DrawObject, DrawCreature and
  DrawLightMask come before TBEGIN; GetCacheField and TMapFrame::Arrange have
  no try block. Switches: ArrangeTalkboxes' modes (tables 0x4c12b4/0x4c12c4:
  4; 9, 11, 18, 19; 22), Draw's party and PK flags (0x4ca0bc, 0x4ca0cc).
- **Source order**: line order (TMapWindow 130-1545, TMapFrame 1564-1606, its
  Arrange without lines last, where the binary has it). The binary emits the
  functions in a different order (the asserted helpers after HandleEvent, the
  mouse handlers after TMapFrame, Draw last).
- **SetChildArea** (note only): Arrange passes the hint button's area as one
  16-byte block (sub esp,0x10 and four stores) while GUI.h's SetChildArea
  takes four ints; the stack is the same. GUI.cpp's decision found plain
  pushes elsewhere, so the signature was left.
  Settled in #27: SetChildArea takes a TRect; Arrange passes TRect(Data140.x, Data140.y, Data148.x,
  Data148.y) (the two points may be one TRect, #31).
  Settled in #31: SkinProperties.Data140 is one TRect (+0x140-+0x14f), and Arrange passes it.
- **callcheck** (14 of 26 functions differ, all explained): constructor calls
  (TWindow's, TCallback's, TMapWindow's, TBlinkingButton's, the vector
  constructor iterator of DrawCreature's colours) are not lexical; qualified
  TWindow:: calls are not resolved; virtual calls (Arrange, SetChildArea,
  RightButtonUp, HandleEvent, InsertChild) are indirect in the decompile;
  LeftButtonDown/LeftButtonUp/RightButtonUp and DrawObject also resolve to
  Control.cpp's and graphics.cpp's free functions of these names; the
  compiler merged call sites (SetAttackID/SendAttack/GetAttackID/AddMessage in
  RightButtonUp, SetHelpSize in HelpMouseMove, DrawSkin in Draw); four of the
  eight IsFieldVisible calls of DrawFieldSprite are inlined; floor, _ftol and
  the stream operators are not in names.tsv.
- **Tests** (mapwindowtest.cpp, about 116,000 checks): one fake world for both
  sides (map objects and effects, creatures, talkboxes, preferences, event
  helps, action state; TCreature's GetDeltaX/Y and GetDrawDirection are fakes
  because they read Creatures.cpp's player step), logs of every drawing,
  surface, Communication, Status, Talk, Help and hotkey call, and the windows
  compared byte for byte after each of 24 kinds of random operations
  (constructor, the inline members against their out-of-line copies, the
  cache, sprites, objects, creatures, fields, whole frames with talkboxes,
  the mouse handlers in every action state, HandleEvent, the hint button's
  Arrange and removal, TMapFrame). Every message, send and drawing kind of
  the file was reached (counts in the summary line). Not exercised: the error
  paths and the hint button's creation (TBlinkingButton is
  DialogElements.cpp's). playertest.cpp's TSurfaceGL stubs for the new slots
  moved to mapwindowtest.cpp as recorders.
- **talktest.cpp fixed** (high): TestFilters deleted the filter vector's
  entries itself and vector<T>'s destructor (since Map.cpp) deleted them
  again. The double delete corrupted the heap only with some memory layouts:
  with this file's test added, build.cmd's longer TEMP path made difftest
  end with 0xc0000374 and no output. The test now clears the pointer.

## MainWindow.cpp

Reconstructed by Claude Opus 5.5 (issue #2), November 1, 2026.

- **Range and data** (high). The MainFrm/MainWindow gap 0x4ac010-0x4ac20d is
  MainWindow.cpp: TMainWindow's destructor (vtable 0x56c498), 0x4ac090 (it
  deletes MainWindow 0x5d16b0 and ModalWindow 0x5d16ac: ExitMainWindow,
  called by ExitControl), the deleting destructor and Draw, whose catch
  blocks (line 127) end the range. The object's data is 0x5d16a0-0x5d16bb:
  the three frame callbacks (OpenURL, SetDisplayMode, ExitProgram, written
  only by InitMainWindow), ModalWindow and MainWindow (both names from
  Control.cpp's assertions) and the unused TCallback 0x5d16b4, built by the
  static initialiser 0x559f30 between MainFrm's 0x559f20 (0x5d1674) and
  Map's 0x559f40, like MapWindow.cpp's (written as a static NoCallback;
  medium). The two texts it uses are its copy of the text table (external.h:
  TextNoBugReportRight 0x5c1ffc, TextAccountURL 0x5c2048; names ours).
  Ghidra missed 0x4ae420 (TSplitter::ReleaseCapture, inline: Dragging=false,
  jmp TElement::ReleaseCapture). 0x4adc40, so far TMapFrame's deleting
  destructor, is TStartWindow's (vtable 0x56c5c0 slot 0); TMapFrame's was
  folded into it. TSplitter's deleting destructor is the folded 0x429590
  (ContentWindows.obj; it calls the jmp-to-~TComponent 0x4672c0).
  Reviewed in #24: confirmed: 0x559f30 builds TCallback(NULL, 0) with no atexit, as our static does.
- **Three classes** (high for the layouts, low for the names). TMainWindow
  (0x4c bytes; MapFrame +0x2c, SideBar +0x30, DialogWindow +0x34 (name from
  "DialogWindow!=NULL"), SidePanel +0x38, Console +0x3c, Splitter +0x40,
  StartWindow +0x44, InGame +0x48), TStartWindow (0x30, Frame +0x2c) and
  TSplitter (a TElement, 0x30: Dragging +0x24, DragX +0x28, DragY +0x2c).
  Slots from the vtables: TMainWindow overrides +0x34, +0x44, +0x6c, +0x70;
  TStartWindow +0x34, +0x44, +0x70; TSplitter +0x48-+0x54 and +0x7c.
- **Function names** (ours, low unless the binary gives them): ChangeMode
  (0x4af5e0; its bool parameter is InGame), ResizeConsole (parameter
  ConsoleHeight from the catch block), GetConsoleWindowHeight (not
  GetConsoleHeight: Preferences.cpp has that name and ChangeMode calls it),
  Walk (dx, dy from the catch block), OpenContainer/CloseContainer
  (Container), OpenTrade/UpdateTrade/CloseTrade, OpenContentWindow and
  CloseContentWindow (Identifier, "unknown identifier"), SetConsoleActive,
  InitMainWindow, ExitMainWindow. TRADE_WINDOW (8) and ACTIONSTATE_NONE (0,
  added to Status.h) are named in assertions; the other identifiers stay
  numbers (6 "Skills" and 7 "Battle" from ContentWindows.cpp's titles, 9,
  0x40 plus a container number; 1-5 the side-bar windows).
- **Source order** (medium). Line order. The binary has a second ascending
  run after the splitter (KeyPress 203, HandleEvent 308-339, ChangeMode 463,
  OpenTrade 560, CloseTrade 590, the constructor 85, InitMainWindow 822),
  which the lines put in between. The destructor and ExitMainWindow (no
  lines) follow the constructor, before Draw, where the binary has them;
  GetConsoleWindowHeight sits between 475 and 503 and TSplitter::Arrange
  between 749 and 780, as in the binary.
  Reviewed in #24: no behaviour: the order of definitions does not change the code.
- **No try blocks left** (high). GetConsoleWindowHeight (0x4ac7c0) and
  TSplitter::Arrange (0x4ae430) have exception frames with 11 states and no
  try block, sharing FuncInfo 0x5a9d48: a TBEGIN/TEND whose body cannot
  throw (the compiler dropped the try block and its catch blocks). Written
  with TBEGIN ... TEND; the TEND variant is not visible.
- **Exception tables** (high), checked with ehinfo.py: OpenContainer try
  states 5-7 (TASSERT(DialogWindow!=NULL) before TBEGIN, two new-expressions
  inside), OpenTrade 10-10 (both assertions before TBEGIN), the constructors
  1-1 and TStartWindow's 1-8 (after the base constructor; seven
  new-expressions), ChangeMode 0-13 (thirteen new-expressions), HandleEvent
  0-10 (two assertions), InitMainWindow 0-6, Draw's inner try 1-1 with a
  catch(...) that throws PROGRAM_ERROR 12. Jump tables checked against the
  binary: 0x4ad48c (OpenContentWindow, 6-9), 0x4aecd4 (KeyPress,
  0x10c-0x119), 0x4af424/0x4af458, 0x4af478/0x4af494 and 0x4af55c/0x4af574
  (HandleEvent's three ranges, with byte tables), 0x4afea8 (ChangeMode, 1-5).
- **Odd behaviour kept**: ChangeMode clears every child pointer except
  SideBar (out of the game it points to the deleted frame); the member
  pointers are uninitialised until the constructor's ChangeMode(false), which
  is safe only because FirstChild is NULL; KeyPress ignores every key outside
  connection state 8; HandleEvent and OpenContentWindow use DialogWindow
  without a NULL check (the events that need it come only from the game);
  TSplitter::MouseMove resizes the global MainWindow, not its parent; the
  splitter's DragX/DragY stay uninitialised until the first press; x has no
  name in TSplitter::MouseMove's catch block (TEND1(y)); event 0x21 falls
  through into event 2 (CopyTaintedClipboardText, then SetActionState(0)).
- **Evaluation order** (medium). SetSize reads Splitter->Height before the
  two minimum heights (the load comes first in the binary; written as a
  local). ChangeMode calls Preferences' GetConsoleHeight once (a local, not
  a macro-style max). The quit question of event 0x1fb is a new INT_PTR[4]
  (operator new[] 0x523817; Control.cpp's message 6 deletes it with
  delete[]).
  Reviewed in #24: confirmed: the minimum heights read between are getters that cannot change
  Splitter->Height, GetConsoleHeight has no side effect, the question array comes from operator new[]
  (0x523817); mainwindowtest compares SetSize and ChangeMode.
- **SkinProperties.Style0D8 retyped** (medium). MainWindow.cpp reads
  +0x10-+0x37 (0x5cd0b0-0x5cd0d4) as five x/y pairs, the positions of "Enter
  Game", "Options", "Info", "Exit Game" and "Access Account"
  (InitSkinProperties stores (0,0), (0,60), (0,90), (0,120), (0,30));
  Data00/Data04 are the button panel's size, Data08 its position. The old
  TRGBColor at +0x10 and the points at +0x1c-+0x34 were guesses; GUI.cpp's
  stores are rewritten with the same offsets and values. +0x38-+0x47 is not
  used here; guitest.cpp's constructor check showed it must be zeroed, so it
  is a TRect (a guess: any type with a constructor would do). Data134 (the
  frame border), Data138 (the splitter height) and Data184 (the side bar
  width) got comments.
  Reviewed in #24: confirmed: guitest compares the constructor's and InitSkinProperties' bytes; within the
  zeroed bytes, five points and a TRect are the same code.
- **External sketches** (low). TConsoleWindow now derives from TWindow (0x6c
  bytes; Data34 kept for Control.cpp). New in external.h, for the files that
  are not reconstructed: TDialogWindow (vtable 0x563f80, 0x4c bytes; slot
  +0xa0 takes (16, window) to add one and (32, NULL) to rearrange; 0x486400
  finds a sub-window by identifier, 0x4865b0 stores the open ones; the inline
  hasContentWindow from the assertion text), TSubWindow and TSubWindowBody
  (the name TContentWindow is taken by Preferences.h's stored sizes; getBody
  is +0x44, the body's slots +0xa8 and +0xb8 are used), TBorder (vtable
  0x561358, 0x68 bytes, constructors 0x4547f0 and 0x454e30, parameter
  _Window), TTextButton (0x466bc0, _Text), TLabel (0x465b40), TObjectIcon
  (0x4526c0), TSideWindow3/4/5 and the ContentWindows.cpp creators (FUN_
  names kept). FUN_004362b0 and FUN_00436510 take the main window as their
  argument (push esi; Ghidra dropped it).
  Reviewed in #24: superseded: the sketched classes are the reconstructed ones now (issues #3-#8), tested.
- **callcheck** (16 of 29 functions differ, all explained): the constructor
  calls of new-expressions, base constructors and destructors and TCallback
  temporaries are not lexical calls; TWindow::Draw, TWindow::SetSize and
  TWindow::InsertChild are names.tsv's TWindow_Draw, TComponent_SetSize and
  TWindow_InsertChild; virtual calls (SetChildPosition, ReleaseCapture,
  SetSize, Console->KeyPress, which the tool matches to Control.cpp's
  KeyPress) are indirect in the binary; the compiler merged the calls of
  Walk in KeyPress, of AddMessage, OpenContentWindow and CloseContentWindow
  in HandleEvent and of AddMessage in TStartWindow::HandleEvent;
  hasContentWindow is inlined; InitMainWindow's GetActionState count in the
  pseudocode includes the assertion text.
- **Tests** (mainwindowtest.cpp, about 14,000 checks over 6,000 random
  operations): the windows of the files that are not reconstructed are
  fakes on both sides (the image's constructors and members jump to the same
  code as ours), every other callee is a logging fake (PatchOwn for ours);
  the real map frames get a logging vtable right after construction and lose
  their map view, so nothing reaches the map. Both main windows get the same
  mode changes, sizes, drawing, keys, events, splitter drags, mouse events,
  containers, trade and content windows, start window events and
  ExitMainWindow/InitMainWindow; after each one the logs and a dump of both
  window trees are compared. A mutation check (three changed lines, then
  two) made thousands of checks fail. Not exercised: the error paths. It
  runs after mapwindowtest.cpp and patches for good as well;
  DIFFTEST_ONLY=MainWindow runs it alone. playertest.cpp lost its
  InitMainWindow stub.

## DialogWindow.cpp

Reconstructed by Claude Opus 5.5 (issue #3), November 1, 2026.

- **Range and data** (high). The Dialogs/DialogWindow gap 0x4854b0-0x48582f
  is DialogWindow.cpp: Dialogs.cpp's last catch block ends at 0x4854ad;
  0x4854b0 (missed by Ghidra) is slot +0x3c of TDialogWindow's vtable
  0x563f80, 0x485690 (also missed) slot +0x40 of TSubWindow's 0x563ed8; the
  other gap functions are the constructors and destructors of the three
  classes and 0x4857b0 (line 63). The object's only datum is the unused
  TCallback 0x5cc428 (initialiser 0x559e20, after Dialogs.cpp's 0x5cc420 at
  0x559e10; medium, written as a static NoCallback like MainWindow.cpp's).
  The file ends at 0x48b37f (graphics.h's operator== follows).
  Reviewed in #24: confirmed: 0x559e20 builds TCallback(NULL, 0) with no atexit, as our static does.
- **Three classes** (high for layouts and vtables, low-medium for names).
  TDialogWindow (0x4c bytes, vtable 0x563f80, a new slot +0xa0 Rearrange):
  Area (a TRect at +0x2c: the constructor stores the four ints twice, the
  member constructor and the body), Dragged +0x3c, Busy +0x40, FreeHeight
  +0x44, UpdateFreeHeight +0x48. TContentWindow (0x44 bytes, vtable
  0x563e38): Identifier, BackgroundY, Background, Dragging, DragX, DragY,
  DragBottom (+0x40, set and never read here). TSubWindow (0x70 bytes,
  vtable 0x563ed8, new slots +0xa0 Minimize and +0xa4 Maximize): Body +0x44,
  two TResizers, the up, close and minimize buttons, Icon, Title, IsMinimized
  +0x64, Data68 (cleared, not used here), RestoreHeight +0x6c. The vtables
  end where the next begins (0x563e38+0xa0, 0x563ed8+0xa8). Names: the base
  class is TContentWindow because the binary's inline
  `DialogWindow->hasContentWindow(TRADE_WINDOW)` looks up the column's
  children (so 0x486400 is GetContentWindow), and SetContentWindowSize
  stores their sizes ("ContentWindow" in Tibia.cfg); Preferences.h's stored
  size struct, ours, became TContentWindowSize. The derived class keeps the
  name TSubWindow of MainWindow.cpp's sketch (getBody, Body). Member and
  function names are ours except IsMinimized, Body, _Body, _Identifier,
  Search, Work, DeltaHeight, Strategy and the catch-block parameters. The
  side-bar windows of ContentWindows.cpp (external.h TSideWindow3/4/5)
  derive from TContentWindow (their constructors call 0x4854c0).
- **No inline header code** (medium). The six functions without frames or
  lines at the start of the object (GetMinimumWidth, TContentWindow's
  constructor, ~TSubWindow, GetFrameHeight, TSubWindow::GetMinimumHeight,
  TDialogWindow's constructor) are written in DialogWindow.cpp in the
  binary's order. GetFrameHeight (0x485670) and TContentWindow's constructor
  are called by ContentWindows.obj, earlier in the link order: a header
  copy would have been kept from there, so they are not header code. The
  same is assumed for the others. TSubWindow's constructor inlines the base
  constructor (auto-inlining, as in Utils.cpp).
  Reviewed in #24: no behaviour: inline or out of line, the code is the same (dialogwindowtest).
- **Source order** (medium). Line order. The binary puts Arrange (169),
  Shrink (377-394), Grow (505) and Rearrange (271-297) at the end of the
  object: each calls a function defined further down (GetFreeHeight 524,
  the strategy helpers), the pattern noted for Creatures.cpp.
  Reviewed in #24: no behaviour: the order of definitions does not change the code.
- **Exception tables** (high), checked with ehinfo.py: every function with a
  try block has states 0-0 except Draw of TContentWindow 0-10 (two
  assertions), DragChild, Shrink and TSubWindow::Arrange 0-5 (one each),
  Rearrange 0-15 (two assertions and the TERROR), TSubWindow's constructor
  1-8 (after the base class; an assertion and two new-expressions), Minimize
  and Maximize 5-5 (assertion before TBEGIN). The constructors of
  TDialogWindow and the destructor have frames without try blocks (the base
  class unwinding). TDialogWindow::SetSize's and TContentWindow::ReleaseCapture's
  catch blocks name no parameter (TEND). Jump table checked: Rearrange's
  0x48b338 with the byte table 0x48b354 (Strategy-1, 0-31): 1, 2, 4, 8, 16,
  32, everything else the default ("unknown strategy").
- **Odd behaviour kept**: the free-space background is nine skins with two
  odd sizes (the bottom left skin 28 is as wide as skin 26, the right edge
  31 as wide as skin 135); TSubWindow's right edge starts at the top edge's
  height; FitChildren clears Busy itself; Shrink tests a second
  GetFreeHeight that cannot be positive; Rearrange 4 lays the children out
  twice; Minimize calls the body's +0xa0 before the body NULL check;
  HideUpButton compares IsChild with true; MakeRoom calls Work->SetSize
  before Work is a child (the creators size their windows first); a
  minimized window shorter or taller than its frame makes Shrink fail its
  assertion or loop for ever (the game's Style194.Data4C is the frame height).
- **External sketches** (low). TSubWindowBody (ContentWindows.cpp) gained
  the comments of +0xa0/+0xa4 and the container body's 0x42e4e0 (FUN_ name
  kept: it shows or hides the window's up button); new: TResizer
  (DialogElements.cpp 0x453be0, vtable 0x5612b0, 0x38 bytes, "_ToResize"),
  TSkinButton::SetSkins (0x458b20, "_ButtonUp", "_ButtonDown"); both have
  low-confidence names.tsv rows so that callcheck resolves them.
  Reviewed in #24: superseded: TResizer, TSkinButton and TSubWindowBody are the reconstructed classes now
  (issues #6, #7).
- **MainWindow.cpp** changed only in names and casts: GetContentWindow
  returns a TContentWindow, cast to TSubWindow where the body is used
  (OpenContainer, UpdateTrade) as the original must; the setters and
  StoreOpenDialogs replace the FUN_ names, UnknownA0 is Rearrange.
- **callcheck** (23 of 47 functions differ, all explained): constructors of
  base classes, ~TWindow and new-expressions (TResizer) are not lexical
  calls; the virtual calls (Rearrange, DeleteChild, GetPrevious, GetParent,
  SetSize, Arrange, GetMinimumHeight, ReleaseCapture, SetChildArea,
  SetChildPosition, FitToChild, HandleEvent, Minimize, Maximize,
  AppendChild) are indirect in the binary; IsChild in the destructor is a
  direct call the tool cannot resolve (two classes have that name); the
  compiler merged Rearrange's PositionChildren calls (7 in the source, 4
  call sites). tools/callcheck.py now resolves `Class::method(` calls
  (with TWindow -> TComponent for SetSize) and no longer matches a member
  call `x->Name(` to a free function of that name (Control.cpp's MouseMove,
  LeftButtonDown, ...); MainWindow.cpp and MapWindow.cpp went from 16 and 14
  differences to 14 and 13 with it.
- **Tests** (dialogwindowtest.cpp, about 24,500 checks over 12,000 random
  operations in 8 rounds): two columns of the image's classes and two of
  ours, one with sub-windows and one with plain content windows; bodies,
  buttons, icons, titles and resizers are shared test objects (the image's
  0x453be0, 0x458b20 and 0x42e4e0 jump to the same code), DrawSkin,
  DrawPicturePart, FillRectangle, SetContentWindowSize and AddOpenDialog log
  on both sides; the logs, results and full dumps of both trees are compared
  after every operation; all patches are undone at the end. A mutation check
  (five changes, one at a time) failed each run. mainwindowtest.cpp lost its
  fakes of the three classes: the dialog windows are real on both sides
  (the ContentWindows.cpp creators make the image's or our TSubWindow and
  TContentWindow), the dumps show their members. The shared fakes live in
  dialogwindowtest.cpp with hooks (difftest.h's TDialogFakes).
  DIFFTEST_ONLY=DialogWindow runs the test alone. Not exercised: the error
  paths.

## ContextMenus.cpp

Reconstructed by Claude Opus 5.5 (issue #4), November 1, 2026.

- **Range** (high). The ContentWindows/ContextMenus gap 0x43d6f0-0x43d86b is
  ContextMenus.cpp: 0x43d6f0 (no frame, no line) and its deleting form
  0x43d710 are the skill menu's destructor (they store the vtable 0x55f5f0
  whose constructor 0x4454f0 is line 1442 and clear the skill window's +0x30),
  0x43d730 is THelpBubble::SetSize (catch blocks at 0x43d86c name line 142);
  every caller of the gap's functions is in ContextMenus.cpp. The object ends
  at 0x4458ff (Control.cpp's 0x445900 follows, already settled). No data of
  its own (all texts are string literals).
- **NoCallback** (high for the datum, medium for the form; issue #26). Besides
  the texts, the object has one datum: the unused TCallback 0x5c4508,
  constructed as TCallback(NULL, 0) (0x496130) by the static initialiser
  0x559de0 (no atexit; its only reference is the initialiser). It sits right
  after ContentWindows.obj's 0x5c4500 (initialiser 0x559dd0) and before
  Control.obj's .bss (0x5c4510). Written as a static NoCallback after the
  includes, like DialogWindow.cpp's.
  Reviewed in #24: confirmed: 0x559de0 builds TCallback(NULL, 0) with no atexit, as our static does.
- **Classes** (high for layouts and vtables, low-medium for names).
  THelpBubble (vtable 0x55f8b8, 0x30 bytes) is a TWindow, not a
  TContextMenu as Control.cpp's sketch had it: its constructor calls TWindow's
  directly and Control.cpp places it only in action state 9 (help).
  Control.cpp's FUN_00442110 is THelpBubble::Place. TContextMenu (0x55f730,
  0x34 bytes: EntryWidth, Separator, Empty), TContextMenuEntry (0x55f6a8, 0x74
  bytes: Text[50], Shortcut[20], TCallback) and nine subclasses, sizes from
  Control.cpp's new-expressions and the constructors. All vtables have 40
  slots (no new virtuals). Only the skill menu has a destructor of its own;
  the others use the folded 0x4adc40, the entry the folded 0x429590 (kept in
  ContentWindows.obj, names.tsv kind stl). Class names are Control.cpp's
  sketches (ours) plus TContextMenuEntry; member names from the assertions
  (LookedObject, LookedObjectX, UsedObjectX, UsedObjectY, LastEntry) and the
  catch-block parameters (_CharacterID, _TextWindow, _LineNumber,
  _ConsoleWindow, _Edit, _Data, _SkillsWindow); the rest is ours. The menus'
  last two constructor parameters (MainWindow's width and height) are named
  ParentWidth/ParentHeight (ours).
- **Inline header functions** (medium). THelpBubble's GetMinimumWidth and
  GetMinimumHeight (0x4420f0, 0x442100, missed by Ghidra) sit right after its
  constructor, TContextMenu::HandleEvent (0x43e9f0, missed as well; it only
  calls SetActionState(0)) right after TContextMenu's: inline virtuals are
  emitted with the vtable, which comes with the constructor. They are written
  in ContextMenus.h. The two Arrange functions (no frame, no line) are in the
  .cpp where the binary has them (after SetSize and after TContextMenu's
  constructor), ~TSkillContextMenu at the top of the file.
  Reviewed in #24: no behaviour: inline or out of line, the code is the same (contextmenustest).
- **Source order** (medium). Line order. The binary moves the help bubble's
  constructor (106) and Place (257), AddEntry (506) and Show (486) behind the
  event handlers, followed by all menu constructors: each calls a function
  defined further down (Creatures.cpp's pattern).
  Reviewed in #24: no behaviour: the order of definitions does not change the code.
- **Exception tables and switches** (high), checked with ehinfo.py: the
  assertions before TBEGIN in the help bubble's constructor (6-8), AddEntry
  (15-16), the text menu (11-11: 1056 and 1057), the request, edit, mark and
  skill menus (6-6) and the channel menu (6-11, its second assertion inside);
  the constructors' try blocks start at state 1 (the base class). Place has
  states 0-30: six assertions, the second of the left-corner case (line 263,
  newY, the same register as newX) was removed by the compiler, which also
  skips 267/268 on that path; written as in the source. KeyPress's catch
  prints Key as the constant 27 (constant propagation). Jump tables checked:
  0x43fd98/0x43fdfc (object events), 0x440350/0x44038c (creature),
  0x4408d8 (VIP, inside a compare tree), 0x44163c (edit), 0x4419ec (mark),
  0x441d34/0x441d48 (skill), 0x443940 and 0x443df8 (party flags 0-4).
- **Event numbers** (high for the values): written in decimal (10001 Look,
  ..., 10100 message, 10200 paste, 10300 VIP, 10400 marks, 10500 skill bars);
  the ranges are clearly decimal. No enumerator names are known, so none are
  invented. The case order follows the binary's code order.
- **Odd behaviour kept**: Show adds an empty entry to a menu without entries
  after the layout loop (it keeps its own width and 0,0); every Show shrinks
  the last entry again; the rotate entry tests the flags of the used object
  even without one; "Invite to Party" is formatted with an unused name
  argument (left out in our sprintf: the compiler warns about it, the output
  is the same); the text and request menus test the addressee array against
  NULL; THelpBubble::SetSize keeps the larger of minimum and maximum when the
  minimum is above it; the help bubble's frame uses skins 52-60, the menu's
  207-215 with the middle 207; the channel menu leaves ShowServerMessages
  unset for the default channel; event 10402 (walk to a mark) has a case but
  no entry; unknown events do nothing in the text and request menus and are
  errors elsewhere. Callees whose names do not fit their use here are kept
  (Talk.cpp's HasUnreadConsoleText decides "Invite to private chat", i.e.
  whether the player has a private channel; worth a review there).
- **Text buffers** (low). Char buffers of the menus are 50 bytes (the entry
  text limit), 200 for the ignore message (_snprintf's limit) and 220 for the
  report title; the binary's frames are not exact enough to give more.
  Reviewed in #24: no behaviour: the buffers receive sprintf of a name (at most 29 characters) and at most
  11 fixed ones, or _snprintf with the binary's limit 200, so a size at or above that cannot show.
- **Data types changed elsewhere**: SkinProperties.Data160/Data168 (two
  TGUIPoints) are one TRect Data160 (the help bubble's text margins, passed
  by value as one 16-byte block like Data0A0); GUI.cpp's InitSkinProperties
  writes .Width/.Height, byte-identical. Map.cpp's DeleteAutomapMark takes
  TMark &Deleted (its only caller passes the menu's mark; it was a
  low-confidence TCoordinate &). New external.h sketches: TTextWindow (the
  console's text view with TextBuffer at +0x24 and four FUN_ methods),
  TConsoleWindow's five FUN_ methods used here, TSkillsWindowBody (ContextMenu
  at +0x30), TEditLine (+0x5c, +0x60, +0x66), TFormattedText (0x465e60),
  TSeparator (0x452cb0); TConsoleData34::Data2C is a TTextWindow *. All
  names low confidence. external.h no longer includes a ContextMenus header:
  ContextMenus.h needs Map.h, which includes external.h (Control.cpp includes
  it directly).
  Reviewed in #24: confirmed for the TRect (the same 16-byte block); the sketches are the reconstructed
  classes now.
- **callcheck** (24 of 36 functions differ, all explained): base class
  constructors and ~TWindow, new-expressions (entries, separators, the
  formatted text, TTalkLine) and member initialisers (TCallback) are not
  lexical calls; virtual calls (GetMinimumWidth/Height, Arrange, SetChildArea,
  FitToChildren, SetSize, the TTalkBuffer queries and Clear) are indirect in
  the binary or resolved to the own class by the tool; the compiler merged
  call sites (AddEntry in the constructors, SendAttack, AddMessage,
  CopyToClipboard); the text menu's assertion strings contain
  "GetCreature(GetPlayerID())".
- **Tests** (contextmenustest.cpp, 32,980 checks over 3,000 windows):
  every window kind built from the image's and our constructors in memory
  with the same contents, then random draws, clicks, right clicks, keys,
  events, Arrange, SetSize/Place/FitsIn (help bubble) and Show/AddEntry/
  AddSeparator; logs of every callee (logging fakes on both sides answering
  from a random world) and dumps of both windows compared after each step,
  then the destructors. A mutation check (six changes, one at a time) failed
  each run. DIFFTEST_ONLY=ContextMenus runs it alone; it restores every patch.
  Not exercised: the error paths, and inputs the original fails on as well
  (a second Show that shrinks the last entry below 1 pixel, a separator as
  the last child).

## ConsoleWindow.cpp

Reconstructed by Claude Opus 5.5 (issue #5), November 1, 2026.

- **Range** (high for the first gap, medium for the second). The
  Communication/ConsoleWindow gap 0x4174d0-0x41864c is ConsoleWindow.obj:
  Communication.cpp's last catch block ends at 0x4174b6; 0x4174d0-0x41851f is
  one run of out-of-line inline copies (common.h's In, graphics.h's
  TRGBColor(), GUI.h's TDrawing() and the TComponent/TElement/TWindow slots
  of the console's five vtables, which Communication.obj does not use;
  DialogElements.h's TEditLine::HasSelection 0x417520; ConsoleWindow.h's
  TTextWindow::HasSelection 0x417590, the inline destructors of TTabBar and
  TTextWindow with their deleting forms, TLineFormat's constructor (lines
  47-54 of ConsoleWindow.h) and Container.h's vector<TLineFormat>), then
  TTextWindow's constructor 0x418520 and MouseMove 0x4185b0 (line 165, whose
  PROGRAM_ERROR catch 0x418639 was the gap's last entry). In and TRGBColor()
  are assigned with the run (low: nothing ties them to ConsoleWindow.obj
  except their place). The ConsoleWindow/ContentWindows gap 0x425920-0x425e6f
  is ContentWindows.obj: ConsoleWindow.cpp's last catch block ends at
  0x425918; 0x425a10/0x425a40 are slots of a ContentWindows vtable,
  0x425a70/0x425ad0/0x425e00 destructors of ContentWindows classes (vtables
  0x55e710, 0x55e7c8, 0x55e880, whose constructors are ContentWindows.cpp's)
  with their deleting forms; the inline copies before them (the clamp 0x425920
  called only by DialogElements.cpp, TRGBColor(int) 0x425950, TElement's
  MouseMove 0x4259f0 and the folded true-returning slots 0x425a00) belong to
  the same run, and ConsoleWindow.obj needs no TElement::MouseMove (both its
  TElement classes override it). 0x425b10 is vector<TMark>'s destructor
  (names.tsv). The ContentWindows functions keep their FUN_ names for its
  issue. ConsoleWindow.obj's vtables are 0x55dcd8-0x55e1a0; its data are its
  copy of DefaultFontColor (0x5c066c) and of the text table
  (0x5c044c-0x5c0664, external.h; names ours). tools/triage.py now lets
  header evidence win over file-ranges.tsv as assertion evidence does (the
  three header functions of the first gap stay header code; four Container.h
  functions of older ranges changed from "range" to "header", same file).
  Reviewed in #24: no behaviour: which object keeps the identical copies does not change the code.
- **NoCallback** (high for the datum, medium for the form; issue #26). The
  object's only .bss datum is the unused TCallback 0x5c4390, constructed as
  TCallback(NULL, 0) (0x496130) by the static initialiser 0x559dc0 (between
  Communication.obj's 0x559da0 and ContentWindows.obj's 0x559dd0; no atexit;
  its only reference is the initialiser). It follows Communication.obj's .bss
  (0x5c4340-0x5c438f) and comes before ContentWindows.obj's (0x5c4398).
  Written as a static NoCallback after the includes, like DialogWindow.cpp's.
  Reviewed in #24: confirmed: 0x559dc0 builds TCallback(NULL, 0) with no atexit, as our static does.
- **Classes** (high for layouts and vtables, low-medium for names).
  TConsoleWindow (0x6c bytes, vtable 0x55e1a0; Data30 is never used),
  TTabBar (0x40, 0x55dcd8, a new slot +0xa0 Rearrange), TTab (0x58,
  0x55e098), TConsole (0x38, 0x55dfc0; the implicit destructor, its deleting
  form folded into 0x4adc40), TTextWindow (0x94, 0x55dd80, new slots +0x84
  FormatLines and +0x88 GetPosition) and the struct TLineFormat (0x28). The
  class names are ours: the variables CurrentConsole, CurrentTab, DefaultTab
  and NewTab gave TConsole and TTab; TTextWindow keeps the name of
  ContextMenus.cpp's sketch ("console text window" in TConsole::Arrange's
  error message). Member names from the binary: LineText, InvisibleLines,
  Input, TextBuffer (and getFirstLineNumber, getLastLineNumber,
  getTextBuffer, getCaption); the rest are ours. Ghidra missed four slots:
  0x417700 (GetMinimumHeight of TTab and TTabBar, folded: the tab height),
  0x417a50 and 0x41acc0 (the ReleaseCapture of TTextWindow and TTab: they
  clear Selecting/Dragging first) and 0x41de70
  (TConsoleWindow::GetMinimumHeight); they and the destructors of TTabBar and
  TTextWindow are written inline in ConsoleWindow.h, TTextWindow's
  constructor (no line, no try block) at the top of the .cpp, where the
  binary has it (DialogWindow.cpp's precedent).
- **Function names** (ours, low-medium unless the binary gives them):
  SetTextBuffer, FormatLines, GetTalkLine, GetLineFormat,
  GetSelectionStart/End, SelectAll, GetSelectedText, GetPosition,
  HasSelection; TTab's SetActive (parameter _Active from the catch block);
  TTabBar's SetCurrentTab, GetContainer ("failed to get composite",
  "Container"), FindTab, ShowTab, StoreOpenChannels, Rearrange;
  TConsoleWindow's SetActive (MainWindow.cpp's SetConsoleActive,
  "Active?1:0"), UpdateButtons, OpenDefaultChannel, OpenChannel (RecvMode,
  SendMode from the catch block), AddChannel, CloseChannel,
  ReportRuleViolation (Question), Get/SetServerMessages, GetTextBuffer,
  SendInput, AddToHistory, ParseInput, SaveChannel. Events are written in
  decimal (201-213, 403), AddMessage's numbers in hex.
- **Source order** (medium). Line order. The binary moves TTextWindow's Draw
  (84), Arrange (148), LeftButtonDoubleClick, RightButtonUp and
  HandleEvent, TConsole::Arrange, TTab's LeftButtonDown and RightButtonUp,
  TTabBar's constructor, SetCurrentTab, DeleteChild and RemoveChild and most
  of TConsoleWindow behind the rest: each calls a function defined further
  down (Creatures.cpp's pattern).
  Reviewed in #24: no behaviour: the order of definitions does not change the code.
- **Exception tables and switches** (high), checked with ehinfo.py (every
  catch is the standard trio): no try block in TLineFormat's constructor (25
  states, five assertions), SetCurrentLineNumber, GetServerMessages and
  GetTextBuffer (0x4193d0 and 0x41f7f0 share one FuncInfo) and TTextWindow's
  constructor (the base class only); assertions before TBEGIN in TTab's
  constructor (11-11) and Draw (10-10), SetCurrentTab (5-10),
  SetServerMessages (10-10) and AddChannel (5-11); the constructors' try
  blocks start at state 1. TTextWindow::Arrange has five states without code
  (0-5): the folded assertion of an inlined SetCurrentLineNumber(-1), written
  as that call (medium; the binary stores CurrentLineNumber directly).
  TConsole::Arrange's error builds a std::stringstream (states 0-7). Jump
  tables checked: ParseInput 0x420048/0x420070 ('A'-'Y'),
  TTextWindow::HandleEvent 0x421430 (16-20), SendInput 0x4241c8/0x4241e4
  (talk modes 0-15; 6 and 12 go to the default), KeyPress 0x424ad4
  (0x100-0x10b), TConsoleWindow::HandleEvent 0x424f68/0x424fa0 (201-403;
  209 is the default).
  Reviewed in #24: confirmed: the inlined call stores CurrentLineNumber as the binary does
  (consolewindowtest).
- **Odd behaviour kept**: FormatLines does not reset LineEnd between the
  parts of a talk line and loops for ever in a width without room for a
  character; TTextWindow::Arrange loops for ever on a line without a format;
  a buffer whose lines were removed is neither drawn nor formatted by Draw
  until the next Arrange (its LinesChanged is dropped); GetSelectedText does
  not test a missing format in a one-line selection; GetPosition asks for the
  bottom line's height twice; LeftButtonDoubleClick tests the character
  before the length; TTabBar's constructor runs Arrange before Busy is set
  and never sets Changed; a dragged tab can be pushed out of the bar by a
  later layout (the next move then fails its assertion); TConsole::Arrange
  places the scroll bar with the skin 173 width added and taken away;
  TTab::~TTab leaves the player's own private channel only when the server
  closed it; ReportRuleViolation shows an open request without event 201;
  AddChannel asserts NewTab after using it; the ignore button's skin shows
  the talk throttle; SendInput cuts the addressee inside the edit line's
  text after clearing the line; ParseInput's "unknown message separator"
  cannot happen.
- **Names changed elsewhere** (binary evidence): Talk.h's
  TTalkBuffer::IsDefault is isDefault (the assertions
  "CurrentConsole->getTextBuffer()->isDefault()==false", lines 2061 and
  2072); Talk.cpp's HasUnreadConsoleText (0x502880) is HasOwnPrivateChannel:
  SendInput sends "#I"/"#X" invitations only when it is true, and the
  function looks for a selected private channel (the one Control.cpp's
  message 0x31 marks as the player's); ContextMenus.cpp and the tests are
  renamed. Talk.h gained the inline TTalkBuffer::GetFirst/GetLast (cursor =
  first/last, returned; FormatLines and AddToHistory have the pattern
  inlined).
- **SkinProperties.Style9E8 retyped** (high for the offsets; the types are
  guesses, its constructor 0x495b70 zeroes +0x04-+0xa7): five TRects (the tab
  bar, an unused one, the edit line, an unused one, the current channel),
  seven TGUIPoints (the button positions), a TRect (the text window in
  TConsole), a TRGBColor (the selection) and the font; GUI.cpp's stores are
  rewritten with the same values and offsets (guitest.cpp compares them byte
  by byte).
  Reviewed in #24: confirmed: guitest compares the constructor's and InitSkinProperties' bytes.
- **DialogElements.cpp sketches** (external.h, low): TEditLine is a TWindow
  (vtable 0x560e98, 0x68 bytes, constructor 0x467b50 with an area, 255 and
  0; SetActive 0x45bc80; the inline HasSelection; slots +0xa0 SetText, +0xa4
  Clear, +0xb8 SetReadOnly, +0xc0 SetFont; Text at +0x2c); TSkinButton got
  its constructor 0x466860 and members; new: TToggleButton (0x4671e0),
  TMultiStateButton (0x4672d0, a varargs constructor: __cdecl with this
  pushed first; Reset 0x452790), TScrollBar (0x469a80) with its
  TScrollBarRange (SetRange 0x460450, Value at +0x30). All have
  low-confidence names.tsv rows.
  Reviewed in #24: superseded: DialogElements.h (issue #7); TScrollBar's last parameter is ButtonRange, 1
  at ConsoleWindow.cpp's call.
- **Callers** changed in names only: MainWindow.cpp (SetActive,
  OpenDefaultChannel), Control.cpp (OpenChannel with the name and caption as
  strings, AddChannel, ReportRuleViolation, CurrentConsole->TextWindow),
  ContextMenus.cpp (GetTalkLine, HasSelection (inline now; the binary calls
  its out-of-line copy), GetSelectedText, SelectAll, the console's methods,
  CurrentConsole->TextWindow); each call checked against decomp/clean.
- **callcheck** (55 of 76 functions differ, all explained): base class
  constructors and destructors, new-expressions (TTab, TTextWindow, the
  DialogElements objects, TTalkBuffer, TTalkLine), TLineFormat and TCallback
  temporaries are not lexical calls; Container.h's operator() (Lines(i)) is
  not seen; virtual calls (GetPosition, ReleaseCapture, Arrange,
  FormatLines, Rearrange, the TTabBar child functions, SetChildPosition,
  SetChildArea, SetSize, GetParent, GetPrevious, HandleEvent and the
  TTalkBuffer inline virtuals) are indirect in the binary; ambiguous member
  names (SetActive, InsertChild, IsChild, SetSize, GetNext, Clear,
  GetTextBuffer) are not resolved or resolved to another class; HasSelection
  is inlined in the selection functions and SetCurrentLineNumber in
  TTextWindow::Arrange; the compiler merged call sites (GetTextPartWidth in
  Draw, SendLeaveChannel in ~TTab, AddMessage and SetCurrentTab in
  HandleEvent); the pseudocode of SendInput and ReportRuleViolation counts
  the assertion texts "GetCreature(GetPlayerID())". (A comment line starting
  with "// 0x" splits a function for the tool; ConsoleWindow.cpp avoids
  them.)
- **Tests** (consolewindowtest.cpp, 30,928 checks over 300 rounds of 20-60
  random operations): two consoles from the image's and our constructors in
  memory with the same contents, sized and given the default channel; then
  sizes, drawing, keys (typing, sending every kind of command, the selection
  keys, hotkeys, the history), events, opening, adding and closing channels,
  the report, the server message switch, saving a channel (the files are
  compared), mouse presses, drags, double and right clicks and the wheel in
  the tab bar and the text, the tab bar's events, the fake buttons'
  callbacks and Talk.cpp changes; logs and dumps of both trees (every member,
  the formatted lines with their talk lines, the hidden tabs) compared after
  each step, then the destructors. The DialogElements classes are fakes on
  both sides (also the definitions that contextmenustest.cpp and
  dialogwindowtest.cpp link); the text buffers are test subclasses of
  TTalkBuffer, one set per side with the same lines; the image's clock,
  localtime, ctime and fprintf and our CRT's _time32 are patched; DrawSkin is
  real on both sides (ours is inlined into TComponent::Draw) and the picture
  parts are logged. A mutation check (eight changes, one at a time) failed
  each run. DIFFTEST_ONLY=ConsoleWindow runs it alone; it restores every
  patch. mainwindowtest.cpp keeps a fake console on both sides: the image's
  constructor and ours (reached through its decorated name with
  /alternatename) build a TWindow of the same size, SetActive and
  OpenDefaultChannel log. contextmenustest.cpp's console methods are fastcall
  fakes of the real methods on both sides (TTextWindow::HasSelection runs on
  the test's selection). Lesson: two test files must not define classes of
  the same name (the console test's first buffer class got
  contextmenustest.cpp's TTestBuffer vtable: the linker keeps one COMDAT).
  Not exercised: the error paths and inputs the original fails on as well (a
  width without room for a character, a dragged tab pushed out of the bar,
  a talk mode without its addressee or channel).

## ContentWindows.cpp

Reconstructed by Claude Opus 5.5 (issue #6), November 1, 2026.

- **Range** (high). ContentWindows.obj is 0x425920-0x43d6ef: the
  ConsoleWindow/ContentWindows gap 0x425920-0x425e6f (settled in issue #5; its
  file-ranges.tsv note now names every function), the assertion and between
  functions up to CreateBuddylistWindow (0x43d390, line 280) and its catch
  blocks; ContextMenus.cpp follows at 0x43d6f0. No header code at the end. The
  run of inline copies at the start: common.h's Limit (0x425920, see below),
  graphics.h's TRGBColor(int), Map.h's TMark() (0x4259a0, missed by Ghidra:
  the constructor the eh vector constructor iterator of vector<TMark>'s
  constructor 0x426170 takes), ContentWindows.h's inline virtuals (0x4259b0
  TTradeWindowBody::Draw, 0x4259c0-0x4259e0 and 0x425b00 TSubWindowBody,
  0x425a10/0x425a40 TContainerWindowBody, all missed by Ghidra and found
  through the vtables), GUI.h's TElement slots, Container.h's vector<TMark>,
  the inline destructors of TBattleWindowBody, TBuddylistWindowBody and
  TCombatControls with their deleting forms, Container.h's
  vector<TSkinButton *>(min, max, block, init) (0x425e70, TRadioButtonGroup's
  inline constructor) and vector<TMark>(min, max, block). 0x434000 (missed by
  Ghidra, no frame) is TTradeWindowBody::GetContentHeight, written in the
  .cpp where the binary has it (between the destructor, line 3048, and
  SetSize, line 3066). 0x427697 is a Ghidra split of Scroll's jump table
  (names.tsv row).
- **Data** (high). .bss 0x5c4398-0x5c4507: the automap's mouse-over buffer
  (char[100]), the skill window's (char[256], "You have %d percent to go")
  and the object's NoCallback (0x5c4500, static initialiser 0x559dd0 between
  ConsoleWindow.obj's 0x559dc0 for 0x5c4390 and ContextMenus.obj's 0x559de0
  for 0x5c4508), which TTradeButtons' constructor copies into its Callback.
  The other two are unused and not in ConsoleWindow.cpp and ContextMenus.cpp
  yet (issue #26). ContentWindows.obj's copy of the text table is 0x5c0678-0x5c08c3
  (external.h, names ours; the regular expression pointer 0x5c08c4 and the
  copy of DefaultFontColor 0x5c08c8 close it, as in ConsoleWindow.obj's copy
  0x5c041c-0x5c0677), ContextMenus.obj's starts at 0x5c08d4.
- **Classes** (high for layouts and vtables, low-medium for names). Nineteen
  classes, sizes from the new-expressions and constructors, vtables from
  vtscan.py: TButtonsWindow (identifier 5, 0x55e968, 0x58), TAutomapWindow (3,
  0x55ea08, 0x80) with TAutomapView (0x55e688, 0x24, a new slot +0x84 GetMark
  returning a TMark by value) and TCompassRose (0x55eaf0, 0x28),
  TCapacityLabel (0x55e578) and TSoulLabel (0x55e600), TPlayerStates
  (0x55eb98, 0x2c), TSmallInventoryWindow (2, 0x55ec98, 0x44), TInventoryView
  (0x55ed38, 0x2c), TCombatControls (0x55e880, 0x44), TStatusWindow (4,
  0x55f500, 0x4c), the bodies TContainerWindowBody (0x55eed0, 0x44, a new
  slot +0xb8 Update), TSkillsWindowBody (0x55efa8, 0x3c), TBattleWindowBody
  (0x55e710, 0x58), TBuddylistWindowBody (0x55e7c8, 0x3c), TTradeWindowBody
  (0x55f370, 0x3c) with TTradeOffers (0x55f138, 0x34), TTradeNames (0x55f200,
  0x38) and TTradeButtons (0x55f2a0, 0x4c). The identifier 1 window is a
  plain TContentWindow made by CreateInventoryWindow. external.h's comments
  on TSideWindow3/4/5 were right (sizes and vtables). Names from the binary:
  TStatusWindow (SkinProperties.StatusWindow), TPlayerStates
  (SkinProperties.PlayerStates), BuddylistWindow (TBuddylistWindowBody),
  RadioButtonGroup (TRadioButtonGroup), the parameters _CheckObject,
  _RoseSkin, islarge, _Container, EventComponent, NumberOfLine, Value,
  Progress, Position, Direction, Code, the member Lines and the locals
  ScrollbarX, destx/desty, DestX/DestY, ItemX/ItemY and skin; TSkillsWindowBody
  keeps the name of the earlier sketch ("_SkillsWindow"). The other class,
  member and function names (and the Create* creators) are ours.
- **TSubWindowBody** (medium). The base of the bodies has no vtable of its
  own: its constructor is inline (Minimized=false right after TWindow's
  constructor; the subclasses' vtables are stored after it, the base one
  was dropped). Its virtuals are inline: Minimize (+0xa0) and Maximize (+0xa4)
  set Minimized, IsMinimized (+0xa8) returns it, GetMaximumHeight (+0xb0)
  returns INT_MAX, and LeftButtonDoubleClick (+0x58) returns true (the folded
  0x425a00 in every body vtable but the VIP list's: a double click on a body
  does not minimize the window; found by the test). GetInitialHeight (+0xac,
  the creators size the windows with it) and GetContentHeight (+0xb4) are
  pure (every body overrides them). The sketch's UnknownA0-B8 and
  FUN_0042e4e0 are gone: TSubWindow calls Minimize/Maximize, TSubWindow::Arrange
  calls TContainerWindowBody::UpdateUpButton (cast, as the original must),
  MainWindow.cpp's OpenContainer calls the container body's Update (+0xb8)
  and IsMinimized (+0xa8, so far "the body allows it").
  Reviewed in #24: confirmed: the folded 0x425a00 returns true, and contentwindowstest found and compares
  it.
- **Limit** (low for the name, medium for the header). 0x425920 clamps
  *Value to Min..Max in either order. ContentWindows.cpp inlines it in
  TTradeWindowBody::SetSize (with std::min/std::max of the maximum and
  minimum heights, each read twice), DialogElements.cpp calls the
  out-of-line copy. A header both include: common.h, next to In.
  Reviewed in #24: confirmed: the clamp is compared wherever it is used.
- **Source order** (medium). Line order (the creators first, lines 106-280).
  The binary moves the creators, the constructors of the bodies,
  TAutomapWindow, TStatusWindow, TSmallInventoryWindow's neighbours and
  TInventoryView's mouse handlers, TPlayerStates' Draw and MouseHover and
  TAutomapWindow::HandleEvent to the end of the object (each calls functions
  defined further down, Creatures.cpp's pattern). Functions without lines
  where the binary has them: TInventoryView::GetSlotAt after HelpMouseMove
  (1344), ~TSkillsWindowBody after its constructor (1980).
  Reviewed in #24: no behaviour: the order of definitions does not change the code.
- **Exception tables and switches** (high), checked with ehinfo.py: the
  assertions before TBEGIN in TButtonsWindow's constructor (6-10),
  TPlayerStates::GetIcon (5-10), TSkillsWindowBody::GetLine (5-15), both
  Draws of the trade parts (10-10) and ShowAcceptButton (5-6); the
  constructors' try blocks start at state 1; TCompassRose's constructor has
  only the assertion's states; TInventoryView::GetSlotAt and
  ~TSkillsWindowBody have TBEGIN/TEND frames without try blocks; the inline
  ~TBattleWindowBody has the base class's state only. Catch blocks without
  names are TEND (the HelpMouseMove functions, GetOpponentAt, ...); the
  mouse wheels name Code. Jump tables checked: Scroll 0x4276e0 (1-4),
  TPlayerStates::GetIcon 0x429e78 (0-9) and the skull 0x429ea0 (1-4),
  TCombatControls::HandleEvent 0x42c1c8 (100-105), GetLine 0x42fc44 (1-15),
  TAutomapWindow::HandleEvent 0x43716c with the byte table 0x437194 (3, 4,
  106-112), TPlayerStates::MouseHover 0x4378f8 (skins 228-239), the battle
  list's party shields 0x43adf8 and skulls 0x43ae08 (1-4).
- **Odd behaviour kept**: the automap's MouseHover prints its buffer to
  stdout and takes a mark only when both coordinates are not 0; at zoom 1 the
  automap's centre is rounded inside the pixel loop; the container rows start
  at Data630.x in GetInitialHeight/GetContentHeight; a container slot cut at
  the top edge shows the top of its picture; TTradeOffers::GetContentHeight
  adds Data68.x; the skill lines with a bar are 10 pixels taller in Draw but
  9 in MouseHover and GetContentHeight; TBuddylistWindowBody::GetBuddyAt does
  not test y; ShowAcceptButton and Wait delete the wait text or the accept
  button without a NULL check; TCombatControls leaves a radio group without a
  selection for an unknown mode (TRadioButtonGroup::Selected is uninitialised);
  the party shield 248 is drawn only while the player is in no party;
  TStatusWindow's Data44/Data48 (100) are never read; TTradeButtons' +0x2c,
  +0x30 are never used.
- **SkinProperties retyped** (medium; GUI.cpp's stores rewritten by offset,
  guitest.cpp compares the bytes): Style288's Data10-Data50 are ten
  TGUIPoints Data10[10] (the inventory's objects), Data60-DataF0 ten TRects
  Data60[10] (the slot areas), Data100-Data124 ten ints Data100[10] (the empty
  slots' skins); Data470-Data494 are the block PlayerStates (its constructor
  is inlined; the assertion names PlayerStates.NumberOfIcons; the extent is
  a guess: the members TPlayerStates reads); Data4AC-Data548 are two
  TCombatControlsProperties blocks Style4AC/Style4FC (height, width, nine
  button positions; TCombatControls' constructor copies a whole block by
  value); Data670-Data687 are two TRGBColors (the skill bars) and
  BuddylistWindow's Data3C/Data44 two TRGBColors Data3C/Data48 (online and
  offline buddies; Data54 is a new buddy). The other blocks are used as
  GUI.h has them (some TRects and TGUIPoints are used as other shapes;
  comments in ContentWindows.cpp).
  Reviewed in #24: confirmed: guitest compares the constructor's and InitSkinProperties' bytes; the
  by-value block is the same 16 bytes.
- **Surface slots named** (medium): +0x0c SetPixel(Surface, x, y, Color), +0x10
  FillRectangleClipped(Surface, Rect, Color, Clip), +0x18
  DrawHorizontalLine(Surface, x, y, Width, Color), from the arguments pushed
  here; external.h's TSurfaceGL and playertest.cpp's stubs follow.
  Reviewed in #24: confirmed: the parameters are the pushed arguments; the tests' recording surface
  compares the calls.
- **DialogElements.cpp sketches** (external.h, low): TSkinButton gained
  RadioButtonGroup (+0x38), Tooltip (+0x3c) and SetRadioButtonGroup (0x458de0,
  "RadioButtonGroup==NULL"); new: TRadioButtonGroup (0x28 bytes: Selected,
  vector<TSkinButton *> Buttons, Count; inline constructor and destructor),
  TTextButton's second constructor (0x466ed0, skins 12/13 and a tooltip),
  TTextToggleButton (0x467230, vtable 0x561d58, 0x84 bytes, State +0x78; it
  passes its own uninitialised Tooltip member to TTextButton, DialogElements.cpp's
  business), TLabel's second constructor (0x465510, an area) and SetText
  (+0x84, 0x455e40), TSkinIcon (0x457710), TScrollBarRange::SetValue
  (0x460710); TScrollBar's last parameter is a step (int; 1 in
  ConsoleWindow.cpp, which now passes 1 instead of true). All have
  low-confidence names.tsv rows.
  Reviewed in #24: superseded: DialogElements.h (issue #7).
- **Callers** changed in names and casts: MainWindow.cpp (the creators,
  TAutomapWindow, TStatusWindow, TButtonsWindow, OpenContainer's Update and
  IsMinimized), DialogWindow.cpp (Minimize, Maximize, UpdateUpButton),
  ContextMenus.cpp/.h (includes ContentWindows.h; TMarkMenuData::Window is a
  TAutomapWindow *), Control.cpp unchanged (TSkillsWindowBody is a forward
  declaration there). ContentWindows.h needs Map.h (TMark), so like
  ContextMenus.h it is included directly by its users.
- **callcheck** (81 of 140 functions differ, all explained): constructors of
  new-expressions and base classes, TCallback temporaries, the inline
  TRadioButtonGroup and TRGBColor(int) and ~TWindow are not lexical calls;
  members called without qualification in constructors (SetSize,
  InsertChild, Arrange) are direct calls in the binary that the tool does
  not resolve; virtual calls (GetParent, Arrange, GetMark, IsMinimized,
  GetContentHeight, GetMaximumHeight, GetMinimumHeight, SetChildPosition,
  SetChildArea, SetText, RightButtonUp, which the tool matches to Control.cpp's)
  are indirect in the binary; hasContentWindow and Limit are inlined;
  Offers->GetContentHeight is resolved to the body's own; the compiler merged
  call sites (SendMoveObject, SetChaseMode, AddMessage, SendUseObject,
  SetStatusMessage, Scroll); Ghidra dropped the GetMouseX/GetMouseY calls in
  the container body's Draw.
- **Tests** (contentwindowstest.cpp, 52,506 checks in the full run over 600 rounds of
  20-60 operations): each round builds one of the ten kinds of windows from
  the image's code and ours (the seven creators, the automap, status and
  buttons windows' constructors) inside a real column (the image's and our
  TDialogWindow), then random sizes, Arrange, drawing, mouse presses (with
  drags), releases, double and right clicks with the modifier flags, the
  wheel, hovering and help mode over random points and the slots, rows,
  icons, marks and the compass rose, the windows' events, the classes'
  members (Scroll, GetMark, GetSlotAt, GetIconAt, GetIcon, GetLine,
  GetOpponentAt, GetBuddyAt, GetOfferAt, Update, UpdateUpButton, the trade
  buttons, the bodies' heights) and changes of the world; logs of every
  callee and dumps of both trees (every member, the DialogElements objects
  with their arguments and state) are compared after each, the logs of the
  destructors at the end. The DialogElements classes are the shared fakes on
  both sides; they now report every object through difftest.h's
  ElementCreated (consolewindowtest.cpp's scroll bar and buttons,
  contextmenustest.cpp's separator, mainwindowtest.cpp's text button, label
  and object icon, and this file's new fakes), and the test gives each a test
  vtable. The image's operator new and ours fill memory with the same
  pattern. The world values the windows set (attack target, modes) have one
  copy per side. A mutation check (fourteen changes, one at a time) failed
  each run. DIFFTEST_ONLY=ContentWindows runs it alone; it restores every
  patch. Other tests: dialogwindowtest.cpp's body overrides the real inline
  slots and redirects UpdateUpButton (both sides) to the shared
  FakeUpdateUpButton; mainwindowtest.cpp redirects the creators and the three
  side-bar constructors (decorated names) to its fakes, its body has a +0xb8
  Update; contextmenustest.cpp uses a same-layout stand-in for the skill
  window body and logs only while it runs; mainwindowtest.cpp's element fakes
  register only while it runs. Not exercised: the error paths, the button
  callbacks (dumped, not sent) and inputs the original fails on as well (the
  counter offer twice, help mode over a hidden body).

## DialogElements.cpp

Reconstructed by Claude Opus 5.5 (issue #7), November 1, 2026.

- **Range** (high). DialogElements.obj is 0x4524e0-0x46b773: the leading run of
  header code settled in an earlier issue (now every function named: the
  inline accessors, destructors with their deleting forms, the inline
  constructors of TObjectIcon, TSeparator and TProgressBar, Container.h's
  vector<int> and vector<TListBoxLine> with their destructors 0x452c50 and
  0x452c60), the line-numbered functions (lines 42-4301) and between them the
  functions without lines where the binary has them; the last catch block
  (TEditText's constructor, line 2466) ends at 0x46b773. The binary moves the
  functions that call functions defined further down to the end of the object
  (the labels', buttons', check box's, edit line's, scroll bars' and list boxes'
  constructors, TFormattedText::Arrange, TCheckBox::SetEnabled, several
  TEditLine, TEditText and TListBox members, TLink's constructor); the source
  follows the line numbers. TEditText's compiler-generated destructor and its
  deleting form (0x46b540, 0x46b520) sit before its constructor (emitted with
  the vtable). 0x469144 is a Ghidra split of TEditText::SetText 0x469120.
- **DialogElements/Dialogs gap** (high): Dialogs.obj's leading run, recorded in
  file-ranges.tsv as Dialogs.cpp (0x46b790-0x46bb5f, 0x46bdf0-0x46c0cf) and
  Container.h (0x46bb60-0x46bdef): inline constructors of Dialogs.h's TLabel
  subclasses (vtables 0x562110-0x562370, whose own slots 0x46db80, 0x46dd40,
  0x46e0e0, ... are Dialogs.cpp code and which Dialogs.cpp also constructs),
  the reconnect text (Dialogs.obj's text table), inline destructors whose
  vtables Dialogs.cpp constructs (0x4733dd, 0x479a70, ...), a label destructor
  that jumps to ~TLabel, the deleting destructors, and the vector of a 31-byte
  element (destructor 0x46bb60, operator() 0x46bb70; its constructor 0x46c0d0
  was Container.h already). Not reconstructed here.
- **Data** (high). DialogElements.obj has no statics, no static initialiser
  (0x559df0 is Control.obj's NoCallback 0x5c4558, 0x559e00 Creatures.obj's
  0x5cc418, 0x559e10 Dialogs.obj's 0x5cc420) and uses none of its copy of the
  text table (0x5c0d8c-0x5c1228, its regular expression pointer 0x5c122c, then
  its DefaultFontColor 0x5c1230, which TFont's constructor reads). Its code
  reads only SkinProperties, Skins and NumberOfSkins.
- **Classes** (high for layouts, vtables and sizes; names see below). 28
  classes and two helper types; sizes from the new-expressions, vtables from
  vtscan.py, the overrides from comparing every vtable with TElement's and
  TWindow's (build/scratch/de7/vtdiff.py): TSeparator (0x5610b0, 0x28),
  TResizer (0x5612b0, 0x38), TBorder (0x561358, 0x68), TLabel (0x560bb8, 0x4c,
  new slot +0x84 SetText), TFormattedText (0x560c40, 0x38, +0xa0 SetText),
  TLink (0x560ce8, 0x3c), TOutfitIcon (0x561970, 0x48), TSkinIcon (0x5614f8,
  0x28), TObjectIcon (0x560d88, 0x50), TSkinButton (0x5619f8, 0x44, +0x84
  IsPressed) with TColorButton (0x561a80, 0x50), TIconButton (0x561b08, 0x48),
  TBlinkingButton (0x561b90, 0x5c), TTextButton (0x561c40, 0x78, +0x88
  SetText), TToggleButton (0x561cd0, 0x50), TTextToggleButton (0x561d58,
  0x84), TMultiStateButton (0x560e10, 0x58), TRadioButtonGroup (inline),
  TCheckBox (0x561de8, 0x34, +0xa0/+0xa4), TEditLine (0x560e98, 0x68, slots
  +0xa0-+0xc4), TPasswordEditLine (0x560f60, 0x6c), TEditText (0x562040,
  0x9c, new +0xc8 SetActive and +0xcc SetText(text, KeepPosition)),
  TFocusGroup (no vtable, 0x28), THorizontalScrollBarRange (0x5617e0, 0x54,
  with a TCallback), THorizontalScrollBar (0x561ed0, 0x38), TScrollBarRange
  (0x561868, 0x4c), TScrollBar (0x561f70, 0x38), TListBox (0x561138, 0x7c,
  +0xa0 StartEdit, +0xa4 StopEdit), TFontListBox (0x5611e0, 0xb0) with
  TListBoxLine (0x18 bytes, formerly "TDialogText") and TProgressBar
  (0x561028, 0x2c). The two scroll ranges are separate classes: their Pressed
  and Dragging flags sit in swapped bytes (+0x38/+0x39), and only the
  horizontal one has a callback.
- **Names**. From the binary: TCheckBox (SkinProperties.CheckBox), TFocusGroup
  (_FocusGroup), getMinimumWidth/getMinimumHeight (see below), the members
  Text, Cursor, MaxLength, FocusGroup, MemberList, Active, FieldWidth, Lines,
  RadioButtonGroup, ToResize, the locals Next, insertlength, appendlength,
  Width, and every parameter named in an assertion or catch block. Kept from
  the sketches (low): TResizer, TBorder, TLabel, TFormattedText, TSkinIcon,
  TObjectIcon, TSkinButton, TToggleButton, TMultiStateButton, TTextButton,
  TTextToggleButton, TBlinkingButton, TEditLine, TScrollBar, TScrollBarRange,
  TSeparator. New and ours (low): TLink, TOutfitIcon, TColorButton, TIconButton,
  TPasswordEditLine, TEditText, THorizontalScrollBar(Range), TListBox,
  TFontListBox, TListBoxLine, TProgressBar; members without evidence are ours
  (Pressed, MouseInside, Repeat, Enabled, SingleLine, Disabled, ReadOnly,
  SelectionStart/Length, FirstVisible, ...). Corrected sketch rows:
  TSkinButton's last parameter is Repeat (a held button resends its callback),
  TTextButton's last parameter of the first constructor is a tooltip, TLabel's
  flag is Shorten, TObjectIcon's flag Framed, TEditLine's flag ReadOnly, the
  console's SetReadOnly (+0xb8) is SetDisabled (+0x65; ReadOnly is the
  constructor's +0x66), TBorder::FUN_00455500 is FitToWindow, TScrollBar's
  parameters are (Min, Max, Value, Visible, ButtonRange).
  Reviewed in #24: confirmed: the parameter roles are what the code does with them (dialogelementstest).
- **getMinimumWidth/getMinimumHeight** (high): the assertions of
  TBorder::SetSize name "Width>=getMinimumWidth()" and
  "Height>=getMinimumHeight()" (lines 242-243), so GUI.h's virtuals +0x3c/+0x40
  are renamed from GetMinimumWidth/GetMinimumHeight in every file, test and
  names.tsv row.
- **Header code** (medium). The out-of-line copies at the start of the object
  are inline in DialogElements.h; several are called only by later objects
  (TOutfitIcon::SetOutfit, TSkinButton::SetEnabled, TListBox::GetSelection and
  SetEditable from Dialogs.cpp; TMultiStateButton::Reset from ConsoleWindow.cpp;
  TEditLine::HasSelection's copy is ConsoleWindow.obj's 0x417520). The folded
  "return false" slots 0x452620 (x, y) and 0x457700 (x, y, Flags) are inline
  overrides of TLabel and the three icons (TSkinIcon also for MouseWheel). The
  scroll bars' KeyPress slot is 0x4731d0, ICF-folded with a Dialogs.cpp
  function of the same code (Range->KeyPress(Key)). 0x4672c0 is GUI.h's
  implicit ~TElement (jumps to ~TComponent), kept here.
  Reviewed in #24: no behaviour: inline or out of line, the code is the same.
- **Exception tables and switches** (high), checked with ehinfo.py for every
  function (build/scratch/de7/ehcheck.py compares the assertions before TBEGIN
  with the try ranges): constructors' try blocks start after the base class,
  the vector members and the assertions; the frames without try blocks (eleven
  states: Arrange of the separator, resizer, frame, check box, edit line,
  ranges and progress bar, TSkinButton::MouseMove, the IsPressed functions,
  TEditLine::SetActive) are TBEGIN/TEND around code that cannot throw; the
  functions with only assertions and no TBEGIN are noted. tools/ehinfo.py now
  also finds the prologue PUSH -1; MOV EAX, FS:[0]; PUSH stub (0x45dfc0,
  TFocusGroup::AddMember, and a few Container.h instances). The link's and the
  password line's HandleEvent name Event with a constant in their catch
  blocks: their try blocks are inside the test of the event. Jump tables:
  GetToken 0x4567ec (states 0-7, 2-4 unused), TFormattedText::Arrange 0x466650
  (token types 0-3, 10-18), TEditLine::HandleEvent 0x45bc6c (16-20),
  TEditText::KeyPress 0x4690d0 with the byte table 0x4690e4 and 0x4690f0.
- **Odd behaviour kept**: TEditLine::RightButtonUp calls TWindow::LeftButtonUp;
  InsertText inserts at the cursor (Position enters only the assertion and the
  rest's length); select all puts the cursor at max(length, 0); a backspace
  with a selection moves the cursor before deleting it; TEditText::Arrange and
  TListBox::Arrange size the scroll bar with skin widths (11, 10) where skin
  heights would fit; TEditText's PageUp moves one line more than a page and
  reads LineStart(-1) on the first line; TFocusGroup::AddMember does not test
  the last member for a duplicate; TToggleButton and TTextToggleButton pass
  their own unset Tooltip to the base class; TMultiStateButton::Reset sets
  State past the last state (it wraps to 0); a '<' that starts no token
  advances TFormattedText's layout twice; a failed <name=...> token keeps its
  type for the next piece; TCheckBox::SetEnabled rebuilds the label from its
  first line only; TObjectIcon's frame only narrows the clipping area; the
  horizontal range sends its callback before limiting the value in KeyPress
  and twice for a held button beside the slider; TListBox::Draw stops at
  VisibleLines or Lines lines; TFontListBox::Clear frees the texts and keeps
  the pointers; TOutfitIcon keeps a direction it never draws.
- **SkinProperties retyped** (medium; GUI.cpp's stores rewritten, guitest.cpp
  compares the bytes): CheckBox.Data08/Data10 are one TRect Data08 (the area
  TCheckBox fits its label into). Surface.h's slot +0x1c is DrawVerticalLine
  (the edit line's cursor), external.h's TSurfaceGL and playertest.cpp follow.
  Reviewed in #24: confirmed: guitest compares the constructor's and InitSkinProperties' bytes.
- **Callers** (high): external.h's sketches are gone; DialogElements.h includes
  external.h (TObject), Status.h and Skins.h, so its users include it
  directly (ConsoleWindow.cpp, ContentWindows.h, ContextMenus.cpp,
  DialogWindow.cpp, MainWindow.cpp, MapWindow.cpp; Control.cpp needs only
  ContextMenus.h's forward declaration). Renamed: Input->SetDisabled,
  SelectionStart/SelectionLength/ReadOnly in ConsoleWindow.cpp and
  ContextMenus.cpp, Frame->FitToWindow in MainWindow.cpp; the placeholder
  literals became false (Repeat, Framed, ReadOnly, State) and NULL (tooltips).
- **callcheck** (all differences explained): constructors of base classes,
  members and new-expressions (TElement, TWindow, TCallback, TDrawing's
  vector constructor iterator, the vectors, the controls' parts) and
  Container.h's operator() are not lexical calls; virtual calls (Arrange,
  IsPressed, IsDisabled, IsChecked, GetPosition, the edit members, StartEdit
  and StopEdit, ReleaseCapture, SetSize, getMinimum*, SetChildArea,
  SetChildPosition, ClipChildren) are indirect in the binary; the binary
  inlines Limit, SetCursor, LimitCursor and SetSelection in places and calls
  them in others; calls of a member of another class with the same name
  (SetActive, SetValue) are resolved to the own class by the tool; OpenURL is
  a function pointer; two SetRange calls of an if/else are one call site in
  the binary; _ftol (0x5124c4) is the compiler's float conversion.
- **Tests**. dialogelementstest.cpp (new; 135,478 checks in the full run over 1,600
  rounds): every round builds one of 27 kinds
  of controls on both sides (the image's constructors at their addresses,
  ours with new) as a child of a root window, three edit lines of a focus
  group or one with a radio group, then 20-60 random operations (sizes,
  Arrange, drawing, mouse moves, clicks with drags and held-button ticks,
  double and right clicks, the wheel, hovering, keys, events, and each
  class's members, the header's inline members against their out-of-line
  copies); the logs of every callee with an effect (text and picture drawing,
  the surface, the mouse-over text, cursor, messages, clipboard, callbacks and
  OpenURL) and a dump of every member of the control and its children are
  compared after each. The metrics, drawing and world are fakes for both
  sides; the image's operator new and ours fill memory with a pattern. A
  mutation check (twenty changes, one at a time) failed each run.
  DIFFTEST_ONLY=DialogElements runs it alone (DE_TRACE, DE_LOGS).
  The earlier window tests keep their fakes as mirror classes of the old
  sketches (tools/difftest/elementfakes.h): PatchElementFakes redirects the
  image's and our constructors (decorated names through /alternatename) and
  non-virtual members to them while each test runs. TObjectIcon's and
  TSeparator's constructors and TMultiStateButton::Reset are inline (inlined
  into our callers), so they run for real on both sides: contextmenustest.cpp
  gives the real separators its test vtable after each construction,
  contentwindowstest.cpp and mainwindowtest.cpp name the real icons and
  separators by vtable (mainwindowtest.cpp fakes DrawObject now).


## SurfaceDX.cpp (issue #9)

- **Object boundary (high)**: DirectDraw is 0x4eae70-0x4efd0f, not just the
  previously settled leading range ending at 0x4ed19f. Vtable 0x56f038
  identifies the functions Ghidra missed, including the pixel writers,
  AttachWindow, both stretch wrappers, the clipped blit wrappers, Init,
  InitDisplay and GetSurfaceRect (assertion line 1411, [bug0000290]). The
  generated source map counts only functions present in the raw decompile;
  the missing entries are still named in names.tsv and implemented from
  instructions, not inferred from that count.
- **The following gap is Direct3D 9 (high)**: 0x4efd10 loads d3d9.dll and
  stores Direct3DCreate9 at 0x71d278; 0x4f1430 and 0x4f03f0 install
  vtable 0x56f480. Assign 0x4efd10-0x4f1ea4 to surfaceDX9.cpp for its own
  reconstruction issue. Do not implement that engine in this issue.
- **Interfaces (high)**: the engine is called DirectX 5, but its GUIDs and
  calls use IDirectDraw4, IDirectDrawSurface4 and IDirect3D3 / Device3 /
  Viewport3 (DirectX 6 interfaces). Use the installed SDK declarations,
  ddraw.lib, dxguid.lib, winmm.lib and user32.lib, not handwritten COM shims.
  d3dtypes.h supplies the empty D3DTLVERTEX constructor at 0x4eae70;
  D3D_OVERLOADS enables it. Its names.tsv kind is stl (SDK/library code).
- **Class and names (medium)**: TSurfaceDX is inferred from the object name.
  Surface, Rect, DirectDraw, Direct3D, hWindow, ViewMode, VMode, GMode,
  FromRect.width/height, ToRect.width/height and the surface-index constants
  come from assertions. The other member/parameter names are ours.
  TBlitSize is a local two-int view solely to retain the asserted lower-case
  width/height names while keeping the existing TRect drawing ABI; it adds
  no binary function or wrapper. (Settled in #29: TBlitSize is gone; the
  slots take TRect FromRect/ToRect, whose members are width and height.) The factory is CreateSurfaceDX (0x4eca00):
  its allocation is 0x250 bytes with an inlined empty derived constructor.
  Reviewed in #24: confirmed: a local view and an empty inlined constructor add no code (surfacedxtest).
- **Layout (high)**: TSurface is 0x70 bytes, not the former 0x44 sketch.
  Doubles at +0x48/+0x50/+0x58/+0x60 give it 8-byte alignment, making +0x04
  padding, not a member. hWindow is still +0x08; mode, size, availability,
  counts and frame timing have real fields. TSurfaceDX adds Recreated at
  +0x70, DirectDraw at +0x74, WindowRect at +0x78, Surface[22] at +0x88,
  Rect[22] at +0xe0 and the three Direct3D pointers at +0x240/+0x244/+0x248.
  +0x0c/+0x20/+0x44/+0x6c/+0x24c remain offset names (low confidence).
  TSurfaceGL retains total size 0x498 by shortening its opaque tail.
- **Folded members (high addresses, medium names)**: eight overrides are
  shared with neighboring engines: ClipRect 0x4f0410, ResetFrameTimer
  0x4f04d0 and SurfaceRecreated 0x4f13e0 (DX9); DrawFrame 0x4fa110,
  DrawFrameClipped 0x4fa2c0, GetCacheSurfaces 0x4fb5e0, IsUnavailable
  0x4fb5f0 and GetFPS 0x4f9d00 (OpenGL). TestEngine is the return-true body
  folded with codecvt at 0x4d7d40; unlike TSurface's implementation it
  does not store the main window. No extra thunk is reconstructed.
- **Shared data ownership (medium/low)**: all engines reference the blit
  counters/statistics and InitStep; place them with the engine-independent
  Surface.cpp data, spanning 0x71d0bc-0x71d0f4, and declare them in Surface.h.
  This revises the earlier boundary argument: 0x71d0f0 is the shared
  NumberOfSurfaces, not the first private DirectDraw datum. The private
  SurfaceMemory[22] starts at 0x71d0f8, followed by WaitForNextFrame's
  Frames at 0x71d150 and LastSecond at 0x71d158. No dynamic static
  initializers are needed for these POD/BSS values. CacheSurfaceWidth and
  CacheSurfaceHeight at 0x5c2568/6c are shared, initially 256 and changed
  to 320 by InitSurfaces. The private DeviceGUID pointer at 0x5c2570
  selects IID_IDirect3DHALDevice. Exact source ownership of shared globals
  is low confidence, but their addresses, types and common use are verified.
  Reviewed in #24: confirmed for the behaviour: zero .bss and the .data words 0x100 (0x5c2568/0x5c256c)
  and a pointer constant (0x5c2570), no initialisers; the owner stays medium/low.
- **EH and switches (high)**: AttachWindow has 15 unwind states,
  StretchBlitUnclipped/StretchBlit 30, Blit/BlitRect/GetSurfaceRect 5,
  InitDisplay 20 and CreateSurfaces 45, all with zero try blocks: assertions
  without TBEGIN. ResetDisplay alone has a PROGRAM_ERROR catch and a
  nested catch-all protecting cleanup. The six-way assertion prologues
  interleave an argument load with the FS:[0] setup; ehinfo.py now accepts
  these and validates the FuncInfo magic. Init's switch is a compare chain
  (0, windowed, fullscreen), not a jump table; the whole object contains
  no indexed indirect jump to a switch table.
- **Odd behavior retained**: Locks set dwSize to 0x6c inside a zeroed
  0x7c-byte descriptor. The image writers leave sub-16-bit surfaces locked.
  A zero RGB mask loops forever in GetPixelColor; valid formats avoid it.
  DirectDrawCreate's initial interface leaks. A failed Direct3D setup only
  releases Direct3D and decrements NumberOfSurfaces even if its render
  target was not allocated. GetDisplayMode/GetSurfaceDesc descriptors are
  not zeroed. Shading uses the original float 1/255 factor promoted to
  double before truncation; do not algebraically simplify it. The mode
  callback tests DDSD_REFRESHRATE in ddpfPixelFormat.dwFlags, not dwFlags
  (0x4ec869); preserve this misplaced-flag test rather than correcting it.
- **Callcheck audit**: 14 of 43 blocks differ. The destructor/base-constructor
  calls are implicit (0x4eae90, 0x4eca00); the parser mistakes COM's
  SetColorKey for TSurface::SetColorKey (0x4eafb0). Vtable dispatch is
  absent from Ghidra's named-call counts at 0x4ebd20/0x4ebe60 (ClipRect,
  SetSourceColorKey), 0x4ec220 (WaitForNextFrame), 0x4ec600 (InitDisplay,
  CreateSurfaces, cleanup), 0x4ec690 (cleanup/reset), 0x4ec9e0 (cleanup),
  0x4ecdd0 (FillRectangle), 0x4ecfd0 (timer/framerate) and 0x4eed40
  (AttachWindow). AddFullscreenMode's 8 versus 9 static GetSurface calls
  are the compiler's duplicated loop-condition test; dynamic calls agree.
  ShadeRectangle's SDK vertex-array constructors and twelve x87 _ftol
  conversions are expressed by C++/D3DRGB, not explicit helper calls.
- **Caller audit**: Surface.cpp now calls CreateSurfaceDX; the other engine
  factories stay external. graphics.cpp's five +0x50 calls become
  SetColorKey; the +0x64 window attachment in Control.cpp and +0x9c
  recreation checks in ContentWindows.cpp/MapWindow.cpp retain their
  arguments and slot offsets. Sketches/fakes follow the retyped slots.
  Naming the base CacheSurfaces member exposed a collision with
  graphicstest.cpp's fake counter; rename that counter GraphicsCacheSurfaces
  so its GetCacheSurfaces override still returns the configured test value.
- **Integration with issue #8**: keep its member GetAvailableEngines and
  the shared +0xac name SetFramerateLimit (our initial name was
  SetFramerate). Both names are inferred; the binary clamps values below
  10 or above 200 to 0xffff. There is one implementation, no alias or
  compatibility wrapper. The new SurfaceDX layout and slot signatures
  are used by the existing engine sketches and all test surfaces.
- **Differential coverage**: surfacedxtest.cpp runs every callable object
  function and folded override through real reconstructed/image code on
  independent fake DirectDraw4/Surface4/Clipper and Direct3D3/Device3/
  Viewport3 pools. Imports, preferences and the clock are scripted before
  either side runs. After each operation compare call logs, all 0x250
  object bytes (pointers normalized), thirteen shared globals, per-surface
  memory kinds, COM state and pixels; shading includes every vertex word.
  All code and IAT patches are restored. The standalone run passes
  1,213,828 checks over 16,089 operations of 56 kinds, with 269 coverage
  points. DX_LOGS writes compared logs; DX_TRACE prints phase coverage.
  Coverage includes every mode transition, forced/recovered displays,
  Direct3D setup failures at steps 2-14, cache/video-memory exhaustion,
  busy/still-drawing retries, both shading outcomes, 8/16/24/32-bit
  formats, the misplaced refresh flag and the 32-bit clock wrap.
  Assertion and PROGRAM_ERROR throw paths cannot run in the mapped
  image without its original CRT initialization; EH and instructions
  establish their behavior instead. GetAttachedSurface initializes only
  DDSCAPS2.dwCaps, so the test deliberately does not read its other words.
- **Mutation sensitivity**: eighteen isolated changes were all detected:
  frame-limit boundary, frame schedule, pixel offset, rectangle fill size,
  clipped-fill statistics, horizontal/vertical line size, RGB input stride,
  mask grayscale coefficient, color key, stretch source size, vertex color,
  offscreen memory kind, 3D surface caps, refresh flag location, mode count,
  surface rectangle width and recreation flag. The initial caps mutation
  removed DDSCAPS_3DDEVICE and crashed the fake world after diverging;
  rerunning it with an extra DDSCAPS_LOCALVIDMEM instead produced 4,371
  ordinary comparison failures. All eighteen targets therefore have
  explicit mismatch evidence; no target escaped. Restore the source after
  each mutation and rebuild the unmutated test before final validation.
- **Final validation (November 2, 2026)**: after integrating issue #8
  from origin/main, tools\compile-check.cmd is warning/error-free and
  tools\difftest\build.cmd passes 13,955,244 differential checks,
  67 sysinc checks and 3,201 arithmetic checks, all with zero failures.
  SurfaceDX contributes 1,213,366 checks in that full run; its separate
  deterministic run passes 1,213,828 checks. The full-suite random stream
  changes its input distribution and totals, not the tested code.

## Dialogs.cpp

Reconstructed by Claude Opus 5.5 (issue #8), November 2, 2026.

- **Range** (high). Dialogs.obj is 0x46b790-0x4854ad: the leading run of
  header code settled in issue #7 (now every function named, see below), the
  line-numbered functions (lines 48-4084) and, without lines, the inline
  virtuals and compiler-generated destructors the binary emits with the
  vtables. The last catch block (the rule violation creator's, line 490)
  ends at 0x4854ad; DialogWindow.obj starts at 0x4854b0 (issue #3). No header
  code or Container.h instance follows the last function. The binary moves
  the creators (lines 48-631), the constructors that call functions defined
  further down (TDialog's 0x47c970, the outfit's, the edit list's, the ignore
  list's, the hotkeys', the error report's, the rule violation's) and the
  rule violation's HandleEvent (line 3124, which calls UpdateActions of line
  3158) to the end of the object, Creatures.cpp's pattern; the source
  follows the line numbers. Ghidra missed 0x46bb00, 0x46bef0, 0x4719a0,
  0x47cf50, 0x47cf60 and 0x47db50 (vtable slots and a qsort comparator).
- **Leading run** (high for the code, medium for the names): the inline
  constructors of TQuestionLabel, TLogoutLabel and TWaitLabel (0x46b790,
  0x46b840, 0x46b8f0; the first two take the font by reference and copy it
  into TLabel's), the inline destructors of TWaitLabel, TReconnectWindow,
  TCharacterListDialog, TNewChannelDialog, TGraphicsOptionsDialog,
  TEditBuddyDialog, TRuleViolationDialog, TEditMarkDialog,
  TAdvancedGraphicsDialog, TLoginDialog and TOutfitDialog (each deletes its
  timer, its array or its radio/focus groups), TReconnectWindow::SetTime
  (0x46ba10, "Trying to reconnect in %d %s."), TEditBuddyDialog::KeyPress
  (0x46bef0), the compiler-generated ~TQuestionLabel (0x46bdf0, jmp ~TLabel,
  shared with TLogoutLabel) and the deleting destructors; CompareNames
  (0x46bb00) is the ignore list's qsort comparator (_stricmp of two char *),
  an inline function of the header by its place (low). Container.h's
  vector<TIgnoreEntry> (destructor 0x46bb60, operator() 0x46bb70,
  constructor 0x46c0d0) clears Name[0] and Permanent of every new entry and of
  init: TIgnoreEntry has an inline default constructor (medium; in
  HandleEvent the store of Name[0] before CopyString is missing, which
  suggests whole-program optimisation).
  Reviewed in #24: confirmed: vector<TIgnoreEntry>'s constructor 0x46c0d0 clears Name[0] and Permanent of
  each entry, as our default constructor; containertest compares that instance on pattern-filled memory.
- **Data** (high). Dialogs.obj's only static is NoCallback 0x5cc420
  (initialiser 0x559e10; 0x559e20, the TCallback 0x5cc428, is DialogWindow.obj's,
  as issue #3 found). Its copy of the text table is 0x5c123c-0x5c1484, then its
  regular expression pointer 0x5c1488 and DefaultFontColor 0x5c148c; the
  entries it reads are declared in external.h (names ours) and the test reads
  the image's texts into them. The code also reads SkinProperties, Skins and
  curEngine 0x71d0c4.
- **Classes** (high for layouts, vtables and sizes; names ours, low, unless
  noted). The frame TDialog (vtable 0x563778, 0xc4 bytes; the assertions name
  the parameters _Caption and _Dialog and the members OkButton, CancelButton
  and ExtraButton, so the body is "the dialog" and the frame is named TDialog)
  and 31 bodies, each a TWindow (or a label) with its own KeyPress and
  HandleEvent: TQuestionLabel 0x562110, TLogoutLabel 0x562198, TWaitLabel
  0x562220, TReconnectWindow 0x5622b0, TLoginDialog 0x5627d8,
  TCharacterListDialog 0x562370, TMoveObjectsDialog 0x562b68 (the member
  DraggedObject is named in an assertion), TOutfitDialog 0x562878,
  TTextDialog 0x562c28, TEditListDialog 0x563868, TIgnoreListDialog 0x5639d8,
  TBugReportDialog 0x562cd8, TNewChannelDialog 0x562418, TPrivateChannelDialog
  0x562db8, TReportDialog 0x562e58, TAddBuddyDialog 0x562ef8,
  TEditBuddyDialog 0x562558 (member CharacterID from an assertion),
  TUpdateDialog 0x562ff0, THelpDialog 0x5630c0, THintsDialog 0x563160,
  TRuleViolationDialog 0x5625f8, TEditMarkDialog 0x562698, TOptionsDialog
  0x563258, TGeneralOptionsDialog 0x563360, TGraphicsOptionsDialog 0x5624b8,
  TAdvancedGraphicsDialog 0x562738, TConsoleOptionsDialog 0x563598,
  TNetworkOptionsDialog 0x563650, THotkeyDialog 0x563a90 and
  TErrorReportDialog 0x563b30. Sizes from the creators' new-expressions. A few
  members are never touched and kept as UnusedNN (the move dialog's +0x58, the
  new channel dialog's +0x34/+0x38, the update dialog's +0x2c).
- **Creators** (names ours, medium): CreateMessageDialog ... CreateErrorReportDialog
  (lines 48-631), with the parameter names of the catch blocks (Timeout,
  CharacterID, DataSize) and typed parameters for external.h's placeholders:
  the receivers of the connecting, question and logout dialogs are TComponent
  pointers, the outfit, text and house-list dialogs take int arrays, the new
  channel dialog the channel IDs and an array of 30-character names, the rule
  violation dialog a name and a TTalkLine, the mark dialog a TCoordinate and a
  TMark (deleted by the dialog). The move and report creators return NULL
  when they make no dialog. Control.cpp's 40 call sites call them with casts
  of its INT_PTR parameters.
  Reviewed in #24: confirmed: pointers and arrays are passed alike; dialogstest compares every creator.
- **Exception tables and switches** (high): ehinfo.py for every function;
  build/scratch/dg8/ehcheck.py finds the assertions before TBEGIN in every
  constructor (TDialog's three, the outfit's, the text dialog's, the edit
  list's two, the new channel dialog's two, the hotkey dialog's UpdateLine)
  and flags only the ignore list's constructor, whose try block starts after
  the base and its two vector members (state 3), as in the source. The update
  dialog's HandleEvent names Event with the constant 2 in its catch block: its
  try block is inside the test. The only jump table is the outfit dialog's part
  names (0x47d648, cases 0-3); the other switches (the help, options, general
  options, rule violation, hotkey and graphics key handlers) are compare chains.
  Ghidra flattened the control flow of many handlers (the move dialog's
  KeyPress, the reconnect and character list events, the new channel dialog's
  Event = 301, the ignore list's event chain): they were rebuilt from the
  assembly.
- **Odd behaviour kept**: the frame's bottom edge starts at Skins[45].height;
  the OK button's own callback stays NoCallback when the creator passes it
  (only Return gets (this, 1)); the new channel dialog's Return sends 302 with
  a name (nothing but closing; the frame's OK button takes Return first);
  "Exclude player" has an "Add" button; the outfit dialog's part loop runs
  over 216 colours (reading past the 133 buttons); the ignore list's Delete
  loops stop one short of the vector's last index and its selection event
  computes the entry twice; the hotkey dialog's event 11 without a selection
  reads before its texts; the move dialog computes the cumulative pattern and
  does not use it; the help menu's Client Help closes before it asks; an
  incomplete rule violation report still closes the dialog; the advanced
  options' selection is reset to the current engine on every draw; the hints
  and help dialogs are not arranged at the end of their constructors.
- **Side changes** (high unless noted): TObject's member Data1 is Data
  (assertion text "In(DraggedObject.Data,1,100)"; every file and test);
  TBorder frames a TComponent (the outfit dialog frames its TOutfitIcon;
  DialogElements.cpp/.h and elementfakes.cpp's decorated names);
  TListBox::GetNumberOfLines is inline in DialogElements.h (its out-of-line
  copy is folded with Talk.h's TChannelTalkBuffer::GetChannelID 0x507060);
  TSurface's slots +0x8c and +0xac are GetFullscreenModes(TFullscreenMode **)
  and SetFramerateLimit(int) (the arguments pushed), and GetAvailableEngines
  is a non-virtual TSurface member (Dialogs.cpp calls it with the surface in
  ECX; Surface.cpp, external.h's TSurfaceGL and playertest.cpp follow);
  external.h's dialog placeholders are gone.
- **SkinProperties retyped** (medium; build/scratch/dg8/retype.py relayouts the
  blocks, checks that the constructor-zeroed bytes stay the same and rewrote
  GUI.cpp's 260 stores by offset; guitest.cpp compares the bytes): the pairs of
  points Dialogs.cpp reads as one rectangle are TRects (SetChildArea's areas)
  in TSkinProperties (0xbac-0x1518, see GUI.h), StyleB08 (the caption, the
  separator and the body rectangle, which FitToChild takes by value, and the
  three button positions as an array), StyleD3C (the ignore list: relaid
  completely; its colour of permanent entries is Data88), StyleE68, StyleF8C,
  Style1114, Style1274 (the label positions are points), Style1084, Style1394;
  0xc88 is an array of four points (the outfit parts), 0x1064 a TRect that is
  zeroed and never read, 0x14d4 the colour of hotkeys sent automatically.
  Reviewed in #24: confirmed: guitest compares the constructor's and InitSkinProperties' bytes.
- **SetChildArea takes a TRect by value** (not changed): every caller in
  Dialogs.obj builds a 16-byte block (sub esp,0x10) for the area, so the
  original signature is SetChildArea(TComponent *, TRect); GUI.h's five-int
  form has the same stack layout, so the source keeps it (issue #27).
  Settled in #27: SetChildArea(TComponent *Child, TRect Area); the callers pass the TRects themselves.
- **callcheck** (all differences explained): constructors of base classes,
  members and new-expressions (TWindow, TCallback, TTimer, TLabel, the
  controls, the vectors, TDrawing's vector constructor iterator) are not
  lexical calls; virtual calls (SetChildArea, FitToChildren, SetChecked,
  IsChecked, SetDisabled, IsDisabled, StartEdit, StopEdit, HandleEvent,
  SetTimeout) are indirect in the binary, and calls on this in constructors are
  direct in the binary but resolved to their own class by the tool; members
  whose name several classes share (GetLine, AddLine, SetLine, Clear,
  SetEnabled, SetActive, InsertChild) are not resolved by the tool; the binary
  merges equal tails (SetActionState, AddMessage, Send) and unrolls the
  rule violation's loops over the seven actions (CheckRight, GetActionName,
  AddLine, GetLine); Limit is inlined; OpenURL and SetDisplayMode are function
  pointers that the tool matches with MainFrm.cpp's functions of the same
  names; OutfitColor's GetOutfitColor call is counted for the move dialog's
  HandleEvent (the tool splits at "// 0x" comments); atol is Ghidra's _atol;
  the error report's stringstream calls are library code.
- **Tests**. dialogstest.cpp (new): every round opens one of 34 dialogs on
  both sides through its creator (the image's at its address, ours directly),
  the DialogElements controls real on both sides, then 20-60 random
  operations (drawing, keys, clicks on the dialog's components with drags,
  double and right clicks, the wheel, hovering, caption drags, the body's own
  events and the frame's, Arrange, the timers, the frame's button setters and
  each body's members against their image copies: SetTime, SetTimeout,
  SetTimer, UpdateActions/GetReason/GetAction, UpdateLine, selections and names
  through the controls' members); the logs of every callee with an effect and
  a dump of the frame's tree (geometry, drawing lists, every member word with
  pointers named, strings for the string members, timers and groups) are
  compared after each. The callees are fakes answering from a random world;
  Preferences.cpp's has one copy per side (the dialogs set and read them in
  one operation). The image's error report constructor cannot run in the
  child (its std::stringstream needs the CRT's static initialisers, which call
  imports the child does not resolve): TestErrorReport runs our constructor
  twice and gives the second object the image's vtable, so its KeyPress,
  HandleEvent and destructor are the image's. CompareNames is checked against
  0x46bb00. A mutation check of 22 changes failed each run (the update
  dialog's two-second close needed small clock steps). 98,889 checks in the
  full run.
  DIFFTEST_ONLY=Dialogs runs it alone (DG_TRACE, DG_LOGS).

## surfaceDX9.cpp

Reconstructed by Claude Opus 5.5 (issue #10), November 2, 2026, in parallel with SurfaceDX.cpp (issue #9,
PR #28, merged first into this branch).

- **Range** (high; agreed on #9 and #10). surfaceDX9.obj is 0x4efd10-0x4f46ef: the whole former
  SurfaceDX/surfaceDX9 gap and the code after the asserted 0x4f2040 that Ghidra had not split. Evidence:
  TSurfaceDX9's vtable 0x56f480 (set by the factory 0x4f1430 and the destructor 0x4f03f0) points into
  that range; its .rdata starts with "Direct3DCreate9"/"d3d9.dll" (0x56f458, used only by 0x4efd10 and
  0x4efd40) right after SurfaceDX.cpp's last string 0x56f428, and ends with 0x56f6ac (0x4f4560); its .bss is
  0x71d170-0x71d28f (SurfaceDX.cpp's own ends at 0x71d158, SurfaceGL.cpp's starts at 0x71d290); .data
  0x5c2574 (CurrentTargetNumber = -1). 0x5c2578 (256) is referenced by nothing; it is left out (owner
  unknown). Only Surface.cpp calls into the range (0x4efd10, 0x4f1430). #9 had recorded 0x4efd10-0x4f1ea4 as
  surfaceDX9.cpp; the row now covers the whole object.
- **The surfaceDX9/SurfaceGL gap 0x4f46f0-0x4fb7ef is SurfaceGL.obj** (high), recorded in file-ranges.tsv
  for issue #11 and not reconstructed: .rdata only from 0x56f6e0 (the WGL_/GL_ extension names of
  glh_extensions.h, whose code is 0x4f46f0-0x4f9260), .bss only from 0x71d290 (the extension pointers),
  TSurfaceGL's constructor 0x4fb670 and destructor 0x4f9860 (vtable 0x573250), "OpenGL works" 0x4f9710.
- **Folded copies** (high). Seven of TSurfaceDX9's slots point to identical code the linker kept from
  another object: DrawFrameClipped 0x4fa2c0, DrawFrame 0x4fa110, BlitClipped 0x4fab70, GetCacheSurfaces
  0x4fb5e0, IsUnavailable 0x4fb5f0, GetFPS 0x4f9d00 (SurfaceGL.obj) and SetFramerateLimit 0x4eb000
  (SurfaceDX.obj); they are written at the end of surfaceDX9.cpp, like TSurface::AttachWindow. The other
  way round, TSurfaceDX's ClipRect, ResetFrameTimer and SurfaceRecreated are surfaceDX9.obj's 0x4f0410,
  0x4f04d0, 0x4f13e0 (names.tsv has the TSurfaceDX9 names; #9's TSurfaceDX rows for those addresses were
  replaced in the merge), and the display mode callback of GetFullscreenModes is SurfaceDX.obj's
  AddFullscreenMode 0x4ec810: surfaceDX9.cpp has its own static copy (EnumModesCallback, identical code).
- **Interfaces** (high). Direct3D 9 is loaded at run time (IsDirect3D9Installed: LoadLibraryA,
  GetProcAddress "Direct3DCreate9"; the module handle is never kept, so ExitDisplay's FreeLibrary is dead
  code, kept). DirectDraw is the imported DirectDrawCreate, queried for IDirectDraw4 (the IID at 0x574204,
  as in SurfaceDX.cpp), used only for the display modes, the stretch capabilities and the desktop depth;
  the first IDirectDraw is never released (kept). The interfaces, D3DPRESENT_PARAMETERS, DDCAPS and
  DDSURFACEDESC2 are the Windows SDK's d3d9.h and ddraw.h: the issue assumed the SDK headers were missing,
  but the installed Windows 10 SDK has both, so no shim header was written (the method order is checked by
  the test's fakes, which have only the slots the code uses).
- **Class** (layout high, names medium/low). TSurfaceDX9, 0x2c0 bytes. Names from the assertions:
  Direct3D (+0x1f4), DirectDraw (+0x1fc), Surface9[] (+0x200: the textures; "Surface9[PRIMARY_SURFACE]==NULL"
  reads +0x200), Rect[] (+0x74, Windows RECTs: "Rect[TEMP_SURFACE].right"), hWindow and ViewMode (TSurface),
  VMode, GMode, ToSurface, FromSurface, Surface, NUMBER_OF_SURFACES_DX9 (22) and the surface constants,
  whose values come from the comparisons (ToSurface!=SCREEN_SURFACE is `cmp 1`, FromSurface!=TEMP_SURFACE
  `cmp 2`, STRETCH_SURFACE 3; the CreateSurfaces assertions check +0x200 + 4*N in the order PRIMARY, SCREEN,
  TEMP, CACHE, AUTOMAP, SPRITE). Ours: Device, RenderTarget[] (level 0 of Surface9[]; SCREEN_SURFACE's is the
  back buffer), CurrentTarget, VertexBuffer, ShadeBuffer, ScreenTexture (SCREEN_SURFACE's texture, 106x106),
  WindowRect, SurfaceArea, Ready (+0x70, returned by SurfaceRecreated). The frame timer members (+0x44-+0x6c)
  and the shared statistics are TSurface's/Surface.cpp's as #9 settled them.
- **Merge with #9** (PR #28, merged into this branch first): #9 had named the shared parts from
  SurfaceDX.cpp, so this file uses its names for TSurface's members (Width, Height, Depth, Unavailable,
  CacheSurfaces, the frame timer +0x44-+0x6c), the slots (SetSourceColorKey +0x54, ResetFrameTimer,
  WaitForNextFrame, InitDisplay, ExitDisplay, CreateSurfaces, ResetDisplay, RestoreSurfaces,
  SurfaceRecreated, GetSurfaceMemory, BlitRectClipped with int arguments), the enums in Surface.h and
  Surface.cpp's statistics (BlitCount, BlitPixels, Data71D0D8, LastBlitCount, LastBlitPixels,
  NumberOfSurfaces, InitStep 0x71d0bc). Evidence from this file added to them: +0x98 is SetSurfaceRect.
  IsDirect3D9Installed and CreateSurfaceDX9 are declared in Surface.h, because SurfaceDX.h's DirectX 6
  d3d.h sets DIRECT3D_VERSION 0x700 and empties a later d3d9.h.
- **Function names** (low unless the slot name came from #9 or Surface.h): IsDirect3D9Installed,
  CreateSurfaceDX9, AddQuad, AddShadeQuad, FlushVertices, SetTexture, SetRenderTarget, ApplyColorKey,
  FlushShade, EnumModesCallback; slot +0x98 SetSurfaceRect (TSurface's empty default renamed from Unknown98:
  surfaceDX9.cpp sets Rect[Surface] there).
- **FromRect and ToRect** (medium). The assertions of Blit, BlitRect and StretchBlit name
  "FromRect.width", "FromRect.height" and "ToRect.width": in the original those slots took rectangles whose
  members are width and height (like TSkin; graphics.h's TRect has Width and Height). The binary passes the
  members as separate words, so the slots keep TSurface's int parameters and the assertions are written with
  TASSERT_TEXT/TASSERT2_TEXT, which give the binary's text and values. Issue #29 asks whether TRect's
  members should be renamed.
  Settled in #29: TRect's members are width and height, the slots take TRect FromRect/ToRect (the callers
  build 16-byte blocks; "the binary passes the members as separate words" was wrong), and the assertions
  are plain TASSERT/TASSERT2 expressions with the binary's text.
- **Exception tables** (high). No function has TBEGIN/TEND: the frames hold only the assertions' strings
  (5 states each). ResetDisplay (0x4f1160) is try { InitDisplay; CreateSurfaces } catch (PROGRAM_ERROR) {
  try { ExitDisplay } catch (...) {} throw } (ehinfo: catch PROGRAM_ERROR at states 0-0, catch ... at 2-2).
  No switch has a jump table (Init's switch on ViewMode is a compare chain).
- **Odd behaviour kept**: FillRectangleClipped passes the stack slot of its Rect argument as ClipRect's
  second rectangle (the pixel count uses the unclipped size); SetSurfaceRect sets SurfaceArea.left only for
  surface 0 (an else without braces, by the look of it); WriteImage below SPRITE_SURFACE always writes into
  ScreenTexture and blits all of it to AUTOMAP_SURFACE with point filtering, keyed with ToSurface's colour;
  ApplyColorKey copies Rect[Surface] of the temporary 64x64 surface; RestoreSurfaces resets a device it can
  reset and then releases it and calls InitSurfaces; FinishFrame compares ViewMode before Present without
  using the result; InitDisplay with no listed modes reads before the NULL array; CreateSurfaces returns quietly
  when the alpha blending states fail, checks no other state, sets only dwSize of the display mode
  description, and its cache texture loop has a second exit for textures from the 25th on (16 are made);
  the mode callback tests DDSD_REFRESHRATE in the pixel format's flags; AddShadeQuad has no check for a full
  buffer; WaitForNextFrame's FPS uses an unsigned frame count.
- **callcheck** (13 differences, all explained): the slot calls (ClipRect, BlitRect, StretchBlitRect,
  WaitForNextFrame, InitDisplay, CreateSurfaces, ExitDisplay, InitSurfaces, ResetFrameTimer) are indirect in
  the binary, and the tool matches some of them with functions of the same name elsewhere (the device's
  Reset with DialogElements' 0x452790, SetFramerateLimit with SurfaceDX.obj's copy 0x4eb000); SetTexture is inlined into FlushShade,
  StretchBlitRect and StretchBlitUnclipped and IsDirect3D9Installed into TestEngine; the base constructor
  and destructor calls are not lexical calls.
- **Tests**. surfacedx9test.cpp (new): every round creates the image's surface with 0x4f1430 and ours with
  CreateSurfaceDX9, runs InitSurfaces and Init (windowed or full screen, forced or not, the device lost or
  not), then 20-80 random operations through the vtable (every slot, the image's object called through its
  own vtable) and ExitSurfaces and the destructors; the logs of every fake call, a dump of every member word
  of both objects (fake objects by kind and number), hashes of every fake object's memory and Surface.cpp's
  statistics are compared after each operation. The Windows functions are fakes on both sides (the image's
  import slots get the fakes, difftest.exe's own import slots are patched during the test, DirectDrawCreate
  is defined by the test); the Direct3D 9 and DirectDraw objects are per-side fakes with COM vtables that log
  every call, keep pixel and vertex memory and log a hash of the vertices each DrawPrimitive draws.
  TestEngine and IsDirect3D9Installed (with and without d3d9.dll) run every seventh round, the failing
  alpha state every eleventh. Not run: the error paths (assertions and PROGRAM_ERRORs; the image cannot
  throw in the child). Mutation check (build/scratch/dx9/mutate.py): 27 changes; 25 made the test fail, two
  are equivalent (BlitClipped ignores the colour key StretchBlit passes; at exactly 1 ms late,
  NextFrame = Now - 1.0 assigns the value NextFrame already has, so `>` and `>=` agree). An earlier run's
  heap corruption was the test's own input (a cache surface shrunk below the 106-pixel writes).
  DIFFTEST_ONLY=SurfaceDX9 runs it alone (DX9_TRACE=1, DX9_LOGS=<file>, DX9_ROUNDS=<n>).

## Resources (issue #17)

- **The pictures are not committed.** The two cursors and the icon are CipSoft's artwork, the other
  cursors and the two bitmaps Microsoft's (MFC). tools/extract_resources.py rebuilds them byte-exact
  from a local Tibia.exe into build/res (gitignored); src/res/Patella.rc refers to them by file name
  and is compiled with /I build\res. The text resources (version information, the MFC strings and
  the MFC dialog) are written out in the script.
- **MFC's resources are written out, not included.** Tibia.exe carries the afxres.rc of MFC 7.1 (with
  the splitter, tracker, OLE and property sheet parts switched off and no afxprint.rc). Including the
  installed MFC 14's afxres.rc would add resources and change texts, so Patella.rc has MFC 7.1's
  entries as the image has them. They are kept even where no code of the image refers to them, so that
  the resource set equals the original's.
- **Names.** The original resource.h is unknown. src/res/resource.h names only the client's resources:
  IDR_MAINFRAME (icon 128, AppWizard's usual name for it), IDC_CROSSHAIR (cursor 0x84) and
  IDC_MAGNIFIER (cursor 0x89), named after their pictures (MainFrm.cpp calls the 0x89 cursor
  HandCursor; left as it is, the picture is a magnifying glass). The MFC entries keep their numbers,
  with afxres.h names in comments only, so the header cannot clash with afxres.h once MFC is installed;
  the afxres.h names are from memory (MFC is not installed), the wheel cursors' names agree with their
  pictures. src/*.cpp keep their literal IDs (MAKEINTRESOURCE(128), 0x84, 0x89), as the binary does.
- **Order of the script.** rc.exe numbers the RT_CURSOR and RT_ICON images with one counter in script
  order; the original numbers (cursors 1-2 German, icons 3-4, MFC cursors 5-20) come out with the
  client's cursors first, then the icon, the version information and MFC's part in afxres.rc's order
  (the wheel cursors are not in ID order). This is also the order of the data in Tibia.exe.
- **Languages.** The two client cursors are German (1031), everything else English (US, 1033); the
  version information's string block is 000004b0 (language neutral, Unicode) with Translation 0, 1200.
- **No AFX_IDS_APP_TITLE (0xE000), no AFX_IDI_STD_FRAME/MDIFRAME (0x7A02/0x7A01).** MFC asks for them,
  the original has none and falls back to the module name and IDI_APPLICATION; Patella keeps them out.
- **Compare.** tools/compare_resources.py compares (type, name, language) and the data; not the
  directory time stamp, the code page field (0 in the image), the order of the data or the .res memory
  flags. The first compile with rc.exe 10.0.26100.0 matched all 55 entries; nothing in the script needed
  a workaround for rc.exe's formats (the version information's padding is the same).
- **Final check is #16's.** Patella.exe cannot be linked yet; notes/resources.md says how #16 adds the
  .res (no afxres.rc, /MANIFEST:NO) and runs compare_resources.py on Patella.exe.

## Unknown-owner data (issue #12)

Settled by Claude Opus 5.5, November 2, 2026. Method: the neighbours of each datum in .data, .rdata
and .bss (each object's contributions are contiguous, in link order), the code that references
those neighbours (objdump of the whole image), and the static initialisers listed per object in
the __xc table 0x5c0004-0x5c00ff. Names ours unless noted. No names.tsv or file-ranges.tsv rows:
both list code only (names.tsv has no data rows).

- **0x5c249c is Player.obj's copy of the text table** (high): TextCriticalError. The "table of UI
  texts 0x5c2480-0x5c2517" is not a table of its own but the tail of a full copy of the text
  header's table, 0x5c22cc-0x5c2517 (147 entries from "You are poisoned" to "Report Rule
  Violation", the same entries as MapWindow.obj's copy 0x5c2064-0x5c22af and Control.obj's
  0x5c0b34-0x5c0d7f). Every object that includes the text header has its own copy (the entries
  have internal linkage), so the object whose code reads an entry owns that copy: only 0x4d9ed5
  (Player.cpp's HandleException) reads it, and Player.cpp's LastMouseCursor (0x5c2518) follows
  directly, then the type descriptors of out_of_range/bad_alloc that Player.cpp's catch blocks
  need, then Preferences.cpp's WindowedWidth (0x5c2558). Like the other copies it stays declared
  in external.h, under "Player.obj's copy of the text table" (the text header is not
  reconstructed); playertest.cpp defines it for difftest.exe. No other entry of Player's copy is
  read (the crash report texts 0x5c24a0/0x5c24a4 are read through other objects' copies, if at all).
- **0x5c2564 is Surface.cpp's** (medium): CommandLineEngine, initialised to -1. It lies between
  Status.cpp's LinkPattern (0x5c2560, its only .data; Status.obj's copy of Status.h's since #33) and
  Surface.cpp's CacheSurfaceWidth/Height (0x5c2568/0x5c256c), so it is the end of Status.obj's .data or
  the start of Surface.obj's.
  Status.cpp never uses it; Surface.cpp's SurfaceInitInstance (0x4eac49) reads it to choose the
  engine (curEngine is Surface.cpp's too), Player.cpp's command-line parser (0x4da5c6) writes it.
  Defined in Surface.cpp, extern in Surface.h; Surface.cpp is linked into difftest.exe, so
  playertest.cpp's definition went.
  Reviewed in #24: confirmed for the behaviour: the .data word is -1 and ours is constant-initialised;
  owner medium.
- **The XOR code tables 0x5733a4/0x5733c4 are sysinc.cpp's** (low): TitleCode and ClassNameCode
  (names from the Player.cpp reconstruction), extern in sysinc.h, defined in sysinc.cpp;
  playertest.cpp keeps a test definition (sysinc.cpp is not in difftest.exe). They lie exactly
  between SurfaceGL.obj's last .rdata string ("Type>=0 && Type<3" 0x573390, read by 0x4fca8c) and
  sysinc.obj's first ("Unknown error." 0x5733f0, GetErrorText's), so the owner is one of the two
  (an object with data only and no code between them would be a file this project has never seen;
  rejected). Both are referenced only by Player.cpp's decoders (0x4d9270, 0x4d93e0). The deciding
  argument is the order within an object's .rdata: Player.obj's own file-scope data (after
  the "error.txt" literal 0x56dd8c: the key tables 0x56dd98/0x56ddb8, then the message map 0x56dde4)
  comes before the vtable and the strings of its functions, i.e. file-scope definitions are
  emitted where they are in the source. As SurfaceGL.cpp's, the tables would have to be defined
  after the last function that uses a string (TSurfaceGL's vtable 0x573250 and the strings of its
  later functions precede them); as sysinc.cpp's they are its first .rdata, defined at the top of
  the file as usual. If #11's reconstruction of SurfaceGL.cpp finds file-scope data at the end of
  that file, revisit this.
  Reviewed in #24: confirmed for the behaviour: constant tables with the binary's values; the owner stays
  low.
- **0x5c4558 is Control.cpp's NoCallback** (high): the static initialiser 0x559df0 constructs it as
  TCallback(NULL, 0) (0x496130, no atexit), nothing else references it. The __xc table lists one
  initialiser per object in link order: ContextMenus.obj's 0x559de0 (0x5c4508), then 0x559df0, then
  0x559e00 for 0x5cc418, then Dialogs.obj's 0x559e10 (0x5cc420). 0x5cc418 is the end of
  Creatures.obj's .bss (directly after the creature table 0x5c68b0-0x5cc417; DialogElements.obj
  has no statics), so Creatures.obj's NoCallback is 0x5cc418, and 0x5c4558, after Control.cpp's
  ShowFPS (0x5c4554) and before Creatures.cpp's first datum (0x5c4560), is Control.obj's. Both
  objects keep it at the end of their .bss, as ContentWindows.obj does (0x5c4500). Both are now
  written as static NoCallback (Control.cpp, Creatures.cpp; the second was not in the issue but
  is settled by the same evidence).
- **0x5d16b4 was already settled** (medium, issue #2): MainWindow.cpp's static NoCallback
  (initialiser 0x559f30, between MainFrm.obj's 0x559f20 and Map.obj's 0x559f40); the issue text
  was stale. Nothing changed.
  Reviewed in #24: confirmed (see Map.cpp's entry).
- **The text table "owner unknown, .data 0x5c0c80-0x5c0d7c"** in external.h is Control.obj's copy
  (0x5c0b34-0x5c0d7f, before Control.cpp's LastPacketType 0x5c0d8c; every entry declared there is
  read only by Control.cpp, 0x4499c8-0x449f1a): the comment says so now.
## SurfaceGL.cpp (issue #11)

Reconstructed by Claude Opus 5.5, November 2, 2026.

- **Range** (high). SurfaceGL.obj is 0x4f46f0-0x4fcb8f: the surfaceDX9/SurfaceGL gap settled in #10 plus
  the asserted functions after it, whose last one (SurfaceRecreated, line 1555) ends at 0x4fcb8f, where
  sysinc.cpp starts. Ghidra missed CreateSurfaces 0x4fb070, GetFullscreenModes 0x4fb4e0, InitSurfaces
  0x4fb700, Blit 0x4fc480, BlitRect 0x4fc6a0, GetSurfaceRect 0x4fc8a0, SurfaceRecreated 0x4fca30 and the
  slots 0x4f9b80/0x4f9bd0/0x4f9c10/0x4f9c40/0x4fa510/0x4fa8b0/0x4fab70; all come from the vtable 0x573250
  and were written from the instructions. 0x4f98a2 is not a function (Ghidra split SelectSurface). The
  object's data: .data 0x5c257c (the filter, GL_LINEAR) and 0x5c2580 (the selected surface, 1); .bss
  0x71d290-0x71d317 (dirty rectangle, pbuffer, main context and DC, SurfaceCreated[22]), the glh pointers
  0x71d318-0x71dba8, glh's strings 0x71dbac/0x71dbb0, the frame counter 0x71dbb4 and 0x71dbb8 (FPS time).
  0x71d2bc is referenced by nothing (padding or an unused static; left out). 0x5c2578 (256) is still
  unowned (#10).
- **glh_extensions.h** (high for the code, medium for the file layout). The leading run 0x4f46f0-0x4f954f
  is NVIDIA's glh_extensions.h with glh_genext.h (the memory error message keeps the path
  "Extern\glh/glh_extensions.h", lines 173 and 191), included with GLH_EXT_SINGLE_FILE: glh_init_extension
  (121 extension names, 549 entry points; a newer version of the header that knows GL_VERSION_1_2-1_5),
  the static ExtensionExists (extName passed in EBX: a register convention the compiler chose for the
  static function), glh_init_extensions and 0x4f9510, which frees the two strings. The NVIDIA SDK is not
  installed, so both headers are reconstructed in src/Extern/glh (the original's include path):
  glh_genext.h is generated from the binary (build/scratch/gl/glhparse.py parses the blocks of 0x4f46f0,
  genext.py writes the header; every name/slot pair is consistent, no slot unused). The pointers are PROC
  except the WGL_ARB_pbuffer/pixel_format ones SurfaceGL.cpp calls (their wglext.h types written out); the
  types do not change the code. A `#line 173 "Extern\\glh/glh_extensions.h"` gives the two CHECK_MEMORY
  messages the original's __FILE__/__LINE__. names.tsv kind stl (library code), the names are glh's
  except glh_shutdown_extensions (0x4f9510, ours, low; it sits right after glh_init_extensions, so it is
  the header's) and the prefix of glh_ExtensionExists. strdup is written _strdup (the same CRT function;
  the modern SDK deprecates the POSIX name). Odd behaviour kept: ExtensionExists' GL_VERSION_1_x answers
  leak padExtName.
  Reviewed in #24: confirmed: our compile pushes 173 and 191 for the two messages, as 0x4f92e2 and
  0x4f93ca do.
- **Class** (layout high, names low unless from the binary). TSurfaceGL, 0x498 bytes: Context +0x70, the
  textures TTextureGL Textures[22] +0x74 (0x20 bytes each: name, size, power-of-two size, the colour key;
  the inline constructor sets the key to (1,1,1) after TRGBColor's default constructor zeroed it), RECT
  Rect[22] +0x334, bool Recreated[3] +0x494. From TSurface it uses +0x0c as the window's HDC
  (Surface.h's Data0C renamed DeviceContext) and +0x28 as a RECT, the client area in screen coordinates
  (Data27[0x11] split into Data27 and ScreenRect). NUMBER_OF_SURFACES_GL, Type (SurfaceRecreated) and
  FromRect are from the assertions. Ours: Context, Textures, TTextureGL and its members, Recreated,
  ScreenRect, DeviceContext, IsOpenGLWorking (Surface.cpp's former FUN_004f9710), CreatePbuffer,
  ReleasePbuffer, CreateTexture, the statics.
- **TSurface slots** (medium). The empty TSurface slots +0x04, +0x08 and +0x80 are overridden only here:
  +0x04 SelectSurface(int Surface) (the drawing target), +0x08 SetTextureFilter(int Filter) (GL_LINEAR or
  GL_NEAREST), +0x80 AddDirtyRect(TRect Rect): the callers build a 16-byte TRect on the stack, so the
  parameter is a TRect (the former four ints had the same stack layout). Surface.h, Surface.cpp,
  names.tsv (TSurface_SelectSurface, TSurface_AddDirtyRect, TSurface_SetSurfaceRect for the old
  Unknown04/80/98 rows) and surfacedxtest.cpp's calls are renamed. The base slot names do not collide with
  TSurfaceDX9's non-virtual SetRenderTarget.
  Reviewed in #24: confirmed: a TRect by value is the callers' 16-byte block; surfacegltest compares the
  calls.
- **Folded copies** (high). Six slots of the vtable point to identical code kept from another object:
  ClipRect 0x4f0410 and ResetFrameTimer 0x4f04d0 (surfaceDX9.obj), SetFramerateLimit 0x4eb000
  (SurfaceDX.obj), CanShadeRectangles and TestEngine 0x4d7d40 (the return-true body folded with the STL's
  codecvt::do_always_noconv); they are written at the end of SurfaceGL.cpp, as surfaceDX9.cpp does. +0x50
  (SetColorKey), +0x64 (AttachWindow) and +0x98 (SetSurfaceRect) are TSurface's. The other way round,
  GetFPS 0x4f9d00, DrawFrame 0x4fa110, DrawFrameClipped 0x4fa2c0, BlitClipped 0x4fab70, GetCacheSurfaces
  0x4fb5e0 and IsUnavailable 0x4fb5f0 are this object's code in the DX5/DX9 vtables: SurfaceGL.cpp has its
  own versions, the same text as surfaceDX9.cpp's/SurfaceDX.cpp's (they agree: the copies of those files
  were tested against the same addresses). names.tsv's TSurfaceDX_* rows for them became TSurfaceGL_*.
  WaitForNextFrame 0x4f9c40 is the same code as surfaceDX9.cpp's with this file's own counters (not
  folded: other data). There is no display mode callback here: GetFullscreenModes uses
  EnumDisplaySettingsA.
- **Order**. Only WriteImage (767-768), WriteMaskedImage (802-803), BlitRectClipped (1023), Blit (1073),
  BlitRect (1086), GetSurfaceRect (1540) and SurfaceRecreated (1555) have lines; they are in binary order,
  so the file is in binary order.
- **EH tables and switches** (high). IsOpenGLWorking: try { ... } catch (...) { return false; } from
  CreatePbuffer to the return (the state never goes back to -1 before it). ResetDisplay: try { InitDisplay }
  catch (...) { try { ExitDisplay } catch (...) { throw 12 } throw 12 } (states 0 and 2). RestoreSurfaces:
  try { CreateSurfaces } catch (...) { throw 12 }. The asserted functions have only their assertions'
  states (10, 10, 5, 5, 5, 5, 5; the last four share one FuncInfo); no TBEGIN. No jump tables.
- **MainFrm.cpp's DisplayChanged** (high). 0x5d1670, set by OnDisplayChange, is read and cleared by Init
  (SurfaceRecreated(2) reports it): it is no longer static; MainFrm.h declares it.
- **Odd behaviour kept**: ReleasePbuffer and CreatePbuffer test the imported functions for NULL; after
  wglCreateContext CreatePbuffer tests the pbuffer again instead of the context; IsOpenGLWorking's early
  returns leave the context current; FillRectangleClipped passes an uninitialised RECT as ClipRect's second
  rectangle and counts the unclipped size; StretchBlitUnclipped's scaled path ignores both colours;
  StretchBlit selects CACHE_SURFACE before reading STRETCH_SURFACE back; WriteImage does not select
  AUTOMAP_SURFACE; CreateSurfaces does not check TEMP_SURFACE's result and has the second way out of the
  cache loop for i >= 24; CreateTexture leaves the texture contents undefined (an uncleared buffer) and
  counts in SurfaceCreated without a bound; InitDisplay sets the view mode after a failed switch back to
  the desktop, changes nothing after a failed full-screen switch and reads before an empty mode list;
  GetFullscreenModes never sets the description's size and does not store NumberOfModes; Init reads
  Resolution->Depth for the pixel format in windowed mode too and only reports missing extensions to the
  debugger; ExitSurfaces frees glh's strings with glh_shutdown_extensions' inlined code.
- **callcheck** (20 of 45 blocks differ, all explained): the slot calls are indirect in the binary
  (SelectSurface, AddDirtyRect, SetTextureFilter, ClipRect, FillRectangle(Clipped) in the frames,
  WaitForNextFrame, GetFullscreenModes, InitDisplay/ExitDisplay/CreateSurfaces/ResetDisplay/
  RestoreSurfaces/UpdateWindowRect in Init and ResetDisplay, ExitDisplay in ExitSurfaces); the base
  constructor/destructor calls are implicit (0x4fb670, 0x4f9860); TRGBColor's operator== is a call in the
  binary (0x48b380, WriteMaskedImage) but an operator here; glh_shutdown_extensions is inlined into
  ExitSurfaces in the binary. The functions Ghidra missed are not counted by the tool; they were compared
  with their instructions.
- **Tests**. surfacegltest.cpp (new): the opengl32/glu32 functions are never linked: the import symbols
  of ours are the test's GLSlot_ variables (/alternatename), gluOrtho2D is defined by the test; the
  user32/gdi32/kernel32/winmm functions are patched in difftest.exe's import table and the image's. The
  fakes log every call (pixels hashed, floating point as bits) and answer from one world that both sides
  start from. Our statics are found once by sentinel values (handles 0x7a5e300x, a filter, a surface
  rectangle) and glh's strings through glh_shutdown_extensions' code; every compare covers the logs, all
  object words, the statics of both files, the shared statistics, SurfaceCreated, DisplayChanged, glh's
  strings and the 549 extension pointers. Rounds: Init and 20-80 random operations of 46 kinds (every
  slot, CreateTexture with Empty, glh rounds with random extension/version strings and requests,
  IsOpenGLWorking failing at each step). Not run: the assertions and PROGRAM_ERRORs (CreateSurfaces throws
  only when the pbuffer fails; the image cannot throw in the child) and glh's out-of-memory exits. The
  test has its own random generator, so the later tests' inputs are unchanged.
  DIFFTEST_ONLY=SurfaceGL (GL_TRACE, GL_LOGS, GL_ROUNDS). Coverage was checked in the compared logs (every
  import and branch, among them the copy-back, the projection growth, the GetClientRect of SelectSurface,
  both "not supported" messages, the readback of StretchBlit, the version answers of ExtensionExists).
  Mutation check (build/scratch/gl/mutate.py, 120 rounds each): 28 changes in SurfaceGL.cpp and
  glh_extensions.h, all detected. Two survived the first run and showed weak inputs, since fixed: the fake
  wglChoosePixelFormatARB stopped logging the attribute list at the first 0 value instead of the first 0 key,
  and GL_VERSION_1_4 was rarely requested under a "1.3" version string.
- **Test surfaces**. The window and graphics tests used external.h's TSurfaceGL sketch as their recording
  surface; it is tools/difftest/testsurface.h's TTestSurface now (the same members and size; the slots
  graphicstest.cpp and mapwindowtest.cpp define stay there, the never-called ones abort inline instead of
  playertest.cpp's stubs, and FUN_004f9710's stub is gone). graphicstest.cpp had taken the recording
  surface from our GetSurface (which created the sketch with new for curEngine 1); it now creates the
  TTestSurface itself and patches our GetSurface for good to return it, which mapwindowtest.cpp then takes
  as before. Every test's check count in the full run is unchanged (DialogWindow 24,567, ContextMenus 33,088,
  ConsoleWindow 30,948, ContentWindows 52,506, DialogElements 135,478, Dialogs 98,889, Graphics 570,954,
  MapWindow 115,169, MainWindow 14,164); the full run passes 14,068,269 checks with SurfaceGL's 53,082.
## Header code (issue #13)

Claude Opus 5.5, November 2, 2026. glh_extensions.h's 0x4f9260 is left to #11 (SurfaceGL.cpp).

- **Every header function was in src/ already** (high). The earlier file issues wrote each one where
  its object needed it: Container.h's templates (Status/Preferences/Talk/Objects/Map), GUI.h's
  GetFirstChild/GetLastChild (0x417de0/0x417f00, TERROR "not possible", lines 681-682),
  ConsoleWindow.h's TLineFormat constructors (0x418020, lines 47-54, and the inline default one),
  Utils.h's TBuffer::SetSize (0x4ca300, line 50), Talk.h's TTalkBuffer constructor, GetText,
  GetChannelID and TRuleViolationChannel::AddText (line 321). Issue #13 checked each against its
  disassembly and exception table again; nothing in the code changed. No .cpp or external.h kept a
  hand-written copy of a header member (every vector/array/matrix user includes Container.h).
- **One template, every instance** (high). All 13 vector<T> instances compile from the one
  Container.h template: the four initialized constructors 0x425e70 (TSkinButton *), 0x453300 (int),
  0x4973f0 (TTimer *) and 0x4fd6e0 (TTalkFilter *) are the same instructions apart from addresses
  (one copy per object: functions with exception tables are not folded), lines 355-356, ten EH
  states. The plain constructors have ten states for elements without a constructor (int,
  TContentWindowSize, TBuddy, TMessage) and eleven for those with one (TLineFormat, TMark,
  TListBoxLine, TIgnoreEntry, THotkey, TErrorReport): the extra state frees the new[] block if an
  element constructor throws; init is default-constructed first (THotkey: TCallback's constructor
  clears +0x1c/+0x20 before the first store). vector<THotkey>::operator() 0x496570 grows with new[]
  (TCallback's constructor on every new entry), copies 12-byte entries by swapping and frees with
  operator delete[] (0x523812): the template's code. The out-of-line destructors are delete[] of the
  entry pointer, whose offset gives the element size (0x1c: 4 bytes, 0x30: 0x18, 0x34: 0x1f, 0x88:
  0x70, matrix's 0x10).
- **names.tsv** (high). The three low rows of GUI.obj's instances (0x496570, 0x4973f0, 0x4976f0) and
  the four medium destructor rows are high now; vector_talkfilter_ctor is
  vector_TTalkFilterPtr_ctor_init and vector_TTimerPtr_ctor vector_TTimerPtr_ctor_init, like
  vector_int_ctor_init and vector_TSkinButtonPtr_ctor_init. Rows keep C identifiers (simplify.py
  substitutes them into the pseudocode and callcheck.py reads Class_method); the note of each row
  names the instance as Container.h writes it, e.g. vector<TTalkFilter *>::vector(min, max, block, init).
- **tools/triage.py's header table** (medium for the rule). A header function counts as written when
  src/ names its address (the "// 0x..." comments next to the definitions); names.tsv rows of kind stl
  count as not source. notes/source-map.md now lists only what is left (glh_extensions.h 0x4f9260).
  The rule is textual: a comment that names an address without defining the function would hide it.
- **Coverage** (high). A temporary build put an INT3 on the first byte of each image instance and of
  each of our compiled instances (addresses from the link map), counted the first hit per test and
  re-armed at the next test (build/scratch/cover, not committed); the full suite passed with it
  (14,015,187 checks, 0 failures). Every image instance was reached by its users' tests (ConsoleWindow,
  ContentWindows, Map, DialogElements, Dialogs, GUI, Help, Preferences, PreferencesFile, Talk,
  Objects, Creatures, Graphics, MapWindow, Status) except the out-of-line destructors 0x425b10,
  0x452c50, 0x452c60, 0x46bb60, 0x4b0b20 (only unwind funclets and atexit thunks call them),
  TBuffer::SetSize 0x4ca300 and the three functions that only throw (0x417de0, 0x417f00, 0x5075c0).
  Our static vectors' constructors run before main (the image's are called explicitly by the tests).
- **containertest.cpp** (DIFFTEST_ONLY=Container, before TestMapWindow): every image instance of
  vector, array, matrix and matrix3d next to ours on random sizes, blocks and init values; after each
  call the objects (all bytes but the entry pointer) and the whole allocated arrays are compared,
  operator()'s returned entry as an index. Both sides' operator new[] fill new memory with the same
  random pattern (as contentwindowstest.cpp does), so entries neither side initialises agree; the
  vector objects get a different pattern than their entries (otherwise an entry wrongly set from an
  uninitialised init looks like a fresh one: the first mutation run missed exactly that). The
  out-of-line destructors run on the image's objects. Also TLineFormat's constructor 0x418020,
  TBuffer::SetSize and GetChannelID 0x507060. Not run: the assertion and index-error paths (the
  image cannot throw in the child, its stringstream cannot run) and the three throwing functions.
  It draws from its own generator, so the later tests (MapWindow, MainWindow) get the same inputs
  as before: the full run is the old total plus its 192,832 checks.
- **Mutation check** (build/scratch/mutate13.py: each mutation rebuilds difftest.exe completely, since
  every object instantiates the templates and the linker keeps one COMDAT, then runs
  DIFFTEST_ONLY=Container): eight changes (the initialized constructor's last entry, initialized in
  the plain constructor, block 0 growing down, filling below min without init, growing up dropping the
  last entry, matrix3d's layout, matrix's ymin, array's entry), all detected after the pattern split.
  An equivalent change exists: dropping `entry[j - start] = h` in the growth loops (the old array is
  freed right after).

## Library region (issue #15)

Classified by Claude Opus 5.5, November 2, 2026.

- **Scope** (high). The library region is symbols.tsv's 1,102 functions of kind library from 0x50eae8
  (after vlong.obj) to the end of .text; the 14,716 Unwind@ funclets after 0x52fc20 are the whole
  program's EH cleanup code (kind eh-funclet) and are not counted. Every one of the 1,102 has a
  names.tsv row now (kind stl, the library first in the note; one row of kind game). Ghidra missed
  further code in MFC and the CRT (342 addresses that code or data refers to are no function start in
  symbols.tsv: vtable slots and SEH filters and handlers inside Ghidra's function bodies); they belong to the same
  library objects as their neighbours (file-ranges.tsv) and have no rows of their own.
- **Counts** (names.tsv notes): MFC 7.1 523, CRT 441, C++ library 58, ATL 7.1 45, Henry Spencer's
  regexp 15, import thunks 8, delay-load 6, Windows SDK multimon.h stubs 5, game code 1. By object block
  (file-ranges.tsv, symbols.tsv's file column): nafxcw.lib 569 (MFC with the ATL string templates and
  the multimon stubs it instantiates), libcmt.lib 441, libcpmt.lib 58, ATL 7.1 4 (CAtlBaseModule),
  delay-load 6, import thunks 8, regexp.c 15, Inventory.cpp 1.
- **How** (high for the named, medium/low for some helper names). Tibia.exe imports no CRT or MFC DLL,
  so the CRT is the static multithreaded libcmt.lib (with _mtinit/_getptd), MFC the static ANSI
  nafxcw.lib, the C++ library libcpmt.lib ("Copyright (c) 1992-2001 by P.J. Plauger" at 0x5c42e2). 850
  functions carry Ghidra's FID names (VS 2003 libraries); they keep them, made identifiers
  (`CWnd::~CWnd` -> CWnd_dtor, templates dropped, one leading underscore of C names dropped; duplicates
  get the address). The 252 others were classified by script (build/scratch/rx/classify.py) and by
  hand: their neighbours' library, RTTI of the vtables they set or sit in (scalar deleting destructors,
  destructors, vtable slots), message-map entries, __finally blocks (`push n; call _unlock`) named after
  their function, throw infos (AfxThrowMemory/InvalidArg/Resource/UserException), and the code itself
  for the ones game code calls (strstr, strchr, srand, localeconv, _ftol2, floor, rename, remove,
  _unlink, _fileno, bad_cast, std::_Lockit, std::_Mutex, _Mtxinit..., _String_base::_Xlen/_Xran,
  CWnd::Get/SetWindowPlacement, AfxGetAfxWndProc, AfxGetThread, CFrameWnd::OnSize, CAsyncSocket's
  constructor, AfxThrowOleException). 36 helper names stay low (MFC_<address>, <Class>_vslot_<n>):
  the library is certain, the function within it is not identified; none is called by game code.
- **atoi** (high). 0x51262a, called by the game (Dialogs, Player, Preferences, vlong), is `jmp atol`:
  atox.c's atoi. names.tsv called it atol; it is atoi now, 0x5125a2 is atol. The sources still write
  atol (same result on Win32): issue #30.
- **Game code in the region** (high). 0x559ea0 (Ghidra found it after the EH funclets) is Inventory.obj's
  dynamic initialiser of Containers (0x5cedd8), like the other objects' initialisers 0x559df0-0x559f81;
  file-ranges.tsv gives it to Inventory.cpp, tools/triage.py now applies file-ranges.tsv to library
  rows as well and makes such a row kind game. No other CipSoft code sits in the region: no zlib, MD5
  or other third-party strings; the sprintf/_snprintf family is the CRT's.
- **_fileno** (high). 0x523017 is the CRT's _fileno although the linker placed it among the MFC objects
  (after AfxInitialize); a one-function libcmt.lib row in file-ranges.tsv.
- **regexp: origin and version** (high). 0x50eb00-0x50fbb8 is Henry Spencer's regexp(3) of 1986 (the
  University of Toronto version posted to Usenet): its 19 messages are that version's ("NULL argument",
  "regexp too big", "out of space", "too many ()", "unmatched ()", "junk on end", "*+ operand could be
  empty", "nested *?+", "invalid [] range", "unmatched []", "internal urp", "?+* follows nothing",
  "trailing \\", "internal disaster", "NULL parameter", "corrupted program", "memory corruption",
  "corrupted pointers", "internal foulup"), MAGIC is 0234 (0x9c), NSUBEXP 10 (OPEN 20, CLOSE 30, the
  jump tables end at CLOSE+9), sizeof(regexp) 0x60 (the 1986 layout: startp, endp, regstart, reganch,
  regmust, regmlen, program), the size limit 32767 and `regsize` counting as in the original. Later
  versions differ in visible ways the binary does not have (the 4.4BSD/Tcl/Vim descendants rename the
  error function, add const, regsub's "damaged regexp fed to regsub" and regerror's "regexp(3): %s"
  belong to separate files). Not in the client: regsub.c, regerror.c, DEBUG's regdump/regprop/regnarrate
  and the STRCSPN fallback (the CRT's strcspn 0x511ee0 is called). The text was written from memory of
  the original (nothing was downloaded) and checked function by function against the assembly
  (build/scratch/rx/rx.asm): every node, flag, test, jump table (regatom 0x50f990/0x50f9b4, regmatch
  0x50efdc/0x50f010, regrepeat 0x50ec88) and message agrees. No function has EH tables.
- **regexp: the client's changes** (the behaviour high, the form low). (1) regerror: every message
  goes through `if (p != NULL) p(message)` with p the .bss word 0x71f034, which nothing writes, so a
  failing regcomp returns NULL and regexec 0 silently. Written as a static regerror in regexp.c calling
  the global `regerrorhandler` (our name; it may have been static in the original); the call sites keep
  the original text. (2) Includes: the client's own "regexp.h" and <string.h>/<stdlib.h> in place of the
  K&R `extern char *malloc();`/`extern char *strchr();` declarations (modern headers reject them with
  malloc's prototype; the binary calls the CRT's functions directly, strlen is inlined). (3) regexp.h has
  prototypes with extern "C" for the C++ callers (the original's K&R declarations mean no parameters in
  C++); regsub and regerror are no longer declared. The file is compiled as C (src/Extern/regexp/
  regexp.c, K&R definitions kept); `#pragma warning(disable: 4018)` keeps /W3 quiet about the original's
  unsigned `strlen(...) >= len`. The order of the source is the original's; the binary's order is the
  compiler's (callees first), and its static functions take register arguments (regc in CL, regnext
  and regrepeat in EAX, regtail in EAX/ECX, regtry in ESI/EBX ...), which our compiler chooses itself.
- **regexp: data** (high). .bss 0x71f010 regdummy, 0x71f014 regsize, 0x71f018 regnpar, 0x71f01c regbol,
  0x71f020 regstartp, 0x71f024 regparse, 0x71f028 regcode, 0x71f02c regendp, 0x71f030 reginput,
  0x71f034 the error hook; .rdata 0x574094-0x5741e1 the messages and META. The object is 16-byte
  aligned like the game's (the libraries are not) and follows vlong.obj, so it is a project file (in a
  subfolder: src/Extern/regexp like glh's Extern\glh). Licence: Spencer's 1986 terms (redistribution
  allowed, altered versions marked) are in the file; the alteration is marked at its top.
- **Callers**. external.h's regcomp/regexec declarations are gone; Status.cpp includes
  Extern/regexp/regexp.h (the names were already the original's). statustest.cpp no longer forwards our
  regcomp/regexec to the image: difftest.exe links regexp.c, so Status.cpp's tests run ours.
- **tools/callcheck.py** (medium). names.tsv's new library rows would have made callcheck count calls
  such as memcpy, strlen, operator new and strcpy in our sources while the pseudocode keeps Ghidra's
  names (_memcpy, ...) or inlines them; a library-region name now counts only when Ghidra left the
  function unnamed and this file's pseudocode calls it by that name. Every file's count is the same or
  lower than before (Dialogs, Preferences, Script and sysinc lose the old atol/_fileno/rename/remove/
  srand differences).
- **Tests**. tools/difftest/regexptest.cpp (new, DIFFTEST_ONLY=Regexp, RX_SEED, RX_ROUNDS, RX_TRACE):
  both regcomps on hand-made patterns for every message and boundary (9 and 10 groups, ranges with ]
  and -, invalid ranges, quantifier errors, backslashes, chars above 0x7f, the size limit at exactly
  32766/32767), random strings of pattern characters, patterns from a small grammar and the client's
  LinkPattern (the image's 0x5c2560) with URL-like texts; compared: NULL or not, the messages, the
  allocation size, regstart/reganch/regmust/regmlen and every program byte; regexec on both programs
  (own and cross) with the result, the messages and all ten startp/endp (from a common sentinel), plus
  hand-made corrupted programs (wrong magic, unknown opcodes, a missing next node, STAR/PLUS of a
  BRANCH or BACK) and NULL arguments; a quarter of the rounds with both hooks NULL as in the client.
  229,958 checks, 0 failures. Coverage (RX_TRACE): every opcode including OPEN1-9/CLOSE1-9, regmust
  with rejections, anchored and regstart programs; every message except four: "junk on end",
  "internal urp" and "internal disaster" cannot happen (the original says so for the first; regpiece is
  never called on '\0', '|' or ')', and strcspn is at least 1 for a non-META character), "out of space"
  needs a failing malloc (not simulated). Mutation check (build/scratch/rx/rxmutate.py, 1,500 rounds): 44
  changes in every function, 41 detected; the three survivors are equivalent: SIMPLE for a two-character
  EXACTLY (regatom backs off before a quantifier, so a multi-character node is never a STAR/PLUS operand),
  reganch's shortcut in regexec (an anchored program fails at every later position anyway and leaves the
  same NULL startp/endp) and ANYBUT's `*reginput == '\0'` test (strchr finds the terminator, so the
  remaining test fails too).
## Review of the low-confidence decisions (issue #24)

Every low or medium entry above that could change behaviour, checked against the binary or a test.
Entries whose low part is only a name, a spelling or a file name are left out (names only: the Script.cpp
patch names, Talk.cpp's table, Objects.cpp's TProperties and flag names, the Preferences/Script gap,
ContextMenus/Control and start/Communication gaps, sysinc.cpp, Help.cpp's names, Inventory.cpp's function
names, surfaceDX9.cpp, Resources, Header code and every "Names"/"Classes" entry). Skipped as other issues':
TRect width/height and FromRect/ToRect (#29), SetChildArea's TRect (#27). Line numbers are those before this
review. (Settled in #29: see "TRect's members (issue #29)" below. Settled in #27: see "SetChildArea
(issue #27)" below.)

Tools (new): tools/xctable.py lists the __xc initialiser table with what each initialiser builds and the
atexit thunk it registers; tools\listing.cmd compiles src files like compile-check.cmd with assembly listings
(build\listing); tools/ehcompare.py prints ehinfo.py next to the unwind states and try blocks of our listing.

- **Static initialisers and destructors at exit**: our listings' dynamic initialisers (??__E) and atexit
  destructors (??__F) per object against the binary's __xc table 0x5c002c-0x5c00c4: the same initialisers in
  the same order, with an atexit destructor exactly where the binary registers one (GUI's Timers/Hotkeys,
  Help, Map's three, Network's TXTEA, Player, Preferences' five, Status' two, Talk's four, TRSA), none for
  the TCallbacks, SkinProperties, Inventory's three (their TObjects are zeroed, as by 0x559ea0/0x559ee0/
  0x559f00), the network buffers and StartTime, after two corrections: MainFrm.obj's TCallback 0x5d1674
  (0x559f20) was missing (now MainFrm.cpp's static NoCallback), and the unused `vlong(65537)` with an atexit
  destructor that Communication.obj (0x5c435c, 0x559da0) and TRSA.obj (0x71eff8, 0x55a1c0, right before
  TRSA.cpp's PublicExponent 0x71f004) construct was missing: one copy per includer of TRSA.h (vlong.obj has
  none, so not vlong.h), now `static vlong HeaderExponent(65537)` in TRSA.h (name ours). Both are unused data:
  no differential test can see them.

Result: 80 entries reviewed, none left open: 79 confirmed (8 of them settled by later issues that replaced
the guess, 15 without any possible effect on behaviour, one with a new test: nextToken's length limits in
scripttest.cpp), one confirmed with its explanation corrected (Map.cpp's TObject initialisers). Source
changes: MainFrm.cpp's NoCallback, TRSA.h's HeaderExponent, the TOffer size comment in Inventory.h.

| Line | Section | Entry | Verdict | Evidence | Change |
|---|---|---|---|---|---|
| 44 | Script.cpp | nextToken's string length check | confirmed + test | 0x4e0203 cmp Size,0x3e7 after every state; 0x4dfde1 cmp ebx,0x14 | scripttest.cpp TestLongTokens |
| 163 | Talk.cpp | Allocation and destruction | confirmed | talktest (behaviour); syntax only is low | - |
| 173 | Talk.cpp | ABI and field types | confirmed | Time only stored and passed to _localtime32; hidden-return ABI | - |
| 220 | Objects.cpp | NoObject 0x719da0 is Objects.cpp's | confirmed (behaviour) | .bss, no initialiser, never written | - |
| 225 | Objects.cpp | Container.h array<T> destructor | confirmed | atexit 0x55a2c0 frees array<TMapPoint>'s entry (+8); ReleaseObjects | - |
| 250 | Objects.cpp | Parameter passing | confirmed | references and pointers are the same ABI; objectstest | - |
| 255 | Objects.cpp | Obj.Data2 holds the hook flag | confirmed | 0x4d6f0f mov eax,[esi+8]; sub eax,0x12; je; dec eax; je | - |
| 305 | Player.cpp | Member functions of CPlayerApp | confirmed (behaviour) | ReadErrorFile writes ECX before reading it; FatalError only push ecx | - |
| 382 | Surface.cpp | TSurface slot parameter lists | confirmed (superseded) | slots typed and tested by issues #9-#11 | - |
| 393 | Surface.cpp | Removed try block in SurfaceInitInstance | confirmed | tools/ehcompare.py (our /EHsc compile): 4 states, 1 try block in both | - |
| 448 | Help.cpp | vector<int> destructor at exit | confirmed (superseded) | atexit 0x55a2b0 frees 0x5ce558; Container.h ~vector since #13 | - |
| 453 | Inventory.cpp | CounterOffer 4 bytes after OwnOffer | confirmed | no reference to 0x5d116c-0x5d116f | Inventory.h comment |
| 486 | MainFrm.cpp | PacketHeader 0x5d1644 is MainFrm.cpp's | confirmed (behaviour) | .bss, no initialiser or atexit | MainFrm.cpp NoCallback 0x5d1674 |
| 494 | MainFrm.cpp | Eliminated try blocks | confirmed | tools/ehcompare.py (our /EHsc compile): 11 states, no try block in all three | - |
| 526 | Creatures.cpp | Trailing gap: template destructor open question | confirmed (superseded) | Container.h ~vector since #13 | - |
| 542 | Creatures.cpp | Static initialisers (NoCallback objects) | confirmed | the __xc table 0x5c002c-0x5c00c4: TCallback(NULL,0), no atexit, one per object | MainFrm.cpp NoCallback |
| 548 | Creatures.cpp | Source order | confirmed (no behaviour) | order of definitions; creaturestest | - |
| 579 | Creatures.cpp | Dropped try blocks | confirmed | tools/ehcompare.py (our /EHsc compile): AddCreature 16, UpdateSkulls 11 states, no try block | - |
| 597 | Creatures.cpp | Types | confirmed | 0x40000000 compares jb/sbb; SkullTime jl/jle | - |
| 621 | Map.cpp | 0x5d16b4 left to MainWindow.cpp | confirmed | initialiser 0x559f30 between MainFrm's and Map's | - |
| 660 | Map.cpp | TObject's default constructor clears it | confirmed; explanation corrected | 0x4a4450 clears 3 words; 0x559ee0/0x559f00 zero the offers' TObjects | note |
| 668 | Map.cpp | TRGBColor constructors | confirmed | store order irrelevant; maptest/graphicstest | - |
| 676 | Map.cpp | Colours by value | confirmed | hidden return pointer = by value | - |
| 705 | Map.cpp | Parameter forms | confirmed | references and pointers are the same ABI; maptest | - |
| 724 | Map.cpp | Locals that the catch blocks print | confirmed (error path only) | only TEND1(i)'s value on an error path | - |
| 730 | Map.cpp | TEffect union | confirmed | offsets from the accessors; maptest; no type-based aliasing in MSVC | - |
| 767 | graphics.cpp | DialogWindow/graphics gap (header placement) | confirmed (no behaviour) | placement of identical inline copies | - |
| 806 | graphics.cpp | Surface slots +0x50/+0x54 parameters | confirmed | sub esp,0xc copies = TRGBColor by value; graphicstest | - |
| 815 | graphics.cpp | TVideoCacheItem's overlapping fields | confirmed | 0x48b3f0 store order; graphicstest memcmp of every item | - |
| 823 | graphics.cpp | TRGBColor's default constructor zeroes | confirmed | DrawOutfit overwrites the array before use; SkinProperties constructor byte for byte | - |
| 830 | graphics.cpp | Dropped code with exception states left | confirmed | tools/ehcompare.py (our /EHsc compile): same states, no try block; DrawPicturePart 26 states, try 15-15 | - |
| 865 | graphics.cpp | Data not static | confirmed (no behaviour) | linkage only | - |
| 871 | graphics.cpp | DrawPicturePartClipped's clip TRect by value | confirmed | sub esp,0x10 and four stores | - |
| 946 | GUI.cpp | FitToChildren/FitToChild take a TRect by value | confirmed | 16-byte block; any 16-byte struct is the same ABI | - |
| 956 | GUI.cpp | NumberOfIcons as Data490 | confirmed (no behaviour) | same offset; SkinProperties byte for byte | - |
| 973 | GUI.cpp | TFont() and DefaultFontColor | confirmed | 0x5c1954, 0x5c1230, 0x5c066c all (224,224,224) | - |
| 1041 | MapWindow.cpp | NoCallback 0x7152e8 form | confirmed | the __xc table 0x5c002c-0x5c00c4 | - |
| 1078 | MapWindow.cpp | TObject converts to int | confirmed | compile-check; mapwindowtest and the other TObject users | - |
| 1098 | MapWindow.cpp | TDrawing constructors | confirmed | new TDrawing[8] zeroing loop at 0x4c1ab4; window dumps | - |
| 1101 | MapWindow.cpp | Surface slots (+0x9c interpretation) | confirmed (no behaviour) | code mirrors the binary; mapwindowtest | - |
| 1117 | MapWindow.cpp | Limit | confirmed | clamp in either order; window tests | - |
| 1194 | MainWindow.cpp | NoCallback 0x5d16b4 form | confirmed | the __xc table 0x5c002c-0x5c00c4 | - |
| 1229 | MainWindow.cpp | Source order | confirmed (no behaviour) | order of definitions | - |
| 1261 | MainWindow.cpp | Evaluation order | confirmed | the reads between are getters; new[] 0x523817; mainwindowtest | - |
| 1267 | MainWindow.cpp | SkinProperties.Style0D8 retyped | confirmed | guitest: constructor and InitSkinProperties byte for byte | - |
| 1278 | MainWindow.cpp | External sketches | superseded | replaced by the real classes (#3-#8) | - |
| 1321 | DialogWindow.cpp | NoCallback 0x5cc428 form | confirmed | the __xc table 0x5c002c-0x5c00c4 | - |
| 1351 | DialogWindow.cpp | No inline header code | confirmed (no behaviour) | placement of identical code | - |
| 1360 | DialogWindow.cpp | Source order | confirmed (no behaviour) | order of definitions | - |
| 1385 | DialogWindow.cpp | External sketches | superseded | real classes (#6, #7) | - |
| 1436 | ContextMenus.cpp | NoCallback form | confirmed | the __xc table 0x5c002c-0x5c00c4 | - |
| 1460 | ContextMenus.cpp | Inline header functions | confirmed (no behaviour) | placement of identical code | - |
| 1468 | ContextMenus.cpp | Source order | confirmed (no behaviour) | order of definitions | - |
| 1502 | ContextMenus.cpp | Text buffers | confirmed (no behaviour) | sprintf of names <= 29 chars + <= 11 fixed; _snprintf limit 200 | - |
| 1505 | ContextMenus.cpp | Data types changed elsewhere | confirmed / superseded | TRect by value = same block; sketches replaced (#5, #7) | - |
| 1543 | ConsoleWindow.cpp | Range (In/TRGBColor() copies) | confirmed (no behaviour) | identical COMDAT copies | - |
| 1573 | ConsoleWindow.cpp | NoCallback form | confirmed | the __xc table 0x5c002c-0x5c00c4 | - |
| 1611 | ConsoleWindow.cpp | Source order | confirmed (no behaviour) | order of definitions | - |
| 1617 | ConsoleWindow.cpp | Arrange's inlined SetCurrentLineNumber(-1) | confirmed | the same store; consolewindowtest | - |
| 1660 | ConsoleWindow.cpp | SkinProperties.Style9E8 retyped | confirmed | guitest byte for byte | - |
| 1667 | ConsoleWindow.cpp | DialogElements.cpp sketches | superseded | real classes (#7) | - |
| 1786 | ContentWindows.cpp | TSubWindowBody | confirmed | folded 0x425a00 returns true; contentwindowstest | - |
| 1800 | ContentWindows.cpp | Limit (header) | confirmed | 0x425920; tests | - |
| 1805 | ContentWindows.cpp | Source order | confirmed (no behaviour) | order of definitions | - |
| 1841 | ContentWindows.cpp | SkinProperties retyped | confirmed | guitest byte for byte | - |
| 1855 | ContentWindows.cpp | Surface slots named | confirmed | pushed arguments; recorded surface | - |
| 1859 | ContentWindows.cpp | DialogElements.cpp sketches | superseded | real classes (#7) | - |
| 1981 | DialogElements.cpp | Names (parameter roles) | confirmed | dialogelementstest | - |
| 2005 | DialogElements.cpp | Header code | confirmed (no behaviour) | placement of identical code | - |
| 2047 | DialogElements.cpp | SkinProperties retyped | confirmed | guitest byte for byte | - |
| 2117 | SurfaceDX.cpp | TBlitSize and the factory | confirmed | surfacedxtest | - |
| 2140 | SurfaceDX.cpp | Shared data ownership | confirmed (behaviour) | .bss zero; 0x5c2568/6c = 0x100; no initialisers | - |
| 2251 | Dialogs.cpp | Leading run (CompareNames, TIgnoreEntry) | confirmed | 0x46c0d0 clears Name[0] and Permanent; containertest | - |
| 2297 | Dialogs.cpp | Creators' parameter types | confirmed | pointers/arrays same ABI; dialogstest | - |
| 2345 | Dialogs.cpp | SkinProperties retyped | confirmed | guitest byte for byte | - |
| 2554 | Unknown-owner data | 0x5c2564 is Surface.cpp's | confirmed (behaviour) | .data word 0xffffffff; no initialiser | - |
| 2561 | Unknown-owner data | XOR tables are sysinc.cpp's | confirmed (behaviour) | .rdata constants; owner only | - |
| 2586 | Unknown-owner data | 0x5d16b4 already settled | confirmed | the __xc table 0x5c002c-0x5c00c4 | - |
| 2607 | SurfaceGL.cpp | glh_extensions.h #line | confirmed | our listing pushes 173 and 191 like 0x4f92e2/0x4f93ca | - |
| 2632 | SurfaceGL.cpp | TSurface slots +0x04/+0x08/+0x80 | confirmed | AddDirtyRect's TRect = the 16-byte block; surfacegltest | - |
| - | Communication/TRSA | unused vlong(65537) statics (not in the log before) | corrected | the __xc table 0x5c002c-0x5c00c4: 0x559da0, 0x55a1c0 | TRSA.h HeaderExponent |

## TRect's members (issue #29)

Claude Opus 5.5, November 2, 2026.

- **Evidence** (every assertion string in Tibia.exe that names a member of a rectangle, from a scan of the
  printable strings for width/height/.x/.y/Rect/skin, build/scratch/slotcalls.py and directcalls.py for the
  call sites):
  - "FromRect.width>0 && FromRect.height>0" (0x56f2a4) with the value names "FromRect.width" and
    "FromRect.height" (0x56f5cc/0x56f5dc): the Blit and BlitRect slots of all three engines (SurfaceDX.cpp
    lines 811/824, surfaceDX9.cpp 1302/1315, SurfaceGL.cpp 1073/1086). BlitRect already took a TRect, so
    FromRect is a TRect, and its size members are width and height.
  - "FromRect.width+1<=Rect[TEMP_SURFACE].right && FromRect.height+1<=..." and "ToRect.width<=
    Rect[STRETCH_SURFACE].right && ToRect.height<=..." (0x56f178/0x56f1d8): StretchBlitUnclipped and
    StretchBlit of SurfaceDX.cpp and StretchBlit of surfaceDX9.cpp.
  - TRect members of SkinProperties named with lower-case .x/.y: "x+SkinProperties.StatusWindow.
    HitpointsBar.x>=0" (and ManaBar), "y+ItemY+SkinProperties.BuddylistWindow.Logo.y>=0" (all three are
    TRects in GUI.h; Buttons.Texture and CheckBox.Item are TGUIPoints).
  - The other rectangles of the client use the same lower-case names: TSkin ("Skins[skin].width",
    "Skins[skin].height") and TDrawing ("DrawingList.x", ".y", ".width", ".height", "DrawingList[i].skin").
  - No string names a rectangle member Width or Height. The capitalised Width/Height strings are
    parameters and locals ("Width>0", "x>=0 && y>=0 && Width>0 && Height>0" of WriteImage, "_Width"),
    TComponent's getWidth()/getHeight() and Objects.cpp's "Entry.Width"/"Entry.Height" (not rectangles).
- **TRect is {x, y, width, height}** (high for width/height, which the assertions name on a TRect parameter;
  x and y were lower case already). graphics.h's members and its constructor's parameters were renamed.
  The rename was compiler-checked: graphics.h's members renamed, every src file and every difftest file
  compiled with column numbers (build/scratch/cc.cmd), and each "'Width': is not a member of 'TRect'"
  error renamed at its exact line and column (build/scratch/fixmembers.py) until the build was clean, so no
  Width/Height of another type (TComponent, TSurface, TFullscreenMode, TFont, the window sizes, the
  parameters) was touched: 906 uses in 23 files (11 in src, 12 in tools/difftest), plus two comments
  (GUI.cpp's FitToChild, DialogWindow.h's Area). No assertion text changed: no TASSERT stringified a TRect member.
- **TSkin stays its own struct** (low). It has TRect's layout and now its member names, but nothing in the
  binary uses a TSkin as a TRect: the callers of DrawPicturePart (0x490c60) push a skin's four members as
  separate ints (eight pushes, no 16-byte block), DrawSkin takes the skin number, and no code copies a skin
  into a TRect parameter. A typedef or a TRect array would give the same code; with no evidence for it,
  Skins.h keeps TSkin.
- **The blit slots take TRects** (high for the 16-byte blocks and the names FromRect/ToRect). Every call
  through the slots builds the rectangle as a 16-byte block on the stack (`sub esp,0x10` and four stores),
  separate from the pushes of the ints: graphics.cpp's ten calls (0x48fa3c Blit: `sub esp,0xc` for the
  colour key and `sub esp,0x10` for Item->x, Item->y, 32, 32, then four pushes; 0x48fc13 BlitClipped:
  clip, colour key, source; 0x48ff5d StretchBlit: clip, highlight, colour key, source, then the surface
  pushed, then the target as a second block, then ToSurface) and every engine-internal call (StretchBlit's
  and StretchBlitUnclipped's Blit/BlitClipped calls, BlitClipped's and BlitRect's BlitRectClipped calls:
  two blocks and four pushes). The new signatures (Surface.h and the three engines; the parameter order and
  the stack layout are the old ones):
  - +0x30 BlitRect(int ToSurface, int x, int y, int FromSurface, TRect FromRect) (parameter renamed from
    Source: its assertion names FromRect)
  - +0x34 Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey)
  - +0x38 BlitRectClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRect Clip)
  - +0x3c BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey,
    TRect Clip)
  - +0x44 StretchBlitUnclipped(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect,
    TRGBColor ColorKey, TRGBColor Highlight) (no caller outside the engines; its assertions name ToRect and
    FromRect like StretchBlit's, and its own calls build the same blocks)
  - +0x48 StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
    TRGBColor Highlight, TRect Clip)
  - +0x40 StretchBlitRect keeps (ToSurface, TRect Destination, FromSurface, TRect Source): it took TRects
    already, and no assertion names its parameters (the names stay ours, low).
  The x, y of Blit/BlitClipped/BlitRect/BlitRectClipped stay ints: the calls push them separately (four
  pushes: ToSurface, x, y, FromSurface). WriteImage, WriteMaskedImage and SetSurfaceRect keep their ints
  (six and eight pushes, no block; WriteImage's assertion "x>=0 && y>=0 && Width>0 && Height>0" names
  separate parameters).
- **The assertions are real expressions now**: SurfaceDX.cpp's TBlitSize view and the TASSERT_TEXT and
  TASSERT2_TEXT macros of surfaceDX9.cpp and SurfaceGL.cpp are gone; the assertions are TASSERT and
  TASSERT2(FromRect.width>0 && FromRect.height>0, FromRect.width, FromRect.height), whose texts are the
  binary's. Inside StretchBlit the second and last blits pass TRect(0, 0, ToRect.width, ToRect.height)
  for the STRETCH_SURFACE part (the binary builds a new block there; whether the original wrote a TRect
  temporary or a local is unknown, the values are the same).
- **Tests**: the test surfaces were changed to the new signatures (testsurface.h's TTestSurface and
  graphicstest.cpp's recorders, which log the same values in the same order as before) and the calls of
  surfacedxtest.cpp, surfacedx9test.cpp and surfacegltest.cpp build TRects where they passed four ints;
  the image's functions are called through the same declarations (same stack layout). The difftest totals
  are unchanged (14,492,680 checks, 67 in sysinctest.exe, 3,201 vlong; 0 failures).
- **callcheck**: no file's list changed (SurfaceDX.cpp 14, surfaceDX9.cpp 13, SurfaceGL.cpp 20,
  graphics.cpp 10, and 18 other files that use TRects: identical output before and after).
- **For #27** (SetChildArea): TRect's members are x, y, width, height now, so SetChildArea(TComponent
  *Child, TRect Area) can use Area.width/Area.height; GUI.cpp's FitToChild/FitToChildren already take a
  TRect by value. The callers in Dialogs.cpp pass R.x, R.y, R.width, R.height of SkinProperties' TRects
  today and can pass the TRect itself.
  Settled in #27: see "SetChildArea (issue #27)" below.

## SetChildArea (issue #27)

Claude Opus 5.5, November 2, 2026.

- **Evidence** (build/scratch/areacalls.py: every indirect call through slot +0x8c and every direct call of
  0x49f5d0, with the argument pattern since the previous call and the function from names.tsv). The slot
  calls of the surface engines (TSurfaceDX9's D3D device calls, 0x479d3c's GetSurface()->+0x8c) are other
  vtables and are left out. That leaves 136 calls (102 direct, 34 through the slot), and every one of them
  builds the area as a 16-byte block: `sub esp,0x10`, four stores and one push of the child. The Arrange
  functions with a larger frame have `sub esp,0x184`/`0x18c` for the frame and then the same block. No call
  pushes four separate values. The source had 136 calls as well, and the counts per file are the same
  (ConsoleWindow 4, ContentWindows 16, ContextMenus 1, DialogElements 6, DialogWindow 5, Dialogs 103,
  MapWindow 1). The calls by file, with the address of each call instruction (all build a block):
  - ConsoleWindow.cpp (4): TConsoleWindow_SetSize (0x41deec, 0x41df22, 0x41df58); TConsole_Arrange (0x4214f9).
  - ContentWindows.cpp (16): TInventoryView_ctor (0x42a873, 0x42a8d7, 0x42a937); TTradeOffers_SetSize
    (0x43183e); TTradeNames_SetSize (0x432b6c, 0x432ba2, 0x432bd8); TTradeButtons_SetSize (0x43353c,
    0x433596); TTradeWindowBody_SetSize (0x4340d5, 0x43410b, 0x434141); CreateSmallInventoryWindow (0x436690,
    0x4366f7, 0x43675a); TAutomapWindow_ctor (0x436bf8).
  - ContextMenus.cpp (1): THelpBubble_SetSize (0x43d838).
  - DialogElements.cpp (6): TLink_Arrange (0x456c70); TListBox_StartEdit (0x46310f); TFontListBox_StartEdit
    (0x464926); TCheckBox_ctor (0x467779); TEditText_Arrange (0x468768); TListBox_Arrange (0x46a1aa).
  - DialogWindow.cpp (5): TSubWindow_SetSize (0x4882b0, 0x488322, 0x48835c); TSubWindow_SetIcon (0x48a143);
    TSubWindow_SetTitle (0x48a333).
  - Dialogs.cpp (103): TDialog_Arrange (0x46cd7b, 0x46cdb1, 0x46cde7); TReconnectWindow_ctor (0x46e6e4,
    0x46e70a, 0x46e730); TLoginDialog_ctor (0x46ee66, 0x46eea2, 0x46eed8, 0x46ef6d, 0x46effa);
    TCharacterListDialog_ctor (0x46f6a5, 0x46f729, 0x46f8bf); TMoveObjectsDialog_ctor (0x47098c, 0x470a05);
    TTextDialog_ctor (0x471704, 0x4717a2, 0x471836); TBugReportDialog_ctor (0x472fdb, 0x473068);
    TNewChannelDialog_ctor (0x4736a7, 0x4736d7, 0x47379d, 0x4737cd); TPrivateChannelDialog_ctor (0x473f7e,
    0x473fae); TReportDialog_ctor (0x4745d0, 0x4745fe); TAddBuddyDialog_ctor (0x474c3d, 0x474c6b);
    TEditBuddyDialog_ctor (0x475348, 0x4753a6, 0x47543c, 0x4755ac, 0x475612, 0x47569f); TUpdateDialog_ctor
    (0x475b4c, 0x475bab); THintsDialog_ctor (0x476db1, 0x476e33); TEditMarkDialog_ctor (0x477e86, 0x477f07,
    0x477f9d); TOptionsDialog_ctor (0x478cfc); TGeneralOptionsDialog_ctor (0x4794a5, 0x4794d8, 0x479509,
    0x47953a, 0x47956d); TGraphicsOptionsDialog_ctor (0x479c6b, 0x479c9e, 0x479ccc, 0x479cfa);
    TAdvancedGraphicsDialog_ctor (0x47a754, 0x47a785, 0x47a7b8, 0x47a7e9); TConsoleOptionsDialog_ctor
    (0x47b145, 0x47b178, 0x47b1a9, 0x47b1da, 0x47b20d); TNetworkOptionsDialog_ctor (0x47b69c);
    TOutfitDialog_ctor (0x47d1d2); TEditListDialog_ctor (0x47d910, 0x47d9f5); TIgnoreListDialog_ctor
    (0x47dc36, 0x47dcc4, 0x47dd55, 0x47de77, 0x47df05, 0x47df6a, 0x47dfcf, 0x47e310, 0x47e3a6, 0x47e437,
    0x47e559, 0x47e5e7, 0x47e64c); THotkeyDialog_ctor (0x47eee0, 0x47ef7d, 0x47f00c, 0x47f0c2, 0x47f12c);
    TErrorReportDialog_ctor (0x47f448, 0x47f47b, 0x47f4a9, 0x47f4da); CreateInfoDialog (0x480a5a, 0x480a8d,
    0x480ac0); TRuleViolationDialog_ctor (0x484744, 0x4847fa, 0x48488e, 0x484912, 0x48499f, 0x484a0e,
    0x484a9b, 0x484b0c, 0x484b99, 0x484c46, 0x484cc2).
  - MapWindow.cpp (1): TMapWindow_Arrange (0x4bfe40).
- **SetChildArea(TComponent *Child, TRect Area)** (high for the 16-byte block; the type is TRect because it
  is the only 16-byte struct, and FitToChild/FitToChildren take theirs the same way). The parameter name
  Area is ours (low). The function's three assertions ("Child->Parent==this", "Width>0" and "Height>0"
  with the values "Width" and "Height") read the window's Width and Height at this+0x1c/+0x20. The catch
  block (line 1690) names nothing, so the binary does not name the area. Area matches FitToChild's
  parameter and the function's name. The body changes Area.x, Area.y, Area.width and Area.height in place,
  as it changed the four int parameters before. The binary keeps them in registers and locals, and the
  values are the same. The stack layout is the old one: the four ints were at the same offsets as the
  TRect's members.
- **The callers**: 118 pass a SkinProperties TRect whose x, y, width and height they used to pass one by
  one; they pass the TRect itself now (Dialogs.cpp 100, ContentWindows.cpp 10, ConsoleWindow.cpp 4,
  DialogWindow.cpp 2, ContextMenus.cpp 1, DialogElements.cpp 1). 18 build TRect(...):
  - computed areas (8): DialogElements.cpp's TLink::Arrange, TEditText::Arrange, TListBox::Arrange and the
    two StartEdits, Dialogs.cpp's TReconnectWindow constructor (three; 0x46e6cc builds the block from the
    computed heights). guitest.cpp's tree operation 9 does the same.
  - pairs of TGUIPoints (10): the position and the size are two points in SkinProperties (MapWindow.cpp's
    Data140/Data148, DialogWindow.cpp's Style194 Data28/30, Data60/68, Data70/78, ContentWindows.cpp's six
    Style6FC pairs). In the binary each pair is copied as 16 contiguous bytes (0x4bfe1d copies
    0x5cd108..0x5cd117), the same code a TRect copy gives. Whether these pairs are TRects is a SkinProperties
    retype: issue #31. The source builds TRect(P.x, P.y, Q.x, Q.y) (medium; the values and the layout are
    the binary's either way).
    Settled in #31: each pair is one TRect, and the ten callers pass it (see "SkinProperties point pairs
    (issue #31)" below).
- **Tests**: no test fake overrides SetChildArea; guitest.cpp's call builds a TRect. Totals unchanged
  (14,492,680 checks in difftest.exe, 67 in sysinctest.exe, 3,201 vlong; 0 failures). tools\compile-check.cmd
  is clean. The first full run had 125 "dialogs differ" failures in the outfit dialog, caused by the
  harness and not by this change, so it was run again. The original side's TColorButton Next pointers sat at
  0x00abab98-like addresses, and dialogstest's IsPattern prints those as fill pattern. Main's build (from
  git archive 82ae6f9) fails the same way with the default %TEMP% (2 failures in the rule violation dialog,
  a word printed P on one side and hex on the other), and both builds pass with build.cmd's TEMP. The
  second full run passed. The dump's dependence on addresses is issue #32.
- **callcheck**: identical output before and after for GUI.cpp (20 of 63 functions differ), ConsoleWindow.cpp
  (55 of 76), ContentWindows.cpp (82 of 140), ContextMenus.cpp (24 of 36), DialogElements.cpp (112 of 160),
  DialogWindow.cpp (23 of 47), Dialogs.cpp (111 of 139) and MapWindow.cpp (14 of 26). SetChildArea is a
  virtual call (indirect) or a qualified TWindow:: call in the decompile, so the tool does not count it, and
  TRect's constructor is not a names.tsv function.

## SkinProperties point pairs (issue #31)

Claude Opus 5.5, November 2, 2026.

- **Evidence** (build/scratch/i31/refs.py in the i31 worktree: every instruction of .text whose operand is an
  absolute address inside one of the ten 16-byte pairs, with the function from names.tsv, and every dword
  outside .text that points into them):
  - Each pair has exactly eight references: InitSkinProperties' (0x498160) four stores, one per word, and the
    four loads of its one SetChildArea call, which copy the words in order (+0, +4, +8, +c) into the
    16-byte block (`sub esp,0x10`, four stores, the push of the child, the call through +0x8c). No other
    function reads or writes any word of them, and no dword in .rdata, .data or .rsrc points into them.
  - No code takes the address of SkinProperties (0x5ccfc8) or of the blocks Style194 (0x5cd15c) and
    Style6FC (0x5cd6c4) except the static initialiser 0x559e50, which passes SkinProperties to its
    constructor 0x496800. The block constructors 0x4954f0 and 0x495700 (and 0x496800 for +0x140) zero every
    word of the pairs in order, which TRect's constructor and two TGUIPoint constructors give alike.
  - No assertion string names them: the only "SkinProperties." strings name PlayerStates.NumberOfIcons,
    StatusWindow.HitpointsBar/ManaBar, BuddylistWindow.Logo, Buttons.Texture and CheckBox.Item.
  - So nothing in the binary uses either half of a pair as a point; the only use is the 16-byte copy that a
    TRect passed by value gives, the same criterion as Dialogs.cpp's retype (issue #8) and the 118 TRects
    passed to SetChildArea (#27).
- **Each pair is one TRect** (medium-high for the type: the code is the same for either type, and the only
  use is the TRect copy; low for the names, kept from the first member because no string names them). The
  values are InitSkinProperties' (x, y, width, height); the calls are the call instructions:

  | Member (address) | Was | Call | Area of | Values |
  |---|---|---|---|---|
  | Data140 (0x5cd108) | Data140/Data148 | 0x4bfe40 TMapWindow::Arrange | the hint button | -35, 5, 32, 32 |
  | Style194.Data28 (0x5cd184) | Data28/Data30 | 0x48a333 TSubWindow::SetTitle | the title | 20, 4, -41, 10 |
  | Style194.Data60 (0x5cd1bc) | Data60/Data68 | 0x488322 TSubWindow::SetSize | Resizer1 | 3, -16, 13, 13 |
  | Style194.Data70 (0x5cd1cc) | Data70/Data78 | 0x48835c TSubWindow::SetSize | Resizer2 | 0, -4, 0, 0 |
  | Style6FC.Data04 (0x5cd6c8) | Data04/Data0C | 0x434141 TTradeWindowBody::SetSize | Names | 0, 0, 0, 12 |
  | Style6FC.Data34 (0x5cd6f8) | Data34/Data3C | 0x432bd8 TTradeNames::SetSize | its Separator | 0, -2, 0, 2 |
  | Style6FC.Data58 (0x5cd71c) | Data58/Data60 | 0x43410b TTradeWindowBody::SetSize | Offers | 0, 12, 0, -26 |
  | Style6FC.DataA8 (0x5cd76c) | DataA8/DataB0 | 0x43183e TTradeOffers::SetSize | its Separator | 77, 0, 2, 0 |
  | Style6FC.DataB8 (0x5cd77c) | DataB8/DataC0 | 0x4340d5 TTradeWindowBody::SetSize | Buttons | 0, -26, 0, 0 |
  | Style6FC.DataE8 (0x5cd7ac) | DataE8/DataF0 | 0x43353c TTradeButtons::SetSize | its Separator | 0, 0, 0, 2 |

  No pair was kept as two points.
- **The source**: GUI.h declares the ten TRects at the old offsets (the layout is the same: a TRect is two
  TGUIPoints' 16 bytes, both int-aligned). InitSkinProperties stores the second point's x and y as the
  TRect's width and height, in the old order (the binary interleaves the stores of neighbouring members as
  its register allocation goes; only the bytes after the function matter, and guitest compares them). The
  ten callers pass the TRect itself instead of TRect(P.x, P.y, Q.x, Q.y); no caller of SetChildArea builds
  an area from two points any more (the 8 computed TRect(...) areas of #27 remain). DialogWindow.h's
  comments of Resizer1/Resizer2 name the new members. dialogwindowtest.cpp's random Style194 values set
  Data28/Data60/Data70.width and .height where they set the second points' x and y (the same statements in
  the same order, so the same Random() sequence); no other test referred to the members.
- **Tests**: totals unchanged (14,492,680 checks in difftest.exe, 67 in sysinctest.exe, 3,201 vlong;
  0 failures) on the first complete run (the run before stopped at dialogwindowtest.cpp's compile errors,
  which led to its change above). tools\compile-check.cmd is clean.
- **callcheck**: identical output before and after for GUI.cpp (20 of 63 functions differ), MapWindow.cpp
  (14 of 26), DialogWindow.cpp (23 of 47) and ContentWindows.cpp (82 of 140).

## external.h removed (issue #14)

Settled by Claude Opus 5.5, November 2, 2026. src/external.h is gone; every declaration is in the header of
the file that defines it, and every file includes the headers it uses (compile-guided: external.h was removed
from all 36 includers, then each file got the headers the compiler asked for). Evidence scripts:
build/scratch/texttable.py (the text table's copies and texts), copyrefs.py (every code reference into and
around each copy) and gaps.py (the data between the copies), all in the i14 worktree.

- **What external.h held and where it went** (high):

  | Content | Now |
  |---|---|
  | TObject (Objects.cpp's type) | Objects.h, unchanged (comment kept); Status.h, Inventory.h, Map.h, Creatures.h, DialogElements.h, ContextMenus.h and ContentWindows.h include Objects.h |
  | PacketHeader (0x5d1644, "owner unknown") | already defined in MainFrm.cpp and declared in MainFrm.h; Network.cpp includes MainFrm.h |
  | TFullscreenMode forward declaration | dropped (Preferences.h has the type) |
  | #include of Talk.h, sysinc.h, GUI.h, ConsoleWindow.h, DialogWindow.h, MainWindow.h, Surface.h, MainFrm.h, <ostream> | in the files that use them (below) |
  | the text tables of seven objects (externs) | src/Text.h (below) |
  | the action-state comment ("Status.cpp. Action state: 0 idle, ...") | Status.h, at ACTIONSTATE_NONE |
  | empty headings ("Map.cpp", "Script.cpp", ...) and the stale FUN_ remark | dropped |

  Includes added: Communication.cpp Text.h; ConsoleWindow.cpp Text.h; ContentWindows.cpp Text.h;
  ContextMenus.cpp ConsoleWindow.h, Text.h, sysinc.h; Control.cpp ConsoleWindow.h, MainWindow.h, Surface.h,
  Text.h, sysinc.h; Creatures.cpp GUI.h (its NoCallback), Text.h, sysinc.h; DialogElements.cpp, Dialogs.cpp,
  DialogWindow.cpp, GUI.cpp, Help.cpp, MapWindow.cpp Text.h; MainFrm.cpp GUI.h, MainWindow.h, Surface.h,
  Text.h; MainWindow.cpp ConsoleWindow.h, Text.h; Map.cpp, Script.cpp, Status.cpp sysinc.h; Network.cpp
  MainFrm.h; Player.cpp MainFrm.h, Text.h; Talk.cpp Text.h, graphics.h (TRGBColor), sysinc.h; Map.h and
  Creatures.h graphics.h (TRGBColor); Dialogs.h Text.h. Preferences.cpp, Surface.cpp, SurfaceDX.cpp,
  surfaceDX9.cpp and SurfaceGL.cpp needed nothing from it. No include cycle appeared: none of the headers
  that used to include external.h is included by Objects.h, graphics.h or Text.h.
- **The text header is src/Text.h, 147 `static const char *` entries** (high for the form, low for the
  file name and the names). The image has sixteen copies of the table in .data (every pointer to "You are
  poisoned" 0x55d1b0 starts one), each 147 pointers (0x24c bytes), all sixteen identical pointer for
  pointer. The code reads the entries from memory (`mov reg, [0x5c....]`), so they are not const pointers
  (a `char *const` would be folded into the code and land in .rdata); they are defined in a header with
  internal linkage, which gives exactly one copy per including object, and every object's code reads its
  own copy. Text.h defines all 147 in the image's order with the image's texts (generated from the binary,
  so the copies of the old test definitions are byte-identical; the test totals confirm it), so a .cpp that
  includes it reads the entry of its own copy: entry n of a copy is at the copy's address + 4n. A table
  (array) or 147 named variables give the same .data; named variables were kept because the users already
  read them by name. The existing names all landed on the entry of their address without a conflict
  (TextSelectType 70 and TextSelectType2 112 are different entries with the same text, as are
  TextEnterCharacterName 67/71 and TextEnterShortDescription 72/113). New names (ours): TextEnterFilename
  (73), TextServerAddress (90), TextServerPort (91), TextRequestClosed (126), TextChannelClosed (127),
  TextPermanentlyIgnoring (134), TextSignupURL (144).
- **The owners of the sixteen copies** (in .data order; each object's .data is contiguous, in link order):

  | Copy | Object | Evidence | Confidence |
  |---|---|---|---|
  | 0x5c0150 | Communication.obj | reads 126, 127 (0x411b60, 0x411eea) | high |
  | 0x5c041c | ConsoleWindow.obj | reads 12-19, 119, 120, 122, 146 | high |
  | 0x5c0678 | ContentWindows.obj | reads 0-11, 20-50, 52, 56, 57 | high |
  | 0x5c08d4 | ContextMenus.obj | no reads; between ContentWindows.obj's and Control.obj's | high |
  | 0x5c0b34 | Control.obj | reads 109-111, 114, 115, 145 | high |
  | 0x5c0d90 | Creatures.obj | no reads; after Control.cpp's LastPacketType 0x5c0d8c, before DialogElements' | high |
  | 0x5c0fe0 | DialogElements.obj | no reads; its DefaultFontColor 0x5c1230 (read by its header code) follows | high |
  | 0x5c123c | Dialogs.obj | reads 53-55, 58-72, 74-89, 92-108, 112, 113, 117, 118, 121, 123, 128-133, 135-142 | high |
  | 0x5c1498 | DialogWindow.obj | no reads; DialogWindow, graphics and GUI come in this order and only one table lies between Dialogs' and GUI's | medium |
  | 0x5c1708 | GUI.obj | no reads; GUI.cpp's DefaultFontColor 0x5c1954 follows directly | high |
  | 0x5c1960 | Help.obj | no reads; two copies lie between GUI.obj's and MainWindow.obj's, where Help, Inventory and MainFrm link | low (Inventory.obj possible) |
  | 0x5c1bb0 | MainFrm.obj | as above; the copy is followed by a DefaultFontColor, and of the three only MainFrm.cpp includes GUI.h (graphics.h) | low |
  | 0x5c1e0c | MainWindow.obj | reads 124, 143 | high |
  | 0x5c2064 | MapWindow.obj | reads 51, 125 | high |
  | 0x5c22cc | Player.obj | reads 116 | high |
  | 0x5c258c | Talk.obj | reads 134 (0x507843) | high |

  Each of those sixteen .cpp files includes Text.h (DialogElements.cpp, ContextMenus.cpp, Creatures.cpp,
  DialogWindow.cpp, GUI.cpp, Help.cpp and MainFrm.cpp read nothing from it). Text.h's header comment lists
  the table. The DialogElements.cpp comment that put its copy at 0x5c0d8c-0x5c1228 was off (0x5c0d8c is
  Control.cpp's LastPacketType, 0x5c0d90 Creatures.obj's copy); it now says 0x5c0fe0-0x5c122b.
- **Dialogs.h includes Text.h** (high): TReconnectBody::SetTime is inline in Dialogs.h and reads
  TextTryingToReconnect (entry 62); its out-of-line copy 0x46ba10 (Dialogs.obj's header code) reads
  Dialogs.obj's 0x5c1334. Dialogs.h's includers (Control.cpp, Dialogs.cpp) both have a copy. Text.h's other
  users are .cpp files.
- **Two Ghidra names and a misplaced text in Talk.cpp** (high): Talk.cpp defined
  `PTR_s_Your_request_has_been_closed__005c0348` and `PTR_s_The_channel_has_been_closed__You_005c034c`
  (Talk.h declared them) for Communication.cpp's two reads; 0x5c0348/0x5c034c are entries 126 and 127 of
  Communication.obj's own copy. Communication.cpp now reads TextRequestClosed/TextChannelClosed from its copy;
  the two globals are gone. Talk.cpp's `static const char *PermanentIgnoreMessage` (0x5c27a4) is entry 134
  of Talk.obj's copy: it reads TextPermanentlyIgnoring now. Same texts, never written: no behaviour change.
  No FUN_00/DAT_00/PTR_ name is left in src/.
- **Tests** (high): the window tests defined the entries their files declared extern (consolewindowtest,
  contentwindowstest, mainwindowtest, mapwindowtest, playertest); those definitions are gone (the src files
  have their copies). dialogstest.cpp's text globals and InitTexts (which pointed our side's entries at the
  image's texts) clashed with Text.h's statics through Dialogs.h and are gone too: this is more than an
  #include change in dialogstest.cpp (issue #32 works on its dump at the same time; the removed block, "Dialogs.obj's
  copy of the text table" with InitTexts and its call, is apart from the dump). Our Dialogs.cpp reads its own copy now, whose
  texts equal the image's; the dumps print the texts behind pointers (ReadString), not the pointers, so they
  compare the same values. Include fixes: contextmenustest/mainwindowtest ConsoleWindow.h, playertest
  Surface.h and sysinc.h (its `const int TitleCode[8]` definition needs the extern declaration, or it gets
  internal linkage and difftest.exe does not link), talktest graphics.h, creaturestest sysinc.h.
- **Tools**: tools/rename_externals.py retired (it renamed FUN_ names and deleted their lines from
  external.h; no FUN_ name is left). tools/triage.py and tools/simplify.py never read external.h.
  notes/agent-prompt.md and notes/reconstruction.md describe the rule that replaces external.h.
- **Not done here**: Status.cpp's LinkPattern pointer (0x55b758) has 17 copies in .data, one per object,
  so it is a header static too (issue #33).
## build.cmd reliability (issue #23)

Claude Opus 5.5 (issue #23), November 2, 2026.

- **Causes found** (high, reproduced). (1) The silent "exit 1" from Git Bash: main's harness with
  talktest.cpp's old double delete (734a5f7 reverted for the experiment) ends with `exit 1` and no output
  from Git Bash; the child died with 0xc0000374 and build.cmd's `|| exit /b 1` said nothing. The new
  harness prints `crash: exception 0xc0000374 (heap corruption) ... in TestTalk` and the parent's line.
  (2) Git Bash runs on main failed with output: 336 failures in dialogstest.cpp ("Dialogs ... (hotkeys):
  dialogs differ") while PowerShell passed. The dump took the original's string pointer 0x02abab30 for
  the 0xab fill pattern (two 0xab bytes) and printed it as a number, ours as the string. Git Bash's larger
  environment block moves the heap, so only some shells hit the 256-byte window 0x..abab... IsPattern now
  needs a 0xab top byte (no pointer of a 32-bit process is that high) or low byte (no object is at an odd
  address). A second flake of the same dump (1 of 10 cmd runs): a number both sides hold, 0x00df64af in a
  TRuleViolationDialog, was unreadable during our dump and readable during the original's (the heap grew
  in between), so one side printed "P". Compare now keeps each page's first readability answer for both
  dumps; every actual read checks the page again (PageReadable), because the cache alone let DumpWord read
  a page the heap had decommitted meanwhile (an access violation in 2 of about 60 runs of
  DIFFTEST_ONLY=Dialogs, found by running it under environment blocks of different sizes and pinned by the
  new crash report plus a /MAP build). (3) `cmd /c ...` from Git Bash: MSYS turns /c into C:/, so cmd starts interactively, prints
  its banner, reads the empty stdin and exits 0 without running anything; run `cmd //c`. (4) A locked
  difftest.exe: a child left running (its parent killed, or Windows Error Reporting holding a crashed
  child) keeps the image locked and the link fails. No other environment difference mattered: vcvarsall,
  cl.exe, link.exe (vcvars puts MSVC's before Git's /usr/bin/link.exe), python and TEMP/TMP (Windows paths
  from Git Bash too) resolve the same in all three shells.
- **Test names through shared memory, not stdout** (medium). The parent passes an inheritable file
  mapping handle as a third argument (`--child <Tibia.exe> <handle>`); the child writes the state and the
  test's name there. It survives the child, so even `__fastfail` (no handler in the process runs) gets
  the test named. A pipe or status file would work as well; this needs no reader thread and no file.
- **Function-like macros announce the tests** (medium). `#define TestX(...) RUN_TEST(TestX, __VA_ARGS__)`
  before main wraps each call in BeginTest/EndTest without changing the registration lines (a macro is
  not expanded again inside its own expansion, so the inner TestX calls the function). The cost: a new
  test needs a macro line; without one a crash in it is reported "after <previous test> (between the
  tests, or in a test that does not announce itself)". The alternative, a table or a RUN(...) on each
  line, would have rewritten every registration line that other branches (#11) add to.
- **Normal end through atexit** (medium). The child marks itself finished in an atexit handler, so the
  DIFFTEST_ONLY early returns need no change; a child that ends without atexit (crash, ExitProcess in
  image code, TerminateProcess) is reported even with exit code 0 (difftest.exe then returns 2).
- **The child ends crashes itself** (medium). Its unhandled-exception filter prints and terminates with
  the exception code (no Windows Error Reporting, no CRT terminate path); terminate() goes to abort(),
  whose SIGABRT handler exits with 3 (abort's own code); invalid parameter exits with 0xc0000417, a pure
  call with 0xc0000409; `__report_gsfailure` is patched into a jump to a reporter that prints the return
  address of the function whose /GS cookie was overwritten. SetThreadStackGuarantee(64 KB) leaves the
  vectored handler stack after a stack overflow. Child exit codes: 0 passed, 1 failures, 2 could not
  start, 3 abort, 4 heap check failed.
- **_heapchk() after every test, always** (high). It costs nothing measurable (full build.cmd runs took
  174-201 s before, 177-273 s after with other worktrees building at the same time; 30 final runs, 10
  each from cmd, PowerShell and Git Bash, all with 14,015,187 + 67 + 3,201 checks and 0 failures). It finds corruption of the heap's own structures (a large-block overrun is reported
  "in the heap check after TestX"); an overrun inside the low-fragmentation heap's small blocks can stay
  unnoticed until the heap trips over it.
- **Job object and timeout** (high). The child is in a job with KILL_ON_JOB_CLOSE (it dies with the
  parent) and DIE_ON_UNHANDLED_EXCEPTION (no error dialog). DIFFTEST_TIMEOUT defaults to 1800 s; a full
  difftest.exe run takes about 2.5 minutes, traced runs can set 0 (none).
- **build.cmd** (high). Each step's failure prints a message and exits 1; the run line of difftest.exe
  and its cl line keep their text (the parent prints the report; issue #11 adds files to those lines),
  only a `call :remove difftest.exe` before them is new. `:remove` deletes the old program before linking;
  if it is locked, PowerShell stops processes whose full path is this checkout's program (never other
  worktrees') and retries the delete for 10 s. ROOT is normalised (`%%~fI`), so TEMP is now
  ...\build\difftest\tmp without `tools\difftest\..\..`. vcvarsall's output goes to build\difftest\vcvars.txt
  and is shown when it fails. A passing run ends with "build.cmd: all tests passed"; the totals lines are
  unchanged. No incremental build: a full run takes about 3 minutes.
## dialogstest dump (issue #32)

dialogstest.cpp failed now and then with "dialogs differ" when the code had not changed. The failures
depended on the heap layout of the run (the environment, TEMP), not on the code.

- **Cause, confirmed**: two rules of the dump decided by addresses.
  - `Readable` asked VirtualQuery at dump time whether a word points to committed memory (`P`) or not (a
    number). The two dumps are built one after the other, and our dump's own strings (about 50 KB for the
    hotkey dialog) grow the heap between them. A non-pointer word in an address range the heap reaches during
    our dump was hex in ours and `P` in the original's: THotkeyDialog's AutoSend bools (words 0x01000100,
    0x01010001 ...) at member words 2323-2331. The reverse (`P` vs hex, the rule violation dialog of #27)
    happens when the heap gives memory back between the dumps.
  - `IsPattern` took every word with two 0xab bytes for the fill pattern. A real pointer such as 0x00abab28
    was hex on that side and the tree name on the other: TColorButton's Next and TOutfitDialog's buttons
    (`abab28` vs `F.4.49`), the help dialog's parent pointers (`abab38` vs `F.3`).
- **Reproduction**: DG_HEAPSHIFT=<bytes> allocates a block of that size first (and keeps it). On main, 5 of
  25 layouts failed (shift 40000, 100000, 250000 with build.cmd's TEMP: 1, 6, 2 failures, all hotkey AutoSend;
  shift 1500000 with the default TEMP: 1, hotkeys; shift 40000 with the default TEMP: 1,195, the outfit
  dialog's 0x00abab.. pointers in the 20 failures printed). The other 20 combinations of the shifts (none,
  1000, 30000, 40000, 100000, 250000, 600000, 1500000, 3000000, 8000000) and TEMPs (build.cmd's, the default,
  a long path) passed. The help dialog's case showed up in the mutation runs.
- **The new rules**: a word is a pointer only by where it points, decided from tables that do not change
  while the two dumps are built:
  - the side's tree components, receiver, parent window and statement (by name, as before);
  - a block allocated while a round runs (both sides' operator new and new[], the image's malloc, the fakes'
    memory): `H<size>+<offset>`, `D<size>+<offset>` when freed. The table is shared by both sides. Our
    operator delete and delete[] and the image's free (its operator delete calls it) mark a block as freed;
    freed blocks stay allocated until the round ends, so a dangling pointer cannot point into a newer block of
    one side only. Allocations of the test's own work (the names, the dumps) are not recorded, and frees of
    recorded blocks during the dumps wait until both dumps are built. The table's nodes come from a heap of
    their own (HeapCreate), not from the operator new it records;
  - the Tibia image, difftest.exe and the DLLs (MEM_IMAGE regions, taken once before the rounds): `S`;
  - the main thread's stack reservation: `K`;
  - anything else is a number (hex). 0xabababab is not an address, so the fill pattern needs no rule of its
    own; a word with some pattern bytes is a number or a pointer by the same rules on both sides.
  Strings (the string members, and texts in the logs) are read only from a pointer and only inside the block,
  image or stack it points into; timers are recognised by the vtable at the start of a live block.
- **Why not "allocation N"**: each side numbering the blocks its pointers reach (first reference in the
  dump) would compare pointer identity, but the same number in both dumps (random members such as
  TTextDialog's object number) can fall into a block that only one side's dump references, and would then
  get different numbers. Size and offset describe the block the same way for both sides. Pointer identity is
  still compared where it was before: pointers to tree components by name. Pointers to other blocks were `P`
  and now compare size and offset as well.
- **Not weakened (mutation check)**: 22 mutations of src\Dialogs.cpp from issue #8, 4 that change only where
  a pointer member points or the timer's lifetime (the outfit and VIP dialogs' radio selections, the login
  dialog's focus, SetTimer without the delete), and the outfit wrap. Each ran against main's dump and the new
  one (same seed, same 98,474 checks). The new dump catches every mutation the old one caught, with the
  same number of failures as the old dump's lowest count over three TEMP layouts. The old dump's count varies
  with the layout (the login focus mutation: 1155, 1159, 1174 and 1501 failures; the outfit wrap: 165, 235 and
  247), the new one does not (1155 and 165 each time). Only the old dump flakes in those runs. The pointer
  mutations are caught by the dump alone: 1690 failures for the outfit part selection, 592 for the VIP icon
  selection, 1155 for the login focus. SetTimer without the delete makes our side throw (GUI.cpp's timer list),
  under both dumps. Issue #8's `Outfit > LastOutfit` to `>=` gives 0 failures under the new dump. Under the old
  dump it gave 5 and 209 failures in two runs, all of them the IsPattern flake in the help dialog: with
  today's random sequence, no outfit reaches LastOutfit by event 6. `Outfit = FirstOutfit + 1` after the wrap
  is caught (165). git diff --stat src is empty afterwards.
- **Layouts**: the new dump passes all 18 layouts tried (no shift, 1000, 30000, 40000, 100000, 250000,
  600000, 1500000, 8000000; build.cmd's TEMP and the default TEMP), including the 5 that failed on main, with
  98,474 checks and 0 failures each. The check count of Dialogs is unchanged (the dump changed what it prints,
  not what it compares).
- **The other tests**: no other test decides pointers by readability or partial 0xab patterns.
  contentwindowstest.cpp, consolewindowtest.cpp, contextmenustest.cpp, dialogwindowtest.cpp,
  dialogelementstest.cpp, mainwindowtest.cpp, surfacedx9test.cpp and surfacegltest.cpp name objects by role or
  kind from their own maps (a pointer they do not know prints as "?", the same on both sides);
  mapwindowtest.cpp compares the windows' bytes at fixed offsets. The raw %p they print are values shared by
  both sides (the receivers of the element fakes, the talk buffer, the mark menu's window, a made-up number),
  and dialogelementstest.cpp's Tip tests only the whole word 0xabababab. They are unchanged.
- **Totals**: unchanged. tools\difftestuild.cmd: 14,492,680 checks in difftest.exe (Dialogs 98,889 in the
  full run, 98,474 alone), 67 in sysinctest.exe, 3,201 vlong, 0 failures. tools\compile-check.cmd is clean.
  so it is a header static too (issue #33). Done: it is Status.h's (see "LinkPattern (issue #33)").

## LinkPattern (issue #33)

Settled by Claude Opus 5.5, November 2, 2026. Status.cpp's `static char *LinkPattern` (the URL regular
expression, a pointer to the text 0x55b758) is now defined in src/Status.h, static, so every object that
includes Status.h has its own copy, as the image shows. Evidence scripts in the i33 worktree's build/scratch:
linkcopies.py (.data 0x5c0100-0x5c2600 dword by dword, with the text table copies and the code references),
statususers.py (per file, the Status.cpp functions and data its code references), incgraph.py (the
transitive #include sets of src/*.cpp).

- **17 copies, one per object** (high): .data holds the pointer 0x55b758 17 times and nowhere else. A
  non-const static is not dropped when its object does not read it (Text.h's 147 entries are in all sixteen
  copies although most objects read none of them), so an object has a copy exactly when it includes the
  header that defines it. Each object's .data is contiguous and in link order (Communication, ConsoleWindow,
  ContentWindows, ContextMenus, Control, Creatures, DialogElements, Dialogs, DialogWindow, graphics, GUI,
  Help, Inventory, MainFrm, MainWindow, Map, MapWindow, Network, Objects, Player, Preferences, Script, Skins,
  Status, Surface, ...), and an object has at most one copy of each header static, which fixes the owners:

  | Copy | Object | Neighbours and evidence | Confidence |
  |---|---|---|---|
  | 0x5c039c | Communication.obj | after its text table 0x5c0150; ConsoleWindow.obj's table, copy and colour (read by its code) follow the RTTI descriptors 0x5c03a0-0x5c0418 | high |
  | 0x5c0668 | ConsoleWindow.obj | between its table 0x5c041c and its DefaultFontColor 0x5c066c (read 0x41755a) | high |
  | 0x5c08c4 | ContentWindows.obj | between its table 0x5c0678 (read 0x437697) and its DefaultFontColor 0x5c08c8 | high |
  | 0x5c0b20 | ContextMenus.obj | between its table 0x5c08d4 and its DefaultFontColor 0x5c0b24; nothing links between ContextMenus.obj and Control.obj | high |
  | 0x5c0b30 | Control.obj | before its table 0x5c0b34, DefaultFontColor 0x5c0d80 and LastPacketType 0x5c0d8c (read 0x44a9aa) | high |
  | 0x5c0fdc | Creatures.obj | after its table 0x5c0d90; DialogElements.obj's table 0x5c0fe0 follows directly | high |
  | 0x5c122c | DialogElements.obj | between its table and its DefaultFontColor 0x5c1230 (read 0x452c86) | high |
  | 0x5c1488 | Dialogs.obj | between its table 0x5c123c and its DefaultFontColor 0x5c148c (read 0x471f7e) | high |
  | 0x5c16e4 | DialogWindow.obj | three copies lie between Dialogs.obj's colour and GUI.obj's table, where DialogWindow, graphics and GUI link: one each, in order | high |
  | 0x5c16f4 | graphics.obj | as above; followed by graphics.obj's DefaultFontColor 0x5c16f8 (no text table: graphics.cpp does not include Text.h) | high |
  | 0x5c1704 | GUI.obj | as above; directly before its table 0x5c1708 and DefaultFontColor 0x5c1954 (read 0x4954c6) | high |
  | 0x5c1bac | MainFrm.obj | see below | medium |
  | 0x5c1e08 | MainWindow.obj | directly before its table 0x5c1e0c (reads 124, 143); see below | medium |
  | 0x5c22b0 | MapWindow.obj | between its table 0x5c2064 and its DefaultFontColor 0x5c22b4 | high |
  | 0x5c22c0 | Network.obj | MapWindow.obj has 0x5c22b0; Network.cpp's 0x5c22c4 (read 0x4cdab0) follows | high |
  | 0x5c22c8 | Player.obj | directly before its table 0x5c22cc and LastMouseCursor 0x5c2518; Objects.obj links between Network.obj and Player.obj but uses nothing of Status.cpp | medium |
  | 0x5c2560 | Status.obj | read by InitStatus (0x4ea992); between Preferences.cpp's WindowedWidth/Height 0x5c2558/0x5c255c and CommandLineEngine 0x5c2564 | high |

  Between GUI.obj's colour and MapWindow.obj's table the data is: table 0x5c1960, copy 0x5c1bac, table
  0x5c1bb0, DefaultFontColor 0x5c1dfc, copy 0x5c1e08, MainWindow.obj's table 0x5c1e0c and colour 0x5c2058,
  for Help, Inventory, MainFrm and MainWindow. MainFrm.cpp (GetMouseX, GetMouseY, GetMouseCursor,
  Set/GetConnectionState, AddMessage) and MainWindow.cpp (CheckAnyRight, CheckRight, SetActionState, ...)
  call Status.cpp, Help.cpp and Inventory.cpp do not; with the copy in the header that declares Status.cpp's
  functions (below), the only split that gives both a copy is MainFrm = 0x5c1bac + table 0x5c1bb0 + colour,
  MainWindow = 0x5c1e08 + its table + colour, and the table 0x5c1960 to Help.obj or Inventory.obj without a
  copy. This agrees with #14's low-confidence owner of the table 0x5c1bb0 (MainFrm.obj) and leaves the table
  0x5c1960 between Help.obj and Inventory.obj, as before. The Player/Objects split is argued the same way.
- **Objects without a copy** (high): Help, Inventory, Map, Objects, Preferences, Script, Skins, Surface,
  SurfaceDX, surfaceDX9, SurfaceGL, sysinc, Talk (it has a text table, 0x5c258c, but no copy), TMyException,
  TRSA, TXTEA, Utils, vlong and regexp. None of them calls a Status.cpp function or touches its data.
- **The header is Status.h** (high that the copies follow Status.cpp's users; medium for Status.h itself, see the
  end of this item): every object whose code calls
  a Status.cpp function or references its data has a copy (Communication 12 functions, ConsoleWindow 7,
  ContentWindows 14, ContextMenus 9, Control 39, Creatures 2, DialogElements 6, Dialogs 28, MainFrm 6,
  MainWindow 10, MapWindow 13, Network 6, Player 5, graphics 1 (GetActionNumber), and Status itself), no object
  without a copy does, and only Status.cpp reads it (CheckClipboardText's link check). The other two objects
  with a copy use nothing of Status.cpp: DialogWindow.cpp includes Status.h through DialogElements.h (whose
  inline code calls SetMouseOverText), and GUI.cpp (below). In our sources the .cpp files that include
  Status.h transitively are exactly the image's set less GUI.cpp; the next-best header, Objects.h, also
  reaches Inventory, Map and Objects (no copies) and misses GUI. Text.h (the text table) and graphics.h
  (DefaultFontColor) are other headers: Network, graphics and Status have a copy but no table, Talk and Help
  (or Inventory) a table but no copy; Communication, Creatures, Network, Player and Status have a copy but
  no colour. A header that Status.h includes could hold it as
  well; nothing in the image tells the two apart, so it stays in Status.h, next to CheckClipboardText.
- **GUI.cpp includes Status.h** (medium): GUI.obj's copy 0x5c1704 says the original GUI.cpp included the
  header (directly or through another header that no copy-less object includes); GUI.cpp calls nothing of
  Status.cpp. It now includes Status.h directly, with a comment; no other include changed.
- **No other datum of the header** (high for .data): the copies are single dwords; what lies next to them is
  either a text table copy (Text.h), a DefaultFontColor copy (graphics.h) or the object's own data, and no
  other value accompanies all seventeen. Zero-initialised header statics would be in .bss and cannot be seen
  this way (not checked).
- **Order inside an object** (observation, not used): the copy comes after the text table in
  Communication, ConsoleWindow, ContentWindows, ContextMenus, Creatures, DialogElements, Dialogs,
  DialogWindow and MapWindow, before it in Control, GUI, MainFrm, MainWindow and Player; the colour always
  comes last, even where graphics.h is included first. Our include orders were not changed for it.
- **Behaviour**: none. The pointer is never written and only Status.cpp reads it (its own copy, as before);
  the other copies are unused, and the modern compiler may drop them. compile-check has no warnings (an
  unused static variable is not a /W3 warning; Text.h's entries are the same case). The test totals are
  unchanged (below).

## Error paths (issue #22)

Claude Opus 5.5 (issue #22), November 2, 2026.

- **Why an exception of the image ended the child** (high, reproduced step by step). (1) The image's
  `_CxxThrowException` (0x511d2b) calls RaiseException through an unbound import slot. (2) Its TMyException
  constructors build the message in a `std::stringstream`, whose locale and stream buffer code takes
  `std::_Lockit` and `_Mutex` locks (critical sections the CRT's start-up never initialised) and calls
  `_getptd` (0x51689a: TLS/FLS slots and calloc of a start-up that never ran). (3) Windows refuses every
  exception handler of the image: RtlDispatchException and RtlUnwind check each handler (RtlIsValidHandler,
  the SafeSEH check: RtlUnwind of this Windows build calls it too, seen in ntdll), and with DEP on, which
  difftest.exe's /NXCOMPAT makes permanent (GetProcessDEPPolicy: flags 3, permanent), a handler outside a
  mapped image is invalid; the image is private memory. A first own dispatch loop in a vectored handler got
  through the search but died in RtlUnwind with STATUS_BAD_STACK (0xc0000028, raised right after the
  RtlIsValidHandler call).
- **Design: the image's own machinery, made to run** (high). `tools/difftest/imageeh.cpp`'s
  EnableImageExceptions: the image's malloc/free (PatchImage), `_getptd` returns one static per-thread block
  set up as `_getptd` sets up a new one (the child has one thread), `_lock`, `_unlock`, `std::_Lockit`,
  `_Mtxinit/_Mtxdst/_Mtxlock/_Mtxunlock` are no-ops, the kernel32 imports the EH and iostream code needs are
  bound to the real functions (RaiseException, RtlUnwind, IsBad*Ptr, VirtualQuery, Get/SetLastError,
  GetCurrentThreadId, Interlocked*), and every other unbound import slot of the image calls a trap that names
  the import and the caller (an error path that reaches Windows unexpectedly is reported, not a jump to a
  random address). For the handlers, a vectored handler (it runs before the frame-based dispatch) replaces the
  handler of every image registration on the thread's chain with `ImageHandlerTrampoline`, a function of
  difftest.exe in its SafeSEH table, which calls the recorded original (the function's `__CxxFrameHandler`
  thunk or `_except_handler3`); registrations made later (a catch block's frames) are replaced at the next
  exception, before anything can unwind them. Windows then dispatches and unwinds as in the client: the
  image's frame handler finds its catch blocks, its TEND blocks build the wrapping TMyException with the
  image's own code and throw again. DisableImageExceptions undoes every byte. Alternatives rejected:
  /NXCOMPAT:NO (works only where the system DEP policy is OptIn), mapping the image as SEC_IMAGE (changes how
  every test maps it), an own dispatcher and unwinder (RtlUnwind's check stays).
- **The trampoline is registered by the assembler** (high): the C++ compiler cannot put a function into the
  SafeSEH table; `imageehseh.asm` (`.safeseh`) does, build.cmd assembles it with ml.exe (`/safeseh`) before the
  difftest.exe line and links the object.
- **The boundary** (high). ImageThrow runs the call in `__try` with a filter that takes only C++ exceptions
  whose ThrowInfo is in the image, copies the thrown object (type name from the image's RTTI; a TMyException's
  Message +0x04 and Origin +0x20, VC7.1 strings; a PROGRAM_ERROR's value) and destroys it (ThrowInfo's
  destructor, as a catch block's end would) before the unwind. OurThrow catches PROGRAM_ERROR, TMyException &
  and `...` and makes the same record.
- **Line numbers** (medium). Our sources do not keep the original's lines (Utils.cpp 118 is the original's
  66), so every "<file> <line>" of a message and the origin is compared with the line replaced, and over the
  whole run each of our lines must stand for one original line and each original line for one of ours (a
  different assertion with the same text in another place fails it). ER_LINES=1 prints the pairs (for a future
  issue that aligns the lines, if wanted). File names and everything else are compared exactly.
- **error.txt** (high). HandleException (0x4d9bd0) runs on both sides into error.txt in a scratch directory and
  the files are compared byte for byte: the image's ofstream through a VC7.1 FILE without a buffer (its
  basic_filebuf reads `_ptr`, `_cnt`, `_base` of its FILE directly, so it cannot get one of our CRT's FILEs:
  heap corruption) whose fopen/fclose/fputc/fwrite/fflush/fseek go to our CRT; time and ctime fixed (patched on
  both sides: the image's `_time32`/`ctime`, our `_time64`/`_ctime64`); GetVersionExA a fake through both
  import tables (service pack texts of at most 29 characters: with 30 or more the original prints its 30-byte
  buffer without a terminator, whatever follows on the stack); OutputControlErrorInformations (Control.cpp,
  not linked into the difftest) writes nothing on both sides; GetCurrentEngine from the same curEngine.
  FatalError is a fake that records the text and raises an exception of its own, caught around the call: the
  original compiled it as noreturn (`int3` after the call in HandleProgramError, other code after it in
  HandleException), so a fake that returns runs into the next function. HandleProgramError's text is compared
  the same way (GetErrorText/GetErrorHint fakes; playertest.cpp's two stubs got different bodies, the linker
  had folded them into one function).
- **Reconstruction bug found and fixed** (high): Script.cpp's file errors carry a reason in the binary that ours
  did not: "file still open" the open file's name, "failed to open file" the name asked for (the four open()
  functions), "negative offset", "out of data" (readByte, readBytes) and "failed to write file" (writeByte,
  writeBytes) the file's name, InstallPatch's "file does not exists" the patched file's name (the raw
  decompile builds the string from `this+8`/`this+4`, `param_1` or the local name buffer before the
  constructor 0x508400). common.h has TERROR_REASON and TERROR1_REASON for them. A scan of the raw decompile of
  every file for throws with a non-literal reason (build/scratch/reasons.py of the issue's worktree) found no
  other file with such throws.
- **Inputs**. errortest.cpp has its own random numbers (ER_SEED); state the messages print is the same on both
  sides (graphics.cpp's counts set on both and restored; the image's Map.cpp field matrix and object vector
  constructed as maptest.cpp does; Tibia.dat loaded on both for Objects.cpp; the talkbox getters only when both
  sides have as many talkboxes, which a note reports otherwise). Leading assertions get values outside their
  ranges (edges, near, INT_MIN/INT_MAX), the other parameters in range.
- **Functions earlier tests redirected** (medium). Some tests redirect image and our functions for good
  (creaturestest.cpp: GetFirstObject, CheckEventHelpName and others on both sides). In the full run such a call
  reaches a fake (a NULL name crashed the child in the first full run), so a comparison whose function, or
  whose wrapped callee, starts with a jump is skipped and counted: 250 of the full run's comparisons,
  none with DIFFTEST_ONLY=Errors. The test runs before TestMapWindow, whose patches are for good too.
- **Covered** (per file, functions with error paths compared): TMyException.cpp (both constructors on random
  files, kinds, names, values, reasons); Player.cpp (HandleException's error.txt and FatalError call,
  HandleProgramError's text, Terminating); Utils.cpp (every assertion: strings, buffers at their ends,
  SetSize, FormatNumber); Map.cpp (the object, effect, field, automap accessors' coordinate and number
  assertions; TEND2 of GetFieldHeight, IsFieldPassable, IsFieldThrowable, IsFieldTranslucent,
  RequestFieldRefresh, FieldRefreshRequested over GetFirstObject's assertions and matrix3d's index error);
  Container.h (array and matrix3d index errors through Objects.cpp and Map.cpp); Objects.cpp (36 property
  getters and the four flag getters on the real Tibia.dat: TEND1/TEND2 over the array index error and the
  Entry flag assertions, GetMissileSpriteNumber, LoadObjects(NULL)); Creatures.cpp (skills, experience,
  creatures, buddies' parameter checks, HideCreature's TEND1); Inventory.cpp (all container accessors, body
  objects with TEND1); Talk.cpp (talkboxes with TEND1/TEND2 over GetTalkbox, the text buffer checks, mode
  checks); Status.cpp (rights, messages, account, characters, clipboard, InitStatus); Preferences.cpp (hotkeys,
  dialogs, channels, MotD, buddies, skill bars); Help.cpp (all four); graphics.cpp (file names, sprites,
  pictures, caches, fonts, DrawPicture); Script.cpp (the tokenizer on random broken scripts: every error() text,
  "identifier too long", "string too long", the get* type errors, open/close errors; the writer's, the binary
  reader's and writer's errors; InstallPatch).
- **Not covered, and why**: Communication.cpp, Control.cpp, Network.cpp, MainFrm.cpp (not linked into
  difftest.exe: sockets, MFC); sysinc.cpp (its own sysinctest.exe; only GetErrorText-style texts);
  SurfaceDX.cpp, surfaceDX9.cpp, SurfaceGL.cpp, Surface.cpp (their PROGRAM_ERROR throws and assertions need the
  fake COM/OpenGL worlds of their tests; the boundary is ready for them: PROGRAM_ERROR codes are compared);
  the window files GUI.cpp, DialogElements.cpp, DialogWindow.cpp, ContextMenus.cpp, ConsoleWindow.cpp,
  ContentWindows.cpp, Dialogs.cpp, MainWindow.cpp, MapWindow.cpp (member functions of objects that only their
  tests' fake worlds can build on both sides); assertions that print state of a file's statics our test cannot
  set (Creatures.cpp's NumberOfBuddies and NumberOfPlayerSteps, Inventory.cpp's offers, Map.cpp's effect
  status, Status.cpp's AccountCharacters with valid numbers). Issue #34 asks to call the window and surface
  files' error paths from their own tests with imageeh.cpp.
- **Mutation check** (high): sixteen changes in src/ (assertion conditions in Utils, Creatures, Preferences,
  Status, Help; TEND parameter lists in Map, Objects, Inventory; TERROR texts in Script; assertion values in
  graphics and Talk; TMyException's wrapping format; error.txt's and HandleProgramError's texts; Script's
  reason removed again): DIFFTEST_ONLY=Errors fails on each, with 4 to 2,895 failures (build/scratch/mutate.py
  of the issue's worktree; each mutated object rebuilt from the restored source before the next).
- **Totals**: DIFFTEST_ONLY=Errors 32,822 checks (9,270 exceptions compared, 8,045 of the calls threw on the
  original's side); in the full run 31,912 (250 skipped), difftest.exe 14,524,592, sysinctest.exe 67, vlong
  3,201, no failures; the later tests' counts are unchanged. A DIFFTEST_ONLY=Errors run takes about a second.
## Building Patella.exe (issue #16)

Settled by Claude Opus 5.5, November 2, 2026, with the Visual Studio MFC component (VS 2026 Community,
MSVC 14.51.36231 atlmfc) installed. Patella.exe builds from Patella.sln/Patella.vcxproj (root of the
repository) and from `tools\build-patella.cmd`, starts and shows its start screen.

- **The project** (decisions of form, not of behaviour). Patella.vcxproj (tabs, like the sources; VS reads
  them), toolset v145, configurations Release|Win32 and Debug|Win32; the sources as wildcards (`src\*.cpp`,
  `src\Extern\regexp\*.c`, which compile as C), so a new file needs no project change. The compiler flags
  are compile-check's: `/std:c++17 /EHsc /O2 /W3 /permissive- /Zc:strictStrings- /D_WIN32_WINNT=0x0601`,
  plus VS's usual WIN32, _WINDOWS and NDEBUG (no source uses assert), `/MT` (`/MTd` for Debug). UseOfMfc
  Static, CharacterSet NotSet: neither _MBCS nor _UNICODE, as in compile-check, and MFC links nafxcw.lib,
  the ANSI library the original has. Output build\<Configuration>\Patella.exe, intermediates
  build\obj\<Configuration> (build/ is gitignored; .vs/ and *.vcxproj.user now too). `VcpkgEnabled` is
  false and the per-user property sheet is not imported: this machine's user-wide vcpkg integration put
  E:\vcpkg's include path and every library it has (`*.lib`) on the command lines.
- **Libraries** (high). Tibia.exe imports GLU32, OPENGL32, DDRAW, WINMM, KERNEL32, USER32, GDI32,
  WINSPOOL.DRV, ADVAPI32, SHELL32, COMCTL32, SHLWAPI, OLEAUT32 and WS2_32, and delay-loads OLEACC.dll
  (dumpbin /dependents; tools/compare_imports.py reads both directories). The project links those import
  libraries in that order plus dxguid.lib (IID_IDirectDraw4, IID_IDirect3D3, IID_IDirect3DHALDevice), with
  `/DELAYLOAD:oleacc.dll` (MSBuild adds delayimp.lib); d3d9.dll is loaded at run time (surfaceDX9.cpp).
  `/NODEFAULTLIB:wsock32.lib` (IgnoreSpecificDefaultLibraries): afxsock.h asks for wsock32.lib, and the
  linker resolved MFC's socket calls from it (it searches the libraries after the one that pulled in an
  object first), so 18 Windows Sockets functions came from WSOCK32.dll; Tibia.exe imports all of them from
  WS2_32.dll, by the same ordinals. Now WS2_32's 23 functions are Tibia.exe's 23. `/MANIFEST:NO`
  (GenerateManifest false): Tibia.exe has no manifest (notes/resources.md). The linker's other defaults stay
  (/DYNAMICBASE, /NXCOMPAT, /SAFESEH; Tibia.exe has none of these flags): the code does not depend on its
  load address.
- **Resources** (high). src\res\Patella.rc is compiled with build\res on the include path; the project's
  ExtractResources target runs `python -I tools/extract_resources.py Tibia.exe build/res` first when
  build\res\resources.tsv is missing (python on PATH, Tibia.exe in the root). afxres.rc is not included.
  `python -I tools/compare_resources.py Tibia.exe build\Release\Patella.exe`: "55 entries in the original,
  55 in Patella.exe, 0 differences" (Release and Debug; build-patella.cmd runs it after every build).
- **What real MFC changed in the sources.** The shim (src/compat/mfc_shim.h) declares only what the
  sources use, so these showed only now. Each change keeps the generated calls and the behaviour:
  - MainFrm.cpp: the 24 Win32 calls on `m_hWnd` (GetClientRect, GetWindowRect, SetTimer, KillTimer,
    IsIconic, IsZoomed, ClientToScreen, ScreenToClient, SetCapture) are written `::GetClientRect(m_hWnd, ...)`.
    Inside CMainFrame's members real MFC's CWnd members of the same names (one argument fewer) hide the
    global functions; the shim had none. The binary calls the user32 functions with this->m_hWnd (MFC 7.1's
    inline CWnd members expanded; the original source may have used the member forms, but MFC 14's members
    are not all inline and some add checks, so the explicit calls are the faithful spelling).
  - Player.cpp: `::LoadIconA(AfxGetResourceHandle(), ...)` twice (CWinApp::LoadIcon hides it).
  - MainFrm.h/.cpp: OnNcHitTest returns LRESULT (MFC 8 changed ON_WM_NCHITTEST's handler type from MFC 7.1's
    UINT; the same 32-bit value on x86), `return Default();` without the UINT cast.
  - Surface.cpp and SurfaceDX.cpp define `DIRECT3D_VERSION 0x0700` before their first include, and SurfaceDX.h
    stops with #error if it is defined to anything else. MFC 14's afxwin.h includes afxrendertarget.h
    (Direct2D), which includes d3d9types.h, which sets DIRECT3D_VERSION to 0x0900 when it is unset; then the
    SDK's d3d.h leaves out the DirectX 6/7 interfaces SurfaceDX.cpp uses (IDirect3D3, D3DTLVERTEX,
    IID_IDirect3DHALDevice...). 0x0700 is d3d.h's own default, so the shim build sees what it saw before;
    d3d9types.h is then empty, which MFC's Direct2D headers do not mind. surfaceDX9.cpp does not include
    d3d.h and keeps 0x0900.
  - SurfaceDX.cpp, surfaceDX9.cpp and SurfaceGL.cpp include <mmsystem.h> for timeGetTime: MFC's afxv_w32.h
    defines WIN32_LEAN_AND_MEAN, so windows.h no longer brings it (the shim includes windows.h in full).
    sysinc.cpp already included it.
  Nothing else: the message map, CPlayerApp's and CMainFrame's overrides, CAsyncSocket's use in Network.cpp
  and the order of afxwin.h/afxsock.h and winsock2.h compiled unchanged.
- **Warnings** (high). None from the compiler or the linker, in Release and Debug (MSBuild "0 Warning(s)";
  build-patella.cmd builds with -warnAsError, so a warning fails it). Nothing is suppressed.
- **compile-check** uses real MFC now; `set TIBIA_MFC_SHIM=1` forces the shim (also chosen when the MFC
  component is missing). Both modes are clean. The difftest and tools\listing.cmd keep the shim.
- **Import tables** (tools/compare_imports.py Tibia.exe build\Release\Patella.exe; Release build).
  Patella.exe imports 18 DLLs and delay-loads OLEACC.dll:

  | DLL | Tibia.exe | Patella.exe | in both |
  |---|---|---|---|
  | GLU32 | 1 | 1 | 1 |
  | OPENGL32 | 32 | 32 | 32 |
  | DDRAW | 1 | 1 | 1 |
  | WINMM | 1 | 2 | 1 (+ PlaySoundA) |
  | KERNEL32 | 123 | 174 | 95 |
  | USER32 | 133 | 228 | 131 (- LoadBitmapA, wsprintfA) |
  | GDI32 | 37 | 101 | 37 |
  | WINSPOOL.DRV | 3 | 3 | 3 |
  | ADVAPI32 | 9 | 11 | 8 (- RegOpenKeyA; + RegDeleteValueA, RegEnumKeyExA, RegEnumValueA) |
  | SHELL32 | 3 | 9 | 3 |
  | COMCTL32 | 4 | - | 0 (InitCommonControls #17 and three ImageList functions of MFC 7.1) |
  | SHLWAPI | 2 | 6 | 2 |
  | OLEAUT32 | 3 | 13 | 3 |
  | WS2_32 | 23 | 23 | 23 |
  | MSIMG32, UxTheme, ole32, gdiplus, IMM32 | - | 2, 12, 21, 22, 3 | MFC 14 only |
  | OLEACC (delay-load) | 2 | 3 | 2 (+ AccessibleObjectFromWindow) |

  The differences are the libraries': MSVC 7.1's libcmt and MFC 7.1 against the UCRT/vcruntime and MFC 14
  (KERNEL32's - side: the ANSI environment and locale functions, GetVersion, HeapCreate, IsBad*Ptr,
  Interlocked*, lstr*, CreateProcessA and MoveFileA of _execl and rename; its + side: the W functions, Fls*,
  SRW locks, CreateProcessW/MoveFileExW of the same CRT calls, ...; MFC 14's visual manager, GDI+, themes,
  OLE drag and drop and IME support pull in the new DLLs). The game code's own imports agree: the IAT slots
  that Tibia.exe's game objects (0x401000-0x50eae8) reference and the import symbols our objects leave
  undefined (dumpbin /symbols of build\obj\Release) are the same 89 functions, except ATL 7.1 header code in
  sysinc.obj (AtlGetConversionACP: GetThreadLocale, GetLocaleInfoA, GetACP, InterlockedExchange; names.tsv
  0x4fcd50-0x4fcdc0) on Tibia's side and MSVC 14 header code on ours (critical sections, process heap,
  DecodePointer, UnregisterClassA in every object that includes afxwin.h); DirectDrawCreate and gluOrtho2D
  are called through import thunks on both sides (not seen by the slot scan in Tibia.exe). A slot match for
  TlsAlloc at 0x4de36a was a call displacement, not a reference. Scripts: build/scratch/i16/gameimports.py
  (i16 worktree).
- **The start** (Release and Debug, from build\run with copies of Tibia.dat, .spr, .pic and .cfg and a copy
  of Patella.exe; Engine = 2 in that Tibia.cfg, OpenGL): a visible top-level window, class "TibiaClient",
  title "Tibia   " (the decoded GetWindowTitle), the start screen with Enter Game, Access Account, Options,
  Info and Exit Game, responding; no error dialog, no error.txt. Closed with WM_CLOSE
  (Process.CloseMainWindow): exit code 0 within the wait, so Stop-Process was not needed; it rewrote the copied
  Tibia.cfg with the same bytes. Screenshot (PrintWindow) build\run\started.png. Nothing was clicked and
  nothing connected.

## Test server (issue #18)

The server, the scripts and the results: notes/testserver.md. Decisions with a reasonable default:

- **Patch copies of the clients rather than use the test-server setting** (high): `TestServer = yes` in
  Tibia.cfg only switches the login to TestServer, and Player.cpp's InitInstance always sets its address
  to test.cipsoft.com:7172 (the command line's "host"/"port" values are parsed and dropped), so no
  supported setting reaches 127.0.0.1:7171. The hosts file and src/ stay untouched; tools/patch_client.py
  rewrites the four login server names in copies, identically in both clients.
- **Patch the clients' RSA modulus, not the server's key** (high): tibia.pem's public modulus is the
  OpenTibia key, not CipSoft's, and only CipSoft has the private half of the client's key. Same digit
  count (309), so the literal is replaced in place.
- **All four server names, not one** (high): InitServerList starts at `rand() % 4` and moves to the next
  server on failure; patching one would leave the login to chance.
- **Only bind_address changed in config.cfg** (high): world_address was already 127.0.0.1, data_path
  "../data" resolves through the server's fallback list to gameserver\data, the rest is gameplay. The game
  port stays on 0.0.0.0 (hard-coded in the server); the existing firewall rules were not changed, the user
  is told (notes/testserver.md).
- **Plain-text Auth for the test account** (medium): schema.sql's CipSoft hash form was refused by the
  server although it verifies in Python; the server accepts plain text (DB_VerifyPassword, path 1). A test
  password on a loopback server; tools/testserver-account.sql documents both.
- **Posted window messages, not SendInput** (high): the clients read the mouse and keys from their message
  queue (MainFrm.cpp), with the modifiers tracked from WM_KEYDOWN/WM_KEYUP, so PostMessage drives them
  without focus or the real mouse, and runs unattended (SendInput needs the foreground and touches the
  user's input). Enter Game has no key, so its place is computed from the skin (GUI.cpp Style0D8).
- **Game entry verified by the client's own TCP connection** (high): an established connection from the
  client's PID to port 7172 (GetExtendedTcpTable) works on both clients without reading their memory or
  the screen; memory reads (binary-specific addresses) were used once as extra evidence.
- **Screen capture by BitBlt of the screen DC** (medium): PrintWindow, with or without
  PW_RENDERFULLCONTENT, returned a white client area; a screen copy shows what DWM composed. With the
  monitor off (as during this run) nothing helps: the screenshots stay white.
- **Ctrl+C for a clean stop** (high): the server's documented shutdown (its SIGINT handler saves and logs
  everyone out); a kill would lose the save. Sent from a helper process attached to the server's console.
  run-testserver.ps1 clears an inherited "ignore Ctrl+C" flag before starting the server: started from
  Git Bash, the server ignored the Ctrl+C.
- **Run folders build\run-orig and build\run-patella** (high): copies of the data files; the WindowedMode
  in their Tibia.cfg was set to (200,200,800,600,no), which the client only uses when it fits the screen.

## Network traffic comparison (issue #19)

The scripts, the session and the results: notes/traffic.md. Decisions with a reasonable default:

- **Compare after decryption, not with a fixed seed and clock** (high): the XTEA key, the RSA block's tail
  and the XTEA padding come from rand() (seeded with timeGetTime in sysinc.cpp); fixing them would mean
  patching both clients' code. tibia.pem, the server's private key, decrypts the RSA block, which gives the
  key for everything else. The masked bytes are checked instead of ignored: they must be one run of MSVC
  rand() results in both clients (proxy.rand_stream).
- **A proxy that rewrites the character list's game port** (high): the game server's address comes from the
  login server, not from the client; the proxy puts its own port in (same length, same padding) for both
  clients alike. The clients are patched only with patch_client.py's existing --port.
- **Replay as the strict comparison of received packets** (high): live server streams cannot match (the
  world moves on its own, the server cuts its frames by time), so "every received packet leads to the same
  client state" is tested by feeding both clients the same recorded server stream (re-encrypted with each
  client's key) and comparing their packets and memory after every step. The live sessions still compare
  the clients' packets against a real server; their server packets are only listed.
- **Replay paced by the client's packets** (medium): each recorded server packet is sent after the client
  packet that preceded it, with the recorded delay; the driver waits until the replay waits for the client
  (wait_idle). A step's state is then taken at the same point of the server stream in both clients.
- **State from memory, not from the screen** (high): the display was off (white screenshots, #18); the
  state is read field by field at the globals' addresses (Tibia.exe from src/ comments, Patella.exe from its
  PDB), without times and animation fields. The window tree is compared too.
- **Click places from the client's memory** (high): the window tree, the skin's slot areas and the map's
  creature name positions give every place; Patella's RTTI names its classes, eight Tibia.exe vtables (from
  src/ comments and the constructors) name the ones the driver needs.
- **A helper client as trade partner, started with "gamemaster"** (medium): a player trade needs a second
  player; a client refuses to start next to another Tibia window unless it is a gamemaster client
  (Player.cpp), and this server ignores the flag (db_sqlite.cc). The helper is the original client and is
  not compared; it is killed when it does not log out or close (it did not, in about half the runs).
- **Trade request, inspect, cancel; no accepted trade** (medium): the cancel leaves both inventories as
  they were; the session still sends 0x7d, 0x7e and 0x80 and opens and closes the trade window.
- **Restore the test characters' player files before each live session and restart the server** (high):
  the session eats the apple and the characters' positions and login order change; with the files put back
  and a fresh server both live sessions start from the same state. Only the two test characters' files are
  touched; a file that did not exist at the start (Patella Second's) is removed again.
- **The character is chosen by name in the list** (high): the server orders the list by level and last
  login, which the sessions change; the driver reads the names from the client's memory (Status.cpp
  Characters).
- **Restart the server when the login server listens on 0.0.0.0** (high): the server's start-up race
  (notes/traffic.md) cannot be fixed without changing the user's server; the script detects it from
  run-testserver.ps1's output and restarts.
- **"hello world", not "hello patella"** (high): a text containing the password makes both clients show the
  "Account Data Warning" instead of sending it.
- **ShowNewHints = no and a fixed window in the run folders' Tibia.cfg** (high): hint dialogs would take
  the driver's keys; the window (10,10,800,600) fits the 1024x768 screen of the monitor-off display, so the
  layout, and every click place, is the same in all runs.

## Written files (issue #21)

The scenarios, the masks and the results: notes/written-files.md. Decisions with a reasonable default:

- **Replays, not live sessions, for every comparison** (high): the files follow the server's data (the
  automap the map the server sends, error.txt the packets before the crash), and a live world moves on its
  own; as in issue #19 one live session of Tibia.exe is recorded and both clients get its server packets.
  The automap files of both replays are also identical to the live run's.
- **Crafted server data for the errors and the patches** (high): the replay can send anything; the error is
  an unknown packet type (Control.cpp ReceivePackets' two TERRORs, in the game and at the login), the patches
  are the login server's own protocol (0x1e/0x1f/0x20), so nothing in either client is patched or injected.
  A crafted script's expected client packets are computed independently of the clients where the form is
  known (the login packets with the new signatures, the 0xe8 debug report from error.txt) and the clients
  are also compared with each other.
- **Patches that change their file** (high): each patched file gets a new signature (Tibia.exe: eight bytes
  of the DOS stub), checked against the script's own result, so a patch that silently failed would differ.
  The difference patch of Tibia.dat uses every operation and every count form of ApplyPatch.
- **No patch.exe** (high): after a Tibia.exe patch the client runs patch.exe (CipSoft's separate patcher, not
  ours, not available). The run folders have none, so `_execl` fails and the client ends; the scenario
  compares Tibia.bak and the exit. What patch.exe does is out of scope (written into notes/written-files.md).
- **ctime as MSVC 7.1 writes it** (high; a reconstruction bug, fixed): Tibia.exe's CRT (libcmt.lib 7.1,
  asctime 0x517f6d: `tm_mday / 10 + '0'`, `tm_mday % 10 + '0'`) writes the day with two digits, "Mon Nov 02
  12:14:46 2026"; the UCRT that Patella links pads it with a space, "Mon Nov  2". The client writes ctime's
  text into error.txt (Player.cpp HandleException) and saved channels (ConsoleWindow.cpp SaveChannel), so
  Patella's files differed on days 1-9 (found in the walk scenario's Default.txt on November 2). Fixed with
  src/compat/crt71.h's CRT71Time, which turns the UCRT's form into MSVC 7.1's at both call sites (a compat
  header, not part of the original source, like the MFC shim). The difftest never ran the image's own ctime
  (errortest.cpp and consolewindowtest.cpp replace it with the test CRT's), and its fixed dates have two-digit
  days, so its results do not change. Other formatting of the UCRT that reaches a written file: none found
  (Talk.cpp's time stamps are sprintf'd from localtime with %02d; the rest is the client's own sprintf).
- **Times masked, with evidence** (high): error.txt's ctime line and the channel file's "Channel saved at"
  differ between two runs of Tibia.exe (the script runs it twice for walk, error and error-login and shows
  that the runs differ exactly there); the masks accept only the two-digit day, so the bug above stays
  visible.
- **error.txt's line numbers compared through a map** (medium): our `__LINE__` differs from the original's
  (known since issue #22); each differing pair must name the same function (our line inside the function's
  source, the original line among the function's assertion and catch lines in symbols.tsv) and stand for one
  line only. The real fix, the original's lines in our messages, is issue #35; until then error.txt and the
  debug reports (0xe8) differ in these numbers.
- **The driver's details** (medium): a step key that makes the client send nothing (a creature on the field,
  which the client knows) is not part of the recorded session; the replay presses the directions of the
  recording's step packets, each until the client sends it. The game connection counts when the proxy has
  it, too (login_client's TCP table check missed an established connection twice). FatalError's box is closed
  through its button's real ID (IDCANCEL in an MB_OK box: WM_COMMAND IDOK does nothing).
- **Options driven through the dialogs** (high): every control of the five option dialogs except the
  fullscreen check box (a display mode change on a monitor-off machine) and the help links (they open a
  browser); the live switch to the OpenGL engine is included. The options set outside these dialogs that
  the sessions touch (AutomapZoom, ConsoleHeight, OpenDialogs) are compared in the walk's Tibia.cfg;
  the VIP list, ignore lists, channels and combat modes are not driven (low: they are other dialogs and
  server data, a possible follow-up).

## Error paths of the window and surface files (issue #34)

Claude Opus 5.5 (issue #34), November 2, 2026.

- **The shared round** (high). errortest.cpp's comparison moved to imageeh.cpp: `CompareThrown` (kind, type,
  PROGRAM_ERROR value, message and origin exactly, line numbers through one map for the whole run; lines are
  mapped only from comparisons that match otherwise, so one difference does not cascade), and `TErrorRound`:
  the constructor saves Random()'s state and seeds it from the test's name (EH_SEED=<n> changes it), enables the
  image's exceptions; Finish disables them, restores Random() and prints "<Test> errors: N checks, M
  exceptions compared (K calls that threw)". `ROUND_COMPARE(R, What, ours, original)` runs our call, then the
  original's. The round's checks are counted apart from the test's own line, so every existing per-test count
  is unchanged. EH_TRACE=1 prints each comparison, EH_TRACE=2 names each call first, EH_LINES=1 the line pairs,
  EH_SEH=1 every exception that is not a TMyException or PROGRAM_ERROR.
- **Throwing test objects** (high). The window tests' bodies, elements and fakes are our classes on both sides;
  in the error rounds their methods throw on demand (a mask per batch), each with an assertion of the side that
  called it: `ThrowSideAssertion` (guitest.cpp) calls TComponent::SetSize with a negative width, ours or the
  image's 0x49bee0 (no test redirects it). An object of a test class on the image side must throw the image's
  exception: ours passes through image frames into the image's `catch (TMyException &)` (matched by type name)
  and is read with the image's layout (crash in the wrapping constructor 0x5086a0). A test object that calls
  base code which may assert must call the image's base code by address on the image side (guitest.cpp's
  TGuiThrower). Our destructors are noexcept (C++11; VC7.1's were not): a fake that a destructor calls must not
  throw (dialogwindowtest.cpp's SetContentWindowSize, called by ~TSubWindow), or the child ends in terminate().
- **Failure injection for the surfaces** (high). SurfaceDX.cpp: the fake DirectDraw makes the n-th
  DirectDrawCreate, QueryInterface, CreateClipper, SetCooperativeLevel, SetDisplayMode, CreateSurface or
  GetAttachedSurface fail (DXScript.FailCall, masks for the offscreen requests); surfaceDX9.cpp: the n-th
  DirectDrawCreate, QueryInterface, Direct3DCreate9, CreateDevice or CreateTexture (World.FailCall); SurfaceGL.cpp:
  no pbuffer. 0 means none, so the counted rounds are unchanged. SurfaceDX's DXOperation/DXCoverage statistics are
  saved around the round.
- **Covered** (sites: every TASSERT/TERROR/TEND/PROGRAM_ERROR throw of the file; build/scratch/sites.py and
  cover.py of the issue's worktree count them from an EH_LINES=1 run):
  - GUI.cpp (guitest.cpp TestGUIErrors): 89 of 96. Not reachable: the TENDs of InitSkinProperties, AddTimer,
    RemoveTimer, TTimer::TTimer, AddHotkey and RemoveHotkey (nothing in their try blocks can throw with a
    consistent table) and FitToChild's (the window's own SetSize/getMinimum* would have to throw; the mirrored
    windows are the real classes).
  - SurfaceDX.cpp (surfacedxtest.cpp TestSurfaceDXErrors): all 31 assertion sites, PROGRAM_ERROR 7-18.
  - surfaceDX9.cpp (surfacedx9test.cpp TestSurfaceDX9Errors): all 25 assertion sites, PROGRAM_ERROR 7, 8, 12,
    14-18 (the others are not thrown by this file).
  - SurfaceGL.cpp (surfacegltest.cpp TestSurfaceGLErrors): all 9 assertion sites, PROGRAM_ERROR 12 (no pbuffer,
    through Init and RestoreSurfaces). Not reachable: 15-18 (CreateTexture always returns true), ResetDisplay's
    two 12s (InitDisplay throws nothing).
  - DialogWindow.cpp (dialogwindowtest.cpp TestDialogWindowErrors): 38 of 51 (every assertion and TERROR site;
    TENDs of the columns' and sub-windows' functions over throwing bodies, elements, parts and AddOpenDialog).
    Not reached: the TENDs whose try blocks call only the real window classes (the column's getMinimumHeight,
    Arrange, DeleteChild, GetContentWindow, PositionChildren, MakeRoom, Grow, GetFreeHeight; TContentWindow's
    ReleaseCapture and LeftButtonUp; ShowUpButton, HideUpButton, SetMinimizeButton): the test objects sit one
    level lower, and a test element directly in a column would be read as a content window (its Identifier
    beyond the object).
  - ContextMenus.cpp (contextmenustest.cpp TestContextMenusErrors): 44 of 70: the menus built and drawn with
    throwing text and skin fakes (the constructors' and Draw's TENDs), unknown events to every HandleEvent (the
    TERROR1s and TEND1s), the object menu's events 10010/10011 with their assertions, AddEntry's text checks,
    unknown party flags, the constructors without their window, text, console buffer or mark data. Not
    reached: THelpBubble's Place and FitsIn assertions and TENDs (the bubble is placed inside the help area by
    the fakes' geometry), its constructor's and SetSize's TENDs, the entries' and the menu's mouse and key TENDs
    (an entry click runs the menu's event: only safe clicks are made), Show's LastEntry assertion and
    AddSeparator's TEND (no empty menu is shown), the object menu's LookedObject assertions, the text menu's
    player assertion and the text and request menus' HandleEvent TENDs (every event is theirs) and the request
    menu's buffer assertion.
  - errortest.cpp TestErrorStatics ("Statics errors: ..."): the assertions that print statics, with the statics
    set on both sides and restored (FindStatic finds a static through its getter's "mov reg, [address]" on both
    sides): GetPlayerStep (Creatures.cpp), the offers (Inventory.cpp), SetCharacter/SetActiveCharacter/
    GetCharacter* (Status.cpp, AccountCharacters), the effect getters for every effect number (Map.cpp).
    NumberOfBuddies: not found (GetNumberOfBuddies has a try block before the read; not covered).
- **Not covered** (open after the first part; the second and third parts below cover them): Surface.cpp (SurfaceInitInstance is called by no test: engine probing through Windows,
  the window and every engine; its second throw has no handler in the binary), DialogElements.cpp,
  ConsoleWindow.cpp, ContentWindows.cpp, Dialogs.cpp, MainWindow.cpp and MapWindow.cpp (no
  error rounds yet: the same pattern applies, the throwing test objects of dialogwindowtest.cpp plus each
  test's own fakes; mapwindowtest.cpp's patches are for good, so its round must run inside that test).
- **Findings** (high). CheckHotkey's TEND2(Key, i): the original prints i = 0 wherever the key is found (the
  counter lives in esi, the catch block reads the stack slot [ebp-0x14] written once before the loop, 0x4a14af),
  ours the index: a compiler artifact of the class of "Locals that the catch blocks print" (Map.cpp), not fixed
  in the source; the test empties the hotkey tables so that the key is the first entry. No reconstruction bugs
  found in the covered files.
- **Full-run failures of the first runs** (high). (1) GUI: a TComponent::Draw drawing with a skin offset of the
  skin's width or more makes DrawSkin's pieces 0 wide, and both versions tile forever (until the logs exhaust
  the heap); DrawSkinClipped with x or y = INT_MIN (it has no assertions on them) made both sides' picture code
  ask for gigabyte buffers (bad_alloc, "unknown exception occurred" on both sides); the heap left behind made
  DrawSkin case 7's allocations fail depending on the layout (on one side or the other, depending on which tests
  ran before). The inputs keep the offsets inside the skin and give DrawSkinClipped no INT_MIN. (2) SurfaceGL
  failed alone as well: FindGlhStrings took any four code bytes of glh_shutdown_extensions as an address and
  matched a misaligned window once difftest.exe's sections grew; it reads only absolute memory operands now.
  Tools for such hunts: DIFFTEST_SKIP=<names> leaves tests out of the full run, DIFFTEST_FAILS=<n> now reaches
  every test's failure printing.
- **Trap** (high): never call Random() inside a two-sided call (the two sides get different values).
- **Mutation check** (high): 20 changes to assertion conditions, TEND parameter lists and PROGRAM_ERROR codes
  of GUI.cpp, DialogWindow.cpp, SurfaceDX.cpp, surfaceDX9.cpp and SurfaceGL.cpp (build/scratch/mutate.py of the
  issue's worktree, each test run alone); all 20 detected: 13 by the error rounds' exception comparisons, one
  by SurfaceGL's log comparison, six (narrowed conditions that legitimate calls hit) by the counted tests. The first run let one survive: surfaceDX9.cpp's PROGRAM_ERROR
  14 of the primary render target, never chosen by the random failure index; the round now fails every creation
  call of Init in turn.
- **Totals** (full build.cmd, November 2, 2026): difftest.exe 14,745,663 checks, sysinctest.exe 67, vlong 3,201, no
  failures; the error rounds' checks: GUI 31,909, DialogWindow 9,501, ContextMenus 8,572, SurfaceDX 155,615, SurfaceDX9 4,934,
  SurfaceGL 2,620, statics 7,920. Every per-test count of the earlier full runs is unchanged (GUI 5,246,029, DialogWindow
  24,567, ContextMenus 33,088, SurfaceDX 1,213,366, SurfaceDX9 58,682, SurfaceGL 53,082, Errors 31,912).

### Second part (MapWindow, MainWindow, ConsoleWindow, ContentWindows, NumberOfBuddies, Surface.cpp)

Claude Opus 5.5 (issue #34, second part), November 2, 2026.

- **Failure injection by call count** (high). The window tests' fakes all log through one Logf per test
  with Side set: Tick() there (and in a few getters) counts the fake calls of one side's call, and the
  FailAt-th throws ThrowSideAssertion of the calling side. Both sides make the same fake calls, so the
  throw lands at the same place; every TEND whose try block reaches a fake is reachable this way. The
  tests' Both() got an error mode: one target action per round fails at a call count drawn from the most
  and the last count that action made (FailIndex: uniform, near the start or near the end, on a
  logarithmic scale); a throw ends the round and the window trees are left as they are (a member may
  point to a deleted window: the dumps are skipped after a throw, the logs compared). The counted parts
  are unchanged (the counts are only recorded); each round's checks are reported apart and the action
  counters saved and restored.
- **Image-side test objects** (high). A test object on the image side whose class does not override a
  method ran our base code (GUI.cpp's TWindow::HelpMouseMove...), whose TEND caught the image's exception,
  read it with our layout and threw ours on. imageeh.h's ImageSideVtable gives such objects a vtable with
  the image's TWindow/TElement entries for every slot the test class inherits unchanged (the rounds of
  consolewindowtest.cpp and contentwindowstest.cpp; the counted parts keep our vtables). The console test's
  button presses call the image's TCallback::Send (0x4a0f00) on the image side.
- **imageeh.cpp** (high): the table of replaced SEH registrations is pruned to the live chain when it has
  more than 192 entries (deep window trees left stale stack addresses: "more than 256 registrations").
- **Destructors** (medium). VC7.1's destructors let exceptions out; ours were noexcept (C++11), so a TEND
  in a destructor (~TTab, ~TConsoleWindow, ~TAutomapWindow, ~TContainerWindowBody, ~TSkillsWindowBody,
  ~TTradeWindowBody: "the handlers rethrow out of the destructor") ended our client in terminate() where
  the original reports the error. TComponent's virtual destructor is noexcept(false) now, so every window
  class's destructor is potentially throwing as in the binary (as TTalkBuffer's, Talk.cpp). MainFrm.cpp's
  CMainFrame destructor (0x4a8760, not in the difftest) has the same property in the binary: one try
  block (state 1) around ExitControl, whose three handlers throw on (0x4a87cd PROGRAM_ERROR,
  0x4a884d TMyException, 0x4a88e4 the "unknown exception" one) with state 0, the CFrameWnd destructor,
  left to unwind. It stays noexcept: CFrameWnd's destructor (MFC, afxwin.h) is noexcept and a
  noexcept(false) override is ill-formed (C2694 with the real MFC; tried). An exception from ExitControl
  ends our client where the original's goes on to the destructor's caller; the comment at the destructor
  says so (low impact: the client is exiting).
- **Reconstruction bugs found and fixed** (high, each from a failing comparison and the binary's code):
  MapWindow.cpp DrawField's object loop prints the object (TEND1(Obj): catch 0x4c72da, line 874, the type
  at [ebp-0x4c]); MapWindow.cpp HelpMouseMove stores the help position in x and y before MoveHelpPosition
  (the parameters' stack slots, 0x4c372a/0x4c3751, 0x4c3793/0x4c37bd), so its TEND2(x, y) prints them;
  MainWindow.cpp ResizeConsole calls Console->getMinimumHeight() once (0x4ac642), ours twice;
  ConsoleWindow.cpp TTextWindow::GetPosition keeps x - Indent in a value of its own (a register,
  0x41a0d8-0x41a0e5): TEND2(x, y) prints the clamped x; ContentWindows.cpp TContainerWindowBody::GetSlotAt
  turns x and y into grid positions in place (0x42e302, 0x42e30c), which its TEND2 prints;
  ContentWindows.cpp TTradeWindowBody::SetSize calls GetMaximumHeight, getMinimumHeight, then both again
  (0x43403f, 0x43404b, 0x434058, 0x434064; the first pair gives the lower bound): ours had the four calls
  in one Limit(...) expression, which our compiler evaluated in another order (getMinimumHeight first; seen
  once the test scroll bar's getMinimumHeight counted as a fake call), now in variables in the binary's
  order; ContentWindows.cpp TBuddylistWindowBody::GetBuddyAt adds the scroll position to y in place (the
  parameter's slot, 0x43611f), which its TEND2(x, y) prints (ours computed it in the expression).
- **Covered** (build/scratch/cover.py of the worktree from an EH_LINES=1 run; a statement over several lines
  counts with any of its lines):
  - MapWindow.cpp (mapwindowtest.cpp TestMapWindowErrors, DIFFTEST_ONLY=MapWindow): 60 of 60. The fakes
    fail by count (Draw, DrawField on the player's field, the objects, creatures, sprites, cache, light,
    talkboxes, fields, the mouse, events, SetSize, the constructors, TMapFrame's SetSize and, with a
    throwing map view, its minimum size); Draw's window bytes are restored before each try (a second Draw
    makes fewer calls); the round has its own logging surface. The assertions get no player (the n-th
    GetCreature finds nothing), maps wider or higher than the surface, unknown party and skull flags,
    effect numbers outside the table, brightness and field indices outside, objects of type 0, creatures
    without an outfit, unknown talkbox modes, use and trade objects of type 0 or CREATURE, hints outside
    1..EVENTHELPS; AddLightSource's own catch block a ground type Tibia.dat does not have.
  - MainWindow.cpp (mainwindowtest.cpp TestMainWindowErrors): 29 of 35. Not reachable: 491 (the binary has
    no try block in GetConsoleWindowHeight), 549, 580 and 617 (CloseContainer, CloseTrade,
    CloseContentWindow: their try blocks call only the real dialog window, GetContentWindow, DeleteChild and
    Rearrange(32), which positions the remaining windows; no fake is called), 717 and 728 (TSplitter's
    constructor and Arrange: new TDrawing[3], the drawing list from the skins).
  - ConsoleWindow.cpp (consolewindowtest.cpp TestConsoleWindowErrors): 95 of 119; the assertions directly
    (NULL text buffer, view, edit line, default tab, current tab or caption; unknown events and talk modes;
    tabs without caption or buffer, a tab in no bar; reports without a tab, a buffer or the player; a
    player's report without a name, whose tab's destructor asserts). Not reachable: 101, 102 (FormatLines
    keeps a format with text for every line), 388, 402, 441, 469, 491, 543 (the try blocks access the
    bounds-checked line formats, call GetLineFormat and new[] only), 690 (TConsole::Arrange: the text
    window's Arrange, run through SetChildArea before the check, recomputes InvisibleLines; the range is
    never inverted), 788, 872, 948 (TTab::Arrange reads the buffer's inline GetChanged/GetClosed and
    GetActionNumber), 806 (Rearrange of a tab in the bar), 913 (TTabBar's constructor), 958 (GetContainer
    throws before Container could be NULL), 1043 (TWindow::IsChild has no assertion), 1104 (moves valid
    tabs between the bar's parts), 1124 (FindTab: loops only), 1712, 1741 (NewTab NULL: needs operator new
    to return NULL), 1794 (SetServerMessages: HandleEvent(203) makes no fake call), 1911 (AddToHistory:
    History->Append), 1975 (ParseInput stops only at '$', '*' or '@'), 2009 (GetTextBuffer asserts first).
  - ContentWindows.cpp (contentwindowstest.cpp TestContentWindowsErrors): 172 of 192. After the 3,000
    rounds of failing actions, 800 rounds of direct calls on new windows (ContentCall, the console test's
    ConsoleCall pattern). The assertions: Draw of every part at negative coordinates, a VIP list scrolled
    by part of a row drawn at 0, 0, HelpMouseMove without a parent, the bodies' SetSize without room for
    the scroll bar, the trade names and buttons sized to nothing (SetChildArea's assertions, GUI.cpp),
    LeftButtonUp over an occupied slot or row with an empty or CREATURE use object (action state 7) or an
    empty trade object (8), GetIcon and GetLine outside their ranges, GetLine's TASSERTB2 (negative values,
    progress outside -1..100), ShowAcceptButton(NULL), TButtonsWindow(NULL, ...), TCompassRose with a skin
    >= NumberOfSkins. Then calls whose first to third fake call fails, in a random order until one throws
    (help, Arrange, events, hover, the getMinimumHeight chain, the wheel, GetInitialHeight, presses on the
    scroll bar, SetSize, Wait, SetCancelButton, the TTradeButtons constructor, mouse events and Draw of
    random parts). The test scroll bar's getMinimumHeight counts as a fake call, and its LeftButtonDown
    counts and runs TWindow's (the image's 0x49dff0 on the image side in the round); the counted part's
    numbers are unchanged. Not reachable: 359 and 1425 (TAutomapWindow::Arrange, TStatusWindow::Arrange:
    TWindow::Arrange reaches only TElement's empty Arrange, then the drawing list), 724 and 961 (the
    TPlayerStates and TSmallInventoryWindow constructors: only new TDrawing[] under the try), 745, 746
    (TComponent::Draw throws first for x < 0 or y < 0: drawing list entry 0 at 0, 0; destx, desty >= 2),
    827 (GetIcon returns 0 or 228-239 only), 943 (the loop's i is 0-9, all cases), 1054 (y + 124 as in
    1048, which throws first), 1208 and 1774 (GetSlotAt, ~TSkillsWindowBody: no calls; no try block in the
    binary), 1364 (TStatusWindow's constructor: SetSize with the skin's constants, Arrange as above), 1387,
    1388, 1394, 1395 (TContentWindow::Draw asserts x >= 0 and y >= 0 first; the bars are at 26, 4 and 26,
    17), 1536 (GetMaximumHeight: GetContentHeight is inline arithmetic), 2480, 2481 (x and y >= 0 asserted
    first; DestX >= 3, DestY clamped to >= 0), 2919 (x >= 0 asserted first; ItemX 0, Logo.x 2).
  - Creatures.cpp NumberOfBuddies (errortest.cpp TestErrorStatics): FindStatic scans 256 bytes (the read
    comes after GetNumberOfBuddies' try block set-up) with BuddyHideOffline cleared on both sides (ours
    through SetBuddyHideOffline, the image's 0x719eac), restored after.
  - Surface.cpp SurfaceInitInstance (errortest.cpp TestSurfaceInitInstance, in the statics round): 1 of 2.
    IsDirect3D9Installed, GetEngine, GetSurface and CreateSurfaceDX9 are fakes on both sides for the round
    (patched and restored; our GetSurface is a call, not inlined), the surface a TTestSurface whose
    TestEngine throws; 12 calls with the engine from Tibia.cfg random and no command line engine, the
    assertion thrown by IsDirect3D9Installed, the creation or TestEngine: PROGRAM_ERROR 12 on both sides,
    curEngine compared. Not reachable: the second PROGRAM_ERROR 12 (ChoosePixelFormat, DescribePixelFormat):
    the binary has one try block (ehinfo 0x4eac20: 4 states, try 0-0, catch ... 0x4eae33); the second
    try's states (2 set at 0x4ead6c, -1 at 0x4ead95) have no handler, the compiler dropped the catch of C
    functions. Ours (/EHa) keeps it.
  - Dialogs.cpp and DialogElements.cpp: done on branch issue-34c, see "Third part" below.
- **Mutation check** (high): 20 changes (build/scratch/mutate34b.py of the worktree, each file's test run
  alone): the seven source fixes above undone where they are testable alone (DrawField's TEND1(Obj),
  HelpMouseMove's help position, ResizeConsole's one call, GetPosition's own TextX, GetSlotAt's x,
  TTradeWindowBody::SetSize's order, GetBuddyAt's y), assertions removed or narrowed (MapWindow's player,
  OpenTrade's window, TTab::Draw's y, GetLine's progress, TButtonsWindow's check object, GetIcon's range,
  the battle list's CREATURE check, the VIP list's parent, SortBuddy's NumberOfBuddies range), TEND and
  TERROR values (MainWindow::HandleEvent, TConsoleWindow::HandleEvent's unknown event, TTab::Draw printing
  y) and SurfaceInitInstance's PROGRAM_ERROR code; all 20 detected (16 by the error rounds' exception
  comparisons, one by a round's log comparison, three by a crash or abort: two removed assertions and the
  narrowed TTab::Draw one, which the counted part hits at y = 0). Trap of the
  script: it restores a source after its build but rebuilds the restored files only at the end, so the next
  mutation of another test ran with the previous object; the Surface.cpp mutation was run again alone
  (detected: PROGRAM_ERROR 11 against 12).
- **Totals** (full build.cmd, November 2, 2026): difftest.exe 15,258,447 checks, sysinctest.exe 67, vlong
  3,201, no failures, "build.cmd: all tests passed". The error rounds' checks: GUI 31,909, DialogWindow 9,501,
  ContextMenus 8,572, ConsoleWindow 287,934, ContentWindows 168,764, SurfaceDX 155,615, SurfaceDX9 4,934,
  SurfaceGL 2,620, statics 8,496, MapWindow 12,359, MainWindow 43,150 (Dialogs and DialogElements: branch
  issue-34c). Every per-test count is main's (GUI 5,246,029, DialogWindow 24,567, ContextMenus 33,088,
  ConsoleWindow 30,948, ContentWindows 52,506, DialogElements 135,478, Dialogs 98,889, SurfaceDX 1,213,366,
  SurfaceDX9 58,682, SurfaceGL 53,082, Container 192,832, Errors 31,912, MapWindow 115,169, MainWindow 14,164).

### Third part: Dialogs.cpp and DialogElements.cpp

Claude Opus 5.5 (issue #34, third part), November 2, 2026.

- **The rounds** (high). dialogstest.cpp TestDialogsErrors ("Dialogs errors: ...") and dialogelementstest.cpp
  TestDialogElementsErrors ("DialogElements errors: ..."), after the counted parts, whose counts are unchanged
  (the round's checks subtracted, the operation counters saved and restored). The second part's pattern: Tick()
  in Logf and in every getter fake (text metrics, action state, messages, clipboard, Preferences, Talk,
  Creatures, Help, object flags, the surface's getters), the FailAt-th call throws ThrowSideAssertion of the
  calling side; Both() gets an error mode (one target action per round fails at a count drawn from the most
  and the last count that action made, now per kind and action: MaxTicks[kind][action], also recorded by the
  counted part, which that does not change); a throw abandons the dialogs or controls and only the logs are
  compared after it. New here: Operation(Forced) runs the target action in half of the round's operations with
  the same random numbers as before (the counted part draws them unchanged), and the rounds go through every
  pair of kind and target action (Dialogs 10 times, 6,650 rounds; DialogElements 40 times, 27,000 rounds;
  DialogElements runs fast). Then direct calls for the assertions (constructors and creators with arguments the
  game does not give, members outside their ranges) and sweeps: a call failing at each of its fake calls in
  turn, each on new objects (Dialogs: TRuleViolationDialog::GetAction, THotkeyDialog::LeftButtonUp, the
  character list's RightButtonUp and MouseWheel; DialogElements: TLabel's constructor, TCheckBox::SetEnabled).
- **Dialogs.cpp specifics** (high). Action 18 is a key press straight to the body: TDialog::KeyPress keeps
  Return and Escape, so the bodies' KeyPress TENDs need it. An abandoned dialog leaves its timers in both timer
  lists, and UpdateTimers would call the deleted dialog: RemoveAbandonedTimers takes every live block of the
  round's allocation table that starts with a timer vtable out of the lists (ours with RemoveTimer, the
  image's with 0x49b0d0). The error report dialog is made by the image's creator (0x484400) in the round: its
  std::stringstream works while the image's exceptions are enabled (the counted part keeps our constructor
  with the image's vtable). World.NoPlayer and World.NoHelpTitles (zero in the counted part) give the new
  channel dialog no player and the hints dialog no matching title.
- **Hybrid vtables** (high). Dialogs: none needed. The image side's test objects are the receivers, which get
  only HandleEvent (their own override), and the parent window, which the graphics dialog only passes to
  AddMessageFirst as a value; the surface is no component. DialogElements: the round's image-side receiver
  (TElement base, 0x5645c8) and target window (TWindow base, 0x564528) get ImageSideVtable (the frames and
  resizers call the target's inherited TWindow methods); the counted part keeps our vtables.
- **Reconstruction bugs found and fixed** (high, each from a failing comparison and the binary's code):
  - Dialogs.cpp TDialog::MouseMove keeps the dragged position in values of its own (ebx and edi,
    0x46d466-0x46d4fa) and never writes the parameters' stack slots; its catch block reads [ebp+0x8] and
    [ebp+0xc] (0x46d572, 0x46d576), so TEND2(x, y) prints the mouse position. Ours wrote the position into x
    and y.
  - DialogElements.cpp TSkinButton::SetSkins: TASSERT1(_ButtonUp<NumberOfSkins, _ButtonUp) and the same for
    _ButtonDown (0x458b20, lines 1228 and 1229: the binary's messages print the skin only); ours printed
    NumberOfSkins as a second value.
  - DialogElements.cpp THorizontalScrollBar's and TScrollBar's constructors end with TEND2(Min, Max), not
    TEND: the binary's catch blocks print "(Min = ...) (Max = ...)" from the parameters (0x4692d0 reads
    [ebp+0x8] and [ebp+0xc]; 0x469a80 the slots after the TRect), so the original's parameters were named Min
    and Max (the range's assertion inside says _Min and _Max). The parameters are renamed in .cpp and .h.
  - DialogElements.cpp TEditLine::KeyPress: once the modifier test of the character branch passes, the binary
    stores the masked code into Key's stack slot (0x4682b2-0x4682b8, mov [ebp+0x8],edi), so TEND1(Key) prints
    the code (97 for 0xc61, 'a' with the 0x400 and 0x800 bits) when the insertion throws. Ours kept Key. Seen only
    in the full run (a different random sequence: the key 0x461|0x800 with a failing InsertChar).
- **Test artifacts** (high): dialogelementstest.cpp's own GetClipboardText (sysinc.cpp is not linked) calls
  the clipboard fake, so it must not tick itself (one paste counted two calls on our side only); dialogstest.cpp's
  UpdateActions member logged the reason and action in one Logf on our side and two on the image's (and
  evaluated GetAction first): two calls now, the same text (EH_SEED=2 showed it). Both test fixes; each test
  passes alone with EH_SEED 1-8 (DialogElements) and 1-6 (Dialogs).
- **Covered** (build/scratch/sites.py and cover.py of the worktree, from an EH_LINES=1 run of each test alone):
  - Dialogs.cpp: 158 of 165. Not reachable: 772 (TWaitLabel::SetTimer: delete Timer and new TTimer, the
    timer list only), 1004 and 1013 (TCharacterListDialog::RightButtonUp and MouseWheel: SetTimeout and
    TWindow's dispatch to the list box, which makes no fake call; the sweeps counted none), 1021 and 1073
    (PositionChanged and SetTimeout: the timer only), 1339 (TEditListDialog::Arrange: the drawing list only),
    2415 (TRuleViolationDialog::GetReason: the inline selection and the Reasons array).
  - DialogElements.cpp: 177 of 218. Not reachable (41): the Arrange functions that only fill the drawing list
    or compute (54 TSeparator, 92 TResizer, 268 TBorder, 1215 TCheckBox, 1340 TEditLine, 2175
    THorizontalScrollBarRange, 2443 TScrollBarRange, 3090 TProgressBar: its division by Maximum, which
    SetMaximum keeps above 0); members without calls (868 TSkinButton::MouseMove, 921/1082/1122 the
    IsPressed functions, 1577 TEditLine::SetActive, 1954 TEditText::SetActive and 2049 TFocusGroup::SetActive,
    which call only TEditLine::SetActive, non-virtual; 2394 SetRange, 2405 SetValue, 2456 MouseMove, 2533
    MouseWheel and 2548 KeyPress of TScrollBarRange, 2608 TScrollBar::HandleEvent, 3100 SetMaximum, 3110
    SetCurrent, 3119 SetComplete of TProgressBar); 529 (GetToken: parsing only) and 650 (Arrange's
    "unknown token type": GetToken gives only the types the switch has); 793 (TSkinIcon's constructor: a
    skin's size is never negative); 973, 974 (TIconButton::Draw: TSkinButton::Draw, called first, asserts
    x>=0 and y>=0, and the texture offset is +2); 1230 (TCheckBox::SetEnabled: the new label has its size and
    is not shortened, so no text metrics; DeleteChild); 1283 (TEditLine's second constructor: no calls); 1625
    (InsertText's second length assertion: the clamps before it make it hold); 1727, 2125, 2631 (the
    constructors of TEditText, THorizontalScrollBarRange and TListBox: their scroll bars and ranges get valid
    arguments, no fake call); 1803, 2597, 2753 (the MouseWheel functions of TEditText, TScrollBar and
    TListBox: TWindow's dispatch to scroll bars and ranges, no fake call); 2326, 2587 (the scroll bars'
    SetSize: the sizes are clamped to the range's minimum and the skin's fixed thickness); 3011
    (TFontListBox::GetLine: the vector read after the range assertion).
  The Dialogs run reaches 58 DialogElements.cpp sites, none that the DialogElements run does not.
- **Mutation check** (high): 19 changes (build/scratch/mutate34c.py of the worktree, each test run alone): in
  Dialogs.cpp the caption length, MouseMove's TEND2 swapped, TLoginDialog::KeyPress's and TWaitLabel::Draw's
  TEND1 to TEND, THelpDialog's unknown event value, the move dialog's pile range, the new channel dialog's
  channel count, the VIP assertion's values swapped, UpdateLine's range, the TEND1 of CreateCharacterListDialog
  and CreateUpdateDialog; in DialogElements.cpp SetSkins' condition, THorizontalScrollBar's TEND2 swapped,
  InsertText's TEND2 to TEND1, SetSelection's range, TFocusGroup::Previous's assertion removed, TLink::Arrange's
  width condition, TEditLine::HandleEvent's TERROR text, TResizer::MouseMove's TEND2. All 19 detected, every
  one by the error rounds' exception comparisons (2 to 148 failures); the removed Previous assertion also makes
  our side read outside the member list (an access violation after the 2 failures).
- **Totals** (full build.cmd, November 2, 2026): difftest.exe 16,642,851 checks, sysinctest.exe 67, vlong 3,201, no
  failures; "DialogElements errors: 1,226,638 checks, 401,790 exceptions compared (12,810 calls that threw)",
  "Dialogs errors: 200,285 checks, 61,097 exceptions compared (4,786 calls that threw)"; DialogElements 135,478 and
  Dialogs 98,889 as before (alone: 134,736 and 98,474), every other test's count as in the second part's run. A
  DIFFTEST_ONLY=Dialogs run takes about a minute (the round about 45 s), DialogElements about 7 s.
## Rendering comparison (issue #20)

The script, the method and the results: notes/rendering.md. Decisions with a reasonable default:

- **Read the frame from the engine's own surfaces, not from the screen** (high): the monitor is off (issue #18's
  screenshots were white, DWM composes nothing), but every engine composes the frame in a surface of its own before
  it shows it. A DLL in the client reads that surface at the moment the client shows it (DirectDraw Blt into the
  primary or Flip, Direct3D 9 Present through GetRenderTargetData, SwapBuffers through glReadPixels): the same
  method for both clients, no display needed, windowed and fullscreen. It is what the client drew, before the
  desktop composition (which would add nothing of the client's).
- **Load the DLL through an import entry in the run folders' copies, not by injection** (high): the first
  version started the client suspended and injected the DLL with CreateRemoteThread/LoadLibraryA from a small
  injector; Windows Defender's behaviour monitoring stopped the injector (Behavior:Win32/DefenseEvasion.A!ml,
  November 2, 2026, 13:20). It was deleted and not worked around. The copies are patched anyway (issue #18's
  patch_client.py); one more import descriptor in a new section is an ordinary dependency for the loader, which
  runs the DLL's DllMain before the client's entry point (so the hooks see StartTime's static initialiser). The
  original Tibia.exe and build\Release\Patella.exe are not changed. Nothing in Defender was changed.
- **Virtual time instead of choosing a capture moment** (high): animations follow two clocks, timeGetTime
  (GetMilliseconds: steps, effects, talk boxes, blinking buttons) and the main timer's ticks (ActionNumber:
  object and outfit phases, the edit cursor, hover, ambient light). Two runs cannot be made to capture at the same
  real moment, and waiting for everything to settle would lose talk boxes and effects. The DLL makes both stand
  still and the script moves them by the same amounts after every step (2000 ms, 20 ticks), so packets and input
  are always handled at the same virtual time. The wall clock follows the same virtual time from a fixed date.
  The timer is not created (SetTimer/KillTimer of id 1 recorded only) and its ticks are sent as WM_TIMER by the
  DLL in the main thread: the client's own OnTimer/TimerEvent runs, nothing of the client is replaced.
- **GetCursorPos fixed** (medium): OnIdle polls the real cursor and feeds moves into the client; a fixed point
  outside the window makes the runs independent of the machine's mouse. The driver's posted mouse messages are
  the only moves, as in issues #18-#21.
- **The measured frame rate is masked** (high): "Current framerate: %2.2f" in the advanced graphics options
  (Dialogs.cpp TAdvancedGraphicsDialog::Draw, GetFPS) is the number of frames the engine drew per second of the
  clock (WaitForNextFrame); with the clock standing still between steps it is how many frames the machine
  drew in real time, so it differs between two runs of the same client (the repeat runs of Tibia.exe show it).
  The mask covers only the number: 100x16 pixels from 112 pixels right of the text's point
  (SkinProperties.Style1394.Data70 from the dialog's corner, read from memory in each run; the colon ends at
  +110, the number starts at +113: measured in the pictures). Low-medium for the pixel offsets; the script fails
  if a difference falls outside the mask, so a wrong mask cannot hide anything else.
- **Fullscreen in the desktop's own mode** (high): the fullscreen check box switches the display mode
  (SetDisplayMode/ChangeDisplaySettings, Direct3D 9 Reset); with FullscreenMode = (1024,768,32), the mode the
  monitor-off desktop has, no mode really changes. The script refuses fullscreen runs when the desktop is in
  another mode, checks the mode before and after every run and stops when it changed. Other fullscreen
  resolutions (a real mode switch) were not tried.
- **The frame rate is a measurement, not a reconstruction difference** (high): Patella.exe shows about 1.5 % more
  frames per second than Tibia.exe on every engine, and draws about as many more frames over a whole session;
  Tibia.exe alone varies by one frame between runs (125.50/126.00). WaitForNextFrame and GetFPS are compared by the
  difftest against a scripted clock in all three engines; the systematic part is the faster build (MSVC 14). Not
  fixed (nothing to fix in src/), masked as above.
- **One recorded session for all game scenes** (high): issue #19's live session of Tibia.exe (orig-live.jsonl)
  has the scenes the issue asks for (game view, side bar with inventory, container, trade, battle, skills and
  VIP windows, the automap, talk boxes, look texts, a context menu); replayed, both clients get the same server
  bytes and the proxy checks every client packet against the recording. No server run was needed. The start
  screen, the option dialogs and the login dialog come before any packet.
- **The option dialogs closed with Cancel/Ok, not changed** (medium): issue #21 drove every option; here they
  are only shown (the advanced dialog shows the engine selected). The engine is chosen in Tibia.cfg (Engine =
  0/1/2), and the script checks that the frames came from that engine's API (the client would fall back
  silently).
- **The side windows at the end of the session** (medium): opening Skills, Battle and VIP before the game steps
  and closing them again left the Battle window open in one try (the second click on its toggle did not close
  it; not investigated, the same in both clients) and the trade window then pushed the bag window out; at the end
  of the session they need no closing. The toggle buttons are placed from the Logout button's node and
  TButtonsWindow's skin points (Tibia.exe has no name for TTextToggleButton).

## Original line numbers (issue #35)

Claude Opus 5.5 (issue #35), November 2, 2026.

- **The form** (the user's decision, 2026-11-02): every site names the original's line as the macro's first
  argument: `TASSERT(66, s!=NULL)`, `TASSERT1/TASSERT2(line, ...)`, `TERROR(1208, "...")`, `TERROR1/TERROR2`,
  `TERROR_REASON/TERROR1_REASON(line, ...)`, `TEND(1280)`, `TEND1(1280, Type)`, `TEND2(line, a, b)`,
  `TEND_HANDLERS(line, ...)`, `TMYEXCEPTION(line, kind, ...)`, and the file-local macros the same way
  (`TASSERTB/TASSERTB1/TASSERTB2` of Map, graphics, SurfaceDX, surfaceDX9, ContentWindows; Creatures.cpp's
  `TASSERT1_BUG/TASSERT2_BUG`; MapWindow.cpp's `TERROR1B`). `__LINE__` is no longer used for messages; no
  `#line`, no tables. `TEND` was an object-like macro and takes the line now. Both handlers of a TEND name the
  same line, as in the binary (every TEND pair of catch blocks pushes one line).
- **The evidence: tools/lines.py** (high). The binary side: objdump of the game code (0x401000 up to the
  library region), every call of the constructors 0x508400 (assertions, errors, the "unknown exception
  occurred" catch blocks) and 0x5086a0 (the wrapping catch blocks); the line is the immediate pushed just
  before the file (the second push before the call; all 3,191 calls have one); the literal strings pushed
  since the previous constructor call or throw give the signature (reason, name2, name1, kind, file, in the
  order the code builds them); the function from symbols.tsv (a catch block's parent). A TEND is the pair of
  its two catch blocks with the same names and line. Copies of a site (header code in several objects,
  inlined functions) are one unit: 2,053 units. Our side: every macro invocation in src/ (headers included,
  #define lines skipped), its literal strings as its macro builds them (#Expr stringized, bug suffixes,
  literal kinds and reasons; strings built at run time left out, they match anything). `align` matched both
  sides per file by order and signature (a maximum-score alignment; an address comment naming the unit's
  function scores a little more) and wrote the lines in; `check` is the proof: each of our sites must name a
  line where the binary has a unit with its signature, every unit must be named by one of ours, the lines
  must grow in our source order, and every exception must be in its EXPLAINED table and verified. Result:
  2,089 sites, 2,053 matched (TASSERT 351, TASSERT1 289, TASSERT2 100, TASSERTB 1, TASSERTB1 57, TASSERTB2 3,
  TASSERT1_BUG 1, TASSERT2_BUG 1, TERROR 58, TERROR1 43, TERROR1B 1, TERROR_REASON 11, TERROR1_REASON 4,
  TMYEXCEPTION 21, TEND 512, TEND1 364, TEND2 236), 36 explained, **0 mismatches**. The order check found no
  inversion besides the one explained; the alignment needed no manual pair besides it.
- **Sites without a unit** (36, each in lines.py's EXPLAINED with the reason, verified by the check):
  - 33 TENDs whose function has no catch block in the binary (FuncInfo with 0 try blocks: the try block calls
    nothing that can throw, the compiler dropped the handlers; the check verifies that symbols.tsv lists no
    catch block for the function): ContentWindows.cpp 2 (TInventoryView::GetSlotAt, ~TSkillsWindowBody), Creatures.cpp 2,
    DialogElements.cpp 13 (the Arrange functions and the IsPressed/SetActive members, issue #34's "not
    reachable" list), Dialogs.cpp 1 (TEditListDialog::Arrange), GUI.cpp 1 (InitSkinProperties), MainFrm.cpp 3,
    MainWindow.cpp 2 (GetConsoleWindowHeight, TSplitter::Arrange), graphics.cpp 9.
  - graphics.cpp DrawPicturePart's Width>0 and Height>0: dropped by the compiler (the function returns
    before them; the exception tables keep their states, the old comment says so).
  - ContextMenus.cpp THelpBubble::Place's second newY>=0: folded into the first (line 262) by the compiler.
  - Their lines are **estimates** (low; no message can carry them: the original has no code for them):
    linear between the neighbouring matched sites' lines (Place's newY: 263, from the pairs 257/258 and
    267/268 of the other cases). Ours keeps these handlers and assertions (/EHa), as before.
  - One site out of our order (high): Map.cpp ProcessMissileEffects' Effect.Status!=0 is line 841 in the
    binary, before the moving branch's 850-858: the original has the arrived missile's branch first (the code
    at 0x4bbe2a falls through to it); ours has it in the else branch. The line is the binary's (841); the
    code is left as it is (the same behaviour; the order is not part of this issue).
- **No binary unit without a site of ours**: every one of the 2,053 units is named.
- **File names** (high): our `__FILE__` is right everywhere: the check compares each unit's file (the
  basename of the binary's string, which is what the messages print) with the file the site is in, including
  the headers Container.h, ConsoleWindow.h, GUI.h, Talk.h and Utils.h and the lower-case graphics.cpp,
  surfaceDX9.cpp, sysinc.cpp. No change. glh_extensions.h's two printf lines already carry the original's file
  and lines (issue #11) and are not TMyException sites.
- **Reconstruction bugs found and fixed** (high, each from a unit the alignment left over):
  - Talk.cpp IsTalkIgnored ends with TEND2(Mode, Channel), not TEND1(Channel): the catch block 0x500186 pushes
    [ebp+0x8] (Mode) and [ebp+0x10] (Channel) with the names "Mode" and "Channel" (0x5001cf-0x5001e1).
    symbols.tsv's params column says "Channel" only (triage.py's string order; not changed).
  - Map.cpp AppendObject's "too many objects on map point" has a reason: a stringstream "ObjectCount: <n> rx:
    <rx> ry: <ry> rz: <rz>" (0x4b918e-0x4b9258: the stream constructor 0x403a10, the four texts 0x56cc20,
    0x56cc18, 0x56cc10, 0x56cc08 and the values [esi], rx, [ebp+0xc], [ebp+0x10], then str() as the reason).
  - Communication.cpp's "unknown talk mode" (the talk packet's default case) has the reason "[bug0000649]"
    (0x410e8f; TERROR1_REASON now).
  None of the three is reached by a test (their sites were not compared before).
- **The difftest compares the lines exactly** (high). imageeh.cpp CompareThrown: the comparison of the
  message and origin with the lines replaced stays, then each line pair is compared exactly, one check per
  pair as before, so every check count stays the same (the map's checks became equality checks).
  EH_LINEMAP=1 compares through the old map (for debugging); EH_LINES=1 and ER_LINES=1 print the lines seen.
- **tools/compare_files.py** (high, not run here: the coordinator runs it): error.txt is compared byte for
  byte except the masked crash time; the line map (check_lines, function_at, original_lines and the SITE
  substitution) is gone. The lines of the #21 run's error.txt (Control.cpp 1208, 1274, 1280, Network.cpp 885,
  Player.cpp 383 in Tibia.exe's) are what our sites name now (Control.cpp ReceivePackets' two TERROR1s and
  TEND1, Network.cpp's TEND1(885, ErrorCode), Player.cpp Run's TMYEXCEPTION(383, ...)).
- **callcheck** (high): `python -I tools/callcheck.py` gives the same output for all 35 files as on main.
- **Mutation check** (high): (1) static, build/scratch/mutate_static.py of the worktree: 15 changes on a copy
  of src/ (TASSERT in Utils.cpp and sysinc.cpp, TASSERTB2 in Map.cpp and ContentWindows.cpp (to the next
  site's line), TASSERT1_BUG in Creatures.cpp, TEND2 in Talk.cpp, TEND1 in Objects.cpp, TMYEXCEPTION in
  Script.cpp, Container.h and Player.cpp (a wrapping one), TERROR_REASON in Script.cpp, TERROR1_REASON in
  Communication.cpp, TERROR1B in MapWindow.cpp, an estimated TEND in graphics.cpp, and two TENDs with the same
  signature swapped in Communication.cpp): all 15 reported (the swap by the order check only). (2) the
  difftest: build/scratch/mutate_diff.py of the worktree: 14 lines shifted by one at once (Utils, Status,
  Preferences (to the next assertion's line), Help, Talk TEND1, Inventory TEND1, Map TASSERTB1, Objects TEND1,
  Script's error() TMYEXCEPTION and TERROR_REASON, graphics TASSERTB1, GUI TComponent::SetSize's assertion, a
  DialogElements TEND2, MapWindow's TERROR1B), the objects rebuilt, DIFFTEST_ONLY=Errors, GUI, DialogElements
  and MapWindow run with every failure printed: 13 detected, each by its own failures ("our <file> line <new>,
  the original's <old>": 2 to 572 each); graphics.cpp's 1218 (DrawText's alignment assertion) is reached by no
  test (the static check catches it). Restored: 0 failures in all four.
- **Totals**: full tools\difftest\build.cmd (November 2, 2026): difftest.exe 16,685,370 checks, sysinctest.exe
  67, vlong 3,201, 0 failures, "build.cmd: all tests passed"; every per-test and error-round count is main's
  (the line checks are one per pair, as the map's were). compile-check clean with real MFC and with
  TIBIA_MFC_SHIM=1; tools\build-patella.cmd 0 warnings, resources 0 differences.

## Communication.cpp and Control.cpp tests (issue #36)

Claude Opus 5.5 (issue #36), November 2, 2026.

- **An executable of their own: communicationtest.exe** (high). difftest.exe's other tests define fakes under the
  names of about 75 functions of Communication.cpp, Control.cpp and Network.cpp (playertest.cpp's Control and Network
  stubs, the Send* recorders of mapwindowtest.cpp, contextmenustest.cpp, consolewindowtest.cpp, contentwindowstest.cpp,
  dialogstest.cpp and mainwindowtest.cpp, which their image sides jump to as well): linking the real files there
  would mean renaming all of them and changing eight tests. As with sysinctest.exe, build.cmd builds a second
  program from difftest.cpp (DIFFTEST_COMMUNICATION; DIFFTEST_ONLY=Communication or Control runs one test) with
  communicationtest.cpp, controltest.cpp, commworld.cpp, imageeh.cpp and every src file but MainFrm.cpp and
  Player.cpp (MFC's frame and application classes, which the shim only declares; their two globals PacketHeader
  and DisplayChanged, the CAsyncSocket members and AfxSocketInit are defined by the test, the socket ones as traps).
  It links with /OPT:NOICF, so our identical functions stay apart (the image's 0x496160 is the one copy of five
  empty Exit functions: our five are patched to the same fake). difftest.exe and its counts are unchanged.
- **The layer below: every callee a fake, on both sides** (high). commworld.cpp has one fake per function the two
  files call (about 300: Map, Creatures, Inventory, Objects, Status, Preferences, Talk, Help, Script, graphics,
  GUI, MainWindow's and ConsoleWindow's members, the dialog creators, the context menu and help bubble
  constructors, Surface, sysinc, Skins, the empty Exits; in controltest.cpp Communication.cpp's functions too).
  The image's function gets a jump to the fake at its address, ours at the start of our function
  (imageeh.cpp's PatchImageJump/PatchOwnCode, undone by DisableImageExceptions); constructors through
  /alternatename. A fake logs its call with its arguments into the log of the side that runs and answers from
  that side's world (creature records, text buffers with lines, windows, a surface, the event queue, the action,
  connection, mouse and help states), which the test builds the same way for both sides before the calls;
  answers that are not state are hashes of the arguments and the round's salt. The comparison after each call:
  both logs, what each side threw (imageeh.cpp's CompareThrown, line numbers exactly), the creature records
  byte for byte, the text buffers' lines and cursors, the worlds' states, the event queues with their parameters,
  the receive buffer's position, ModalWindow and MainWindow. The worlds' windows, text buffers and surface are our
  classes (TCtlWindow from TWindow, TCommBuffer from TTalkBuffer, TCtlSurface from TSurface): the image calls
  their virtual functions through the same slots.
- **Network.cpp's buffers stay real** (high). GetByte ... GetBytes and BeginCommand ... SendBytes run on both sides
  for real on the buffers the test sets: the image's ReceiveBuffer (0x719d68) and SendBuffer (0x719d5c, which the
  test constructs: its initialiser 0x559fd0 never ran), ours found as the `mov ecx, imm32` before the first call
  in our GetByte and BeginCommand. So a truncated packet fails in each side's own TReadBuffer with its own
  exception, and every packet a sender builds is compared byte for byte: SendCommand is a fake that logs the
  send buffer. GetNextCommand is a fake that serves the test's frames as PrepareNextPacket would (a byte left
  over in a frame is read as the next type). Network.cpp itself is issue #37's.
- **The image's statics and libraries** (high). Constructed by the test: TRSA.cpp's PublicExponent (0x71f004) and
  Communication.obj's copy of the header's HeaderExponent (0x5c435c, vlong 0x50c560). The skins and
  SkinProperties (Redraw's cursor hotspots and tooltip) come from both sides' real InitSkins and
  InitSkinProperties before the fakes. rand is one sequence per side (the same), the image's sprintf, _snprintf
  and _strdup are our CRT's, Sleep is bound in both import tables to a fake. The object flags the fake
  GetObjectFlag answers are Tibia.dat's (our Objects.cpp before the fakes), so recorded and generated map data
  parse as in the client.
- **Inputs** (high). The frames of issue #19's two live recordings (tools/difftest/commseeds.h, generated by
  commseeds.py from compare_traffic.py's `<client>-live.jsonl`: 55 decrypted server frames), six times each,
  changed after the first pass; generated frames of 1-4 messages of every type of both tables (61 game, 8 login)
  with edge values, map data consistent with the flags, creatures that exist or not, strings of every length up
  to 2,500; unknown types; truncated frames, changed and appended bytes; patches over several frames (a second
  ReceivePatch in a run of ReceivePackets after an exception, as the client calls it again); the readers
  ReadCreature, ReadField, ReadMap and ReadFloor directly; every sender with random and edge arguments
  (0xffff positions, the talk modes each overload accepts). Not generated, both sides' bugs kept in the source:
  a patch block after a ReceivePatch or ReceiveWholeFile that failed its "no transfer" assertion (both have
  already stored the new size: the block writes past the old buffer on both sides, heap corruption; a test
  round holds one patch start, mutations never write 0x1e, 0x1f or 0x20 into login frames), and a patch size
  that makes new[] fail (VC7.1's MFC operator new and ours fail differently). A block of a zero-sized patch
  divides by zero on both sides (caught by catch (...)): difftest.cpp's ExpectDivideByZero keeps the crash report
  quiet while the test runs.
- **The stack below each call is filled with one pattern** (high): a catch block that prints a local not assigned
  yet (ReceivePackets' Container when ReceiveOpenContainer/ReceiveCloseContainer fail at their first byte) prints
  the same value on both sides, and a local the original initialises and ours did not shows up.
- **Statics** (high). Control.cpp's LastPacketType, ReconnectTimeout, LastMouseMove, WindowHandle and the four
  callbacks are compared after every round: the image's at their addresses, ours found once by values the test
  gives them through our functions (InitWindow, MouseMove, ProcessEvents' event 0x3f, ReceivePackets).
  LastPacketTypes and LastPacketIndex through OutputControlErrorInformations: ours into a std::ostringstream, the
  image's (0x445900) into its own std::stringstream (constructor 0x403a10, str() 0x412a60). Reconnect and ShowFPS
  through what they change (ConnectionClosed, Redraw). Communication.cpp's statics (PatchData and the patch's
  numbers, SkipFields, the cancel counter) are compared through their effects: the rounds keep them from frame to
  frame and call to call (a patch's blocks, the skipped fields, the 16th cancelled step); OutfitRequested directly.
- **Error rounds** (high). Communication: every frame type, three generated frames each, with the n-th fake call
  failing for every n (up to 300) with the side's own TMyException (TReadBuffer::GetByte on an empty buffer,
  Utils.cpp 299), CancelPatch failing, every sender with the arguments its assertions refuse and with failing
  calls, and last TRSA::Encrypt throwing a const char * (the image's with its ThrowInfo 0x5bb5d0) or a structured
  exception (the login packets' two catch blocks). Control: InitControl/ExitControl, every scenario, ProcessEvents
  and ReceivePackets with the n-th call failing with a TMyException, a structured exception (catch (...)) or a
  PROGRAM_ERROR (the image's: _CxxThrowException with ThrowInfo 0x57b3c4). Every error site of both files is
  reached: Communication.cpp 162 of 162, Control.cpp 47 of 47 (build/scratch/coverage.py of the worktree, from
  the CM_LOGS/CT_LOGS logs).
- **Reconstruction bugs found and fixed** (high, each from a difference and the binary):
  - Talk.h: TTalkBuffer's two Remove overloads were in the wrong vtable slots: the binary's vtable (0x573738) has
    Remove(int) at +0x1c and Remove(TTalkLine *) at +0x20 (ReceiveRemoveRuleViolation calls +0x20 with a line);
    MSVC puts overloaded virtuals in reverse order of declaration, so Remove(TTalkLine *) is now declared first.
    (The only overloaded virtual pair in src/.)
  - ReceiveChangeObject read the stack position after AbsoluteToRelative; the binary reads it before (0x40cff3).
  - Seventeen locals (sixteen functions) the catch blocks print are zero-initialised before the try block in the binary (`xor reg,reg;
    mov [ebp-x],reg` before the try state): CreatureID of the seven creature updates, ChannelID of four channel
    packets, CharacterID of BuddyLogin/BuddyLogout, Position of ReceiveDeleteInventoryObject, value of
    ReceiveGamemasterActions, PlayerX/PlayerY of ReceiveMap.
  - SendLogin and SendGameLogin: the catch (...) blocks end the byte dump without std::endl (the binary goes from
    the loop to str(), 0x413bc7), unlike the catch (const char *) blocks.
  - Control.cpp MouseMove, help mode: the bubble is kept in a local and ModalWindow written after Place
    (0x446b2a reads it after HelpMouseMove, 0x446b8f writes it): an exception in GetHelpHandle, the bubble's
    constructor or Place leaves ModalWindow as it was (dangling after the delete, as in the client). A global
    store cannot move past an external call, so this is the source's order.
- **A compiler artifact, not fixed** (high): Resize in fullscreen mode keeps the mode's size in registers and its
  catch block prints the arguments as they were passed (0x4459ed); ours assigns the size to the parameters and
  prints that. The same class as CheckHotkey's i (issue #34): the source is left as it is; the error round passes
  the mode's size as the arguments in that mode.
- **Not explained, excluded** (medium): a structured exception raised by a fake getter while the original builds
  an assertion's message (MouseMove's line 461 prints GetActionState(); state 5 of its try block) left the
  original's frame without reaching its catch (...) (an unhandled exception in the child), while ours caught it.
  A TMyException at the same place is caught and compared on both sides. The three getters whose values Control.cpp's
  assertions print (GetActionState, GetActiveCharacter, GetAccountCharacters) throw only TMyExceptions in the error
  round (commworld.cpp AssertionGetter). Probably VC7.1's frame handler and the temporaries of the message; not
  a difference of the reconstruction.
- **Deleted test windows are freed when the round ends** (high): a window deleted in a round keeps its memory, so
  a pointer the code keeps to it (ModalWindow after the case above) is named "deleted Wn" on both sides; with
  reuse the name depended on the heap's layout.
- **Every function compared**. Communication.cpp: all 130 definitions (the senders and SendLogout, the 65
  Receive* handlers through both dispatch tables, ReadCreature/ReadField/ReadMap/ReadFloor also directly,
  PrepareMovement, CancelPatch, InitCommunication); ExitCommunication by its code (a single ret, as 0x496160;
  the fakes replace both). Control.cpp: all 27 functions, OutputControlErrorInformations through the image's
  stream. Callees never called by the tests: the ExitProgram, OpenURL and SetDisplayMode callbacks (Control.cpp
  only stores them and passes them to the dialog creators, which are fakes).
- **Mutation check** (high): 27 changes (build/scratch/mutate36.py of the worktree; each mutated object rebuilt, its test run
  alone, the object rebuilt from the restored source): 16 in Communication.cpp (a sent word, a walk direction, the
  0xffff position check, the patch progress, a string's maximum length, the name capitalisation, the skip marker,
  the teleport distance, the talk time's sign, the cancel threshold, the text message mode, the buddy state, the
  floor limit, a TEND line, a zero-initialised local, the rule violation name comparison), 11 in Control.cpp (the
  frame rate key, the both-buttons flags, the tooltip clamping, the reconnect time, a channel's send mode, the
  ambient light tick, the packet type ring, a TERROR line, InitWindow's Reconnect, the logout dialog's data,
  LastPacketType): all 27 detected (2 to 3,889 failures each). The first run let four survive: the cancel
  threshold and the local (inputs added: sixteen and more cancelled steps at one position; every message cut after
  each of its first 40 bytes in the error round) and two equivalent changes (a comparison of 29 characters of
  names of at most 29; a tooltip clamp whose two branches agree at the edge), replaced by the last two above.
- **Totals**: full tools\difftest\build.cmd (November 2, 2026): difftest.exe 16,685,370 checks (main's, unchanged),
  sysinctest.exe 67, communicationtest.exe 1,134,513 (Communication 364,041 and its error round 492,325; Control
  114,595 and its error round 163,552), vlong 3,201, 0 failures, "build.cmd: all tests passed". Alone:
  DIFFTEST_ONLY=Communication about 8 s, Control about 2 s. compile-check clean with real MFC and with
  TIBIA_MFC_SHIM=1; tools\build-patella.cmd 0 warnings, resources 0 differences; tools/lines.py check 0 mismatches.
## Network.cpp and MainFrm.cpp (issue #37)

Claude Opus 5.5 (issue #37), November 2, 2026. Two lines of comparison: the difftest (networktest.cpp,
mainfrmtest.cpp: every function in the child against the original's code, logs and state after random
operations, an error round) and the real clients (tools/compare_network.py: crafted and fuzzed server streams
through tools/proxy.py's replay, notes/network.md).

- **The files are compiled into their tests** (high). networktest.cpp `#include`s src/Network.cpp and
  mainfrmtest.cpp src/MainFrm.cpp (build.cmd no longer links MainFrm.cpp itself): their statics (Network.cpp's
  server list, buffers, socket, timeouts; MainFrm.cpp's cursors, button flags, wheel remainder, class name) are
  then the tests' and are set and compared by name; the image's are at the addresses of the source comments. The
  code is the same; only the translation unit differs. The alternative (finding our statics by sentinel values,
  surfacegltest.cpp's way) would need one search per static.
- **The MFC boundary** (high). MFC is linked statically into both clients (MFC 7.1's nafxcw.lib in the image,
  MFC 14 in Patella.exe), so their MFC code differs by design. The tests compare at the calls into MFC: the image's
  MFC functions (CAsyncSocket's constructor, destructor, Create, Connect, Close, Receive, Send, AfxSocketInit;
  CFrameWnd's constructor, destructor, OnCreate, OnSize, OnClose, OnDestroy, OnSetCursor, PreCreateWindow; CWnd's
  Default, Get/SetWindowPlacement, ModifyStyle/Ex, SetWindowPos, MoveWindow, FromHandle, DestroyWindow;
  AfxGetModuleState, through which MFC 7.1's inline AfxGetApp and AfxGetInstanceHandle read the module state)
  start with a jump to the tests' fakes, and the shim's declarations (the difftest builds with the shim) are
  defined by the tests as the same fakes (they were playertest.cpp's abort() stubs). What MFC itself does
  (message dispatch through the message map, CWnd::Default's DefWindowProc, the socket notifications' window)
  is not the reconstruction's and is compared only through the real clients (compare_traffic.py,
  compare_files.py, compare_rendering.py, compare_network.py).
- **Windows functions** (high). The image calls ws2_32 by ordinal (103 WSAAsyncGetHostByName, 111
  WSAGetLastError, 108 WSACancelAsyncRequest, 22 shutdown, 12 inet_ntoa): its import slots get the fakes;
  difftest.exe does not link ws2_32, networktest.cpp's NWSlot_ variables are the `__imp__` symbols
  (/alternatename, surfacegltest.cpp's way). user32 (PostMessageA, Get{Client,Window}Rect, IsIconic, IsZoomed,
  Set/KillTimer, SetCursor, GetCursorPos, PtInRect, ClientToScreen, ScreenToClient, SetCapture, ReleaseCapture,
  GetSystemMetrics, LoadCursorA, DestroyCursor) and shell32's ShellExecuteA through both import tables
  (surfacedx9test.cpp's way). rand() is a fake on both sides (the image's 0x511f33 and our CRT's): one MSVC
  generator per side from the same seed before every operation, so SendCommand's padding is compared too.
- **playertest.cpp's stubs** (high). The linker folded the identical `{ abort(); }` stubs into one function, so
  redirecting one (as mainfrmtest.cpp does with Control.cpp's LeftButtonDown, KeyPress, ...) redirected all;
  each stub now calls StubCalled(__FUNCTION__) (a body of its own). Its stubs of Network.cpp's functions and
  of the MFC members MainFrm.cpp calls are gone (now the real file and mainfrmtest.cpp's fakes). If Control.cpp
  is linked into the difftest later (issue #36), its real functions replace the stubs and mainfrmtest.cpp's
  Redirect of them works the same (restored at the end).
- **The C functions never throw in MainFrm's error round** (high). AddFrameSize's and ExitProgram's TENDs (and
  KeyUp's) have no handlers in the binary: their try blocks call only C functions (GetClientRect,
  GetWindowRect, PostMessageA; KeyUp only TranslateKey, inlined), which /EHsc assumes cannot throw, so VC7.1
  dropped the handlers (notes above, "MainFrm.cpp and the MainFrm/MainWindow gap"; issue #24 found the same in our
  /EHsc compile: Patella.exe has no handlers there either). The difftest compiles with /EHa, which keeps ours, so
  a throwing user32 fake made our AddFrameSize wrap the exception ("MainFrm.cpp 388") where the original lets it
  through (58 failures in the first run, all through AddFrameSize and ExitProgram). A C function cannot throw a
  C++ exception in either client, so the user32/shell32 fakes do not throw in the round; every TEND of the file
  is still reached through the MFC and game fakes.
- **Compared** (Network.cpp, all 44 functions of 0x4ca0e0-0x4cf982): InitServerList, SetTestServerPort,
  ResetServerAvailability, SetConnectionTimeout, SetNetworkWindow, SetTestServerAddress, SetServerError,
  SetServerIPAddress, GetCurrentServer, GetServerErrorCode, BeginCommand, SendByte, SendWord, SendQuad,
  SendString, SendBytes, SendCommand (both forms, the padding, the encryption), PrepareNextPacket,
  GetNextCommand, GetByte, GetWord, GetQuad, both GetStrings, GetBytes, CheckConnectionTimeout, GetErrorMessage
  (every error code it names, both types, the test server's text), ResolveHostname, GetResolvedIPAddress,
  CreateSocket, ConnectSocket, SetSymmetricKey, Disconnect (its draining loop), ConnectToLoginServer (the
  recursion over the four servers), ConnectToGameServer, TSocket's OnConnect, OnReceive (frames in pieces,
  coalesced, empty, longer and shorter inner lengths, Receive returning 0 or an error), OnClose and OnSend
  (0x4eac00, empty), ShutdownConnection, InitNetwork, ExitNetwork; the compiler's TSocket destructor and scalar
  deleting destructor (0x4ca2f0, 0x4ca2d0: the image's own, run by every `delete Socket`), CObject's inline
  operator delete (0x4ca2c0, in the unwind funclet of `new TSocket`: not reached, nothing throws in
  CAsyncSocket's constructor) and TBuffer::SetSize (0x4ca300, Utils.h). The error round reaches 50 of the
  file's 54 error sites (every assertion, TERROR and TEND; PROGRAM_ERROR_SOCKET_INIT); not reachable: 355 and
  398 (the catch (...) around the XTEA block loops: EncryptBlock and DecryptBlock cannot throw), 698
  (SetSymmetricKey's catch (const char *): SetKey throws nothing) and 961 (ExitNetwork's TEND: only the socket's
  destructor is inside, and MFC's does not throw; ours is noexcept). BeginCommand's TEND (275) needs a send
  buffer smaller than its 5 header bytes, which the test sets (the client's is 0x800).
- **Compared** (MainFrm.cpp, all 36 functions of 0x4a8210-0x4abec8): TranslateKey (also playertest.cpp's
  exhaustive check), OutputErrorInformations (the image writes into a fake ostream: its operator<<(const char *)
  0x402630, operator<<(int) 0x402f00 and put 0x402c00 are hooked only around that call, because the image's
  TMyException constructors use them too; ours into a std::ostringstream), the constructor, the destructor,
  ExitProgram, CloseProgram, SetDisplayMode (every view mode and engine), OpenURL, PreCreateWindow,
  OnHostResolved (with and without an error, in every connection state), OnActivateApp, AddFrameSize, OnSize
  (the minimum 550x450 and its neighbours, minimised), OnGetMinMaxInfo, OnNcHitTest, OnTimer, OnClose (Closing,
  CloseRequest's answer), DestroyWindow, OnDestroy, UpdateMouseCursor (every cursor number), OnSetCursor (inside
  and outside the client area, fullscreen), the six mouse button handlers (capture, fullscreen screen points),
  OnCaptureChanged, OnMouseWheel (notches, remainders, both signs), KeyDown, KeyUp, OnKeyDown/OnSysKeyDown
  (0x4ab7c0) and OnKeyUp/OnSysKeyUp (0x4ab7d0) with every modifier and the extended flag (Alt+F4), OnChar,
  SetDefaultWindowPosition, OnCreate, OnDisplayChange (screen sizes at and around the window's). Not called:
  the scalar deleting destructor 0x4ab9b0 (compiler-generated: the destructor and operator delete; MFC's
  PostNcDestroy deletes the frame) and the message map (data, MFC's dispatch). The error round reaches all 29
  TEND sites the binary has except the destructor's 123: our destructor cannot throw (issue #34, CFrameWnd's
  noexcept destructor), so the round never makes ExitControl throw from it.
- **Results** (high): no difference in either file. networktest.cpp and mainfrmtest.cpp found no reconstruction
  bug; the first runs' failures were the tests' own (a side-dependent random call, a stale socket stream after a
  reconnect, negative indexes in the send program) and the /EHa artifact above.
- **Mutation check** (high): 28 changes, each alone (build/scratch/mutate37.py of the issue's worktree): Network.cpp
  15 (the login port, the test server's error code, the server list's wrap, the overload check, the padding's
  rand() range, the packet header's length, OnReceive's error check, an error text, ResolveHostname's timeout,
  Disconnect's shutdown mode, ConnectToLoginServer's state, OnConnect's state, a TEND line, ConnectToGameServer's
  message, CheckConnectionTimeout's comparison), MainFrm.cpp 13 (Alt+F4's key, OnChar's letters, the wheel's
  remainder, OnSize's minimum, OnLButtonUp's capture release, SetDisplayMode's engine, the wait cursor,
  OnCaptureChanged's view mode, OnDisplayChange's comparison, OnHostResolved's error type, the constructor's view
  mode, CloseProgram's RunPatch, a TEND2's order): all 28 detected (9 to 26,823 failures). Three needed better
  inputs first (OnSize's and OnDisplayChange's boundaries, Alt+F4 more often), one was an equivalent mutant
  (OnReceive's `Count == 0`: a Receive that returns 0 adds no data, so the scan finds nothing new) and was
  replaced. Restored: 0 failures.
- **The real clients: replays, no server** (high). tools/compare_network.py (notes/network.md) plays crafted and
  fuzzed streams made from issue #19's live recording of Tibia.exe (copied to build\network) to both clients
  through tools/proxy.py's replay, which can now break, re-key, re-length, hold, split and cut frames and leave a
  port closed. The test server was not started.
- **"Disconnects at every point"** (medium): every structural point of the login answer (before the frame,
  after one and both size bytes, inside the body, at its last byte, after the frame, inside the second frame's
  size and body and at its last byte) and seven points of the game stream (0, 1, 2, 3, 100 and half the bytes of
  the map frame, inside the second frame), not every byte offset: between two such points the client's code
  takes the same path (OnReceive waits for the rest of a frame), and each run takes about 20 s.
- **Variable scenarios** (high): a frame under a wrong XTEA key, and an empty frame coalesced with the next frame
  (PrepareNextPacket then takes the next frame's undecrypted bytes as the inner length and the commands), make
  the client read garbage that depends on its session key, which rand() makes anew in every run (seeded with
  timeGetTime, notes/traffic.md). They are run twice with Tibia.exe and reported as variable only when its two
  runs differ as well (they did, every time); the outcome is the same kind in both clients (a critical error
  box or a reconnection, depending on the garbage).
- **An arrival-timing difference, not the reconstruction's** (medium-high). The first full run had three frames
  (two empty ones and the map) sent with three separate sends: Patella.exe, consistently, read the first empty
  frame alone and took the login connection's stale bytes still in ReceiveData as its inner length (a critical
  error, "unknown packet type during game"), Tibia.exe read all three at once and stopped with GetNextCommand
  returning -1 while the map frame waited (the original's odd behaviour after an empty frame: the second empty
  frame is the first one's inner length, the buffer's Position equals its Size, and no further FD_READ comes).
  Sent so that the arrival is fixed (the empty frame alone and the map frame 0.5 s later; the two empty frames
  and the map frame in one send) both clients do exactly the same, and the difftest compares this code with
  empty frames in every arrangement against the original's machine code (no difference). So the clients differ
  only in when their message loop reads (Patella.exe answers FD_READ sooner); the scenario was replaced by the
  fixed-arrival ones.
- **The proxy's own socket errors are not compared** (high): after the critical error box a client may exit
  with server data still unread, and Windows then resets the connection; whether it does depends on when the
  client exits (game-bad-size: once for Patella.exe in the final run, never in four other runs of either client).
  The packets, the boxes, error.txt and the exit are compared in full; the proxy's mismatches too.
- **Results of the real clients** (high): 51 scenarios, 47 the same, 4 variable (proven by Tibia.exe's second
  runs), no difference (notes/network.md). No reconstruction bug; src/ unchanged.
- **Totals** (full tools\difftest\build.cmd, November 2, 2026): difftest.exe 16,848,752 checks, sysinctest.exe 67,
  vlong 3,201, 0 failures, "build.cmd: all tests passed"; main's 16,685,370 plus Network 74,928, its error round
  38,878, MainFrm 31,424, its error round 18,152 (alone: Network 76,188 and 38,878, MainFrm 31,916 and 18,152: the
  counted parts start from another Random() state in the full run). Both tests put Random()'s state back at the
  end, so every later count (MapWindow, MainWindow and their rounds) is main's; without that MainWindow's round
  had 41,985 checks instead of 43,150. compile-check clean with real MFC and with TIBIA_MFC_SHIM=1; lines.py check
  0 mismatches (src/ unchanged, so Patella.exe was built only for the real-client runs: 0 warnings, resources 0
  differences).

## Modernization (Patella-Modern, November 2, 2026)

The fork's decisions; the details, alternatives and reasons are in notes/modernization-plan.md (sections 3, 6,
9 and 11).

- **D1 Equivalence contract:** tier A (packets, written files, frames, user-visible behaviour) never changes
  except by a recorded decision; tier B (difftest, the original layout) until M6; tier C (error.txt, import
  table, resources) until M5/M6.
- **D2 error.txt removed** (M5): no crash report file, no report dialog, no packet 0xE8. A fatal error shows the
  "Tibia Error" message box and writes the message to stderr. The game server only logs 0xE8
  (gameserver/src/game/receiving.cc CErrorFileEntry).
- **D3 Targets:** Windows x64 and Linux x64; no 32-bit product (oracle-x86 is a test build until M6). SDL3 for
  the window, input and clipboard on both (GLFW considered).
- **D4 Linux details:** the client's 8-bit code page inside, UTF-8 at the platform boundary; Tibia.cfg next to
  the data files; AppImage on Linux, a zip on Windows.
- **D5 Rendering:** Vulkan 1.3 only, hand-written (volk, VMA, vk-bootstrap), on both platforms; a pinned CPU
  Vulkan driver (lavapipe or SwiftShader) as the fallback and for the tests. The engine buttons become a
  graphics device choice.
- **D6 Compilers:** GCC 16 on Linux, MSVC on Windows x64, Clang on Linux in CI; C++23 baseline, C++26 through
  src/compat/cxx26.h.
- **D7 Reference frames:** Tibia.exe's DirectX 9 frames.
- **Build:** CMake (presets, Ninja), one `cmake --workflow` command per platform; the .sln/.vcxproj and .cmd
  scripts go.
- **D8** configurable servers and RSA key; **D9** the built-in patcher removed; **D10** integer scaling of an
  original-size canvas; **D11** borderless fullscreen; **D12** a settings file of the fork's own, Tibia.cfg
  unchanged; **D13** vsync by default; **D14** data file lookup, no CipSoft files in packages; **D15**
  namespace patella, the original naming style kept; **D16** a small developer log.
- **D17 Public:** the repository goes public after the M0 cleanup, its history published as it is (it contains
  earlier analysis output: rea-out/, names.tsv, symbols.tsv). The legal risk of publishing a reconstruction of a
  proprietary client was raised and accepted. No CipSoft files in the repository or in any package.

## CMake: the oracle-x86 preset (issue #4)

Settled by Claude Opus 5.5, November 3, 2026 (CMake 4.1.2, Ninja 1.13.2, MSVC 14.51 x86). `CMakeLists.txt`
(CMake 3.30+), `CMakePresets.json` (Ninja Multi-Config, configurations Release and Debug, binary directory
build\<preset>) and `cmake/OracleX86.cmake`. `cmake --workflow --preset oracle-x86` configures, builds Release
and runs the tests; the other presets of the plan (section 5) come with #5 and #7.

- **Environment** (form). The preset does not set up the compiler: it runs from a developer prompt for x86
  (`vcvarsall.bat x86`, the HostX86\x86 tools the .cmd scripts use); its architecture and toolset are marked
  `external` for IDEs that do it. CMAKE_C/CXX_COMPILER are `cl` (mingw-w64's gcc is on PATH here), and the
  configure stops unless MSVC targets 32-bit. Python is found on PATH before the registry
  (Python3_FIND_REGISTRY LAST; the registry had an older 3.11).
- **Flags** (high). Every one of CMake's default flags is cleared (CMAKE_<LANG>_FLAGS[_<CONFIG>], the linker
  flags, CMAKE_<LANG>_STANDARD_LIBRARIES), so the command lines carry only the options written in
  OracleX86.cmake. Patella.exe gets Patella.vcxproj's options in MSBuild's order (build\Release\build.log and
  the Debug log: /JMC /Od /RTC1 /MTd for Debug, /O2 /Oi /Gy /MT for Release), rc.exe `/l0x0409` with NDEBUG or
  _DEBUG and build\oracle-x86\res as its only include path, and the same linker options and import libraries
  in the same order. The four test programs get build.cmd's `/std:c++17 /MT /O2 /W3 /DTIBIA_MFC_SHIM
  /D_WIN32_WINNT=0x0601 /I src` with /EHa (/EHsc for vlongtest), for the C file too, the same sources in the
  same order, the same libraries and linker options, in Release and Debug alike; ml.exe `/nologo /safeseh`.
  patcap.dll gets build-capture.cmd's `/O2 /W4 /WX /MT`. One define is added that cannot be turned off:
  Ninja Multi-Config defines `CMAKE_INTDIR="<configuration>"` on every compile; no source uses it. CMake links
  through `cmake -E vs_link_exe`, which embeds a manifest unless it sees /MANIFEST:NO: Patella.exe has that
  already, and the test programs and patcap.dll get it too, since the ones cl links have no manifest either.
- **Object order** (form). The objects link in MSBuild's order: src\*.cpp sorted without case, then
  src\Extern\regexp\*.c. With regexp.c sorted in among the .cpp files, .text came out 96 bytes longer.
- **Result** (high). Patella.exe (Release): `compare_resources.py` 55 entries, 0 differences;
  `compare_imports.py` against Tibia.exe prints exactly what it prints for build\Release\Patella.exe, and
  against build\Release\Patella.exe no difference (the same DLLs in the same order, the same functions,
  OLEACC.dll delay-loaded). Section by section the two images have the same sizes, .rsrc and .fptable
  identical; .rdata differs in two strings, the rest are addresses that moved with them: the PDB path in the
  debug directory, and the `#line 173 "Extern\glh/glh_extensions.h"` name in glh_extensions.h, which /FC makes
  absolute from the compiler's working directory (MSBuild's: C:\Patella-Modern\Extern\glh\glh_extensions.h;
  Ninja's: C:\Patella-Modern\build\oracle-x86\Extern\glh\glh_extensions.h; the original prints
  "Extern\glh/glh_extensions.h" when malloc fails there, and so do the test programs, which build without /FC;
  issue #14 compiles SurfaceGL.cpp without /FC, "/FC and the glh_extensions.h #line name (issue #14)").
  No compiler or linker warning in either build. The CTest tests give exactly build.cmd's counts.
- **Resources** (form). tools/extract_resources.py runs at configure time (as the plan has it for Linux, section
  5) into build\oracle-x86\res; Tibia.exe and the script are configure dependencies, so a new Tibia.exe
  configures again. PATELLA_TIBIA_EXE (default the root's Tibia.exe) is also the tests' argument.
- **Tests** (form). CTest tests difftest, sysinctest, communicationtest (the program with Tibia.exe's path as
  its argument) and vlongtest (cmake/RunVlongTest.cmake: vlongtest.exe into a file, then vlongcheck.py), each
  with a private TEMP and TMP under build\oracle-x86\tmp, as build.cmd gives its run one; a CTest timeout of
  7200 s for difftest and communicationtest (DIFFTEST_TIMEOUT still stops a hung child after 1800 s). The test
  preset prints the programs' output (verbosity verbose) so the counts are in the log. patcap.dll is a target
  only: compare_rendering.py still builds and uses build\capture\patcap.dll.

## Compiler settings, warnings and code style (issue #8)

Settled by Claude Opus 5.5, November 3, 2026 (MSVC 14.51 / cl 19.51, CMake 4.1.2, clang-format and clang-tidy
22.1.3 from Visual Studio, mingw-w64 GCC 16.2). `cmake/Warnings.cmake`, `cmake/Hardening.cmake`, `.clang-format`,
`.clang-format-ignore`, `.clang-tidy`, `tools/tidy.py`, `.editorconfig`; one reformat commit of src/.

- **Modules as functions** (form). `patella_target_language`, `patella_target_warnings` and
  `patella_target_hardening` set options on one target, nothing global, so the oracle-x86 targets keep exactly
  Patella.vcxproj's and build.cmd's flags (/std:c++17 /W3 /Zc:strictStrings- ...; OracleX86.cmake is unchanged)
  and the presets of #5 and #7 apply them to their own targets. No preset applies them yet.
- **Language** (high). MSVC: `/std:c++latest` (cl 19.51 knows /std:c++23preview but not /std:c++23; the issue
  chose c++latest), `/permissive- /Zc:__cplusplus` for C++, `/utf-8 /Zc:preprocessor /Zc:inline` for C and C++.
  GCC and Clang: `-std=c++26`. The standard is an option, not CXX_STANDARD, which would add a second flag. The
  sources are ASCII apart from comments in Utils.cpp and Utils.h (UTF-8), so /utf-8 changes no literal.
- **String literals** (high). Without /Zc:strictStrings- (strict by /permissive-) one literal of src/ failed:
  Status.h's `static char *LinkPattern = "..."`. It is a `const char *` now: the pointer keeps its size and its
  place in .data of every object that includes Status.h; InitStatus casts it for regcomp, whose K&R signature
  takes `char *` (src/Extern unchanged). The oracle's Patella.exe is unchanged (below).
- **Warning baseline** (high). Measured with a local, uncommitted CMake project (build\issue-8\check: every
  src/*.cpp and regexp.c with Patella's definitions, x86 against static MFC, the three functions applied, linked
  with /guard:cf /CETCOMPAT), first with `--compile-no-warning-as-error` and an empty baseline: 1,234 /W4
  warnings in 24 of the 36 translation units (931 C4100 unreferenced parameter, 180 C4458/C4456/C4459 hidden
  names, 99 C4244 conversions, and in regexp.c 16 C4131 old-style declarators, 7 C4310 casts, 1 C4701; many come from GUI.h, DialogElements.h and ContentWindows.h, so a file is on the list when
  its translation unit warns, wherever the warning is). Those 24 are PATELLA_WARNING_BASELINE (/W3); the other
  12 build at /W4. With the list and /WX: 0 warnings, 0 errors. The list is x86's: the x64 build (#5) and GCC
  and Clang (#7) measure again; GCC and Clang use the same list with `-Wall` as the lower level until then.
  Warning fixes are left for M3: each changes src/ and needs the binary comparison.
- **External headers** (form). `/external:anglebrackets /external:W0`: at /W4 the Windows SDK and MFC headers
  would warn in every file. The plan does not name it; without it /W4 is not usable with MFC.
- **Warnings as errors** (form). CMake's COMPILE_WARNING_AS_ERROR, so `cmake --compile-no-warning-as-error`
  measures without editing a file. The level is a per-source COMPILE_OPTIONS, so no command line has two levels
  (cl warns D9025 when /W3 overrides /W4).
- **Hardening** (high). MSVC: /guard:cf (compiler and linker), /CETCOMPAT (linker; ASLR and DEP are link.exe's
  defaults). GCC/Clang: -fstack-protector-strong; `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3` only in optimised
  configurations (glibc warns without optimisation, an error with -Werror; some distributions predefine level 2);
  -D_GLIBCXX_ASSERTIONS in Debug; POSITION_INDEPENDENT_CODE with check_pie_supported (CMP0083 is NEW from 3.30,
  so CMake links with -pie); `-z relro -z now` on Linux only. The GCC options were checked with mingw-w64 GCC
  16.2 on vlong.cpp, TRSA.cpp and TXTEA.cpp (Release and Debug, 0 warnings; TRSA.cpp at the full level). Clang
  is not installed here (no clang-cl, no clang): #7 checks it. regexp.c does not compile with GCC 16, whose
  default C is C23 (K&R definitions): #7 needs a C standard for it (C17).
- **.clang-format** (form). Written from the code: tabs of four (UseTab ForContinuationAndIndentation, so
  alignment inside a line stays spaces), WebKit braces (function bodies on their own line), no indented case
  labels, `public:` at the class's column, pointers to the right, breaks before binary operators, ColumnLimit 0
  (the author's breaks), includes not sorted, comments not reflowed. WhitespaceSensitiveMacros lists every
  macro of src/ that stringizes an argument (TASSERT*, TERROR*, TEND*, the local TASSERTB*, TASSERT*_BUG and
  TERROR1B, and TMYEXCEPTION); StatementMacros TBEGIN. What clang-format 22 cannot keep: (1) trailing comments
  aligned by hand: `AlignTrailingComments: Leave` with tabs writes tabs in the middle of lines and moves the
  comments to tab stops, so they are aligned over consecutive lines (Kind Always, OverEmptyLines 0), most of the
  reformat's lines; (2) several statements or case labels on one line: split, except in hand-laid tables, which
  `// clang-format off/on` keeps (Communication.cpp's patch file table, two groups of Control.cpp's packet
  switch, Creatures.cpp's direction table, Utils.cpp's colour table, Talk.cpp's GetTalkColor, surfaceDX9.cpp's
  two vertex blocks, the pixel format and attribute lists of Surface.cpp and SurfaceGL.cpp), as are common.h's
  TEND_HANDLERS and the MFC shim's message map macros, which clang-format parses as code; (3) a break inside a
  nested call when the outer call's arguments are not broken: with ColumnLimit 0 clang-format joins such a
  statement into one line (up to 177 columns). Before the reformat the break of those 31 statements moved to the
  outer argument before the nested call (26) or before a binary operator (5), where clang-format keeps it.
  src/Extern keeps its style (.clang-format-ignore; a moved line after glh_extensions.h's #line would change
  __LINE__), and tools/ is not in the check yet.
- **Reformat** (high). One commit, formatting only: 65 files, 3,172 lines in, 2,980 out. Lines moved, which
  changes nothing: every error site names its line as the macro's first argument (common.h), no code of src/
  uses __LINE__ outside src/Extern, and the binary is identical (below). No TASSERT/TERROR/TEND argument
  changed: all 2,108 calls have the same text between their parentheses, whitespace included (a script
  comparing them in order per file against main); `git diff -U0 | grep` shows four src/ lines, which differ
  before the macro name (DialogElements.cpp's over-indented switch) or after its closing parenthesis (GUI.h's
  trailing comments).
- **clang-tidy** (form). `.clang-tidy`: bugprone-* without the checks that are noise for a reconstruction
  (easily-swappable-parameters, narrowing-conversions, implicit-widening-of-multiplication-result,
  reserved-identifier), cppcoreguidelines-no-malloc, -owning-memory, -pro-type-vararg, -use-enum-class,
  modernize-use-nodiscard, -use-nullptr, -use-override, -use-std-print (plan section 8); headers of src/ but
  not src/Extern. `tools/tidy.py` (no dependency beyond Python): finds clang-tidy (CLANG_TIDY, PATH, Visual
  Studio's), runs it per file with `-Wno-unused-command-line-argument -Wno-error` (MSVC's command lines carry
  options clang does not use, and /WX), splits the findings by whether they lie inside a macro call's
  parentheses, and after `--fix` compares every file of src/ with its state before and puts back the old text
  of each edit inside such a call (line diff, then character diff inside the changed lines). Shown with
  build\issue-8\check's compile_commands.json and `--extra-arg=-m32` (clang-tidy takes its own default target,
  x64, which fails the layout static_asserts of an x86 build): Utils, vlong, TRSA, Talk, Status and Map.cpp give
  266 findings outside and 47 inside the arguments; `--fix --checks=-*,modernize-use-nullptr` on Utils, Talk and
  Map.cpp applied the replacements of NULL outside the macros (101 lines in five files) and reverted the 43
  inside (no changed macro line in the diff), and on a mixed line (`if (p == NULL) TASSERT(12, p!=NULL);`) the
  condition changes while the argument stays. The baseline that "no new findings" (plan 4.1) needs comes with
  the x64 build's compile_commands.json (#5) or CI.
- **.editorconfig** (form). Tabs of four, LF, UTF-8, final newline, no trailing whitespace; spaces for YAML
  (.clang-format and .clang-tidy are YAML) and Markdown (the notes indent list continuations with spaces), whose
  trailing whitespace stays.
- **Result** (high). oracle-x86 Patella.exe (Release) of main (d38b5a8) and of the branch, built in the same
  directory: the same section sizes; .text, .data, .fptable, .rsrc and .reloc identical; .rdata and the headers
  differ only in the link timestamps and one byte of the CodeView record (the PDB GUID). compare_imports.py and
  compare_resources.py against Tibia.exe print the same for both (55 resources, 0 differences). lines.py check:
  2,053 sites matched, 0 mismatches, the same output as for main. The CTest counts are main's (notes/progress.md,
  "Last difftest").

## src/compat/cxx26.h and the unit tests (issue #9)

Settled by Claude Opus 5.5, November 3, 2026 (MSVC 14.51 / cl 19.51 x64, mingw-w64 GCC 16.2, CMake 4.1.2, Ninja
1.13.2, doctest 2.5.3). `src/compat/cxx26.h`, `tests/unit/` (CMakeLists.txt, main.cpp, cxx26_contracts_test.cpp,
cxx26_static_vector_test.cpp, RunContractViolation.cmake), the presets windows-x64 and windows-x64-gcc,
`cmake/Warnings.cmake` (two additions for GCC). No client code changed; nothing of src/ includes the header yet.

- **Selection** (high). Per feature by its test macro: `__cpp_contracts >= 202502` selects contract_assert,
  `__cpp_lib_inplace_vector >= 202603` (libstdc++ 16's value, whose try_ functions return `std::optional<T &>`)
  selects std::inplace_vector; `PATELLA_HAS_CONTRACTS` and `PATELLA_HAS_INPLACE_VECTOR` say which. Defining
  `PATELLA_CXX26_STANDIN` for a whole target takes the stand-ins also where the real features exist (never for one
  file: the two paths differ in types). MSVC 19.51 has neither feature, GCC 16.2 with -std=c++26 both.
- **CONTRACT_ASSERT(Predicate)** (high). The plan's "contract macros"; one macro, since `pre` and `post` belong to
  a declaration and have no macro form. Real: `contract_assert(__VA_ARGS__)` (variadic, so commas inside template
  arguments pass). Stand-in: the "enforce" semantic, GCC's default: the predicate converted with
  static_cast<bool> (a top-level comma is an error, as in contract_assert), evaluated once; when false or when it
  throws, GCC's message (`contract violation in function F at FILE:LINE: PREDICATE`, then the
  `[assertion_kind: assert, semantic: enforce, mode: ..., terminating: yes]` line, mode evaluation_exception when it
  threw, where GCC 16.2 says predicate_false) through std::print to stderr, then std::terminate, inside the catch
  handler when the predicate threw, as GCC. A do/while statement, usable in constexpr functions (a violation in
  constant evaluation does not compile on either path). Not reproducible: const locals inside the predicate, the
  other semantics, a replaceable handler. The name is the client's all-capitals macro style (TASSERT) without the T,
  which belongs to the error.txt macros.
- **libstdc++exp** (high). GCC 16.2 keeps the default violation handler (`handle_contract_violation`) in
  libstdc++exp.a, as it keeps std::stacktrace; without it any contract_assert fails to link.
  `patella_target_language` links it for GCC 16 and newer.
- **TStaticVector<T, N>** (high). In namespace patella (D15; the client is not in it until M7). Real: an alias of
  std::inplace_vector. Stand-in: libstdc++ 16's interface member by member (no try_append_range, which 202603
  dropped), with its results and exceptions: std::bad_alloc beyond the capacity, checked before anything changes
  when the number of new elements is known (sized or forward), after filling up when it is not (input iterators and
  ranges); std::out_of_range from at(); trivially copyable, movable and destructible for such T (constrained
  defaulted members); constexpr for trivially default-constructible, trivially copyable T (the slots get values
  during constant evaluation, which makes the union's array active, as libstdc++ does). Storage: an anonymous union
  with `T Slots[N]` and a size_t. Differences, documented in the header: iterators are pointers; try_push_back and
  try_emplace_back return `T *` (C++23 has no `std::optional<T &>`; code uses only `if (auto E = ...)`, `*E`,
  `E->`); N == 0 is not an empty type; the preconditions (operator[], front, back, pop_back, the unchecked_
  functions, positions) are CONTRACT_ASSERTs, where std::inplace_vector's are undefined behaviour; sizeof differs.
  The tests found two places where the difference shows: iterators of TStaticVector<int, 8> and <int, 6> are one
  type on the stand-in, distinct ones in libstdc++; and `&V.push_back(1) == &V[0]` evaluated V[0] first on GCC, which
  the stand-in reported as a violation.
- **Unit tests** (form). doctest 2.5.3 (plan 4.6 and 5): `find_package(doctest 2.5.3 CONFIG)` first, otherwise
  FetchContent of the release archive with its SHA256 (FIND_PACKAGE_ARGS); its own tests, main library and install
  off. tests/unit builds the same three sources twice: `tests_unit` (the compiler's selection) and
  `tests_unit_cxx26_standin` (PATELLA_CXX26_STANDIN), both with patella_target_language, _warnings and _hardening.
  PATELLA_EXPECT_CXX26 (MSVC 0, GCC 16 1) makes the tests fail when the wrong path was taken. The violations end
  the process, so each program has `--contract-violation <scenario>` and RunContractViolation.cmake (a CTest test per
  scenario) checks that it ended abnormally before going on and that stderr has the message; the precondition
  scenario applies to the stand-in only (exit 77, CTest's skip, on the real path). main sets
  `_set_abort_behavior(0, ...)` so that no abort message box and no Windows Error Reporting appear.
- **Presets** (form). windows-x64 (MSVC x64 from a `vcvarsall.bat x64` prompt) and windows-x64-gcc (mingw-w64 GCC
  on PATH; plan D6, "C++26 parity build on Windows"), names from the plan's table; each with Release and Debug
  build presets, test presets and a workflow preset, and compile_commands.json. They build only tests/unit; #5
  adds the client to windows-x64, #7 the Linux presets. The top-level CMakeLists.txt adds tests/unit when
  PATELLA_ORACLE is off (it stopped with an error before). The oracle's targets and flags are unchanged.
- **mingw-w64 runtime** (high). The first GCC test run loaded an older libstdc++-6.dll that came first on PATH, and
  the loader showed an "Entry Point Not Found" dialog. `-static` fixes that in Release, but GCC 16.2's Debug build
  (-O0, C++26) then fails to link: the object defines std::logic_error's copy constructor and so does the static
  libstdc++.a (cow-stdexcept.o, multiple definition; reproduced with a three-line program). So patella_target_language
  copies the compiler's libstdc++-6.dll, libgcc_s_seh-1.dll and libwinpthread-1.dll next to each mingw executable
  after the link, where Windows looks first; the tests then run with only System32 on PATH.
- **Warnings.cmake** (form). A source shared by two targets got its warning level twice (a source property belongs
  to the directory); it is set once now.
- **Result** (high). windows-x64 (stand-ins) and windows-x64-gcc (real features, and the stand-ins forced): each 8
  CTest tests, 0 failures, 0 warnings; doctest 18 test cases, 158 assertions (tests_unit) and 160
  (tests_unit_cxx26_standin, two more path checks), Release and Debug. The oracle-x86 preset configures and builds
  with 0 warnings; its tests were not run (nothing it builds changed).

## Windows x64 build (issue #5)

Settled by Claude Opus 5.5, November 3, 2026 (MSVC 14.51 / cl 19.51 x64 and x86, static MFC, CMake 4.1.2, Ninja
1.13.2). `cmake/WindowsX64.cmake`, `cmake/Resources.cmake`, the windows-x64 preset's description, a fix in
`cmake/Warnings.cmake`, and the x64 fixes in src/. Not done here: the live checks of the issue (start, log in,
compare_traffic.py, compare_files.py), which need client_state.py's PDB offsets (#6) and are run afterwards.

- **The client in windows-x64** (form). `PATELLA_CLIENT` (default ON) adds Patella.exe to the preset when the
  compiler is MSVC (windows-x64-gcc still builds only the unit tests; the client under mingw is not part of M1).
  It has the oracle's sources (src\*.cpp sorted without case, then regexp.c), definitions (`_WIN32_WINNT=0x0601`,
  NDEBUG/_DEBUG; WIN32 and _WINDOWS from CMake's defaults), import libraries in the same order and linker options:
  static x64 MFC (nafxcw.lib through the MFC headers) and the static CRT, oleacc.dll delay-loaded, wsock32.lib
  excluded, **no manifest**: a manifest would select common controls 6 and change the DPI handling, which the user
  sees. Otherwise the product settings: patella_target_language, _warnings and _hardening, CMake's default
  optimisation flags, a PDB in both configurations (/Zi, /DEBUG; /OPT:REF /OPT:ICF /INCREMENTAL:NO in Release,
  which /DEBUG would turn off). The oracle's /Oy-, /Gy, /sdl-, /Zc:strictStrings- ... are not taken over.
- **Resources** (form). The configure-time extraction of the pictures from Tibia.exe is a function now
  (`patella_extract_resources`, cmake/Resources.cmake) that the oracle and windows-x64 call (same command for the
  oracle). Without Tibia.exe the windows-x64 configure stops with a message naming PATELLA_TIBIA_EXE and
  `-DPATELLA_CLIENT=OFF` (for machines and hosted CI without CipSoft files).
- **Warnings.cmake: include_guard(DIRECTORY)** (high). With GLOBAL, the second directory that included the file
  (the top level for the client, after tests/unit) skipped it and had no PATELLA_WARNING_BASELINE or
  PATELLA_ROOT_DIR, which are directory variables: every file of the client compiled at /W4. The functions are
  defined again per directory, which is harmless.
- **Layout static_asserts** (high). The asserts of the original's sizes and offsets that pointers change
  (Communication.cpp: TTalkBuffer's first and cursor, TTalkLine's Addressee; Talk.cpp: TTalkLine, TTalkBuffer and
  the buffer, channel and filter classes; SurfaceDX.cpp: TSurfaceDX) are inside `#if PATELLA_ORIGINAL_LAYOUT`,
  which common.h defines as 1 on x86 (`_M_IX86`) only, so they stay active in the oracle build (rule 4 of the
  plan); the ones that hold on x64 too (TCreature, TBuddyEntry, TTalkbox) stay unconditional. No structure member
  changed its type.
- **x64 fixes** (high). The first x64 build: 15 errors and, at the baseline levels, 45 warnings, none of them
  C4311/C4312/C4302 (the message queue already carries INT_PTR, from upstream). Fixed: `CMainFrame::OnTimer` takes
  UINT_PTR (x64 MFC's ON_WM_TIMER does not convert; on x86 UINT_PTR is unsigned int, the same type and mangled
  name); 36 C4267 (`(int)strlen(...)`, `(int)strcspn(...)` in DialogElements.cpp/.h, Dialogs.cpp, graphics.cpp and
  regexp.c), 9 C4244: regexp.c's two pointer differences `(int)(scan - val)`, sysinc.cpp's clipboard size
  `(unsigned int)Size`, and Control.cpp's six message parameters that carry an ID or line number (`(int)Param1`,
  `(uint32)Param1`; the producers put an int or uint32 there). Each is the conversion the x86 code did implicitly,
  between types of the same width there, so the x86 code is the same (below). No TASSERT/TERROR/TEND argument
  changed (`build\issue-8\tools\macrocheck.py main issue-5`: 2,108 calls, 0 files differ).
- **C4701 in Talk.cpp** (medium). Debug x64 (unoptimised) warns that UpdateTalkboxes' `i` may be used
  uninitialised: TEND1(517, i) reports it when GetMilliseconds throws before the loop, as the original does.
  Initialising it would add a store to the oracle's code, so the warning is turned off around that function
  (`#pragma warning(push/disable : 4701/pop)` under `_MSC_VER`; a code generation warning ignores a pragma inside
  the function). The x86 Release measurement of #8 did not see it.
- **x64 warning baseline** (high). Measured like #8 (every source at /W4, `--compile-no-warning-as-error`,
  build\issue-5\measure, Release): 1,234 warnings in the same 24 translation units, with the same codes (931
  C4100, 159 C4458, 99 C4244, 17 C4459, 16 C4131, 7 C4310, 4 C4456, 1 C4701), so PATELLA_WARNING_BASELINE is
  unchanged and no x64-specific warning is baselined. Before the fixes above, x64 added 36 C4267 and 9 C4244
  (INT_PTR, SIZE_T, __int64), now 0.
- **Result** (high). windows-x64 (`cmake --workflow --preset windows-x64 --fresh`, then the Debug build and
  tests): Patella.exe and the unit tests with 0 warnings and 0 errors under /WX in Release and Debug; 8 CTest
  tests, 0 failures, in both. The Release exe is x64 with High Entropy VA, ASLR, DEP, Control Flow Guard and CET
  compatibility. windows-x64-gcc unchanged (8 tests, 0 failures, Release and Debug). oracle-x86 (`--fresh`): 0
  warnings, CTest 4/4 with main's counts (notes/progress.md, "Last difftest"). oracle-x86 Release Patella.exe of
  main (822f7df) and of the branch: the same section sizes; .text, .data, .fptable, .rsrc and .reloc identical;
  the headers and .rdata differ only in the link timestamps and one byte of the CodeView record (the PDB GUID);
  compare_imports.py and compare_resources.py against Tibia.exe print the same as for main (55 resources, 0
  differences).
- **Not checked yet** (form). Nothing has run the x64 client. Two things only a run shows: DirectDraw's
  Direct3D 7 device (SurfaceDX.cpp, IID_IDirect3DHALDevice) in a 64-bit process, and any place that depends on
  layout without the compiler seeing it; the live checks of the issue (compare_traffic.py, compare_files.py,
  with client_state.py's PDB offsets from #6) cover the second.

## client_state.py: field offsets from the PDB (issue #6)

Settled by Claude Opus 5.5, November 3, 2026 (Python 3.13 x64, System32 dbghelp.dll, MSVC 14.51 x86 and x64).
`tools/client_state.py`, `tools/session_client.py` (its offsets), `tools/compare_files.py` (player_position),
`tools/compare_traffic.py` (`--patella`). No change to src/ or the CMake files: the oracle-x86 PDB has every type
and member the tools read.

- **One table** (high). Every offset the tools read is an entry of `LAYOUT_FIELDS` (`Type.Member[.Member]`, the
  original's offset, the member's kind: int, bool, ptr, char[N], a type, T[N]); strides are `LAYOUT_SIZES`, the
  GetFirstChild slot `LAYOUT_VSLOTS`, the globals `ORIGINAL_GLOBALS` (address and kind). Tibia.exe uses the
  constants (`Layout.original()`); Patella.exe resolves every entry in its PDB (`Layout.from_pdb`: SymGetTypeInfo
  for members, base classes included, lengths, array counts and vtable offsets; SymEnumSymbols for the globals, as
  before) and refuses to run when an entry is missing or its kind differs (a field read as a 4-byte int that is
  not one, a pointer that is not the image's pointer size). Array lengths (skills, creatures, body slots,
  containers, objects per point/container/offer, name lengths) come from the kinds, so they follow the PDB too.
  Fields' entry count stays the constant MAP_POINTS (a #define, not in the PDB).
- **Equal for x86** (high). `python -I tools/client_state.py --layout build\oracle-x86\Release\Patella.exe`
  lists every entry with the original's value and the PDB's: 33 globals (kind), 73 fields, 7 sizes, 1 vtable slot
  and the GetFirstChild code, 0 mismatches. A one-off check (not in the repository) read the same fake process
  memory with main's client_state.py and this one: state(), its parts, gui(), characters(), the skin reads with
  main's literal offsets against the member paths, player_position, and main's pdb_symbols against the new
  globals: no difference. The reads happen in the same order with the same sizes, so even a ReadError's text
  is the same.
- **64-bit** (medium until an x64 Patella has run). Pointers are read with the image's pointer size (`ptr`,
  `var_ptr`); the process's bitness (IsWow64Process) must match the PDB's image (PE machine), and a 64-bit
  process needs a 64-bit Python; EnumProcessModulesEx lists LIST_MODULES_64BIT or _32BIT modules accordingly.
  RTTI: the complete object locator's pTypeDescriptor (+12) is an image-relative offset on x64, an address on
  x86; the TypeDescriptor's name follows two pointers. A window's GetFirstChild is `48 8b 41 XX c3` on x64 with
  XX = TWindow.FirstChild (from the PDB). The dbghelp search path starts with the exe's folder, so the PDB next to
  the exe is found when the build folder moved (a PDB that does not match the image is not loaded). Verified
  against a 64-bit MSVC test process with TComponent/TWindow-like classes and a PDB (offsets, slot 0x40,
  bitness, module base, RTTI names, the pattern, gui() end to end); not yet against an x64 Patella.exe.
- **Raw offsets** (form). `skin_rects`/`skin_int` take a member path of TSkinProperties; an int is still
  accepted when the layout equals the original (`Layout.frozen`: Tibia.exe and the oracle build).
  compare_rendering.py's four raw skin offsets and compare_files.py's and compare_rendering.py's literal
  component offsets (TCheckBox +0x30, the option dialogs' controls, TAutomapWindow's zoom buttons, ...) were
  right on x86 only; Patella-Modern issue #15 (below) named them in the table.
- **dbghelp** (form). System32's, loaded by full path: it reads MSVC 14.51's PDBs, x86 and x64; the Windows SDK
  Debuggers copy is not needed.

## patch_client.py and the GUI comparisons on x64 (Patella-Modern issue #15)

Settled by Claude Opus 5.5, November 3, 2026 (Python 3.13 x64, System32 dbghelp.dll; the builds of 21a40b1).
`tools/patch_client.py`, `tools/client_state.py` (LAYOUT_FIELDS), `tools/compare_files.py` (and its
`--patella`), `tools/compare_rendering.py`. No change to src/ or the CMake files.

- **patch_client.py on PE32+** (high). It refused every image that was not PE32. It now reads the optional
  header's magic (0x10b: ImageBase at +28, 4 bytes; 0x20b: at +24, 8 bytes). The names and the modulus were
  found by searching the file already and need nothing else. The port's anchor, the first code reference to
  the server.tibia.com literal, is on x86 the literal's absolute address (unchanged code path); on x64 a
  RIP-relative disp32: a ModRM byte with mod 00, r/m 101 before it (`b & 0xc7 == 0x05`) and the target, the
  address after the disp32 plus the disp32, inside the literal. That target rule holds for instructions
  without an immediate after the disp32, which InitServerList's strcpy loads are (`movups xmm0,
  [rip+disp32]`, `movzx eax, byte ptr [rip+disp32]`). In the x64 build the anchor is at file 0x84aa3, the four
  stores `c7 05 <disp32> 03 1c 00 00` follow at 0x84b20/31/42/53 (one per server, unrolled like the x86
  build's); the same nine ranges as on x86 are patched (348 bytes with `--port`, 344 without). On Tibia.exe and
  the oracle-x86 build, main's and this version's outputs are byte for byte the same (three argument sets).
- **The GUI comparisons' offsets** (high). Every literal offset compare_files.py and compare_rendering.py
  added to a component pointer is a LAYOUT_FIELDS entry now (28 entries: TCheckBox.Checked, the general,
  console, network, graphics and advanced options' controls, THorizontalScrollBar.LeftButton,
  THotkeyDialog.AutoSendCheckBox and Selected, TEditMarkDialog.TypeButtons (kind `ptr[14]`; an array of
  pointers must be count times the pointer size), TAutomapWindow's ZoomInButton/ZoomOutButton,
  TSkinProperties.Data60C, Data61C, Style1394.Data70.x/.y); pointers are read with `ptr()` (4 or 8 bytes)
  instead of `u32()`. On the oracle-x86 PDB all 44 former lookups (the 14 TypeButtons elements counted one by
  one) give the old constant and read width, and so does Layout.original() for Tibia.exe; `--layout` on
  oracle-x86: 101 fields, 0 mismatches, frozen; on windows-x64: 0 problems (e.g. TCheckBox.Checked 0x50,
  THotkeyDialog.AutoSendCheckBox 0x2480, TAutomapWindow.ZoomInButton 0x80; the SkinProperties members keep
  their offsets).
- **compare_files.py --patella** (form). As compare_traffic.py's: the Patella.exe to test, x86 or x64, with
  its PDB next to it. compare_rendering.py has none: its capture DLL (tools/capture/patcap.c) and the import it
  adds (`add_import`) are 32-bit, so it cannot run an x64 client yet; its offsets are ready for it.
- **Not checked yet** (form). Nothing has run the x64 client: the live compare_traffic.py and compare_files.py
  runs with `--patella build\windows-x64\Release\Patella.exe` show whether the patched x64 copy logs in through
  the proxy and whether the dialogs' controls are found.

## Removing the Visual Studio project and the scripts (issue #10)

Settled by Claude Opus 5.5, November 3, 2026 (CMake 4.1.2, Ninja 1.13.2, MSVC 14.51, GCC 16.2, Python 3.13).
Deleted: `Patella.sln`, `Patella.vcxproj`, `tools\build-patella.cmd`, `tools\compile-check.cmd`,
`tools\build-resources.cmd`, `tools\build-capture.cmd`, `tools\difftest\build.cmd`, `tools\run-testserver.ps1`,
`tools\stop-testserver.ps1`. `tools\decompilation.ps1` stays (upstream's analysis workspace). No change to src/.

- **The entry point** (form). The MSVC presets mark architecture and toolset `external`, so CMake does not set up
  the compiler (the comment from #4). `tools/build.py` (Python, no package) takes the place of the scripts'
  `call vcvarsall.bat`: `python -I tools/build.py <workflow preset> [cmake --workflow options]` reads the preset
  from CMakePresets.json, and for a preset whose compiler is cl with an external architecture runs vcvarsall.bat
  with that architecture (x86 for oracle-x86, x64 for windows-x64; host_target when the toolset's host differs)
  and runs `cmake --workflow` in the environment it leaves; other presets (mingw-w64 GCC, Linux) run in the shell's
  own environment. vcvarsall.bat is found through vswhere (the newest Visual Studio with the C++ tools), else at
  the path the scripts used; a shell that is already a developer prompt for that target and host is used as it is.
  `--env <preset> <command>` runs any other command (cmake --build, ctest) the same way. Considered: a .cmd
  wrapper (the kind of script the issue removes), CMake toolchain files that call vcvarsall (CMake cannot import an
  environment at configure time; the build and test steps would not have it). New workflow presets
  windows-x64-debug and windows-x64-gcc-debug make Debug one command too.
- **What each script did, and where it is now** (high).
  - build-patella.cmd: the oracle-x86 Patella target (same options as the vcxproj since #4) and, new, a
    POST_BUILD step that runs compare_resources.py against PATELLA_TIBIA_EXE after every link; a difference fails
    the build, as it failed the script. The image is unchanged (a step after the link): the Release Patella.exe of
    this branch and main's differ only in the link timestamps and the PDB GUID (.text, .data, .rsrc, .reloc
    identical), compare_imports.py's output is unchanged.
    Not carried over: MSBuild's -warnAsError, which made a warning fail the script. The oracle keeps the
    vcxproj's /WX- (the preset keeps its flags until M6), so a warning in the oracle build shows in the log but
    does not stop it; the gates count warnings in the log (0). Issue #17 makes tools/build.py fail on them (below).
  - compile-check.cmd: with real MFC its compile is a subset of the Patella target's (the same sources, the same
    standard, warning level and conformance switches); the shim mode (TIBIA_MFC_SHIM, the check for machines
    without the MFC component, used as a gate up to #37) is the new OBJECT target mfc_shim_check with the script's
    options, built with the preset (0 warnings). One file alone: its object file as the build target.
  - build-resources.cmd: extract_resources.py at configure time (cmake/Resources.cmake), rc.exe in the Patella
    target, compare_resources.py after the link. Only the separate build\res\Patella.res is not produced any more.
  - build-capture.cmd: the patcap target (since #4, same options). compare_rendering.py now uses
    build\oracle-x86\Release\patcap.dll and builds the target (configuring the preset first if needed, through
    tools/build.py's environment) when the DLL is missing or older than patcap.c, as it ran the script before; the
    branch's DLL and main's differ only in the link timestamp.
  - difftest\build.cmd: the four CTest tests (since #4). Not carried over: the script deleted each test program
    before linking it and stopped a copy of this checkout that still ran; with Ninja the link of a locked program
    fails and the copy must be stopped by hand (difftest's job object makes such copies rare).
- **The test server in Python** (high). `tools/testserver.py start|stop` does what the .ps1 scripts did: start
  runs tibia-server.exe from the server folder (default C:\Tibia\gameserver, `--server-dir`) in a new console
  that is hidden (`--window`: minimised), stdin from NUL, stdout/stderr to build\testserver\server.log/.err,
  the PID to server.pid, refuses when a server from that folder runs or 7171/7172 has a listener, clears the
  inherited "ignore Ctrl+C" flag first, and waits (`--timeout`, 300 s) until that process listens on both ports;
  stop sends Ctrl+C from a copy of the script started with DETACHED_PROCESS (FreeConsole, AttachConsole,
  SetConsoleCtrlHandler(NULL, TRUE), GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0)), waits (`--timeout`, 60 s),
  `--force` kills. The same exit codes and the same "listening on <address>:<port>, ..." line (compare_traffic.py
  looks for 0.0.0.0:7171 in it). The ports come from GetExtendedTcpTable (owner PIDs, IPv4 and IPv6), the
  processes from K32EnumProcesses and QueryFullProcessImageNameW, all through ctypes; on Linux from /proc and
  SIGINT. One deliberate difference: stop-testserver.ps1 fell back to any tibia-server.exe from the folder when
  server.pid was missing or stale; the test server may be the user's own instance, so stop now touches only the
  process in server.pid, and only if its image path and its creation time (the second line of server.pid) are
  still the ones start recorded. Any other server makes stop report its PID and exit 1. compare_traffic.py's
  server_running() (refuse to run while any server from the folder runs) uses the same process list instead of a
  PowerShell call.
- **The comparisons' default Patella.exe** (form). build\Release\Patella.exe was the MSBuild output; nothing writes
  it any more. compare_traffic.py, compare_files.py and the scripts that take their set-up (compare_rendering.py,
  compare_network.py) default to build\oracle-x86\Release\Patella.exe, the same image.
- **Gates** (high). From Git Bash, no developer prompt: `python -I tools/build.py oracle-x86 --fresh`: 0 warnings,
  4/4 tests, 16,848,752 / 67 / 1,134,513 / 3,201 checks, 0 failures, resources 0 differences.
  windows-x64 Release and Debug (Patella.exe and the unit tests, every source compiled) and windows-x64-gcc: 0
  warnings, 8/8 tests each (on GCC the stand-in-only precondition scenario of tests_unit is skipped, as since #9).
  The server started and stopped twice with testserver.py (both ports listening, a clean "Server shut down
  successfully.", ports free and no process afterwards). Logs: build\logs\issue-10.

## Warnings fail the oracle build (issue #17)

Settled by Claude Opus 5.5, November 3, 2026 (CMake 4.1.2, Ninja 1.13.2, MSVC 14.51, Python 3.13). `tools/build.py`;
comments in `src/compat/mfc_shim.h` and `src/res/Patella.rc`.

- **Where the check is** (form). The oracle keeps the former Patella.vcxproj's /WX- until M6, so the failure cannot
  come from a compiler flag. `tools/build.py`, the Windows entry point since #10, runs cmake with its output
  piped, writes every byte through unchanged and scans each line, up to cmake --workflow's "Executing workflow
  step N of M: test preset" line, for `warning <letters><4-5 digits>` (cl's C and D, link's LNK, rc's RC, ml's A,
  cvtres's CVT, lib's LNK) and GCC's and Clang's `<file>:<line>[:<col>]: warning: `. After the command ends it lists
  the warnings again on stderr and exits 1 when the command itself succeeded (otherwise the command's exit code).
  Every workflow preset and `--env <preset> cmake ...` are checked; the product presets fail earlier already
  (/WX, -Werror), so for them it changes nothing. The tests' output is not scanned: test programs print what
  they like. Considered: a CTest test that reads the build log (Ninja keeps no log of the compilers' output; the
  test would need the build to write one), a compiler launcher (CMAKE_<LANG>_COMPILER_LAUNCHER and
  _LINKER_LAUNCHER: a Python process per compile and link, and the launcher would be part of every command line
  of the oracle), MSBuild-style `/WX` on the link only (changes the oracle's linker flags). Limits: Ninja
  rebuilds only what changed, so an up-to-date tree reports nothing (`--fresh` checks every file, as the gates
  do), and plain `cmake --workflow` from a developer prompt does not check (as before). With its output piped,
  Ninja prints each `[n/N]` line instead of updating one line in a console.
- **Comments** (form). mfc_shim.h named tools\compile-check.cmd (and its rule "afxwin.h missing or TIBIA_MFC_SHIM
  set", which no build has any more) and Patella.vcxproj; it now names the mfc_shim_check target and the Patella
  target, in the same six lines. Patella.rc named tools/build-resources.cmd; it now says CMake extracts the
  pictures into build/<preset>/res and compiles with it, in the same one line. No other file of src/ names a
  removed script (the cmake/ comments call them "former").
- **Gates** (high). `python -I tools/build.py oracle-x86` with an unused local variable appended to MainFrm.cpp
  (C4101 three times: Patella, mfc_shim_check and difftest, whose mainfrmtest.cpp includes MainFrm.cpp; reverted):
  exit 1 with the three warnings listed, after the tests ran and passed; on the reverted tree exit 0. `python -I tools/build.py oracle-x86 --fresh`: 0 warnings, 4/4
  tests, 16,848,752 / 67 / 1,134,513 / 3,201 checks, 0 failures, resources 0 differences. windows-x64 and
  windows-x64-gcc: 0 warnings, the tests pass. Patella.exe (Release) against main's (build\issue-10\main-oracle):
  .text, .data, .fptable, .rsrc and .reloc identical, .rdata and the headers differ only in the link timestamps
  and one byte of the PDB GUID. Logs: build\logs\issue-17.

## /FC and the glh_extensions.h #line name (issue #14)

Settled by Claude Opus 5.5, November 3, 2026 (CMake 4.1.2, Ninja 1.13.2, MSVC 14.51). `cmake/OracleX86.cmake` only;
src/ unchanged. The owner approved making the message match the original's (comment on #14).

- **The cause** (high). glh_extensions.h gives its two CHECK_MEMORY printfs the original's file and line with
  `#line 173 "Extern\\glh/glh_extensions.h"`. /FC (MSBuild's "use full paths", in the vcxproj's options) makes a
  relative name absolute from the compiler's working directory, which for Ninja is build\oracle-x86: the oracle
  printed `C:\Patella-Modern\build\oracle-x86\Extern\glh\glh_extensions.h`, the original
  `Extern\glh/glh_extensions.h`. cl has no option that cancels /FC.
- **The fix** (high). SurfaceGL.cpp, the one source that includes glh_extensions.h, compiles without /FC, in the
  Patella target only. A target's options cannot leave out one source, so the vcxproj's last two options,
  `/FC /Zc:strictStrings-`, moved from the target to the sources' COMPILE_OPTIONS (which CMake puts after the
  target's): /FC on every source but SurfaceGL.cpp, then /Zc:strictStrings- on every source, each inside
  `$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,Patella>:...>`, because source properties belong to the directory and
  mfc_shim_check compiles the same sources with its own options. The generated Ninja files (Release and Debug)
  differ from before in one line each: SurfaceGL.cpp's flags in Patella without /FC; every other command line
  is the same text. The configure stops if SurfaceGL.cpp is not among the sources. Considered: dropping /FC for
  every file (one token, but 36 command lines change, and /FC also spells every `__FILE__` there, TMyException's
  file names in .rdata, which only happen to be absolute already because Ninja passes absolute paths), and
  changing the source (allowed only if no build-side fix works). Without /FC, SurfaceGL.cpp's own `__FILE__`
  stays `C:\Patella-Modern\src\SurfaceGL.cpp`: a scratch compile with and without /FC gives the same string
  symbol for it, and the #line name `Extern\glh/glh_extensions.h`.
- **Result** (high). oracle-x86 Release Patella.exe against issue-17's (both built in build\oracle-x86): the string
  at VA 0x6d5768 is now `Extern\glh/glh_extensions.h` (27 bytes, as Tibia.exe's at 0x5731d8) instead of the
  62-byte path; its address and the two pushes of it (operands at 0x4f9653 and 0x4f9671) are unchanged. Because it is 35 bytes
  shorter, the .rdata contents after it move down: by 36 bytes up to 0x6d59a0, by 32 up to the TLS directory,
  by 64 from there (alignment of later items absorbs or doubles the shift); .rdata's virtual size 0xaec50 ->
  0xaec10, its raw size and every section's address and raw size unchanged. Every other difference is an
  address of a moved item: 11,361 pointers in .text, 825 in .data, 10,759 in .rdata (relocation sites); 1,328
  RVAs (628 IAT entries and the 628 of the import lookup tables, 36 in the import directory, 3 in the delay
  import directory, 6 in the debug directory, 27 more, among them the load config's volatile metadata header
  and the delay import tables); the header's import, debug, TLS, load config and delay
  import directory RVAs, the .rdata and .reloc virtual sizes and the relocation directory's size (the same
  130,879 relocation sites, moved with their items; 16 bytes more of block headers); the POGO debug record's
  size of the .rdata group (0x4f500 -> 0x4f4c0); and the link timestamps and PDB GUID. An uncommitted script
  (build\issue-14\scratch\shiftcompare.py) accounts for every differing byte this way: 0 unexplained.
  compare_imports.py prints the same as for issue-17's exe; compare_resources.py 0 differences. The windows-x64
  Patella.exe never had /FC (cmake/WindowsX64.cmake) and already prints `Extern\glh/glh_extensions.h`.
- **Gates** (high). `python -I tools/build.py oracle-x86`: 0 warnings, 4/4 tests, 16,848,752 / 67 / 1,134,513 /
  3,201 checks, 0 failures. `python -I tools/build.py windows-x64 --fresh`: 0 warnings, 8/8 tests. The Debug
  oracle exe was not compared (its command line changes the same way). Logs: build\logs\issue-14.

## Clean run folders for the GUI comparisons (issue #12)

Settled by Claude Opus 5.5, November 3, 2026 (Python 3.13). `tools/compare_traffic.py` (setup's `extra`,
`clean_run_dir`), `tools/compare_files.py`, `tools/compare_network.py`, `tools/compare_rendering.py`,
`tools/session_client.py`. No change to src/ or the build.

- **One cleaning function** (high). `compare_traffic.clean_run_dir(name, keep=())` empties a run folder before
  every run of every GUI comparison: everything but the data files, the patched client and `keep` goes, files and
  subfolders alike; a data file whose sha256 differs from the root's (the patch scenario rewrites them) is copied
  again; what is left is checked, and the removed names are logged. It replaces compare_files.py's own reset (which
  deleted files only, kept a Tibia.exe in every folder and left folders alone); compare_traffic.py cleaned nothing
  (only rewrote Tibia.cfg), so automap files of the live run went into the replays; compare_rendering.py cleaned
  nothing either. Where it is called: compare_traffic.py's play() (live and replay runs) and the helper's
  folder in live_run(); compare_files.py's prepare() (every scenario, the live recording too) and so
  compare_network.py's run(); compare_rendering.py's run() with `keep=("patcap.dll",)`. Considered: a fresh copy
  of the whole folder per run (the 15.9 MB Tibia.spr every time; the patched client would have to be made again).
- **orig-repeat has its own folder** (high). The second Tibia.exe run (compare_files.py `orig-2`, now
  `orig-repeat`; compare_rendering.py `orig-repeat`; compare_network.py's `<scenario>-repeat`) ran in the orig
  folder. `setup(extra=["orig-repeat"])` makes `<folder>\orig-repeat` with its own patched Tibia.exe (the same
  bytes as orig's), and the tools name a run by its folder (`client_of("orig-repeat")` is orig). Results stay
  where they were (compare_files.py `<out>\repeat\<scenario>\orig`, compare_rendering.py `<engine>\orig-repeat`).
- **Waits that explain the first-attempt failures** (medium until the live runs). Three first attempts on
  November 3 failed in the GUI driving and passed on a rerun (build\logs\live-x64):
  - traffic, Tibia.exe live: "no window of class TMapWindow" at "look north", right after "say". Driver.node read
    the window tree once and gave up. The tree is read from the running client without a lock (gui() follows the
    Next pointers while the client may be changing the list), so a single read can miss a window that is there.
    Driver.node now reads again for up to 5 s (NODE_TIMEOUT) before the window counts as missing; a window that is
    really absent still fails, 5 s later.
  - replay into x86 Patella: "dropped its connection after 11 s". The log shows the proxy accepted the game
    connection a second after the Return and the replay went on; session_client.login then found no established
    connection in the client's TCP table for 10 s, raised, and the client was killed: the reset (WinError 10054)
    was the kill, not the client. compare_files.py already worked around the same miss (issue #21: "missed an
    established game connection twice"). login() now takes the proxy and counts its game login packet as well
    (compare_traffic.py passes it through run_session; compare_files.py's game_login uses it); a failure lists
    the client's TCP connections. Why the table misses the connection is open: GetExtendedTcpTable returned the
    connection in 300 of 300 reads of a test connection of this process (build\logs\issue-12).
  - files options, Tibia.exe: "no TAdvancedGraphicsDialog within 10 s" after the click on Advanced. wait_node
    returned a dialog as soon as it was in the tree and the next click went to the place of the button read right
    after it. wait_node now returns once the dialog's place has stopped changing (the same in two reads 0.2 s
    apart), and the clicks that open a dialog (Options, its five buttons, Advanced) are repeated when nothing
    opened within 4 s (`click_open`, at most three clicks, the button's place read again before each). A click
    that opened the dialog later than 4 s would be repeated into the dialog; nothing in the logs takes that long
    (the slowest, the graphics options with the engine switch, take 9 s for all their clicks).
  Not settled without a live run: whether these were the causes (the leftovers in the run folders can explain
  none of the three: the traffic folders kept automap files, which change no window, and compare_files.py cleaned
  its files already).
- **Gates** (high). py_compile and `python -I -W error -c "import ..."` of the five tools, `client_state.py
  --self-test`; `build\scratch\check_runfolders.py` (in build\logs\issue-12): setup with orig-repeat on a scratch
  folder set (patch_client.py on Tibia.exe and the oracle-x86 Patella.exe), leftovers planted in every folder
  (automap file, error.txt, Default.txt, a screenshot, an old Tibia.cfg, Tibia.bak, a subfolder, a Tibia.spr with
  another signature) and each tool's cleaning call: only the data files (equal to the root's), the client(s),
  Tibia.cfg and the seed remain; the folders of orig, patella and orig-repeat differ. `check_waits.py`: node,
  login, wait_node and click_open against fake clients. No client was started.

## A shorter walk for compare_files.py (issue #16)

Settled by Claude Opus 5.5, November 3, 2026 (Python 3.13; the test server's data in C:\Tibia\gameserver, only
read). `tools/compare_files.py`. No change to src/ or the build.

- **What the walk has to cover** (high). Its written files: the automap blocks 125/125 on floors 6, 7, 8 and
  125/126 on floor 7 (12512506.map, 12512507.map with two marks, 12512508.map, 12512607.map; the four files of
  every November 2 and 3 run), Default.txt and Tibia.cfg; the marks set, edited and deleted; the zoom. From
  Map.cpp: UpdateAutomap records the player's floor only (ReadField and the field updates with
  rz == GetMapLevel()), the map's 18x14 fields at x-8..x+9, y-6..y+7; only UpdateAutomap and AddAutomapMark mark a
  block changed (GetAutomapColor and the path finder load blocks without it). So: floors 6 and 8 with y <= 32248,
  floor 7 with y >= 32249, every view inside x 32000-32255, marks inside block 125/125/7.
  | | before (issue #21) | now |
  | :--- | :--- | :--- |
  | start | the temple, (32097,32219,7) | the ladder of a house in the town's south, (32109,32244,7), written into the player file |
  | actions | 116 steps | 19: the ladder (a use), 18 steps |
  | floors | 7, 6, 7, 8, 7, 6, 7 | 7, 6, 7, 8, 7 |
  | end | (32110,32252,7) | (32107,32249,7) |
  | automap blocks | 12512506, 12512507, 12512508, 12512607 | the same (`--check-route`) |
  | marks | +20,0 (set, edited), -12,+10 (set), 0,-15 (set, deleted), in 12512507 | the same offsets, in 12512507 |
  | default channel, Tibia.cfg, logout | yes | yes |
  | Tibia.exe runs of the walk | three: live, replay, repeat | one: the live run (the repeat with `--repeat`) |
  | time (route only) | 148 s live, 168 s per replay | about 24 s and 28 s (the same per-action time) |
  | walk time of a default run | about 13.5 min (four walks) | about 2.5 min (two walks) |
  New in the session: one use packet (0x82, the ladder), compared like every client packet.
- **The ladder, not the stairs** (medium until the live run). A search over every start on floor 7 with the
  server's map (scratch planner, build\logs\issue-16\route-search.log): on foot 14 steps from (32025,32242,7)
  west of the town (wolf and spider homes within a few fields; an attack would block the logout and could kill the
  level-1 character), 43 inside the town; with ladders 18 actions from (32054,32240,7) (nearer the wolves) and 19
  from the town house. The default is the 19-action ladder walk; the 44-step stairs walk (`--walk stairs`, start
  (32095,32235,7), a ramp and its stairs for floor 6) stays as the fallback if the use does not work live. The use
  is a Ctrl+right click on the player's own field: with ClassicControl off that is MapWindow's 0x400 use, and
  GetTopUseObject returns the first FORCEUSE object, the ladder under the player. moveuse.dat's rule is
  unconditional (`Use, IsType (Obj1,1948) -> MoveRel(User,Obj1,[0,1,-1])`).
- **The start through the player file** (high). record_walk writes the start into Patella Tester's
  15146574.usr (only the CurrentPosition value of the snapshot's text; StartPosition, the temple, stays) before
  the server starts, and restore_players puts the snapshot back afterwards, as before. A character without a
  player file is refused (log it in once). Replays do not need it: the recording carries the position.
- **One Tibia.exe walk** (high). The live recording's run is Tibia.exe's walk result (its files in
  walk-live\orig, its packets the recording's); only Patella.exe replays the walk. The November 2 results showed
  the live run's automap files and Tibia.cfg identical to both replays', and the proxy compares every packet of
  Patella.exe's replay with the recording, i.e. with Tibia.exe's live packets. With `--skip-live` there is no live
  run, so Tibia.exe replays the recording as before; walk-live.walk names the recording's walk (an older
  recording, from the temple, fails at the start position: record it again).
- **The repeat is an option** (high). `--repeat` runs Tibia.exe a second time for walk, error and error-login
  (the evidence for the masks: the times in Default.txt and error.txt); it was the default, `--no-repeat` is
  still accepted and changes nothing. The masks keep their evidence from the November 2 and 3 runs.
- **--check-route** (high). compare_files.py reads the sector files, objects.srv, houses.dat and moveuse.dat and
  walks a route: every target field one to stand on (a Bank object; no Unpass, Avoid or event object; at most one
  object with a Height and no event, the upper half of a ramp; not a house) or a plain floor change whose landing
  is one to stand on and equals floor_changes, every floor_changes entry used, the ladder's rule for U, the end,
  the automap blocks (exactly the four), the marks. The old route passes the same check with the same four blocks
  (its floor changes are the ones its live runs went through), so the model agrees with the server where it has
  been tried.
- **Gates** (high). py_compile and the imports (compare_files, compare_network), `client_state.py --self-test`;
  `compare_files.py --check-route` (both walks ok) and the old route through the same check
  (check_old_new.py); check_walk_helpers.py (set_start on a copy of the player file changes only
  CurrentPosition, walk_keys maps 0x82 to U, next_position follows both walks to their end); the issue #12
  run-folder check again. Logs: build\logs\issue-16. Not run: `compare_files.py --skip-live` (it starts clients).

## The Linux presets and patella_core (issue #7)

Settled by Claude Opus 5.5, November 3, 2026 (Docker 29.6.2 with its Linux engine; in the image GCC 16.2.0,
Clang 22.1.8, CMake 3.31.6, Ninja 1.12.1; on Windows MSVC 14.51, mingw-w64 GCC 16.2, CMake 4.1.2, Ninja 1.13.2).
`docker/linux-x64/Dockerfile`, `cmake/Core.cmake`, the presets linux-x64-gcc and linux-x64-clang,
`cmake/Warnings.cmake` (per-compiler baselines, C17), `cmake/Hardening.cmake` (PATELLA_SANITIZE), `tools/build.py`
(Docker), `tests/unit/core_*` and `core_expected.py`; in src/ common.h, vlong.h, vlong.cpp, regexp.c and
compat/cxx26.h, each value-preserving (below).

- **The image** (high). Docker Hub has a GCC 16 Linux image: the official `gcc:16.2.0-trixie` (Debian 13, GCC built
  from source by the Docker library into /usr/local, with libstdc++exp.a, which contract_assert needs; /usr/local/lib64
  first in ld.so.conf), so GCC is not built here. `docker/linux-x64/Dockerfile` takes it pinned by its index digest
  (`sha256:07280eb2e744f1b7a2d8d4a3894c089ea07266c109758afc388a634bd098e4eb`) and adds Debian trixie's packages pinned
  by version: clang-22 and libclang-rt-22-dev `1:22.1.8-1~deb13u4` (compiler-rt: the sanitizers), cmake and
  cmake-data `3.31.6-2`, ninja-build `1.12.1-1`; `clang`/`clang++` link to the -22 binaries. Built with
  `docker build -t patella-modern/linux-x64:gcc16.2-clang22.1 docker/linux-x64` in 24 s (the base image pulled
  before); local image ID `sha256:048574ca92a594c10fc337fe2341540855918b22b02e495e5b1a2bce71cdcf32` (not pushed to a
  registry, so it has no repository digest). When Debian replaces a pinned version in a point release the build
  stops at apt-get install; the Dockerfile says how to move the pin (the new version, or snapshot.debian.org).
  Clang uses Debian's GCC 14 installation (its default), so its C++ library is libstdc++ 14 and cxx26.h takes the
  stand-ins there; at run time the loader finds GCC 16's newer libstdc++.so.6 first, which is compatible.
  Considered: Clang on GCC 16's libstdc++ (`--gcc-install-dir`): untested pairing, and the plan allows either
  library; Debian's Clang 19: Clang 22 is in trixie as a stable update and is current.
- **Presets** (form). A hidden `linux-base` (condition: a Linux host; PATELLA_CLIENT OFF, compile_commands.json),
  `linux-x64-gcc` (gcc, g++) and `linux-x64-clang` (clang, clang++, `PATELLA_SANITIZE=address,undefined`), each with
  Release and Debug build and test presets and the workflows linux-x64-gcc, -gcc-debug, -clang, -clang-debug. The
  clang test presets set `ASAN_OPTIONS=detect_leaks=1` and `UBSAN_OPTIONS=print_stacktrace=1`. PATELLA_CLIENT was
  already MSVC-only in CMakeLists.txt; OFF says so on Linux. Nothing in the Linux configure reads Tibia.exe.
- **build.py in Docker** (form). On Windows, a preset whose condition asks for a Linux host runs in the image, in both
  forms of the script (`python -I tools/build.py linux-x64-gcc`, `--env linux-x64-gcc <command>`; the image name can
  be overridden with PATELLA_DOCKER_IMAGE). The repository is mounted read-only at /src; the files `git ls-files
  --cached --others --exclude-standard` names (so never the gitignored CipSoft files or build/) are copied with tar,
  with their times, to /work on the container's own file system, and the command runs there; build\<preset> is
  mounted as /work/build/<preset> (the build tree and the downloaded doctest stay; with the times kept, Ninja
  rebuilds only what changed). The output goes through the same warning scan. Considered: building on the bind
  mount itself (Docker Desktop's Windows mounts are case-insensitive, so an include in the wrong case would pass,
  one of the issue's checks), a separate shell script (another entry point next to build.py).
- **patella_core** (high). `cmake/Core.cmake`, a static library built by every product preset (windows-x64,
  windows-x64-gcc and the Linux ones; not the oracle): vlong.cpp, TRSA.cpp, TXTEA.cpp, TMyException.cpp (the type
  the TASSERT/TERROR/TEND macros throw), Utils.cpp, Skins.cpp, src/Extern/regexp/regexp.c, and Container.h (header
  only). They compile without a platform header: the target defines `PATELLA_CORE`, which leaves MFC and the
  Windows headers out of common.h, so a core source that used them would not compile (plan section 8). Left out:
  Script.cpp (io.h, sysinc.h), Objects.cpp and Inventory.cpp (they compile with PATELLA_CORE but call Script.cpp,
  graphics and the GUI), and the rest of src/ (errors without windows.h: Status.cpp 28, Creatures.cpp 25, Talk.cpp 11,
  Map.cpp 5, Preferences.cpp 2, Help.cpp 1, measured with GCC 16 -fsyntax-only). The client and the oracle still
  compile these sources themselves with MFC (the oracle's object order is fixed); linking the client with the
  library is for later.
- **The issue's portability list** (high). `char`: signed on x86 and x64 with MSVC, GCC and Clang; Utils.cpp's
  IsAlpha compares with `(char)0xe4` and the like, so the unsigned value 0xe4 is no letter where char is signed;
  tests_core asserts `std::is_signed_v<char>` (and int and unsigned 32-bit, little-endian) and checks both. `wchar_t`:
  no core source uses it. `long` (8 bytes on Linux): vlong.h's three CipSoft overloads `operator+(const vlong &,
  const long &)`, `operator*` and `operator<` converted the long to unsigned implicitly, which -Wconversion reports
  on Linux; they cast now (the same conversion; on Windows x86 and x64 long is 4 bytes, so the code is the same);
  regexp.c's `static long regsize` holds sizes below 32767 on any width. File names: every include of the core
  matches the file's case, and the Linux builds run on a case-sensitive file system. `_snprintf`, `_stricmp` and
  calling-convention keywords: none in the core (for later: `_snprintf` in ContentWindows, ContextMenus, Control,
  Dialogs.cpp/.h, Help, Map, Preferences, Script, Status; `_stricmp` in Creatures, Dialogs.h, Player; `__cdecl` in
  DialogElements.cpp; `WINAPI`/`CALLBACK` in SurfaceDX, surfaceDX9, Player; Dialogs.cpp opens "tibia.pic",
  "tibia.dat" and "tibia.spr" in lower case, where Control.cpp and Communication.cpp write "Tibia.*").
- **C17** (high). GCC 16's default C is C23, in which regexp.c's K&R definitions do not compile.
  patella_target_language gives C sources `-std=c17` on GCC and Clang. Clang warns about K&R definitions in every C
  standard and at every level (`-Wdeprecated-non-prototype`, 97 warnings in regexp.c), so patella_target_warnings turns that
  one off for C on Clang: the K&R form is the oracle's code until M6. MSVC's C is unchanged.
- **src/ changes** (high). Each is value-preserving, and the oracle shows it (below).
  - common.h: `#if defined(PATELLA_CORE)` before the MFC/shim choice. Without PATELLA_CORE the preprocessed text is
    the same; only the lines after it move (no code of src/ uses __LINE__ outside src/Extern).
  - vlong.h: the three casts (above).
  - vlong.cpp (fast_mul): `a[limit - 1] &= (1 << keep) - 1` overflowed int when keep was 31 (INT_MIN - 1), which
    UBSan reported in tests_core (linux-x64-clang). `(1u << keep) - 1` has the same bits for every keep 1..31, which
    is what MSVC computed (.text is identical).
  - regexp.c: `#pragma warning(disable: 4018)` only under `_MSC_VER` (GCC and Clang warn about an unknown pragma at
    -Wall); `-Wmaybe-uninitialized` off around reg() for GCC: parno is set and read only when paren is, which GCC's
    optimiser (Release, -Wall) does not see, and initialising it would add a store to the oracle's code, the reason
    Talk.cpp's C4701 is turned off the same way (issue #5). Both marked "Patella-Modern change", as the file's
    licence asks of altered versions.
  - compat/cxx26.h (no source of the client includes it): in constant evaluation, the stand-in TStaticVector now
    gives a destroyed slot of a trivial T a value again, and pop_back goes through DestroyFrom. Clang 22 refused to
    evaluate the trivial copy constructor of a TStaticVector<int, 8> after an erase ("subobject 'Slots' is not
    initialized": an array element whose lifetime had ended), which GCC and MSVC accept. Outside constant evaluation
    nothing changes.
  - tests/unit/cxx26_static_vector_test.cpp: TLessOnly's unused defaulted operator== is `[[maybe_unused]]` (Clang's
    -Wunneeded-internal-declaration).
- **Warning baselines** (high). The list of #8 is `PATELLA_WARNING_BASELINE_MSVC` now, unchanged (patella_core's and
  tests_core's new translation units build clean at /W4: TRSA, TMyException and Skins without MFC, and the tests);
  `PATELLA_WARNING_BASELINE_GCC` and `_CLANG` are new, in the same form (the full level, else -Wall, per translation
  unit). Measured for what builds there, patella_core and the unit tests, with every source at the full level and
  `--compile-no-warning-as-error`, Release and Debug (build\logs\issue-7\measure-*.log; counts per configuration):
  - GCC 16.2 on Linux (and the same files with mingw-w64 GCC 16.2): vlong.cpp (1 -Wconversion size_t to unsigned in
    fast_mul, 1 -Wshadow `min`), TXTEA.cpp (1 -Wshadow `Key`), Utils.cpp (3 -Wshadow `Data`), regexp.c (2
    -Wconversion, 2 -Wsign-conversion, 1 -Wsign-compare), tests/unit/core_container_test.cpp (13 -Wshadow:
    Container.h's constructor parameters hide its members).
  - Clang 22.1: vlong.cpp (1 -Wshadow, 6 -Wsign-conversion), TXTEA.cpp (1 -Wshadow), Utils.cpp (11
    -Wsign-conversion), Skins.cpp (1 -Wsign-conversion), regexp.c (2 -Wsign-conversion, 1 -Wsign-compare, 13
    -Wstrict-prototypes), core_container_test.cpp (7 -Wsign-conversion: Container.h's int sizes for new[]). Clang's
    -Wconversion includes -Wsign-conversion, GCC's does not for C++.
  - TRSA.cpp, TMyException.cpp, and on GCC Skins.cpp build at the full level; so do the other test sources. The test
    file is on the GCC and Clang lists because of Container.h, a warning fix for M3 like the others.
- **Sanitizers** (form). `PATELLA_SANITIZE` (a cache variable; Hardening.cmake): `-fsanitize=<list>
  -fno-sanitize-recover=all -fno-omit-frame-pointer` on compile and link of the product targets, GCC and Clang only
  (the configure stops on MSVC; windows-x64-asan is later). Every finding ends the test; LeakSanitizer runs
  (detect_leaks=1) and found nothing; UBSan found the vlong overflow above.
- **tests_core** (form). One executable (tests/unit), doctest's own main: the platform assumptions; vlong and TRSA
  with tools/difftest/vlongtest.cpp's 3,201 cases (the same generator) digested with FNV-1a against the digest
  `tests/unit/core_expected.py` computes with Python's exact integers, the way vlongcheck.py checks vlongtest.exe, plus
  small cases, RSA of 1 and 2 and TRSA's error texts; TXTEA against XTEA written from the algorithm (the all-zero
  vector is the published dee9d4d8 f7131ed9); Utils (character classes by signed char, the string helpers, the
  outfit colours against Python's doubles, the packet buffers, TMyException's messages and origin); Skins; the
  regexp (subexpressions, Latin-1 classes, the error texts through regerrorhandler, Status.h's LinkPattern); and
  Container.h's four templates with their error messages. 17 test cases, 139 assertions, the same on all four
  compilers.
- **Gates** (high). In the image (`python -I tools/build.py <preset> --fresh` from Windows): linux-x64-gcc 9/9 CTest
  tests (the real path's precondition scenario skipped, as on mingw since #9), tests_unit 18 test cases / 158
  assertions (cxx26.h: contracts and inplace_vector real), tests_unit_cxx26_standin 18 / 160, tests_core 17 / 139;
  linux-x64-clang (ASan, UBSan) 9/9, tests_unit 18 / 156 (stand-ins), standin 18 / 160, tests_core 17 / 139; the
  -debug workflows the same; 0 warnings. windows-x64 and windows-x64-gcc `--fresh` (and windows-x64-gcc-debug): 9/9,
  0 warnings, tests_core 17 / 139. oracle-x86 `--fresh`: 0 warnings, CTest 4/4, 16,848,752 / 67 / 1,134,513 / 3,201 checks, 0 failures,
  resources 0 differences. The oracle's Release Patella.exe against
  main's (e740ddb, built in build\mainoracle, a binary directory of the same length as build\oracle-x86, so the PDB
  path in .rdata has the same length): .text, .data, .fptable, .rsrc and .reloc identical; the headers and .rdata
  differ only in the link timestamps and the CodeView record (PDB GUID and the directory name);
  `build\issue-8\tools\macrocheck.py main issue-7`: 2,108 calls, 0 files differ. Logs: build\logs\issue-7.
- **For CI (#11)** (form). The hosted Linux job can run the image: build it from docker/linux-x64 in the job (about
  half a minute on top of pulling gcc:16.2.0-trixie) or push it to a registry (GHCR) and pin the digest, then
  `cmake --workflow --preset linux-x64-gcc` and `linux-x64-clang` in the checkout (a Linux runner checks out with
  case-sensitive names, so build.py's copy is not needed there). Nothing in these presets reads a CipSoft file;
  FetchContent downloads doctest once per build tree (cache build/<preset>/_deps). The windows-x64 job needs
  `-DPATELLA_CLIENT=OFF` without Tibia.exe and then builds patella_core and the unit tests.

## CI: hosted and self-hosted workflows (issue #11)

Settled by Claude Opus 5.5, November 3, 2026 (GitHub Actions: windows-latest was the image windows-2025-vs2026
with MSVC 19.51, ubuntu-latest Ubuntu 26.04; the image of docker/linux-x64 with GCC 16.2.0, Clang, clang-format and
clang-tidy 22.1.8). `.github/workflows/hosted.yml`, `.github/workflows/self-hosted.yml`, `tools/tidy.py`
(`--all`, `--baseline`, `--write-baseline`), `tools/tidy-baseline.tsv`, `docker/linux-x64/Dockerfile`. No change to
src/.

- **Hosted jobs** (form). Five jobs, in parallel, on push (every branch), pull_request and workflow_dispatch:
  windows-x64 with MSVC (configure with `-DPATELLA_CLIENT=OFF` through `tools/build.py --env windows-x64`, because
  `cmake --workflow` takes no -D and the client's configure reads Tibia.exe; then the Release and Debug builds and
  their ctest presets), linux-x64-gcc and linux-x64-clang (a matrix; `python3 -I tools/build.py <preset>` and
  `<preset>-debug` in the container, so the warning check of #17 applies there too), clang-format and clang-tidy.
  build.py's own Docker path (the copy to a case-sensitive /work) is for Windows hosts; a Linux runner mounts its
  case-sensitive checkout at /work. A pull request run is cancelled by a newer push to it; branch runs are not.
- **The image in CI** (form). Built in every Linux job from the pinned Dockerfile (the base image by digest, the
  packages by version), about 30 s, rather than pushed to GHCR: no registry, no package permission, no second
  place to keep in step with the Dockerfile. No Docker layer cache (it would need another action).
- **clang-format and clang-tidy in the image** (high). The Dockerfile gets Debian's clang-format-22 and
  clang-tidy-22 (`1:22.1.8-1~deb13u4`, the version of its clang-22), linked as clang-format and clang-tidy; the tag
  stays gcc16.2-clang22.1, so a local image must be built again. Considered: Visual Studio's LLVM on the Windows
  runner (another version than the local VS 2026's 22.1.3 is possible on any image update, and formatting and
  findings change between major versions). The local check with Visual Studio's 22.1.3 and the image's 22.1.8 both
  report nothing on the same files.
- **clang-format's files** (form). CLAUDE.md's list and tests/unit/*.cpp, the list of notes/reconstruction.md
  ("Checking"), which was longer; CLAUDE.md names tests/unit/*.cpp now too.
- **The clang-tidy baseline** (high). tools/tidy.py with `--baseline <file>` counts the findings outside the
  TASSERT/TERROR/TEND arguments by (file, check, message), not by line, so that moving code changes nothing, and
  fails only when a key has more findings than the list; fewer are reported as gone (write the list again to drop
  them). `--write-baseline` writes the list (tab-separated, sorted, a header with the command). The compile commands
  are linux-x64-clang's (configure only): the hosted runners have neither Tibia.exe (the client's configure) nor a
  pinned clang-tidy for MSVC's command lines, and Linux compiles only patella_core's sources there (vlong, TRSA,
  TXTEA, TMyException, Utils, Skins), with the headers of src/ they include. `--all` takes the sources of
  compile_commands.json under src/ but not src/Extern, so the check grows with patella_core; a newly added core
  source brings its findings as new ones, which the change fixes or adds to the list on purpose. The first list:
  31 findings in 26 entries (and 16 inside macro arguments, not in the list).
- **Self-hosted workflow** (high). Jobs oracle-x86 (`tools/build.py oracle-x86`: build and the four CTest tests),
  windows-x64 (client and unit tests, Release and Debug), comparisons (compare_traffic.py and compare_files.py with
  `--patella build\windows-x64\Release\Patella.exe`, compare_rendering.py `--modes windowed` from compare_traffic's
  recording, then `testserver.py stop --force` always) and linux-docker (the Linux presets through build.py in the
  local image: the place of the Linux replays of M2 and M8). Triggers: `push` and `workflow_dispatch` only. Each
  job's `if` also requires the repository Coldensjo/Patella-Modern (a fork's copy never asks for a runner), the
  event push or workflow_dispatch (should someone add a trigger later), and the repository variable
  `PATELLA_SELF_HOSTED == 'true'`. Runner labels `self-hosted, Windows, X64, patella`; one run at a time
  (concurrency group, nothing cancelled: one machine, the server's fixed ports). The CipSoft files are copied from
  `PATELLA_CIPSOFT_DIR` (a variable, default C:\Tibia) into the checkout's root, Tibia.exe's sha256 checked against
  CLAUDE.md's; the next checkout's git clean removes them; no artifact or cache is uploaded. Each job checks out
  again (git clean), so the comparisons job builds both clients itself.
- **Until the runner exists** (high). With the variable unset every job is skipped at once: the push of 4f0f0dc
  gave the run "self-hosted" with four skipped jobs and nothing queued. Considered: a commented-out `on:` or a
  disabled workflow (`gh workflow disable`; a re-enable is as easy, but the state is not visible in the file and a
  workflow_dispatch would still queue when enabled), `runs-on` chosen by an expression (a queued job all the same).
  The variable keeps the trigger rule as it is and is one command to turn on (notes/reconstruction.md,
  "Checking", with the runner's setup: a separate unprivileged user, its logged-in session with run.cmd rather than
  a service, because the comparisons drive client windows; approval for outside contributors' workflows).
- **Not here** (form). A clang-tidy run over the whole of src/ needs the client's compile commands (windows-x64 with
  Tibia.exe) and MSVC's headers; it belongs to the self-hosted workflow with a baseline of its own once someone
  wants it. Golden corpus (4.3) and Linux replays (M2, M8) do not exist yet.
- **Result** (high). Hosted run of 4f0f0dc green: windows-x64 1 min 44 s, linux-x64-gcc 2 min 18 s, linux-x64-clang
  2 min 20 s, clang-format 58 s, clang-tidy 1 min 7 s; CTest 9/9 in each of the six build configurations (tests_unit
  18 test cases with 158 assertions on MSVC and GCC, 156 on Clang; tests_unit_cxx26_standin 18 / 160; tests_core 17 /
  139). A throwaway branch (issue-11-negative, deleted after the run) with `int  is_one( ) const {return n==1;}` in
  vlong.h failed exactly the clang-format job (six violations at vlong.h:44) and the clang-tidy job ("new: 1 x
  src/vlong.h: function 'is_one' should be marked [[nodiscard]]", 32 against 31), the build jobs green. Logs:
  build\logs\issue-11.

## compare_rendering.py on the x64 client (issue #18)

Settled by Claude Opus 5.5, November 3, 2026 (MSVC 14.51, CMake 4.1.2, Python 3.13). `cmake/Capture.cmake` (new),
`cmake/OracleX86.cmake`, `CMakeLists.txt`, `tools/compare_rendering.py`, `tools/capture/patcap.c` (comment only),
`.github/workflows/self-hosted.yml`. No change to src/.

- **One source, two DLLs** (high). patcap.dll is loaded into the client it captures, so it must have the client's
  bitness; a run of the x64 client still needs the 32-bit one for Tibia.exe. patcap.c builds unchanged for x64:
  `IMAGE_THUNK_DATA` is pointer-sized, the COM vtable indices (Blt 5, Flip 11, CreateSurface 6, CreateDevice 16,
  Present 17) are the same, the shared memory holds only LONGs and characters (the timer window's handle is
  stored truncated, for information), 0 warnings at /W4 /WX in Release and Debug. The target moved to
  `cmake/Capture.cmake`, which the oracle includes where it defined it (its command lines are the same: `-MT /O2
  /W4 /WX`, `/MANIFEST:NO`, compared in the generated ninja files of main's and this build) and CMakeLists.txt
  includes for an MSVC 64-bit build, with or without the client, so the hosted windows-x64 job compiles it too.
  The x64 build keeps CMake's default flags (Release `/O2 /Ob2 /DNDEBUG`, Debug `/Od /RTC1`; `/O2` in Debug would
  contradict `/RTC1`). A test instrument: neither Warnings.cmake nor Hardening.cmake.
- **Which DLL** (high). compare_rendering.py picks the preset by each run folder's client's PE machine field
  (client_state.pe_machine: 0x14c oracle-x86, 0x8664 windows-x64) and builds that preset's patcap target when
  `build\<preset>\Release\patcap.dll` is missing or older than patcap.c, as before for the oracle.
- **add_import on PE32+** (high). The optional header's magic chooses the layout: PE32+ data directories start
  at +112 (not +96), and the lookup and address tables get 64-bit thunks, aligned to 8 after the descriptors.
  It refuses an image with fewer than 12 data directories (it writes the bound import entry). Checked offline on
  copies in %TEMP%, never run (build\logs\issue-18\add-import-offline.log, dumpbin-x64.log): the x64 Patella.exe
  keeps its 18 imports unchanged and gets patcap.dll!PatcapLoaded in a ninth section `.patcap`, which dumpbin
  reads (bound imports 0, SizeOfImage covers it, CFG and CET flags untouched); on Tibia.exe the output is byte
  for byte the previous add_import's. The x64 client imports ten of patcap's eleven hooks by name, as the x86
  builds do; GetSystemTimePreciseAsFileTime comes through the hooked GetProcAddress.
- **--patella** (form). As compare_traffic.py's and compare_files.py's: the Patella.exe to test, x86 or x64, with
  its PDB next to it; the report names it. The self-hosted comparisons job passes the x64 client's.
- **Not checked yet** (form). No client has run with the x64 patcap.dll here: the windowed run of the x64
  client against Tibia.exe (the issue's last criterion) is the live check, to be run on the desktop.

## Test mode: virtual clock and scripted input (Patella-Modern issue #21)

Settled by Claude Opus 5.5, November 3, 2026. notes/testmode.md describes the result; src/TestHooks.h,
src/testmode/, cmake/TestMode.cmake, cmake/WindowsX64.cmake (PatellaTest), tests/unit/testmode_*.cpp,
tests/testmode/login-walk.script, tools/testmode_check.py.

- **A test build, not a switch in the product** (high; plan 4.4: "compiled only into test builds"). PatellaTest.exe
  is a second target of the preset windows-x64, the client's sources compiled again with PATELLA_TEST_MODE
  (option PATELLA_TEST_BUILD, default ON, so every windows-x64 build and the self-hosted CI build it).
  Considered: a preset of its own (windows-x64-test: a third configure and build tree for one definition) and a
  command-line switch in Patella.exe (the product would carry the test mode). The hooks in the client are macros
  in src/TestHooks.h, included by common.h (not in PATELLA_CORE): outside the test build each is the system call
  it replaces, token for token (timeGetTime(), _time32(Time), time(Time), Sleep(ms), GetCursorPos(Point),
  ::SetTimer(...)), and the driver's call in OnIdle is a statement macro that is empty. Its first form, an empty
  statement `((void)0);`, made MSVC's unoptimised Debug build emit a nop, which moved the code after it. The
  class change of the test build (CPlayerApp::PreTranslateMessage) is inside `#if defined(PATELLA_TEST_MODE)`,
  so the oracle's layouts and Patella.exe's are untouched.
- **What goes through the clock** (high). The three clocks the client reads (notes/rendering.md, "Making time
  equal"): timeGetTime (sysinc.cpp: StartTime, GetMilliseconds, srand; the engines' ResetFrameTimer and
  WaitForNextFrame), the wall clock (_time32 in Talk.cpp, Creatures.cpp, ContentWindows.cpp, ConsoleWindow.cpp;
  time in Player.cpp's error.txt) and the frame timer's Sleep; plus the two things patcap.dll fakes besides: the
  100 ms main timer (not created; the driver makes its ticks) and GetCursorPos (a virtual cursor). One IClock
  (Milliseconds, WallSeconds, FrameWait) with TSystemClock (timeGetTime, _time64, Sleep: the test build without a
  script) and TVirtualClock. The other Sleep calls (the engines' surface polling, Redraw's Sleep(10) while the
  surface is lost) and the network's own timing stay real. _localtime32 and ctime keep the machine's time zone.
- **The virtual clock's start** (high). patcap.c's: timeGetTime 100000, the wall clock 2026-11-02 12:00:00 UTC
  (1793620800, FILETIME_BASE in seconds; a unit test derives it from both). So srand gets 100000 in every run,
  and the test build makes the same XTEA keys and RSA padding in every run. The clock moves only forward, only by
  the driver; FrameWait does not move it (frames are not time) and sleeps a real millisecond, so the idle loop
  (which redraws until the clock moves) does not spin.
- **Script format** (form). Plain text, one command per line, times relative (`wait`) or absolute (`at`),
  composite commands (`key Ctrl+L`, `click x y Shift`) expanded by the parser into primitive events, so the
  driver dispatches only one kind of thing and the unit tests see exactly what the client will get. Considered:
  JSON lines (verbose to write by hand; issue #23's recordings can still be converted into this form, or become
  a second event source). `key` makes the character TranslateMessage would make, from a fixed US-layout table,
  not ToAscii: the machine's keyboard layout must not change a run. `text` makes characters only, as
  tools/login_client.py types. Key codes are Windows' virtual keys and set-1 scan codes as numbers, so the parser
  stays portable (patella_testmode builds and is tested on every product preset, Linux included); the platform
  layer of M8 maps them. Coordinates may be anchored to the right or bottom edge (`B-220`), because the client
  area's size depends on the window frame's metrics; tools/login_client.py's Enter Game is (113, height - 220).
- **The driver in OnIdle, one event per call** (high). MFC calls OnIdle whenever the queue is empty and the
  client's OnIdle always returns TRUE, so a hook at its start runs between every two passes of the client's idle
  work (the mouse poll, ProcessEvents, Redraw), on the client's own thread. Each call dispatches one due event (so
  the client handles each, and polls a move before the button that follows, as with input at human speed), or
  moves the clock to the next stop (the next multiple of 100 ms, a tick, or the next event). Events go to the main
  frame through DispatchMessageA, as the message loop dispatches them (MFC's Default() and the handlers see the
  same message); not SendMessage, and not PostMessage (posted input would meet the filter below and
  TranslateMessage). Ticks are WM_TIMER 1, as the real timer's.
- **The desktop's input is dropped** (high). With a script, CPlayerApp::PreTranslateMessage drops the keyboard
  and mouse messages and the main timer's WM_TIMER from the queue, so a user's mouse over the window cannot
  disturb a run (the limit in notes/reconstruction.md, "Test server", and the crash of November 2 there). Window
  management, WM_CLOSE and the network's WM_USER+1 pass. WM_ACTIVATEAPP (sent, not queued) still clears the
  client's modifier flags when the window loses the focus, so scripts hold modifiers only within one command.
- **Pace** (high). Against a live server the virtual clock has to give the server real time: `pace` (real ms per
  100 virtual ms, default 100) holds each stop until its real time has come; 0 runs as fast as the client does
  (for issue #23's replays without a server). What still depends on the real time with a live server is when the
  server's packets arrive; the check's script waits generously, and the check counts the server's pings (0x1e,
  which the client answers).
- **Errors** (form). A script that does not parse ends the program before the window opens with exit code 3 and
  the message on stderr, in the log and through OutputDebugString: never a dialog (the runs are unattended).
- **Static CRT** (form). The client links the static CRT, patella_testmode (like the unit tests) the DLL one;
  PatellaTest compiles the two portable sources itself instead of linking the library (LNK2038 otherwise).
- **Proofs without a client** (high). tests_testmode (15 test cases, 308 assertions on every product preset): the
  clock, the parser (every command, error and escape), the key table, the scheduler, and the driver's loop
  simulated on the virtual clock twice (identical, with the expected order of ticks and events). The oracle's
  Patella.exe built from main (ec11f81) and from this branch at the same path (__FILE__ is in .rdata) compare with
  0 unexpected differences (issue #8's pecompare: only the link time stamps and the PDB record differ), and the
  difftest counts are unchanged; the same for windows-x64's Release Patella.exe. Its Debug Patella.exe is not
  byte-identical: with the new header included, MSVC's unoptimised build emits some string literals of every
  object in another order (dumpbin: the objects' code is the same apart from the literals' internal names), which
  moves data. main with only common.h's include and src/TestHooks.h added (no other change) gives exactly this
  branch's Debug image (0 unexpected differences), and main with four comment lines added to common.h gives
  main's: the difference is the header's preprocessing alone. Neither Patella.exe (Release, Debug) nor its PDB
  contains a test-mode name (testmode, TVirtualClock, TScheduler, ParseScript, --test-script, DropInput, ...);
  the PDBs name src/TestHooks.h among the included headers, as the oracle's does. PatellaTest.exe and its PDB
  contain them all.
- **Not here** (form). The live determinism check needs the test server and a client window; it is
  tools/testmode_check.py, for the orchestrator or the self-hosted workflow (not added to it here). The state
  dump, packet log and frame dump are issue #22; the session scripts and reference recordings issue #23.

## Test mode: the dumps (Patella-Modern issue #22)

Settled by Claude Opus 5.5, November 3, 2026. notes/testmode.md, "The dumps", describes the result;
src/testmode/StateDump.*, PacketLog.*, Png.*, ClientStateWin32.cpp, ClientView.h, TestModeWin32.cpp,
src/TestHooks.h, the View functions at the ends of Creatures.cpp, Inventory.cpp, Map.cpp and Status.cpp, the
hooks in Network.cpp, Communication.cpp and vulkan/SurfaceVulkan.cpp, tests/unit/testmode_dump_test.cpp,
tests/testmode/login-walk.script, tools/testmode_check.py, tools/testmode_dumps.py.

- **The state dump is client_state.py's reading, made inside** (high; the issue's criterion). The same keys in
  the same order, the same integer forms (4-byte values signed, a creature's ID unsigned, the bools as their
  byte), the same text limits (char[N] to the first NUL, the console's C strings to 512 bytes, a label's to 256)
  and the same clamps; gui()'s walk (siblings through Next, 500 at most; children of a TWindow; the RTTI class
  name from the decorated `.?AV<name>@@`), with this process's addresses as "ptr" and "vtable". So the dump and
  the outside reading of the same moment can be compared with `==`, and #23 can compare a test client's dumps
  with Tibia.exe's readings after dropping the addresses. A component is a window when it is a TWindow
  (dynamic_cast); client_state.py tests whether its GetFirstChild is TWindow's code, the only override of
  TComponent's throwing one, which gives the same answer. Considered: a dump of our own design (more fields,
  times); it could not be checked against the existing reading, which is the issue's criterion, and #23's
  references come from that reading.
- **How the dump reaches file-local data** (high). Most of what client_state.py reads is static in its file
  (Creatures.cpp's Player* and Creatures, Inventory.cpp, Map.cpp's Fields, Status.cpp's ConnectionState). Each
  of those files ends with a View function inside `#if defined(PATELLA_TEST_MODE)` that hands out const
  pointers (src/testmode/ClientView.h, included through src/TestHooks.h only in the test build). At the end of
  the file no line of the product's code moves, and outside the test build the preprocessor removes it.
  Considered: the existing getters (GetPlayerHealth ...): not one for every field, some with TBEGIN/TEND, and
  calling client code is a change of what the client does; extern declarations instead of static: a change of
  the oracle's sources and linkage.
- **The same moment, from outside** (high). The check needs client_state.py to read the moment the client
  dumps. `--test-sync <name>`: after writing a dump the client sets `<name>-ready` and waits for
  `<name>-continue` (events the checker makes; at most 300 s), on the client's own thread at the driver's step in
  OnIdle, so nothing of the client runs meanwhile (the network's notifications are window messages on that
  thread). The wait is taken out of the pace (the driver's real start moves by it), so the virtual clock, the
  events and the packets the client sends do not see it; what can see it is the server, which waits too (its
  pings, as before, depend on real time). Considered: suspending the process from outside at a time the
  checker guesses (not the same moment); reading only after the run (the state has moved on).
- **A dump takes no idle call of its own** (high). The driver delivers one input event per idle call (issue
  #21); a `dump` is done in the call that then delivers the next event or moves the clock, so a script with
  dumps gives the client the same events in the same idle calls as without. A `frame` does take a call, as an
  input event does: it waits for the next frame drawn, and a frame drawn after the next event would show that
  event's effect.
- **The packet log at the encryption, both ways** (high). SendCommand writes the packet into SendData and then
  encrypts it in place, so the hook sits between (the payload, the padding the client's rand() made, before
  XTEA); for the unencrypted first packets the RSA block is already encrypted there, so a second hook in
  SendLogin/SendGameLogin keeps the block before RSA.Encrypt and the log writes the first packet with it: that is
  exactly proxy.py's "plain" of a first packet (it decrypts the block with the server's key). The received
  packets are logged where PrepareNextPacket has decrypted them: the client's view, which the proxy's decrypted
  capture must equal. Considered: logging only the sent packets (the plan says "a packet log", and #23's replay
  is about the server side, so both directions are useful). The connections are numbered as proxy.py numbers
  them (kind from the first packet's opcode, ordinal per kind), so the two can be matched without times.
- **The line format** (form). Text, one packet per line, seven space-separated fields, hex payloads: diffable,
  readable in an editor, as simple to parse in Python as in C++ (both are: PacketLog.cpp's parser, the unit
  tests, testmode_dumps.py). Considered: JSON lines like proxy.py's recordings (heavier, and the plan asked for a
  line format). Time stamps are the virtual clock's, so a log can be compared across runs, but the comparison
  with the proxy does not use them.
- **The frame dump through the engine's frame observer** (high). TSurfaceVulkan already calls patcap.dll's
  observer before each Present with the frame's mask and the canvas read back (issue #19); without patcap.dll
  the test build installs its own observer there (one statement-macro hook in InitDisplay), so a `frame` gets
  the frame as it is shown and its device-filtered pixels, the same data compare_rendering.py gets. The engine
  keeps masks only while an observer exists, so the test mode observes only when the script has `frame`
  commands. Considered: TSurfaceVulkan::ReadScreen from the driver's step (any moment's canvas, not the shown
  frame, and no mask).
- **PNG with stored deflate blocks** (form; no new dependency). 8-bit RGB, filter 0, the image data as zlib
  stored blocks with CRC-32 and Adler-32 written in Png.cpp (about 150 lines, unit-tested against known values
  and by inflating the stream back); larger than compressed PNG (an 800x600 frame about 1.4 MB) but readable by
  every tool, compare_rendering.py's read_png included. The mask as a second PNG, white where filtered, as
  compare_rendering.py's `<scene>.filtered.png`, so #23 applies the same tolerance with the same code.
- **The portable part in patella_testmode** (high). The JSON writer and the dump's data (StateDump), the line
  format and the connections (PacketLog) and the PNG encoder (Png) are portable and tested on every product
  preset (tests_testmode: 9 new test cases, 243 assertions); only the reading of the client and the files are in
  the Windows side.
- **The proofs without a client** (high). The unit tests; `tools/testmode_check.py --self-test` (the
  comparisons on made-up runs: equal data passes, each kind of difference fails and is named); the static
  comparison of the dump's key lists (StateDump.h) with client_state.py's (its source read with ast); and
  client_state.py's `--layout` on PatellaTest.exe (every entry resolved in its PDB). Patella.exe and the
  oracle-x86 Patella.exe built from main and from the branch at the same path: 0 unexpected differences
  (pecompare); neither they nor their PDBs contain a test-mode name of #22 (ViewCreatures, PacketSent,
  StateDumpJson, FrameObservers, patella-state, ...). The live check (e, f against the test server) is for the
  orchestrator: it needs the server and a client window.

## The Vulkan engine (Patella-Modern issue #19)

Settled by Claude Opus 5.5, November 3, 2026 (MSVC 14.51, CMake 4.1.2, Vulkan loader 1.4.357). notes/vulkan.md
describes the result; src/vulkan/, cmake/Vulkan.cmake, cmake/WindowsX64.cmake, src/Surface.cpp (the choice of
engine 2), tests/vulkan/, tests/unit/render_quads_test.cpp, tools/capture/patcap.c, tools/compare_rendering.py.

- **surfaceDX9.cpp's counterpart, call for call** (high). The frames are compared with Tibia.exe's DirectX 9
  frames (D7), so TSurfaceVulkan keeps surfaceDX9.cpp's logic and replaces only the Direct3D 9 calls: the same
  surface sizes (the 512x512 video cache surfaces, not SurfaceGL.cpp's 256), the same queue of quads and when it
  is drawn (a texture or target change, 9,600 vertices, the end of the frame), the sampler state applied when the
  queue is drawn, the statistics, the odd behaviour (SetSurfaceRect's else, the automap upload whatever the
  surface, StretchBlit through TEMP_SURFACE and STRETCH_SURFACE), and the file statics that outlive the surface
  object. Considered: a fresh 2D compositor with a sprite atlas (plan 6.2's sketch); it would have to reproduce
  the same order of operations to give the same pixels, which is what the counterpart does directly. The back end
  below it (Renderer.cpp) is the small portable compositor the plan describes and the frame dump needs.
- **Pixels** (high for the copies; "Needs the user" below for the stretches). Direct3D 9's pixel centres are on
  whole coordinates; Quads.cpp makes a one-to-one, one-colour quad a nearest-sampled copy with whole corners (no
  pixel centre on an edge, the texel edges on the pixel edges: the exact texels on every device), and gives every
  other quad to the hardware as a Direct3D 9 driver does (corners + 0.5, attributes unchanged). A first version
  kept whole corners for all quads and moved the attributes half a pixel back along their gradients instead;
  tests_vulkan gave the same numbers, and the Direct3D 9 driver's form is the one more likely to give the same
  bits on the same GPU. ColorFill refuses a rectangle that is not inside the surface, as Direct3D 9's runtime
  does (D3DERR_INVALIDCALL; medium: from the documentation, not seen on a device). A stage without a texture
  samples opaque white (low; only right after a device is created, before the first SetTexture).
- **The CPU driver: Mesa's lavapipe** (high). The plan's two candidates: lavapipe (Mesa) and SwiftShader
  (Google). lavapipe: prebuilt for Windows by the mesa-dist-win project for every Mesa release, MSVC-built, the
  archive's SHA256 published by GitHub; one self-contained DLL (only system DLLs); Vulkan 1.4 conformant; the
  same driver is the Linux distributions' `mesa-vulkan-drivers`, so the Docker image of the Linux tests (plan
  4.7, M8) can run the same code; maintained with Mesa. SwiftShader: no prebuilt releases outside Chromium's
  builds, so it would be built from source with its own LLVM (a long build in every fresh build tree, or a
  second pinned binary we would have to produce and host); Vulkan 1.3. Pinned: Mesa 26.2.5 (October 30), not
  26.2.6 (released the morning of this issue, November 3). Cost: 56 MB next to the executable, and llvmpipe
  compiles its shaders with LLVM when the pipelines are created (tests_vulkan's device creations and tests take
  about 3 s together).
- **Loading it** (high). Only through the loader's driver setting in the client's own process:
  `VK_DRIVER_FILES` (and `VK_ICD_FILENAMES` for older loaders) set to the manifest next to the executable for the
  instance's and device's creation, then restored. No registry key, no system-wide install, no change to the
  user's Vulkan configuration. `auto` asks the system's drivers first and loads the CPU driver only when they
  offer no Vulkan 1.3 GPU (plan 6.2: "loaded only when no hardware device is found"). An elevated process ignores
  the variables (the loader's rule); the engine then reports that the CPU driver offers no device.
- **The choice of the engine for now: `PATELLA_VULKAN`** (form; plan 6.3 needs the user). Plan 6.3 turns the
  engine buttons into a device choice stored in the fork's settings file (D12); both are user-visible and neither
  exists yet, and the three engines stay in windows-x64 until issue #20. So the engine is chosen by an
  environment variable that the user does not see, standing in for engine 2 (DirectX 9, the reference), with
  `auto`, `gpu`, `gpuN`, `cpu`: Tibia.cfg, its engine numbers and the dialog are unchanged. Considered: an engine
  number 3 (Tibia.cfg would get a number outside the original's range, which 6.3 rules out), a command-line
  argument (Player.cpp's parser is reconstructed code, and compare_rendering.py's clients get no arguments).
- **DirectDraw for the modes and the stretch capabilities** (high, for now). surfaceDX9.cpp lists the display
  modes with DirectDraw 4 and sets the default of "no stretching" from its capabilities; the graphics options
  show that list. The Vulkan engine does the same, so the dialog shows the same list as Tibia.exe's DirectX 9.
  Issue #20 (the engines removed, DirectDraw with them) and D11 (borderless fullscreen; the resolution list becomes
  a scale) replace it. Fullscreen sets the mode with ChangeDisplaySettings, as Direct3D 9's fullscreen device
  does.
- **Present mode IMMEDIATE** (high). The originals present without waiting (D3DPRESENT_INTERVAL_IMMEDIATE) and
  pace with their own frame limiter. D13 (vsync by default) needs the settings file for its switch; until then
  vsync would change what the user sees.
- **The capture: the engine calls patcap.dll** (high). Reading a swapchain image from an injected DLL would need
  the engine's device, queue, a transfer usage on the swapchain and a copy at vkQueuePresentKHR; the engine has
  the canvas and a read-back anyway (the frame dump needs it). So when patcap.dll is loaded the engine reports its
  device and offers each frame (PatcapVulkanDevice, PatcapVulkanFrame, PatcapVulkanPixels); patcap's shared memory
  grows by the Vulkan fields at its end (version 2; the earlier offsets unchanged, so compare_rendering.py's other
  engines and the 32-bit DLL see the same layout). Product code names patcap.dll: three lookups per device,
  removed with patcap's engine hooks when the frame dump replaces them (plan 6.2, issue #22).
- **No TASSERT, TERROR or TEND in the new code** (form). Their texts are compared by macrocheck and difftest until
  M5, and new ones in a new file would be differences of their own; the engine ignores an index out of range
  instead of surfaceDX9.cpp's assertion (no caller passes one).
- **Warnings** (form). TSurfaceVulkan builds at /W4 like every file outside the baseline list (the list does not
  grow): it does not include GUI.h (not /W4-clean; one declaration of DrawSkin instead) and names its parameters
  so that none hides a TSurface member. The libraries' sources (volk, vk-bootstrap, VMA) compile in
  `patella_vulkan_deps` with `/W0`, glslang with its own CMakeLists and `/W0`: not our code, and their own
  warning flags are not ours either.
- **Shaders** (form). GLSL 450 in src/vulkan/shaders, compiled by the glslang executable of the same build into
  headers of SPIR-V words per configuration (build/<preset>/shaders/<Config>); no Vulkan SDK.
- **Proofs without a window** (high). tests_render (every preset) checks Quads.cpp; tests_vulkan renders known
  primitives headless on the CPU driver and on the GPU and compares them with a model of Direct3D 9's pixels
  (notes/vulkan.md, "Checks"): exact where the model says exact on both devices, at most 1 per channel where
  the device filters. The oracle's Patella.exe built from main (3655cdf) and from this branch at the same path
  compare with 0 unexpected differences (pecompare), macrocheck finds no difference (2,108 calls), and the
  difftest counts are unchanged.
- **The filtered stretches** (decided by the user on November 3, 2026; below). The plan's contract (6.1, 4.5) is "exact outside the light"
  because it took every operation but the light for integer work. surfaceDX9.cpp stretches with Direct3D 9's
  bilinear filter (D3DTEXF_LINEAR): the start screen's picture over the whole window, the map view when it is
  stretched (the default when the hardware can stretch), the scaled item pictures. Those pixels come from the
  device's filter in Tibia.exe too. On the GPU the Vulkan engine gives the hardware what a Direct3D 9 driver
  gives it; on the CPU driver they can differ by 1 per channel. Either the light's tolerance extends to the
  bilinear stretches (compare_rendering.py `--tolerance 1`), or the engine filters them in its own integer
  arithmetic (the same on every device, but then exactly like no graphics card's Direct3D 9).

### After the live comparison (November 3, 2026)

The orchestrator ran `compare_rendering.py --engines vulkan` and `--engines vulkan-cpu` (windowed, `--no-repeat`,
recording build\traffic\result-x64\orig-live.jsonl) against Tibia.exe's DirectX 9 frames. RTX 5080: all 26 scenes
identical, the stretched map and start screen included, except the minimap; llvmpipe: the same minimap difference
and the bilinear stretches at most 2 per channel (mean about 0.35). The user decided the open points.

- **The minimap: an Edge copy** (high). The automap's upload (WriteImage below SPRITE_SURFACE) draws the 106x106
  ScreenTexture to AUTOMAP_SURFACE with point sampling and u, v from 0 to 1, so every sample falls on a texel edge.
  Quads.cpp took that for a copy of texel k at pixel k; Tibia.exe shows texel max(k - 1, 0) (the pictures shifted
  by one pixel match but for the separately drawn cross). The hardware's interpolated coordinate lands a hair
  below each edge except at the corner: tests_vulkan, drawing a texture of texel coordinates through that quad,
  shows exactly that mapping on the RTX 5080 through Vulkan when the quad goes to the hardware as Direct3D 9 gives
  it, and texel k on lavapipe. So the rule is the reference's, and the engine applies it on every device as a copy
  (texel k - 1 through a nearest, clamp-to-edge sampler; the clamp gives pixel 0 texel 0): an Edge copy, only for
  point sampling from edge 0. Considered: leaving such quads to the hardware (right on the GPU, wrong on lavapipe).
- **The tolerance: ±2 on the device-filtered pixels only** (the user's decision; high for how it is applied). The
  user allowed up to 2 per channel for the bilinear stretches and the light, everything else exact. Asked to apply
  it as narrowly as possible, the engine marks those pixels itself instead of the comparison guessing regions:
  TFilterMasks (src/vulkan/FilterMask.cpp) follows every fill, upload and quad with a byte per pixel and image
  (filtered, transparent), kept only while patcap.dll watches; the captured frame's mask goes to patcap
  (PatcapVulkanMask, `<capture>.mask`) and compare_rendering.py (`--region-tolerance`, default 2) applies the
  tolerance on it alone, on every device, exact elsewhere. A first version kept rectangles per image; an exact copy
  could not remove them (its transparent texels leave the target), so the dialogs over the stretched start screen
  would have got the tolerance too. Considered: `--tolerance 2` for vulkan-cpu only (wider: every pixel of the
  frame); per-scene regions in the script (would have to know the client's layout). Wider than needed in one place:
  the transparent texels of a scaled item picture after ApplyColorKey (known only on the GPU) count as filtered.
- **Engine 2 is the Vulkan engine** (the user's decision). In the x64 client, Tibia.cfg's engine 2 (and the
  "DirectX 9" button) runs the Vulkan engine with no environment variable; `PATELLA_VULKAN=off` (0, no, dx9) gives
  engine 2 back to surfaceDX9.cpp for comparisons until issue #20 removes it, and `gpu`, `gpuN`, `cpu` choose the
  device until plan 6.3's choice comes. compare_rendering.py gives the x64 Patella.exe `PATELLA_VULKAN=off` for
  `--engines dx9` (and the other original engines), the device for `vulkan` and `vulkan-cpu`. The oracle is
  untouched: Surface.cpp's macros are still surfaceDX9.cpp's calls there (pecompare against main 193dee3: 0
  unexpected differences).
- **Not checked yet** (form). No client window has been opened: the live comparison against Tibia.exe on the GPU
  and on the CPU driver (`compare_rendering.py --engines vulkan,vulkan-cpu`) is the orchestrator's (notes/vulkan.md,
  "Checks"). Risks it may show: Vulkan presenting to a window that IsOpenGLWorking gave an OpenGL pixel format at
  start-up (SurfaceInitInstance tests OpenGL after the engine 2 test), and the bilinear stretches above.

## The golden corpus (Patella-Modern issue #24)

Settled by Claude Opus 5.5, November 3, 2026. notes/reconstruction.md, "Checking", describes the use;
tests/golden/ (golden.h has the format), cmake/OracleX86.cmake (golden_corpus), tests/golden/CMakeLists.txt.

- **One test source, two sides** (high). A ported test is compiled into the oracle's test program (GOLDEN_ORACLE,
  the original called as before) and into the x64 runner, where `ORIGINAL(...)` and `#if GOLDEN_ORACLE` blocks
  are empty and `golden::Output` reads the original's value back. The CHECK statements are the same code, so the
  runner makes exactly the oracle's checks, which the per-test counts in the corpus confirm. Considered: a
  recorder at the CHECK level (a CHECK sees only a boolean: the runner could not recompute our side against
  it) and a separate runner copy of each test (two copies that drift apart). Inputs are regenerated by the same
  random numbers (each test starts at the seed the recording saved) and compared with the recorded ones.
- **By value** (high; plan 4.3). Records are numbers, arrays of numbers and byte strings with a field name and a
  role; no structure is stored as memory. Where the original's object matters, its fields are recorded: the
  Regexp test records the original's program (its size, regstart, reganch, regmlen, regmust as an offset, the
  program bytes) and regexec's startp/endp as offsets into the string, and the runner rebuilds the program in
  its own layout, so the cross runs (ours on the original's program) and the allocation-size check (_msize less
  sizeof(regexp), the program's size on either side) run unchanged. One place reads uninitialised memory:
  Buffers' PutBytes takes 7 bytes of texts that may be shorter, so the whole text buffer is recorded (an
  output: the oracle's bytes) and the runner uses it.
- **Staleness** (high). The format version and the SHA-256 of Tibia.exe are in every file and manifest; the runner
  accepts only format GOLDEN_FORMAT and the 7.72 client's hash, and only a corpus from a complete run without
  failures (the manifest is written after the last test; recording refuses DIFFTEST_ONLY and DIFFTEST_SKIP).
  Each value's name and role, each input, and each test's check and value counts are compared, so a changed
  test is reported, not misread. The test sources are not fingerprinted on purpose: after M6 the corpus cannot be
  recorded again, and the runner's tests must be able to change (API changes on our side) as long as they ask
  for the same values.
- **Where it lives** (high). build/golden by default (PATELLA_GOLDEN_DIR in both presets; the runner takes a path
  argument), ignored with build/. The corpus is derived from CipSoft's code: never committed, pushed or uploaded.
  17 MB now, of which Regexp 7.2 MB and Buffers 5.0 MB.
- **CI** (high). Hosted CI builds golden_runner (windows-x64, MSVC) and runs the CTest test `golden`, which finds
  no corpus, prints why and exits 77: CTest reports it as skipped, not failed. The self-hosted workflow's job
  `golden` records the corpus in its checkout and replays it with PATELLA_GOLDEN_REQUIRE=ON (no corpus fails
  there); the checkout's git clean removes it before the next run. Considered: the runner in the self-hosted
  workflow only (hosted CI would not even build it, and its build breaks would show up late).
- **Scope** (needs the user). Ported: difftest's XTEA, Characters, Colors, Strings, FormatNumber, Buffers,
  Skins (moved from difftest.cpp and statustest.cpp) and Regexp (from regexptest.cpp), sysinctest's Sysinc: the
  tests whose our side is in patella_core or portable (sysinc.cpp compiled against src/compat/mfc_shim.h). Not
  ported: difftest's other 28 tests (16,475,234 checks) and communicationtest (1,134,513): their our side is the
  client's GUI, game and engine code with the difftest's fakes and patches on both sides, and the original's
  side reads and writes its objects at the 32-bit offsets. Each needs the same treatment test by test (the
  original's object state recorded by field), which is the bulk of the work the plan gives until M6. The runner
  lists them with their counts in every run. The Linux runner is later (plan M2: x64 Windows first): the runner's
  CMake returns on anything but MSVC x64.

## The original engines removed from windows-x64 (Patella-Modern issue #20)

Settled by Claude Opus 5.5, November 3, 2026 (MSVC 14.51, CMake 4.1.2). cmake/WindowsX64.cmake, cmake/Vulkan.cmake
(comments), src/Surface.cpp, src/Control.cpp, src/vulkan/SurfaceVulkan.h and .cpp, tools/compare_rendering.py,
.github/workflows/self-hosted.yml (comment). notes/vulkan.md, "Choosing the engine", describes the result.

- **Out of the x64 build** (high). windows-x64's glob drops SurfaceDX.cpp, SurfaceGL.cpp and surfaceDX9.cpp; ddraw,
  dxguid, opengl32 and glu32 are not linked; the Vulkan engine is no longer an option (`PATELLA_VULKAN` the CMake
  option is gone; the definition stays, set for the client). Patella.exe and PatellaTest.exe import 15 DLLs
  (none of ddraw.dll, d3d9.dll, opengl32.dll, glu32.dll; no string naming them either). The files stay in src/ for
  the oracle, whose Patella.exe is unchanged (pecompare against main d624a34 at the same path: 0 unexpected
  differences; macrocheck: 2,108 calls, 0 differences; the reconstructed files' changes are x64-only branches
  behind `#if defined(PATELLA_VULKAN)`, the oracle's text in them unchanged).
- **curEngine is always 2 in the x64 client** (high). graphics.cpp sets the video cache's colour keys only for
  engines 1 and 2, the FPS line and error.txt name the engine: the Vulkan engine reproduces DirectX 9's frames, so
  every such question gets DirectX 9's answer, as Tibia.exe's on this machine (error.txt's "Graphic Engine: 2",
  "DX9"). Considered: curEngine following Tibia.cfg's number with the Vulkan surface for every number (engine 0
  would draw sprites without their colour keys).
- **Tibia.cfg's Engine keeps the original's number** (high; plan 6.3 "Tibia.cfg keeps an engine number in the
  original's range, so it stays valid for Tibia.exe"). A number 0-2 from Tibia.cfg or the command line's `engine
  N` is stored as it is, none or one out of range becomes 2; the advanced options' buttons store their number
  (Control.cpp ChangeEngine), as the original stores it after a switch. That is exactly what Tibia.exe writes on a
  machine where its engines work: compare_files.py's options session clicks the OpenGL button, and both clients
  then write `Engine = 1`. Considered: always 2 (compare_files.py would differ, and a Tibia.cfg shared with
  Tibia.exe would lose the user's choice there); the buttons doing nothing (the same difference).
- **The dialog stays as it is, for now** (needs the user; below). Plan 6.3 replaces the engine buttons with a
  device choice stored in the fork's settings file (D12). The settings file does not exist yet, the skin has three
  button places where 6.3 asks for "Automatic", one entry per GPU and "CPU (compatibility)", and the dialog's
  texts and help texts are not given; all of it is user-visible. Until the user decides, the advanced options look
  as Tibia.exe's on a machine with Direct3D 9 and OpenGL (AvailableEngines 3 when the Vulkan test passes, so
  compare_rendering.py's advanced-options scene stays identical), "DirectX9" is selected and every button draws
  with Vulkan. Considered: disabling DirectX5 and OpenGL (a visible difference that is neither the original nor
  6.3).
- **No fallback without Vulkan** (high, for the code; "Found, not done" for shipping). The original falls back to
  DirectX 5; the x64 client has no other engine, so without a Vulkan loader (vulkan-1.dll) or device the start
  fails with PROGRAM_ERROR 12. The shipped CPU driver covers machines without a Vulkan GPU, not machines without
  any Vulkan driver (the loader comes with every GPU driver that has Vulkan; the plan names no loader to ship).
- **The display modes from EnumDisplaySettings** (high on this machine). DirectDraw 4 was still used by the Vulkan
  engine for the mode list, the desktop's depth and the stretch capabilities. DirectDraw's EnumDisplayModes
  without DDEDM_REFRESHRATES gives each size and depth once in the order the display lists them;
  EnumDisplaySettings lists every refresh rate. A probe that asks both (build\logs\issue-20, no window) gives
  the same 24 modes in the same order here once the refresh rates are folded; the client's callback set the
  frequency to 0 always (it tests DDSD_REFRESHRATE among the pixel format's flags). CanStretch is true: Vulkan
  stretches on every device, and DirectDraw reports the four flags here. Risk: a display whose DirectDraw lists
  emulated 16-bit modes or another order would show another list (only on other machines).
- **PATELLA_VULKAN chooses only the device** (form). `off`, `0`, `no`, `dx9` gave engine 2 back to Direct3D 9;
  they are automatic now. The variable stays until 6.3's device choice and D12's settings file replace it.
- **compare_rendering.py** (high). The original engines need the oracle's Patella.exe; for an x64 one the script
  refuses them (not with --compare-only), and the default engines follow --patella (dx5,dx9,gl for the oracle's,
  vulkan,vulkan-cpu for an x64 one), so the self-hosted workflow's command runs the Vulkan engine on both devices.
  patcap.c is unchanged: Tibia.exe's runs need its DirectDraw, Direct3D 9 and OpenGL hooks; in the x64 client it
  finds seven of its hooks (the clock, the timer, the cursor, PeekMessageA, GetProcAddress) and the engine's own
  calls. Offline: --compare-only on issue #19's GPU and CPU results gives "all scenes identical" with the x64
  Patella.exe of this branch.
- **Not checked yet** (form). No client window was opened: compare_traffic.py, compare_files.py and
  compare_rendering.py (vulkan, vulkan-cpu) on the x64 client are the orchestrator's live check.

## The graphics device choice and Patella.ini (Patella-Modern issue #31)

Settled by Claude Opus 5.5, November 3, 2026, after the user's decisions of the same day (layout: three buttons
in the skin's three places, "Automatic", "GPU" cycling through the GPUs and showing the chosen one's name, "CPU
(compatibility)"; applied at once as the original's engine change, in fullscreen as the original; `Patella.ini`,
plain `key=value`, a parser of our own, next to Tibia.cfg, only the device key for now). notes/vulkan.md,
"Choosing the device", describes the result; src/settings/, src/vulkan/DeviceChoice.*, src/vulkan/DeviceSettings.*,
src/vulkan/Renderer.* (ListGpus), src/Dialogs.cpp and .h, src/Control.cpp, src/Surface.cpp (x64 branches),
tools/compare_files.py, tools/compare_rendering.py, tools/client_state.py.

- **The buttons keep the original's places, events and Tibia.cfg numbers** (high). "Automatic" in DirectX5's place
  (event 800, Engine 0), "GPU" in DirectX9's (801, 2), "CPU" in OpenGL's (802, 1), in the user's order from the
  left. Each click stores its place's number, as the original's button there stores it after a switch (a failed
  switch stores nothing, as there), so Tibia.cfg is what Tibia.exe writes after the same clicks, for any sequence
  of clicks on any machine where Tibia.exe's engines work, not only compare_files.py's one click on OpenGL's
  place (both write `Engine = 1`). Considered: numbers chosen for meaning (Automatic and GPU 2, Tibia.exe's own
  choice with Direct3D 9; CPU 1): Tibia.exe would start in DirectX 9 after "Automatic" instead of DirectX 5, but a
  scenario clicking the first place would differ from Tibia.exe's file; Tibia.cfg's number not changed by the
  buttons at all (compare_files.py's options scenario would differ: 2 against 1). The cost: with a Tibia.cfg
  shared with Tibia.exe, "Automatic" makes Tibia.exe start with DirectX 5. The member names (DirectX5Button,
  DirectX9Button, OpenGLButton) stay: compare_files.py reads OpenGLButton through the PDB.
- **"CPU", not "CPU (compatibility)", on the button; the device's name above the buttons** (form; the user's texts
  as far as the skin allows). The buttons are 58 pixels wide (skin 16) and their font draws "Automatic" in 45,
  "CPU (compatibility)" in 87 and a GPU's name ("NVIDIA GeForce RTX 5080") in 115 pixels (measured from
  Tibia.pic's font 3), so neither fits; a longer text would run over the next button (TTextButton draws it
  unclipped). The CPU button's help text begins "CPU (compatibility)". The chosen GPU's name (the user's "showing
  the chosen GPU's name") goes into the line above the buttons, the original's "Select graphics engine:" (font 4),
  as `Device: <name>` over the dialog's width (200 pixels; `Device: NVIDIA GeForce RTX 5080` is exactly 200), with
  TLabel's own shortening ("...") and the whole text as its help text when longer. It names the device in use for
  every choice (Automatic's and CPU's too). Considered: a line of the small font in the 16-pixel gap between the
  buttons and the light check box (not a place the skin has); the name only in the GPU button's help text (not
  shown unless the pointer rests there). Help texts: "Use the best graphics device: a GPU, else the CPU", "Use the
  graphics card (GPU); with several, each click takes the next", "CPU (compatibility): draw without the graphics
  card".
- **GPU: the best first, then the next round** (high). From Automatic or CPU the GPU button chooses `gpu`, the best
  GPU (the device Automatic takes), stored without an index so that it survives a change of the loader's order;
  every further click on a machine with several GPUs takes the next of the list (`gpuN`), round; with one GPU it
  changes nothing. Considered: starting at gpu0 (on a laptop whose integrated GPU comes first, the first click
  would leave the faster GPU).
- **The GPU list does not depend on the window** (high). `gpuN` counted in vk-bootstrap's list with the window's
  surface as a requirement, which only a device with a window can ask; the dialog's list and Patella.ini's index
  must be the same list at start-up, in the dialog and with or without a window (tests_vulkan, headless). So
  ListGpus and an indexed device both select without the surface but with VK_KHR_swapchain required, and an
  indexed device's presentation support is checked when it is created (vkGetPhysicalDeviceSurfaceSupportKHR, as
  before); the swapchain is built with the window's surface given explicitly. Automatic and `gpu` select as before
  (with the surface). The list is read once at start-up, before any device exists (no GPU comes or goes while
  the client runs, in practice). (Corrected in issue-31-fix: this point first said that vk-bootstrap keeps the
  functions of the *last* instance; it keeps those of the *first*, which is what crashed the start; below.)
- **Applied as the original's switch** (high; the user's decision). ChangeEngine's x64 branch does what the
  original's does with a device instead of an engine: ExitSurfaces, SurfaceExitInstance, TestEngine on a new
  surface, the old choice back when it fails, InitSurfaces; the next CheckSurface creates the surfaces and resets
  the video cache. In fullscreen mode the dialog disables the buttons, as the original's. No TASSERT, TERROR or
  TEND in the new code (form; macrocheck).
- **Patella.ini: written when a click changes the choice** (high). Not with SavePreferences (Preferences.cpp is
  reconstructed code, and Tibia.cfg's saving must stay the original's); the original's engine switch takes effect
  at once too and is not undone by Cancel. Only the `Device` line changes; every other line and the file's line
  ends stay (Settings.h), so a later D12 key, a comment or a key from a newer client survive. A new file gets one
  comment line and CRLF (Tibia.cfg's line ends). Read once at start-up from the working directory, as Tibia.cfg.
  Considered: writing at the exit (a crash would lose the choice), a fixed file rewritten whole (unknown keys lost).
- **Parser rules** (form). `key=value` split at the first `=`; spaces and tabs around both dropped; `#` or `;`
  first: a comment; keys compared without regard to ASCII case; the first line with a key counts; a line without
  a key is kept and never read; a UTF-8 byte order mark is kept; the line end is the first line's (CRLF or LF);
  Set refuses what would not read back the same (an empty key, `=`, a comment mark or line ends in the key, line
  ends, NUL or edge spaces in the value); a file over 1 MiB is not read. In patella_core (portable, tests_core on
  every preset); the x64 client compiles the source itself (the static CRT), as it does the other core sources.
- **PATELLA_VULKAN: the comparisons' override** (high). It stays for compare_rendering.py (gpu, cpu),
  testmode_check.py (cpu) and the tests: set and not empty, it is the start-up choice instead of Patella.ini's and
  is never written; a value it does not know is `auto`, as before (`off`, `dx9`). The dialog still switches and
  writes Patella.ini while it is set. Considered: removing it (the comparisons would have to write Patella.ini into
  every run folder, which compare_files.py then would have to tell from the client's own writing).
- **A stored device that is gone: Automatic** (high). At start-up a choice whose test fails is replaced by
  Automatic for the run and tested again, as the original falls back to another engine; Patella.ini keeps it.
  Without that a Patella.ini copied from another machine (`gpu1`) or a missing CPU driver would end the start with
  PROGRAM_ERROR 12. The command line's `engine N` skips the test, as in the original (a broken choice then fails
  at the first CheckSurface).
- **Enabled buttons** (form). GPU with at least one GPU in the list, CPU with the CPU driver's manifest next to the
  executable, Automatic always, all three disabled in fullscreen. The original's AvailableEngines (set as before)
  no longer decides them, so the command line's `engine N` no longer disables two of them (a device choice has
  nothing to do with the original's engine test).
- **The tier A exception and the comparisons** (high). compare_rendering.py masks, for the Vulkan engines, the
  dialog's top 35 rows (the line and the buttons) in both runs of the advanced-options scene; the rest stays exact
  against Tibia.exe, and the oracle's engines keep the original dialog unmasked. Considered: an expected-frame
  mechanism (a recorded Patella frame of the area as the reference): it would depend on the device (the name in the
  line, the selected button differ between `vulkan` and `vulkan-cpu`) and belongs to the reference recordings of
  issue #23, which another task is building now. compare_files.py copies the CPU driver next to the x64 client in
  its run folder (otherwise the CPU button is disabled there and the click in OpenGL's place does nothing) and
  reports Patella.ini without comparing it (Tibia.exe writes none). client_state.py names the four skin fields the
  mask reads (raw offsets are refused on the x64 layout).
- **Not built** (the user's scope): vsync (D13), the implicit-layer switch (D12/D16), the other D12 keys.
- **Not checked yet** (form). No client window was opened (another task used the test server and the desktop):
  compare_files.py, compare_rendering.py (vulkan, vulkan-cpu) and compare_traffic.py on the x64 client are the
  orchestrator's live check; the dialog's new look has not been seen.

### The start-up crash (issue-31-fix, November 3, 2026)

Found by the issue #23 agent on main 26ac7b7: Patella.exe and PatellaTest.exe (x64) ended at start-up with
0xC0000409 (fail-fast 10, a Control Flow Guard check), with or without the CPU driver, PATELLA_VULKAN or a
Tibia.cfg; the stack: an access violation in vk-bootstrap's get_present_queue_index, from
PhysicalDeviceSelector::is_device_suitable, select_devices, FindDevices, TRenderer's Open, TSurfaceVulkan::TestEngine.

- **The cause** (high; VkBootstrap.cpp of vk-bootstrap v1.4.363 read, and reproduced). vk-bootstrap keeps the
  instance-level functions it calls itself (the surface queries of the device selection and the swapchain,
  vkDestroySurfaceKHR, the debug messenger's) in one table per process, filled by `init_instance_funcs` from the
  first instance it builds and never again (`if (instance_functions_initialized) return;`). The loader returns
  an extension's function only for an instance with that extension enabled. Before #31 the first instance of the
  client was the start-up test's, with VK_KHR_surface and VK_KHR_win32_surface. Issue #31's device list
  (ListDevices, headless, no surface extension) came first, so the table's vkGetPhysicalDeviceSurfaceSupportKHR
  and the other surface functions were null, and the window's device selection called a null pointer. #23's guess
  (a pointer stale from the destroyed headless instance) is close: the table does come from that instance, but the
  failing pointers were null, not stale; the others are the loader's trampolines, which serve every instance, so
  they keep working after their instance is gone (as they did before #31, when the test's instance was destroyed
  before the engine's). tests_vulkan never saw it: all its devices were headless.
- **The fix: every instance enables the same instance extensions** (high). Renderer.cpp NewInstanceBuilder builds
  every instance of the back end (ListDevices' and Open's): VK_KHR_surface, the platform's surface extension
  (VK_KHR_win32_surface) and VK_EXT_debug_utils, each where the loader offers it, and the one a request names. So
  whichever instance comes first fills the whole table. Considered: resetting vk-bootstrap's table before each
  instance (it is a file-static of VkBootstrap.cpp with no public reset; patching a pinned dependency); listing
  the devices without vk-bootstrap (the list must be exactly vk-bootstrap's select_devices with the same criteria,
  so GpuIndex means the same device); listing them on the engine's own instance (there is no engine instance at
  start-up before the test, and the dialog's list must not depend on one). The same rule closes the same trap for
  the debug messenger: a first instance without VK_EXT_debug_utils would have left its functions null for a later
  PATELLA_VULKAN_VALIDATION=1 instance.
- **The regression test** (high). tests_vulkan "a device for a window after headless instances (the start-up
  order)": the device list and the CPU driver's (headless instances, after the headless tests before it), then a
  renderer on each device (CPU driver, GPU) for a window that is never shown (no WS_VISIBLE, no ShowWindow,
  destroyed at the end), and one frame presented (the swapchain's surface queries). On 26ac7b7 it ends the program
  with 0xC0000409, as the client (build\logs\issue-31\fix-regression-before.log; every other test passes without
  it); with the fix 11 test cases, 88 assertions pass (fix-regression-after.log).
- **Not checked yet** (form). The client's start itself: it always shows its main window (Player.cpp
  ShowWindow), so it was not started here; the orchestrator runs the start-up check.

## Reference recordings (Patella-Modern issue #23)

Settled by Claude Opus 5.5, November 3, 2026. notes/testmode.md, "Reference recordings", describes the result;
src/testmode/ServerReplay.*, InputScript.* (clock manual, advance, cursor, post, save), TestModeWin32.cpp (the
replay's sockets, the quiet passes, the message boxes), src/TestHooks.h and Player.cpp (PATELLA_MESSAGE_BOX),
tests/unit/testmode_replay_test.cpp, tools/reference.py, tools/reference_record.py, tools/proxy.py (vclock), the
forwarding in compare_traffic.py, compare_files.py and compare_rendering.py.

- **The reference runs are Tibia.exe under patcap.dll, replaying a recording** (high). The live run's timing
  belongs to the server and the desktop; the replay with patcap's clock, which moves only by the record step's
  advances, gives every state, frame and written time a defined virtual moment. compare_rendering.py had shown
  that Tibia.exe and Patella under patcap draw the same frames, compare_traffic.py that a replay gives the
  states of the live run's server data. So one reference run per session gives packets, states, frames and
  files at once, and the live server is used only to make the server side once.
- **The script is written by the record step from what it did** (high). The sessions' drivers read their click
  points from Tibia.exe's memory (window tree, skin, map names); written down as the posted messages
  (`post`) and the clock's advances, they need no layout knowledge in the test client and keep the order of
  events exactly. The alternative, hand-written scripts per session, would repeat the drivers' point logic by
  hand and drift from them.
- **The test client replays the server side itself, over loopback** (high). Inside the test build, the driver
  owns the server's sockets on 127.0.0.1 and decides when a packet goes, so it can hold the virtual clock and the
  script until the network has settled (connect, a client packet read, a server packet decrypted, a close
  answered): every packet reaches the client at the same point of the script, without real-time races, and a
  session runs in seconds. The client's own Network.cpp is untouched (no new hook there: the existing RSA block
  and packet hooks give the key and the counts, the connection state the connect); an external Python replay
  (tools/proxy.py) would have needed a handshake with the virtual clock for every packet. Each wait gives up
  after 10 s and says so, so a broken replay cannot hang a run. The format is a line file of our own (no JSON
  parser in C++).
- **The packets' virtual times come from the proxy** (medium). The proxy stores patcap's clock when it sends a
  replayed packet; the clock stands still while the packet is on its way (settles wait for the server to be
  quiet before an advance), so that is the moment Tibia.exe handles it. Measured: every session's packets and
  states match with these times.
- **Quiet passes** (medium). Between two of the record step's messages Tibia.exe runs many idle passes (each
  with ProcessEvents, which opens the dialogs queued by the last event); the test driver gave one per event, and
  the error session's Return came before the character list was open. With a manual clock the driver now waits
  10 idle calls after every event and network action: deterministic (idle calls, not time), and every session
  matches with it.
- **No masks for times, keys or padding** (high; the issue's question). The virtual clock gives rand() the same
  seed, so the XTEA keys, the RSA blocks' random bytes and the padding are compared as they are, and they are
  equal; the wall clock starts at patcap's and both clients run with TZ=UTC0, so error.txt's crash time and
  Default.txt's "Channel saved at" are compared byte for byte. The remaining masks are the recorded ones: the
  measured frame rate (as compare_rendering.py) and issue #31's device choice in the advanced options (frame
  rectangle; the texts of the windows inside it in the state), plus Patella.ini, the x64 client's own file.
- **Message boxes are not shown in a scripted run** (high; "a test program must never open a modal error
  dialog"). PATELLA_MESSAGE_BOX is MessageBoxA outside the test build (Patella.exe unchanged: pecompare). The box's
  modal loop is emulated (messages dispatched, the replay pumped, until 500 ms of quiet), because Tibia.exe saves
  Tibia.cfg in it (the server's close after ShutdownConnection): without it the error session lacked Tibia.cfg.
- **The frozen clock needs settles where real time did the work** (medium). Tibia.exe draws its first frame only
  after the clock moved (every session starts with a settle), the patch-exe client leaves the "Downloading"
  dialog and closes only with time; those sessions advance the clock where compare_files.py slept.
- **Local data** (high). The reference set holds Tibia.exe's frames, files, packets and, for patch-exe, a copy of
  the patched Tibia.exe: CipSoft-derived, gitignored under build\, never committed or uploaded. Staleness: the
  format and the SHA-256 of Tibia.exe and the data files in meta.json.
- **The old live modes stay** (high). They are the way to compare two live clients and to make new recordings;
  the record/compare split adds `record` and `compare` as first words of the three tools.

## The test build in CI and the cold start (Patella-Modern issue #25)

Settled by Claude Opus 5.5, November 3, 2026. cmake/WindowsX64.cmake (PATELLA_CLIENT_COMPILE_ONLY,
PatellaTest_compile), .github/workflows/hosted.yml (job windows-test-build), .github/workflows/self-hosted.yml
(testmode_check.py in the comparisons job), tools/testmode_check.py; notes/testmode.md, "The cold start (issue
#25)".

- **Hosted CI compiles the test build as an object library, without resources and without a link** (high).
  The client's only tie to Tibia.exe at build time is the resource script, whose pictures are extracted from it
  (cmake/Resources.cmake); hosted runners have no CipSoft file (plan 4.7). Placeholder pictures would let it
  link, but they would be a second, made-up resource set to keep in step, and a linked PatellaTest.exe without
  the real resources is nothing anyone should run; the issue asks for the compile. So
  PATELLA_CLIENT_COMPILE_ONLY builds `PatellaTest_compile` (OBJECT) from PatellaTest.exe's C and C++ sources,
  with the settings of one shared function (patella_client_compile_settings), so the two cannot drift: the
  compiler flags of all 44 compiles are the same as PatellaTest.exe's, and Patella.exe's and PatellaTest.exe's
  are unchanged against main (compared in build-tree ninja files). Patella.exe's own preprocessing (without
  PATELLA_TEST_MODE) is not compiled there; the self-hosted windows-x64 job builds both.
- **A job of its own** (medium). The existing windows-msvc job stays as it was (PATELLA_CLIENT=OFF, unit tests);
  the new job fetches the pinned Vulkan dependencies (cmake/Vulkan.cmake, as every client configure does) and
  builds only the target, so tests_vulkan is neither built nor run on a runner without a GPU.
- **Wait for "Server Online!", and retry once** (high; the issue offers either). The server opens its ports
  before it loads the world (server.log: "Starting Login Server", "Initializing connections & network", then the
  map sectors, then "Server Online!"), so listening ports do not mean a game server that answers. Waiting for
  that line removes the cause; but whether the server's output reaches server.log as it is written is the
  server's business (its CRT buffers a redirected stdout unless it flushes), so the wait is skipped when the
  early line is not in the file yet, and the retry (a run whose game logins got no server packet at all) covers
  what the wait cannot see. A game login that the server answers is never retried: a real failure after the
  login still fails the check. An active probe of the game port would need a game login, which changes the
  player files and the server's state the runs compare.

## NULL to nullptr outside the error macros (Patella-Modern issue #32)

Settled by Claude Opus 5.5, November 3, 2026. The first change of M3 (plan section 7): `modernize-use-nullptr`
is clean on src/ outside the arguments of TASSERT*, TERROR* and TEND*.

- **clang-tidy through tools/tidy.py on every configuration that compiles src/** (high). `python -I tools/tidy.py
  -p <dir> --checks=-*,modernize-use-nullptr --fix --all` on compile_commands.json files that hold one target's
  Release commands each: windows-x64's Patella, PatellaTest (PATELLA_TEST_MODE), patella_core,
  patella_testmode and patella_render_vulkan, and oracle-x86's Patella, mfc_shim_check, difftest,
  communicationtest, sysinctest and vlongtest (`--extra-arg=-m32`; the oracle preset does not export compile
  commands, so it was configured once with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`). One run per target, because
  tidy.py takes each file once and the targets' definitions select different code (the x86 engines, the test
  hooks, the MFC shim). 1,223 replacements: 1,222 `NULL`, one `0` (vlong.h's flex_unit). tidy.py reverted
  every fix inside a macro argument; the 262 `NULL`s there wait for M5 (macrocheck: the 2,108 calls unchanged).
- **The 40 the check does not report, by hand** (high). modernize-use-nullptr leaves a null constant alone when
  its target type is a template parameter: Container.h's `vector<T *>` with T a pointer, its `init` argument
  (`vector<TTalkFilter *> IgnoreList(0, 9, 10, NULL)`) and the `T &` that operator() returns (`List(i) !=
  NULL`, `Timers(i) = NULL`; Talk.cpp, GUI.cpp, DialogElements.cpp and .h); and a variadic argument
  (Player.cpp's `_execl("patch.exe", "patch.exe", NULL)`, a null pointer by _execl's contract). They are the same
  replacement, made at the positions of the remaining `NULL` tokens outside macro arguments, comments and
  strings, so the issue's title holds too: no `NULL` is left in src/ outside those. The images show that it is
  the same code: on x86 a null pointer and the int 0 are the same 32 bits, and on x64 MSVC zeroes the variadic
  slot alike (pecompare of the x64 Patella.exe: `.text` identical).
- **Comments and strings keep NULL** (high). The 41 in comments (`TCallback(NULL, 0)` as the original's
  initialiser was read, notes on the binary) describe the original; none is in a string.
- **clang-format realigns the trailing comments next to a changed line** (high). clang-tidy formats its fixes
  with the repository's .clang-format; after the hand replacements `clang-format -i` on GUI.h, GUI.cpp and
  Talk.cpp aligned the comment columns again. Whitespace only, no macro argument touched.
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built
  from main 3249d6b and from the branch at the same path, differ only in the link time stamps and the PDB
  record (pecompare); the difftest counts are unchanged; the clang-tidy baseline lost its ten use-nullptr
  entries (patella_core's Skins.cpp, TRSA.cpp, TXTEA.cpp, vlong.cpp, vlong.h).

## Patella.exe's sources in the hosted CI (Patella-Modern issue #46)

Settled by Claude Opus 5.5, November 3, 2026. cmake/WindowsX64.cmake (Patella_compile), .github/workflows/hosted.yml
(job windows-test-build).

- **A second object library, Patella_compile, beside PatellaTest_compile** (high). Issue #25 compiled only the
  test build's preprocessing; Patella.exe's (the same sources without PATELLA_TEST_MODE, so the other branches of
  src/TestHooks.h, Creatures.cpp, Inventory.cpp, Map.cpp, Player.cpp/.h and Status.cpp) was compiled only on
  the self-hosted runner. PATELLA_CLIENT_COMPILE_ONLY now also builds `Patella_compile` (OBJECT) from Patella.exe's C and C++ sources (no resource script), with the
  same shared function patella_client_compile_settings, so its flags cannot drift from Patella.exe's. Proof: the
  ninja files of the windows-x64 tree and of a compile-only tree give the same rule, FLAGS, DEFINES and INCLUDES
  for all 36 sources of Patella.exe (Release and Debug) and all 44 of PatellaTest.exe; Patella.exe's and
  PatellaTest.exe's compile variables are unchanged against main 6a20d32.
- **In the same job, as a step of its own** (medium). The configure (and its Vulkan download) is shared; a
  separate build step for each object library shows in the log which of the two failed. Release only, as for
  PatellaTest_compile: the Debug flags differ only in optimisation and the debug CRT, and the self-hosted
  windows-x64 job builds both configurations.

## override on every overrider, and no final yet (Patella-Modern issue #33)

Settled by Claude Opus 5.5, November 3, 2026. The second change of M3 (plan section 7): `modernize-use-override`
is clean on src/.

- **clang-tidy through tools/tidy.py on every configuration that compiles src/** (high). As in issue #32: `python
  -I tools/tidy.py -p <dir> --checks=-*,modernize-use-override --fix --all` once per target, on compile_commands.json
  files holding one target's Release commands each (oracle-x86's Patella, mfc_shim_check, difftest,
  communicationtest, sysinctest and vlongtest with `--extra-arg=-m32`; windows-x64's Patella, PatellaTest,
  patella_core, patella_testmode and patella_render_vulkan). 667 declarations in 17 files got `override`: 21
  that said `virtual` (the keyword goes, as the check does it), and 646 overriders that said neither (the
  original's style: GUI.h's subclasses repeat TComponent's functions without `virtual`). Destructors of derived
  classes are among them (`~TTextWindow() override`). The plan's "167 `virtual`" counted the keyword only. Not
  one fix fell inside a TASSERT/TERROR/TEND argument (macrocheck: the 2,108 calls unchanged). The second run of
  every target reports no use-override finding; MainFrm.cpp's two clang-diagnostic errors (the MFC message
  map's ON_MESSAGE, also on main) did not stop the fixes of MainFrm.h, which the other translation units reach.
- **No case outside the check's view** (high). Unlike use-nullptr there is no template or variadic blind spot:
  every function that overrides is a member of a class that some target compiles, and the remaining `virtual`s
  of src/ are the first declarations (TComponent, TTalkBuffer, TSurface, the subclasses' own new vtable slots
  such as ConsoleWindow.h's FormatLines at +0x84, the MFC shim's base classes, IClock, the exception and
  cipher classes' destructors).
- **clang-format realigns the trailing comments next to a changed line** (high). `clang-format -i` on nine
  headers aligned the comment columns again: 59 lines, whitespace only (checked with the whitespace removed).
- **No `final` in this change** (high). The issue asked for `final` where nothing derives; 107 classes of src/
  with an override have no subclass in src/ or tests/. Marking them `final` lets the compiler call their virtual
  functions directly through a pointer of the class's own type, and it does: built at the same path, the oracle
  Patella.exe's `.text` shrinks by 0x400 bytes and the x64 Patella.exe's by 0x1400, with 208,590 and 257,090
  differing byte runs (pecompare against the override-only build). The oracle must stay the reconstruction of
  Tibia.exe's code, so `final` waits until the oracle retires (M6), where a devirtualized call changes nothing
  that is still compared; it then goes in as its own change, with the leaf list measured again.
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built
  from main 6a20d32 and from the branch at the same path, differ only in the link time stamps and the PDB record
  (pecompare: `.text` identical); the difftest counts are unchanged; the clang-tidy baseline did not hold a
  use-override entry and is unchanged (21).

## using instead of typedef; uint8/16/32 as the std types (Patella-Modern issue #35)

Settled by Claude Opus 5.5, November 3, 2026. The third change of M3 (plan section 7): `modernize-use-using` is
clean on src/ (outside src/Extern), and common.h's `uint8`, `uint16` and `uint32` are aliases of
`std::uint8_t`, `std::uint16_t` and `std::uint32_t`.

- **clang-tidy through tools/tidy.py on every configuration that compiles src/** (high). As in issues #32 and
  #33: `python -I tools/tidy.py -p <dir> --checks=-*,modernize-use-using --fix --all` once per target, on
  compile_commands.json files holding one target's Release commands each (oracle-x86's Patella,
  mfc_shim_check, difftest, communicationtest, sysinctest and vlongtest with `--extra-arg=-m32`; windows-x64's
  Patella, PatellaTest, patella_core, patella_testmode and patella_render_vulkan). Six typedefs: ConsoleWindow.cpp's
  `Word`, surfaceDX9.cpp's `TDirect3DCreate9` (oracle only) and SurfaceVulkan.h's four observer function
  pointers (x64 only). The second run of every target reports no use-using finding (MainFrm.cpp's two
  clang-diagnostic errors, also on main, aside). None was inside a TASSERT/TERROR/TEND argument (macrocheck:
  the 2,108 calls unchanged).
- **common.h's three by hand, as the std types** (high). The plan's "5 typedefs" in common.h were three
  (`uint8`, `uint16`, `uint32`; src/ had nine in all). They become `using uint8 = std::uint8_t;` and so on, with
  `<cstdint>` and `<type_traits>` included, each followed by a static_assert of its size and of its type.
- **Same type, not only same size** (high). An alias whose type changed (`unsigned long` for `unsigned int`)
  would change mangled names, overloads, template instantiations and the PDB's type names. It does not here:
  `std::uint8_t`, `std::uint16_t` and `std::uint32_t` are `unsigned char`, `unsigned short` and `unsigned int`
  in MSVC's stdint.h (x86 and x64), mingw-w64's, and glibc's (GCC and Clang on Linux, LP64: only `uint64_t` is
  `unsigned long` there). The static_asserts check exactly that (`std::is_same_v<uint32, unsigned int>`) on
  every preset, oracle-x86 included, so a library where it differs fails the build instead of changing code.
  No tool reads these names from a PDB: tools/client_state.py looks up classes, `int` and template instances
  of classes; the test mode and the comparison tools name no integer typedef.
- **.clang-tidy enables modernize-use-using** (medium). The check is clean, so adding it costs no baseline
  entry (still 21 findings, 0 new), and the hosted CI's baseline check fails a new `typedef` in src/.
  tests/ and tools/ (difftest's function pointer typedefs, patcap.c) are outside the issue and outside
  tidy.py's `--all`; they stay as they are.
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built
  from main c57b39c and from the branch at the same path, differ only in the link time stamps and the PDB record
  (pecompare: `.text`, `.data` and the rest identical); the difftest counts are unchanged; the clang-tidy
  baseline is unchanged (21).

## The <c...> forms of the C headers; each file includes what it uses (Patella-Modern issue #40)

Settled by Claude Opus 5.5, November 3, 2026. The fourth change of M3 (plan section 7), in two commits that can be
reviewed and reverted alone: first `modernize-deprecated-headers`, then the include check, which the user chose
to be clang-tidy's `misc-include-cleaner` through tools/tidy.py (no include-what-you-use: no new dependency).

- **modernize-deprecated-headers through tools/tidy.py on every configuration that compiles src/** (high). As in
  issues #32, #33 and #35: `python -I tools/tidy.py -p <dir> --checks=-*,modernize-deprecated-headers --fix --all`
  once per target, on compile_commands.json files holding one target's Release commands each (oracle-x86's
  Patella, mfc_shim_check, difftest, communicationtest, sysinctest and vlongtest with `--extra-arg=-m32`;
  windows-x64's Patella, PatellaTest, patella_core, patella_testmode and patella_render_vulkan). 19 includes in
  12 files: common.h's `<string.h>`, `<stdio.h>`, `<stdlib.h>`, ContentWindows.h's `<limits.h>`, Talk.h's
  `<time.h>`, and ConsoleWindow.cpp, Creatures.cpp, DialogElements.cpp, Dialogs.cpp, MainWindow.cpp, Map.cpp,
  MapWindow.cpp, Player.cpp and Talk.cpp. The second run of every target reports none (MainFrm.cpp's two
  clang-diagnostic errors, the MFC message map's ON_MESSAGE, also on main, aside). No fix was inside a
  TASSERT/TERROR/TEND argument (macrocheck: the 2,108 calls unchanged).
- **The headers too: CheckHeaderFile** (high). The check looks only at the main file's own includes unless
  `modernize-deprecated-headers.CheckHeaderFile` is true; without it the first run missed common.h,
  ContentWindows.h and Talk.h. .clang-tidy enables the check with that option, so the hosted CI's baseline check
  fails a new `<string.h>` in src/ (outside src/Extern); the check is clean, so the baseline keeps its 21 entries.
- **Names stay unqualified** (high). `<cstring>` and the others declare `std::strlen` and so on; the code calls
  `strlen`, `sprintf`, `abs`. MSVC's, mingw-w64's and libstdc++'s `<c...>` headers include the C header and so
  declare the global names as well, which is all the code needs; qualifying the calls (`std::`) is not part of
  this issue (it would touch macro arguments too, which wait for M5).
- **No overload changes** (high). `<cmath>` instead of `<math.h>` (Map.cpp, MapWindow.cpp) can make float
  overloads visible: MapWindow.cpp's `floor(ViewWidth * (11.0f / 15.0f))` takes a float, and the UCRT's
  `<math.h>` declares only `double floor(double)`. They were visible before too: common.h's `<string>` reaches
  `<cmath>` through `<istream>` and `<xlocnum>` before either file's own include (cl /showIncludes on main's
  MapWindow.cpp, oracle-x86), so every call resolves as before; `abs` is called with ints only, `sqrt` with
  doubles. The images confirm it.
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built from
  main e089936 and from the commit at the same path, differ only in the link time stamps and the PDB record
  (pecompare: `.text`, `.data` and the rest identical).
- **misc-include-cleaner: the translation units of every target** (high). The second commit. The check reports,
  for the main file of each compile command, every symbol whose header the file does not include itself
  ("no header providing X is directly included") and every include it uses nothing from. Run once per target
  as above (`--checks=-*,misc-include-cleaner`), first without a configuration: 5,146 findings, nearly all
  for the Windows SDK's internal headers (minwindef.h, winuser.h, basetsd.h, corecrt_io.h) and MFC's (afxstr.h).
  After the configuration below: 196 includes added in 47 files and two removed, then 0 findings on all 11
  targets, inside the TASSERT/TERROR/TEND arguments too (MainFrm.cpp's two clang-diagnostic errors aside: in
  the targets with real MFC the message map does not parse, so MainFrm.cpp is checked by mfc_shim_check's
  commands, where it compiles).
- **The fixes placed by hand, not by `--fix`** (high). clang-tidy's own insertions (tried, then reverted) put
  includes at the top of the include block: before `#define DIRECT3D_VERSION 0x0700` (SurfaceDX.cpp, which
  then failed to compile on d3dvec.inl), before `#define GLH_EXT_SINGLE_FILE` (SurfaceGL.cpp), before
  common.h (surfaceDX9.cpp), outside the `#if defined(PATELLA_VULKAN)` blocks (Surface.cpp), and each target
  removed what another target needs (vulkan/DeviceChoice.h, used only by x64 code). So the fixes of every
  target were collected (`--fix` in a scratch state, the diff recorded, the state restored), merged per file
  with the targets that want each include, and written by a script: every new include after the file's
  existing ones, so it comes after the include that brought it in before (no header is included earlier than
  before, so no macro such as `_CRT_SECURE_NO_WARNINGS`, `DIRECT3D_VERSION`, `GLH_EXT_SINGLE_FILE` or
  `WIN32_LEAN_AND_MEAN` changes); an include only x64 code uses goes into that code's `#if defined(PATELLA_VULKAN)`
  block (DeviceChoice.h in Control.cpp, Dialogs.cpp and Surface.cpp). cl /showIncludes on every changed file
  (x64 Patella's commands, against the first commit): the same headers in the same first-inclusion order,
  except the new `<ostream>` (below) and DialogWindow.cpp's Utils.h.
- **IgnoreHeaders: the platform** (high). `misc-include-cleaner.IgnoreHeaders` in .clang-tidy names the Windows
  SDK and the UCRT (`Windows Kits`), MFC (`atlmfc`) and the MFC shim that stands for it in mfc_shim_check
  (`compat/mfc_shim.h`). Their symbols are declared in internal headers that the SDK and MFC mean to be reached
  through windows.h, io.h and afxwin.h (common.h includes MFC first, before anything of Windows, which MFC
  requires); include-cleaner knows no umbrella headers for them, so it would ask for winuser.h or afxstr.h. The
  standard library is checked by its names (`<cstring>` for strcpy, `<ostream>` for std::endl), as is the
  Vulkan SDK (`<vulkan/vulkan_core.h>` next to volk.h in Renderer.cpp and SurfaceVulkan.cpp).
- **Pragmas where the include is the point** (high). `// IWYU pragma: keep` on the includes that are there for
  the image, not for a symbol: Text.h in the seven objects that read none of its texts but carry their own copy
  of the table (ContextMenus, Creatures, DialogElements, DialogWindow, GUI, Help, MainFrm; Text.h's list) and
  Status.h in GUI.cpp (GUI.obj's LinkPattern copy, 0x5c1704); removing them changes .data. `<typeinfo>` in
  ClientStateWin32.cpp too: `typeid` needs it, and include-cleaner does not see that use. `// IWYU pragma:
  export` on common.h's TestHooks.h and TMyException.h and on TestHooks.h's testmode/ClientView.h: they are
  part of common.h's interface (TBEGIN/TEND throw TMyException, the PATELLA_* hooks replace system calls), so a
  file that includes common.h has them.
- **Removed** (high). DialogWindow.cpp's Utils.h (nothing of it used; no data in it) and SurfaceVulkan.cpp's
  `<cctype>` (no character test left in it).
- **The new `<ostream>` moves a table on x64** (high). Communication.cpp, Dialogs.cpp and Player.cpp use
  `std::endl` and had it through `<sstream>`; with MSVC 14.51 in C++23 `<ostream>` itself also brings in
  `<format>` and `<print>`, so these three objects now carry the 200-byte digit-pair table `"000102...99"` of
  `<charconv>` as a COMDAT, and the linker places the one copy it keeps at another .rdata address (0x357460
  for 0x355940 in the x64 Patella.exe; the strings between move up by 200 bytes). Section sizes and `.pdata`
  are identical; `.rdata` in the range differs only by that move (the same bytes in another order); the
  instructions are identical once addresses and displacements are normalised (dumpbin /disasm of both images:
  917,676 and 956,657 lines, Patella.exe and PatellaTest.exe, 0 differing). No behaviour depends on where a
  constant table is. The oracle Patella.exe is unchanged (C++17: `<ostream>` brings nothing new).
- **Headers: not done** (high). clang-tidy checks a file's includes only when it is the main file, so the run
  over the compile commands covers the .cpp files. Checked on their own (each header of src/ as the main file,
  with the commands clang-tidy interpolates from each target), the headers have about 50 findings per target:
  missing includes that the .cpp files supply before them (uint8 in SurfaceDX.h, FILE in Script.h and
  Talk.h, size_t in vulkan/Renderer.h), and includes the header itself does not use, which the .cpp files
  rely on (Communication.h's Talk.h and `<sstream>`, Dialogs.h's Text.h). Removing those changes which objects
  get Text.h's and Status.h's internal-linkage tables, i.e. .data, header by header; and the interpolated
  commands parse some headers wrongly (as C, or without the Windows headers). That is its own change, with
  each removal measured on the images; not done here.
- **.clang-tidy enables misc-include-cleaner** (medium). On linux-x64-clang's commands (patella_core) it is
  clean with libstdc++ as well, so the hosted CI's baseline check fails a new missing include there; the
  baseline keeps its 21 entries (0 new, 0 gone). The Windows targets are checked by running tidy.py on their
  commands as above (not in the hosted CI, which has no MFC).
- **Proof** (high). Second commit against the first, built at the same path: the oracle Patella.exe differs only
  in the link time stamps and the PDB record (pecompare: `.text` and `.data` identical); the x64 Patella.exe and
  PatellaTest.exe differ by the table move above (pecompare: 7,073 and 7,112 differing ranges, all of them
  addresses; disassembly identical after normalisation). macrocheck: the 2,108 calls unchanged. The difftest
  counts are unchanged.

## [[nodiscard]], [[fallthrough]] and [[maybe_unused]] (Patella-Modern issue #38)

Settled by Claude Opus 5.5, November 3, 2026. The fifth change of M3 (plan section 7), in three commits that can be
reviewed and reverted alone, one per pattern: `modernize-use-nodiscard` is clean on src/, every case that falls
through says `[[fallthrough]]`, and no unused parameter is hidden by a cast to void or a pragma.

- **modernize-use-nodiscard through tools/tidy.py on every configuration that compiles src/** (high). As in issues
  #32, #33, #35 and #40: `python -I tools/tidy.py -p <dir> --checks=-*,modernize-use-nodiscard --fix --all` once per
  target, on compile_commands.json files holding one target's Release commands each (oracle-x86's Patella,
  mfc_shim_check, difftest, communicationtest, sysinctest and vlongtest with `--extra-arg=-m32`; windows-x64's
  Patella, PatellaTest, patella_core, patella_testmode and patella_render_vulkan). 17 const member functions in 7
  files: TMyException.h's GetMessage and GetOrigin, Talk.h's TTalkLine::GetText, compat/mfc_shim.h's GetSafeHwnd,
  vlong.h's get, is_zero, test, bits and cf, vulkan/Renderer.h's Created, Device, ImageWidth, ImageHeight and
  PresentMode, Renderer.cpp's TState::ImageTooSmall and SurfaceVulkan.h's DeviceName. No caller discards one of
  their results, so no function was left out and no caller changed (MSVC's C4834 would fail the build). The check
  marks only const member functions with a result and no non-const reference parameter, so this is what it finds;
  the coding standard's "[[nodiscard]] by default" for new code stays a review rule. The second run of every
  target reports none (MainFrm.cpp's two clang-diagnostic errors, also on main, aside). No fix inside a
  TASSERT/TERROR/TEND argument. The clang-tidy baseline loses its seven use-nodiscard entries (21 to 14).
- **The six cases that fall through are the original's** (high). Clang's implicit-fallthrough warning, through
  clang-tidy (`clang-diagnostic-implicit-fallthrough` with `--extra-arg=-Wimplicit-fallthrough`) on the same 11
  targets, finds six, each already commented "fall through" by the reconstruction: TConsoleWindow::HandleEvent
  (0x424b10) 202 into 201 and 210 into 203, Control.cpp's MouseMove (0x4469b0) action state 1 into default,
  TMainWindow::HandleEvent (0x4aed10) 0x21 into 2, TSurfaceDX9::Init (0x4f1240) FULLSCREEN_MODE into 0, and
  SurfaceVulkan.cpp's Init, which follows TSurfaceDX9::Init. difftest calls the first four against the image
  (consolewindowtest's HandleEvent with 202 and 210, controltest's MouseMove, mainwindowtest's HandleEvent,
  surfacedx9test's Init), so the fall-through is the binary's behaviour, not a reconstruction bug; nothing was
  changed but the comment, which became `[[fallthrough]];`. No unmarked fall-through was found, so none is listed
  as a possible bug.
- **The fall-through warning on GCC and Clang only** (high). cmake/Warnings.cmake adds it for the C++ sources of
  every product target, the baseline files included: GCC `-Wimplicit-fallthrough=5` (GCC's -Wextra has level 3,
  which also accepts a "fall through" comment; level 5 accepts only the attribute; an explicit level holds
  whatever the order of -Wextra), Clang `-Wimplicit-fallthrough` (not in -Wextra). MSVC has no compiler warning
  for it, only the code analysis's C26819 (/analyze), which no preset runs; the client's sources are checked by
  Clang through clang-tidy as above. Not for C: the regexp (src/Extern) is third-party C17, which has no
  attribute. The oracle-x86 preset never uses cmake/Warnings.cmake.
- **[[maybe_unused]] on every unused parameter** (high). The suppressions in src/ and tests/: Talk.h's four casts
  to void (TTalkBuffer::SetWriteEnabled, TRuleViolationChannel::AddText), testmode/ClientStateWin32.cpp's
  `#pragma warning(disable : 4100)` around the client's headers, and tests/golden/regexp_tests.cpp's cast of
  `Pattern` (used only when the runner calls the original). They become `[[maybe_unused]]` on the parameter. The
  pragma covered the parameters of the headers' inline functions (GUI.h's TElement handlers, DialogElements.h,
  ContentWindows.h, ContextMenus.h), which the /W3 baseline files never reported; so the unused warnings are now
  on for every C++ source of the product targets, the baseline files included: MSVC `/w34100 /w34189 /w34505`
  (unreferenced parameter, initialised but unused local, removed internal function: level 4, now level 3; C4101
  is level 3 already), GCC and Clang `-Wunused -Wunused-parameter -Wunused-but-set-parameter`. Measured with
  clang-tidy on the 11 targets (`clang-diagnostic-unused-*` with `-Wunused -Wunused-parameter` and the other
  unused groups): 212 unused parameters in 18 files of src/, nothing else (no unused variable, function, typedef
  or exception parameter). They are the original's signatures: TComponent's virtual handlers that a subclass
  ignores (x, y, Flags), the MFC message handlers, TSurface's engine-specific virtuals. A script put
  `[[maybe_unused]]` before each such parameter's declaration (at its definition, where the warning is), and
  `clang-format -i` realigned the trailing comments of the changed lines (whitespace only). The second run reports
  none; the MSVC builds (x64 Release and Debug, with the new flags) and GCC and Clang report none.
- **Not names dropped, not unnamed parameters** (medium). An unnamed parameter would also silence the warning,
  but the names are the reconstruction's reading of the original (x, y, Flags at the vtable slot's offsets) and
  the issue asks for the attribute. The two places where the reconstruction dropped a name to avoid a warning
  stay as they are: InitObjects's `catch (TMyException &)` in Objects.cpp (an unnamed handler declares no
  variable, so there is nothing to mark; naming it only to mark it unused adds nothing), and ContextMenus.cpp's
  extra argument the binary passes to "Invite to Party" (a format argument, not an unused entity).
- **tools/ is outside** (medium). The difftest's sources (tools/difftest: creaturestest.cpp's and errortest.cpp's
  casts to void, networktest.cpp's unused Command, the `#pragma warning(disable: 4100)` of mainfrmtest.cpp,
  networktest.cpp, surfacedx9test.cpp and surfacegltest.cpp) are the oracle's test instrument, built with the
  oracle's /W3 (where C4100 is off, so the pragmas do nothing) and never with cmake/Warnings.cmake; they retire
  with the oracle in M6. The `(void)Vector.at(3)` of tests/unit/cxx26_static_vector_test.cpp discards a
  [[nodiscard]] result on purpose inside CHECK_THROWS_AS; it is not an unused entity.
- **Attributes change no code** (high). Built at the same path, the oracle Patella.exe and the windows-x64 Release
  Patella.exe and PatellaTest.exe differ from main 2ee58c9's, and from each commit's predecessor, only in the link
  time stamps and the PDB record (pecompare: `.text` and `.data` identical after each of the three commits).
  macrocheck 2ee58c9 against each commit: the 2,108 calls unchanged.

## Constant #defines to constexpr (Patella-Modern issue #36)

Settled by Claude Opus 5.5, November 3, 2026. The sixth change of M3 (plan section 7), by hand, one commit: no
constant `#define` is left in src/ (outside src/Extern and the resource IDs) except the ones listed below, each
with the reason it stays a macro.

- **38 constants, by a script on the exact definition lines** (high). Creatures.h's CREATURES, BUDDIES and
  NOT_AN_OPPONENT; graphics.h's PATH_MAX and HASH_TABLE_SIZE; Help.h's EVENTHELPS; Inventory.h's BODY_POSITIONS,
  OPEN_CONTAINERS, CONTAINER_OBJECTS and OBJECTS_PER_OFFER; MainFrm.h's KEY_SHIFT, KEY_CONTROL and KEY_ALT;
  MainWindow.h's TRADE_WINDOW; Map.h's MAPSIZE_X, MAPSIZE_Y, MAPSIZE_Z, OBJECTS_PER_POINT and EFFECTS; Map.cpp's
  AUTOMAP_BLOCKS; MapWindow.h's MAP_WIDTH and MAP_HEIGHT; Network.h's NUMBEROFSERVER, ERROR_LOGINSERVER_OVERLOADED
  and ERROR_DISCONNECTED; Objects.h's CREATURE; Preferences.h's HOTKEYS, HOTKEY_LENGTH, BUDDYDESCRIPTION_LENGTH and
  SKILLS; Script.h's MAX_IDENT_LENGTH and MAX_STRING_LENGTH; Skins.h's LASTSKIN; Status.h's RIGHT_ANY,
  MESSAGE_NONE, MESSAGE_STARTPATCHER and ACTIONSTATE_NONE; vlong.cpp's BPU. The plan's "79" was a rougher count
  (it took in configuration macros); these are all the object-like macros of src/ whose body is a value.
- **The macro's type, kept** (high). Every one but BPU was a decimal or hexadecimal literal that fits in `int`
  (the largest is NOT_AN_OPPONENT, 0x7fffffff), so its type was `int`; each becomes `inline constexpr int NAME =
  <the same literal>;` in its header (one entity for the program, no copy per object; C++17, which the oracle's
  options have) and `constexpr int` in Map.cpp. BPU was `(8 * sizeof(unsigned))`, a `size_t`; it becomes
  `constexpr auto BPU = 8 * sizeof(unsigned);`, the same expression and so the same type, which vlong.cpp's
  arithmetic (`n * BPU`, `x %= BPU`) and its lo/hi/lh macros rely on. A literal is one token, so no use depends
  on the parentheses or the precedence of an expansion; signed/unsigned comparisons (`strlen(FileName)<PATH_MAX`,
  `Random() % CREATURES`) convert the same `int` as before.
- **constexpr, not enumerators** (medium). The only defines that look like sets (the KEY_* modifier flags, the two
  MESSAGE_* bounds, the two ERROR_* codes) are used as plain ints: or-ed into and tested against `int KeyFlags`,
  passed to `In(int, int, int)`, compared with an int error code. An unscoped enumerator has its enumeration's
  type, which changes overload resolution, template deduction and what a variadic call sees; `constexpr int`
  keeps the type exactly, and enum class (issue #37) is a separate step.
- **The ones inside TASSERT arguments too** (high). 26 of the names appear 128 times in asserted expressions
  (`In(Container,0,OPEN_CONTAINERS-1)`, `getWidth()<=MAP_WIDTH*32`, `SrcObj.Type!=CREATURE`). The issue had them
  wait for M5, for fear that the message text would change. It does not: every assertion macro (common.h's
  TASSERT, TASSERT1, TASSERT2, and the local TASSERTB*, TASSERT*_BUG of Map.cpp, ContentWindows.cpp,
  Creatures.cpp, graphics.cpp, SurfaceDX.cpp and surfaceDX9.cpp) stringizes its own parameter (`#Expr`), and a
  parameter preceded by `#` is not macro-expanded before it is stringized; none passes Expr on to another macro
  that stringizes it. So `"In(Container,0,OPEN_CONTAINERS-1)"` was the message text with the macro and is the
  same with the constant, and the argument text is unchanged (macrocheck: 2,108 calls, 0 differences). None of
  the names is used in `#if`, in token pasting or in a string concatenation, and the values pass through
  `In(int, int, int)` and int comparisons as before. The images prove it: the message strings are in .rdata,
  and .rdata is identical.
- **What stays a macro** (high). Read by the preprocessor or by a header's own `#if`/`#ifdef`: common.h's
  `_CRT_SECURE_NO_WARNINGS`, `_WINSOCK_DEPRECATED_NO_WARNINGS` and PATELLA_ORIGINAL_LAYOUT; compat/cxx26.h's
  PATELLA_HAS_CONTRACTS and PATELLA_HAS_INPLACE_VECTOR; Surface.cpp's and SurfaceDX.cpp's DIRECT3D_VERSION;
  SurfaceDX.h's D3D_OVERLOADS; SurfaceGL.cpp's GLH_EXT_SINGLE_FILE; vulkan/DeviceSettings.cpp's
  WIN32_LEAN_AND_MEAN; vulkan/Renderer.h's VK_NO_PROTOTYPES; vulkan/Vma.cpp's VMA_IMPLEMENTATION,
  VMA_STATIC_VULKAN_FUNCTIONS and VMA_DYNAMIC_VULKAN_FUNCTIONS. API names: SurfaceGL.cpp's GL_CLAMP_TO_EDGE and
  WGL_DRAW_TO_PBUFFER_ARB, WGL_SUPPORT_OPENGL_ARB, WGL_DOUBLE_BUFFER_ARB, WGL_ACCELERATION_ARB,
  WGL_FULL_ACCELERATION_ARB, WGL_COLOR_BITS_ARB and WGL_ALPHA_BITS_ARB stand in for the Khronos glext.h and
  wglext.h, which define the same names as macros; a variable of that name breaks as soon as such a header is
  included, and the code is the oracle's only (OpenGL is not in the x64 client), retiring in M6. The resource IDs
  of src/res/resource.h (the resource compiler reads them). Not constants: the error macros and their local
  variants, CONTRACT_ASSERT, the MFC shim's message-map macros, TestHooks.h's PATELLA_* hooks and vlong.cpp's
  lo, hi and lh. Outside src/ (not in the issue): tests/golden/golden.h's GOLDEN_ORACLE (`#if`) and
  tools/difftest/difftest.cpp's STATUS_HEAP_CORRUPTION (an NTSTATUS, which the SDK spells as a macro).
- **No new clang-tidy check** (medium). modernize-macro-to-enum would ask for enumerators, which this change
  chose not to use, and cppcoreguidelines-macro-usage reports every function-like macro (the error macros
  above all); neither fits as a CI guard. The baseline is unchanged (14).
- **PATH_MAX on Linux** (medium). POSIX's `<limits.h>` defines PATH_MAX as a macro (4096; mingw-w64's as 260);
  graphics.cpp is not in patella_core, so no preset that compiles it sees one today. When the client is built on
  Linux (M8) the name collides, as the macro did (a redefinition); the name is the original's, in two assertion
  texts, so renaming it waits until those texts may change (M5).
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built from
  main 93315c1 and from the branch at the same path, differ only in the link time stamps and the PDB record
  (pecompare: `.text`, `.rdata` and `.data` identical); macrocheck 93315c1 issue-36: 2,108 calls, 0 differences;
  the difftest counts are unchanged.

## Enums with an explicit underlying type, then enum class (Patella-Modern issue #37)

Settled by Claude Opus 5.5, November 3, 2026. The seventh change of M3 (plan section 7), by hand, in two commits that
can be reviewed and reverted alone: every enum of src/ has an explicit underlying type, and the one that nothing uses
as an int is an `enum class`.

- **`: int`, the type MSVC gives them now** (high). The eleven enums of the reconstruction (common.h's PROGRAM_ERROR,
  Script.h's TOKEN, Talk.h's TALK_MODE, and the unnamed ones of graphics.h (the alignments, the cache item kinds),
  Objects.h (the Tibia.dat flags), Surface.h (the view modes, the surface indices), surfaceDX9.h, SurfaceGL.h and
  Talk.h (TALKBOXES, STATUSMESSAGE_LENGTH)), SurfaceVulkan.h's unnamed NUMBER_OF_SURFACES_VULKAN and the three enum
  classes of src/vulkan (TDeviceKind, TDeviceButton, TFilter, which were `int` by default) say `: int`. Each is
  followed by `static_assert(sizeof(E) == 4)` and `static_assert(std::is_same_v<std::underlying_type_t<E>, int>)`
  (`decltype` of an enumerator for the unnamed ones), in the header, so every preset that includes it checks them,
  oracle-x86 included. TReadScriptFile::Token (+0x000) is the only structure member of one of these types; its 4
  bytes are the asserted size. The test mode's four enum classes already said `std::uint8_t`.
- **GCC and Clang: unsigned int before, int now** (high). MSVC gives an unscoped enum without a fixed type `int`;
  GCC and Clang give it `unsigned int` when no enumerator is negative (checked with mingw-w64 GCC 16.2:
  `std::underlying_type_t` of such an enum is `unsigned int`). The explicit type is MSVC's, the compiler of the
  original and of the reference builds, so the asserts hold on every preset. Nothing observable changes on GCC and
  Clang either: an unscoped enum promotes to `int` in both cases (its values fit in `int`), and every value of `int`
  is now a valid value of the enum where before only those of its bit range were. The Linux presets compile only
  common.h's PROGRAM_ERROR of these (patella_core), and no core source throws or catches it.
- **Mangled names and RTTI unchanged** (high). MSVC encodes an enum as `W4<name>@@` for an `int` underlying type,
  scoped or not, so PROGRAM_ERROR's catchable type `.?AW4PROGRAM_ERROR@@` (in .rdata; the difftest's imageeh.cpp
  matches the original's by that name), HandleProgramError's decorated name and every other symbol keep their
  spelling; the Itanium ABI does not encode the underlying type. tools/client_state.py, the test mode's state dump
  and the comparison tools look up none of these enums or enumerators (they read classes, `int` and class template
  instances from the PDB), so their output is unaffected.
- **TOKEN becomes `enum class TOKEN : int`** (high). The second commit. TReadScriptFile's token kind is only assigned
  (nextToken) and compared with `==`/`!=` (Script.cpp's getters and readers, Preferences.cpp's options parser); no
  enumerator is in a TASSERT/TERROR/TEND argument (the errors are `error("identifier expected")` texts). The 39
  uses are qualified (`TOKEN::SPECIAL`). Outside src/, the difftest's scripttest.cpp and errortest.cpp qualify them
  too (7 uses), and scripttest.cpp's printf-style CHECK passes the two tokens as `(int)` (a scoped enumeration is not
  promoted in a variadic call). Its `(TOKEN)0x55555555` cast stays as it is.
- **The other ten stay unscoped** (high). Each is used as an int:
  - PROGRAM_ERROR: Player.cpp's HandleProgramError streams it (`<< Error`, "Error Code 12") and passes it to
    GetErrorText and GetErrorHint (int parameters); the difftest's imageeh.h stores it in an int Code. The throws
    are casts from int (`throw (PROGRAM_ERROR)12`), which an enum class would allow, but the conversions back
    would need explicit casts at those sites. It stays in the clang-tidy baseline (cppcoreguidelines-use-enum-class).
  - TALK_MODE: its enumerators are in eight TASSERT1 arguments (Communication.cpp, Talk.cpp:
    `Mode==TALK_SAY || ...`, `Mode>=0&&Mode<TALK_MODE_BEYONDLAST`), where `TALK_MODE::TALK_SAY` would change the
    message text; C++20's `using enum` would keep the text, but the oracle compiles as C++17. Beyond M5 it is not
    eligible either: the mode is an int everywhere (TTalkbox::Mode and TTalkLine::Mode, the packets' talk mode byte,
    the int parameters of SendTalk, AddTextBufferLine and AddMessageTalkbox, `switch` over an int in
    ConsoleWindow.cpp and MapWindow.cpp, the ordering comparison with TALK_MODE_BEYONDLAST, the difftest's int
    tables).
  - The eight unnamed ones (and SurfaceVulkan.h's): an unnamed enum has no name to scope its enumerators by; they
    are int constants. LEFT_ALIGN and the others go into TFont's `int Alignment`; the item kinds into the cache
    items' int Type; the Tibia.dat flags are bit numbers (GetObjectFlag(Type, CUMULATIVE)); the surface indices
    index arrays (`Rect[PRIMARY_SURFACE]`, `Surface9[SPRITE_SURFACE]`) and are compared with int parameters; the
    NUMBER_OF_SURFACES_* and TALKBOXES/STATUSMESSAGE_LENGTH are array bounds and loop limits. At least 53 lines
    use them inside TASSERT/TERROR/TEND arguments (36 of them the surface indices). Turning them into `inline constexpr int` (issue #36's form) would be a
    separate change; it is not part of this issue.
  clang-tidy's cppcoreguidelines-use-enum-class (enabled in .clang-tidy) on the 11 targets' commands (as in issue
  #32: oracle-x86's Patella, mfc_shim_check, difftest, communicationtest, sysinctest and vlongtest with
  `--extra-arg=-m32`; windows-x64's Patella, PatellaTest, patella_core, patella_testmode and patella_render_vulkan)
  reports exactly these eleven after the change (TOKEN gone; MainFrm.cpp's two clang-diagnostic errors of the MFC
  message map, also on main, aside).
- **No `using enum`** (high). It would let TOKEN's uses stay unqualified, but the oracle-x86 preset is C++17, and the
  qualified names are what an enum class is for.
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built from
  main c797628 and from each commit at the same path, differ only in the link time stamps and the PDB record
  (pecompare: `.text`, `.rdata` apart from those, `.data`, `.pdata` and `.reloc` identical after each of the two
  commits). macrocheck c797628 against each commit: the 2,108 calls unchanged. The difftest counts are unchanged; the
  clang-tidy baseline is unchanged (14; PROGRAM_ERROR's use-enum-class entry stays).

## = default, = delete, and owning classes that cannot be copied (Patella-Modern issue #34)

Settled by Claude Opus 5.5, November 3, 2026. The eighth change of M3 (plan section 7), in three commits that can be
reviewed and reverted alone: `modernize-use-equals-default` and `modernize-use-equals-delete` are clean on src/ and
enabled in .clang-tidy, and every class whose destructor frees what it holds cannot be copied.

- **clang-tidy through tools/tidy.py on every configuration that compiles src/** (high). As in issue #32: one run per
  target on compile_commands.json files holding one target's Release commands each (oracle-x86's Patella,
  mfc_shim_check, difftest, communicationtest, sysinctest and vlongtest with `--extra-arg=-m32`, the preset configured
  once with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; windows-x64's Patella, PatellaTest, patella_core, patella_testmode
  and patella_render_vulkan). use-equals-default found eight empty special members, use-equals-delete nothing; with
  both checks' IgnoreMacros off the result is the same. MainFrm.cpp's two clang-diagnostic errors (the MFC message
  map, also on main) aside, both checks report nothing after the change.
- **Out of the class, `= default` keeps everything** (high). TElement::TElement, TSurface::~TSurface,
  TSurfaceGL::TSurfaceGL, TMyException::~TMyException and TRSA::~TRSA are defined in their .cpp files. A function
  defaulted after its first declaration is still user-provided: `T()` and `new T()` do not zero the object first, the
  declaration's exception specification stays, and the definition stays in the same object. All five classes are
  polymorphic, so none was trivial or an aggregate before or after. The images are identical.
- **In the class, two keep their empty bodies** (high). Defaulted on its first declaration a special member is no
  longer user-provided:
  - `TSurfaceDX() {}`: as `= default`, `TSurfaceDX()` and `TSurfaceDX{}` would value-initialize, that is zero the
    0x250 bytes before TSurface's constructor runs. The only creation today is CreateSurfaceDX's `new TSurfaceDX`
    (default-initialization, unaffected), so the images would not show it; the body stays so that the class keeps
    the original's "initialises nothing" for every form of creation.
  - `virtual ~TXTEA() {}`: as `= default` the images change: the oracle Patella.exe's .text grows by 0x60 bytes and
    the x64 Patella.exe's and PatellaTest.exe's code and RTTI (TXTEA's type descriptor in .data) move, 160,649 and
    15,439 differing runs (pecompare, each of the eight changes built alone on main at the same path; the other
    seven changed nothing). The body stays.
  Both carry `// NOLINT(modernize-use-equals-default)` and a comment with the reason, so the check stays clean in
  CI. The MFC shim's `virtual ~CObject() {}` (mfc_shim_check and the difftest only, not in a compared image)
  becomes `= default`.
- **use-equals-delete has nothing to do** (high). It turns a private, undefined copy constructor or assignment (the
  pre-C++11 idiom) into `= delete`; src/ has none. Only vulkan/Renderer.h's TRenderer and Renderer.cpp's TDriverFiles
  (new code) and testmode/Clock.h's IClock declare copy members, already deleted, and vlong defines its own.
- **The owning classes, by hand** (high). A class owns a pointer when its destructor frees what it holds. The
  destructors of src/ were scanned for delete, delete[], free, fclose, close and the functions that wrap them
  (DeleteChild, DeleteTextBuffer, TTalkBuffer::Clear), then read one by one. 40 classes get `X(const X &) = delete;`
  and `X &operator=(const X &) = delete;`, in their first public section (after the constructors in TTalkLine and
  flex_unit, whose members come first):
  GUI.h's TComponent (DrawingList) and TWindow (its children); ConsoleWindow.h's TTextWindow, TTabBar, TTab and
  TConsoleWindow; ContentWindows.h's TCombatControls and TBuddylistWindowBody; DialogElements.h's TLabel,
  TFormattedText, TLink, TMultiStateButton, TEditLine, TPasswordEditLine, TListBox and TFontListBox; Dialogs.h's
  TWaitLabel, TReconnectWindow, TLoginDialog, TCharacterListDialog, TOutfitDialog, TNewChannelDialog,
  TEditBuddyDialog, TRuleViolationDialog, TEditMarkDialog, TGraphicsOptionsDialog and TAdvancedGraphicsDialog;
  DialogWindow.h's TSubWindow; MainFrm.h's CMainFrame (MFC's CObject already hides its copy; the shim's did not);
  Container.h's array, matrix, matrix3d and vector; Script.h's TReadScriptFile, TWriteScriptFile (FILE *),
  TReadBinaryFile and TWriteBinaryFile (descriptors); Talk.h's TTalkBuffer (its lines) and TTalkLine (Line, from its
  constructor; TTalkBuffer frees it); vlong.h's flex_unit (vlong_value inherits it).
  - Nothing copies any of them: every target of oracle-x86 (difftest and the MFC shim build included), windows-x64,
    windows-x64-gcc and the Linux presets compiles unchanged, so no copy had to be defined.
  - The classes that only allocate in their constructors (the windows and dialogs that create their child
    components, 45 more by a scan of the constructors) hand what they allocate to TWindow, which frees it; their copy
    is deleted through TComponent and TWindow. A scratch translation unit with `static_assert(!std::is_copy_constructible_v<X>
    && !std::is_copy_assignable_v<X>)` for all of them, the 40 above, vlong_value and the four templates at `int`
    compiled on windows-x64 (and failed, as it should, on TTalkbox, a plain struct).
  - Not owners, left alone: TTimer (unregisters itself), the windows whose destructors only store state or clear
    back pointers (TMainWindow, TMapWindow, TAutomapWindow, TContainerWindowBody, TSkillsWindowBody,
    TTradeWindowBody, TBattleWindowBody, TSkillContextMenu), the surfaces (their COM objects and contexts are freed
    by ExitSurfaces, not by the destructor; TSurfaceVulkan's TRenderer is a unique_ptr), the vulkan, settings and
    testmode classes (standard library members).
  - Deleted members add no code and no data, and change no size, alignment or vtable. The one class that was
    trivially copyable, TTalkLine, no longer is; nothing passes it by value (it could not compile), and the images
    are identical.
- **Both checks in .clang-tidy** (medium). The second commit adds `modernize-use-equals-default` and
  `modernize-use-equals-delete` to the checks, so CI's clang-tidy (linux-x64-clang, patella_core and the headers it
  includes) keeps them clean. The baseline is unchanged (14 findings, 0 new, 0 gone).
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built from main
  b123283 and from each of the three commits at the same path, differ only in the link time stamps and the PDB record
  (pecompare: `.text`, `.data` and `.pdata` identical, 0 unexpected differences). macrocheck b123283 against each
  commit: the 2,108 calls unchanged. The difftest counts are unchanged.

## const locals and const member functions (Patella-Modern issue #39)

Settled by Claude Opus 5.5, November 3, 2026. The ninth change of M3 (plan section 7), in two commits that can be
reviewed and reverted alone, one per check, and a third with what CI's clang-tidy found beyond them (below):
`misc-const-correctness` and `readability-make-member-function-const` are clean on src/ and enabled in .clang-tidy.

- **clang-tidy through tools/tidy.py on every configuration that compiles src/** (high). As in issue #32: one run per
  target on compile_commands.json files holding one target's Release commands each (oracle-x86's Patella,
  mfc_shim_check, difftest, communicationtest, sysinctest and vlongtest with `--extra-arg=-m32`, the preset configured
  once with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; windows-x64's Patella, PatellaTest, patella_core, patella_testmode
  and patella_render_vulkan), `--fix` one target after the other. Measured first without fixing: 1,163 distinct
  misc-const-correctness findings and 50 readability-make-member-function-const findings over the 11 targets. After
  the change both checks report nothing on any target (MainFrm.cpp's two clang-diagnostic errors of the MFC message
  map, also on main, aside). No finding and no fix was inside a TASSERT/TERROR/TEND argument (a declaration is never
  inside one); macrocheck 762f81b against each commit: the 2,108 calls unchanged.
- **What misc-const-correctness covers: locals, with its defaults** (high). clang-tidy 22's check looks at local
  variables only, not at parameters or members. Its options decide which locals: values (`int Mode = Box->Mode;`),
  references (`TObject &Obj`) and the pointees of pointers (`TTalkbox *Box` to `const TTalkbox *Box`); not the
  pointers themselves (WarnPointersAsValues, `T *const p`, off by default: noise without safety). The defaults are
  kept and spelled out in .clang-tidy, so that a change of clang-tidy's defaults does not change what CI asks for.
  They are noisy (about 800 of the findings are scalar locals), but a const local is what the check exists for, the
  fix is mechanical, and every local that matters to the image is found by comparing the images, not by guessing
  which kinds are safe. A narrower configuration (references and pointees only) would have left the check unusable
  as a CI guard for the rest.
- **The fixes, normalised to the code's const placement** (high). clang-tidy writes `int const Mode` and
  `TTalkbox const *Box` (east const); src/ writes `const` first everywhere (no `int const` on main). The changed lines
  were re-formatted with the repository's .clang-format plus `QualifierAlignment: Left`, restricted to those lines
  (`clang-format --lines`), and the 29 declarations of ALL-CAPS Windows types that clang-format leaves alone
  (`RECT const R`, `SOCKET const Socket`: it takes them for macros) by a script. Every changed line differs from
  main only by the inserted `const` (git's word diff). A future `tidy.py --fix` of this check writes east const
  again; normalise it the same way.
- **By hand** (high). The check does not fix a declaration with several declarators (`int ox = OffsetX[i], oy =
  OffsetY[i];`): MapWindow.cpp's ten such declarations got `const` for the whole declaration (no declarator is
  modified; every compiler agrees). TSurfaceDX::TestShading's (0x4ecdd0) `Pixels`, which the runs after the first
  could not fix because of the broken fix below in the same file. Commit 1: 1,144 `const`s in 52 files.
- **One false positive** (high). TSurfaceDX::SetPixel writes through `uint8 *Destination` with
  `*Destination++ = Bytes[LoopIndex];`, yet clang-tidy 22 takes its pointee for const; the fix did not compile on
  the other targets. It stays `uint8 *`, with a NOLINT and the reason.
- **One local stays non-const for the image** (high). Built at the same path, the oracle Patella.exe after the fixes
  differed from main's in 124,344 ranges (`.text`, `.rdata`, `.data`, `.reloc`; the x64 images were identical). The
  objects of main and of the branch, compared section by section without the debug sections: only Talk.obj's code
  differed, and ConsoleWindow.obj held the same sections in another order (which leaves the image unchanged). In
  TTalkBuffer::Remove(int) (0x503210) `int i = 0` is never changed (the binary never increments it; the loop asserts
  for any nonzero index); as `const int` the compiler drops its stack slot (`sub esp,20Ch` becomes `208h`) and
  compares with the constant. It stays `int`, with a NOLINT and the reason; the oracle image is then identical.
  The local aggregates of constants, which a compiler may emit once in `.rdata` instead of building them on the
  stack, were built both ways: Skins.cpp's InitSkins table (`SkinTable[]`, 262 entries; the binary builds it on the
  stack on every call, 12,506 bytes of code) and SurfaceGL.cpp's two `PIXELFORMATDESCRIPTOR`s (oracle only). Const,
  all three images are identical (MSVC still builds them on the stack), so they are const.
- **readability-make-member-function-const: 60 member functions** (high). The second commit. A member function that
  reads its object's members but changes none, calls no non-const member and hands out no non-const reference or
  pointer to a member becomes const, in its declaration and its definition: TConsoleWindow's SetActive,
  GetServerMessages, GetTextBuffer, ParseInput and SaveChannel; TTextWindow's getFirstLineNumber,
  getLastLineNumber and HasSelection, TConsole's and TTab's getTextBuffer; TPlayerStates::GetNumberOfIcons and
  GetIconAt, TContainerWindowBody::GetSlotAt, TSkillsWindowBody::GetLine, TTradeOffers::GetOfferAt; TCreature's
  GetHealthColor, IsMarked, GetDeltaX, GetDeltaY, GetDrawDirection and GetAnimationPhase; TListBox's GetLineAt,
  GetSelection and GetNumberOfLines, TEditLine::HasSelection; TSubWindow::getBody; TRuleViolationDialog::GetAction;
  TTimer::GetTime and IsExpired, TCallback::Send, TComponent's getWidth and getHeight; CMainFrame::TranslateKey; ten
  of TMainWindow's (ResizeConsole, GetConsoleWindowHeight, OpenContainer, CloseContainer, OpenTrade, UpdateTrade,
  CloseTrade, OpenContentWindow, CloseContentWindow, SetConsoleActive); TWriteScriptFile's writeLn, writeText,
  writeString and writeNumber, TReadBinaryFile::eof; three of TSurfaceDX's and two of TSurfaceDX9's;
  TTalkBuffer::Write; and six helpers of vulkan/Renderer.cpp's TRenderer::TState. A const local can let a function
  qualify and a const function more locals, so both checks were applied again until neither reported anything: the
  second round made Renderer.cpp's Allocate const and five more pointees (ConsoleWindow.cpp, MainWindow.cpp,
  MapWindow.cpp's three `Player`s), which belong to the second commit.
- **const as the check means it: the object's own members** (medium). Several of these change something else:
  TMainWindow's OpenContainer opens a window in the dialog column through a pointer member, TWriteScriptFile's
  writers write to its `FILE *`, the Vulkan helpers create and destroy device objects. `const` on a member function
  promises that the object's members do not change, which holds; it does not promise that nothing happens. Leaving
  such functions out by judgement would make the rule unenforceable in CI, so the check's result is taken as it is,
  with one exception: the MFC shim's `CWnd::UpdateWindow` keeps MFC's own non-const declaration (NOLINT), so that
  the shim keeps standing for MFC.
- **No virtual function, no overload, no layout** (high). The check leaves virtual functions alone, so no vtable and
  no override changes (override is on everywhere since issue #33: a const base without const overriders would not
  compile). Container.h's operator() has no const overload and no class of src/ has const/non-const pairs, so no call
  inside a function made const resolves to another overload. A member function's const qualifies `this`, not the
  class: no size, offset or alignment changes, and the static_asserts of the oracle-x86 build hold.
- **The decorated names change; nothing looks them up** (high). A const member function is `QBE` instead of `QAE` in
  MSVC's x86 names (`QEBA` instead of `QEAA` on x64), so the PDBs name these 60 functions differently. Nothing reads
  them: tools/client_state.py takes globals by their plain names (SymEnumSymbols) and virtual slots from the type
  information; the test mode's state dump (src/testmode/ClientStateWin32.cpp) reads classes by their RTTI names; the
  difftest's `/alternatename`s name constructors only, and its tests call these functions through objects or image
  addresses (creaturestest.cpp's GetDrawDirection, guitest.cpp's IsExpired, mainfrmtest.cpp's TranslateKey); the
  comparison tools read no function names. The images are identical, so the code and data are too.
- **Parameters: not in these checks** (medium). The issue's title names parameters, but neither check looks at them
  in clang-tidy 22 (misc-const-correctness has no parameter analysis). `readability-non-const-parameter` (pointer
  parameters only) would change function signatures, with them the decorated names and the difftest's typed calls of
  image functions; it is not part of this issue.
- **What clang-tidy 22.1.8 finds and 22.1.3 does not** (high). The third commit. The hosted CI's baseline check
  (linux-x64-clang in the Docker image, clang-tidy 22.1.8) reported 11 more `std::size_t` locals of src/testmode
  (InputScript.cpp, PacketLog.cpp, Png.cpp, ServerReplay.cpp: positions from `find` and sizes) that Visual Studio's
  clang-tidy 22.1.3 did not report on the Windows targets' commands of the same files; none is modified, none is
  in platform code. They are const by hand. The same commit makes InitSkins's table const after measuring it
  (above). The x64 images are again identical to main's; the testmode files are not in the oracle.
- **Both checks in .clang-tidy** (medium). CI's clang-tidy (linux-x64-clang: patella_core and the headers it includes)
  keeps them clean; patella_core had 36 misc-const-correctness findings before (vlong.cpp, settings/Settings.cpp,
  Utils.cpp, Skins.cpp, TMyException.cpp, TRSA.cpp), none after. The baseline held none of either check.
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built from main
  762f81b and from each of the three commits at the same path, differ only in the link time stamps and the PDB
  record (pecompare: 0 unexpected differences; `.text`, `.rdata`, `.data`, `.pdata` and `.reloc` identical).
  macrocheck 762f81b against each commit: the 2,108 calls unchanged. The difftest counts are unchanged; the
  clang-tidy baseline is unchanged (14 findings, 0 new, 0 gone).

## Named casts instead of C-style casts (Patella-Modern issue #41)

Settled by Claude Opus 5.5, November 4, 2026. The tenth change of M3 (plan section 7), in two commits that can be
reviewed and reverted alone: `cppcoreguidelines-pro-type-cstyle-cast` is clean on src/ outside the error macros, and
enabled in .clang-tidy.

- **clang-tidy through tools/tidy.py on every configuration that compiles src/** (high). As in issues #32 to #39:
  one run per target on compile_commands.json files holding one target's Release commands each (oracle-x86's
  Patella, mfc_shim_check, difftest, communicationtest, sysinctest and vlongtest with `--extra-arg=-m32`, the
  preset configured once with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; windows-x64's Patella, PatellaTest,
  patella_core, patella_testmode and patella_render_vulkan), then CI's clang-tidy 22.1.8 in the Docker image on
  linux-x64-clang's commands. Measured on main: 243 distinct findings over the 11 targets (204 "convert between
  unrelated types", 37 downcasts, 2 that cast away const) in 25 files, and one more inside a TASSERT argument
  (Objects.cpp's `TASSERT1(67, ..., *(int*)Entry.Flags)`, left for M5; macrocheck 0). After the change the check
  reports nothing on any target (MainFrm.cpp's two clang-diagnostic errors of the MFC message map, also on main,
  aside); patella_core's six (TXTEA.cpp) are gone in 22.1.8's run too.
- **What the check reports, and what it does not** (high). Not every C cast: the check reports a C cast that is not
  a plain conversion, i.e. one clang classifies as a bit cast (pointer to unrelated pointer, integer to pointer and
  back, function pointers, and also `void *` to `T *`), a downcast, or one that drops const. Arithmetic casts
  (`(int)Param2`, `(uint16)Length`, `(PROGRAM_ERROR)12`) and upcasts are static_casts it accepts as they are; the
  plan's "about 228" counted all of them by eye. They stay C casts in this issue (a later style change could make
  them functional or static_casts; none changes meaning).
- **Each named cast is the one the C cast meant** (high). A C cast is the first of const_cast, static_cast,
  static_cast+const_cast, reinterpret_cast, reinterpret_cast+const_cast that compiles. The check offers no fixes;
  a script (build/logs/issue-41/rewrite.py, not committed) rewrote each reported cast from its column, rightmost
  first, and the rest was read one by one:
  - the 37 downcasts (`(TTab *)Child`, `(TAutomapWindow *)GetParent()`, `(CPlayerApp *)AfxGetApp()`,
    `(CMainFrame *)m_pMainWnd`...) become static_cast: clang classified them as downcasts, so both classes are
    complete there and related, and static_cast applies the same base offset, if any, that the C cast did. A
    static_cast that would not compile (unrelated or incomplete classes) is the case where the C cast falls back to
    reinterpret_cast; none failed on MSVC x86, MSVC x64 or clang.
  - `void *` to a typed pointer (malloc, GlobalLock, `DDSURFACEDESC2::lpSurface`, `D3DLOCKED_RECT::pBits`, the
    EnumDisplayModes context, ClearMessages's two `void *` parameters: 25) is a static_cast for the C cast too, though
    the check calls it "unrelated types";
  - the rest are reinterpret_casts: the message queue's `INT_PTR` parameters to and from pointers (string literals,
    `new` objects, `this`; Control.cpp, Status.cpp, Communication.cpp and the windows that post messages), `RECT`
    halves as `POINT *` for ClientToScreen, `T **` as `void **` for QueryInterface and Lock, byte buffers as `uint32 *`
    (TXTEA's key and blocks, SetCharacter's address), `char *` as `uint8 *` (Script), the resolver's buffer as
    `HOSTENT *`, `HICON` as `LPARAM`, `FILE *` as `INT_PTR` (TTalkBuffer::Write's catch slot), and the function
    pointers of GetProcAddress and vkGetInstanceProcAddr. SurfaceVulkan.cpp's patcap observers keep their step
    through `void *` (`reinterpret_cast<T>(reinterpret_cast<void *>(GetProcAddress(...)))`): 181 reinterpret_casts.
  - The two that drop const are const_casts: ConsoleWindow.cpp's `TextBuffer->GetAddressee()` and Status.cpp's
    `regcomp(LinkPattern)` (src/Extern/regexp's regcomp takes `char *`).
  - One by choice: Dialogs.h's qsort comparison CompareNames read `*(const char **)Name1` from a `const void *`,
    a static_cast+const_cast for the C cast. It now reads through `static_cast<const char *const *>(Name1)`: the
    same load of the same `const char *`, without dropping a const nobody writes through.
  No integer-pointer cast among them changes size: every integer side is `INT_PTR` or `LPARAM` (pointer-sized on
  both targets since M1); the casts that narrow `INT_PTR` to `int` (`(int)Param2`, `(int)Data[0]`) are arithmetic,
  not reported, and unchanged.
- **TBEGIN and a cast** (high). ConsoleWindow.cpp's TTab::RightButtonUp read `TBEGIN((TTabBar *)GetParent())` with
  `->SetCurrentTab(this);` on the next line: clang-format (issue #8) took the parenthesized cast after the statement
  macro for its argument list. With the named cast it is `TBEGIN` and a statement again, on the same two lines. No
  file's line count changes (the line numbers in comments and the binary's line table stay where they were).
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built from main
  ac12de2 and from each commit at the same path (the second after `--fresh` builds of every preset), differ only in
  the link time stamps and the PDB record (pecompare: `.text`, `.data` and `.reloc` identical, `.rdata` apart from
  those, 0 unexpected differences). macrocheck ac12de2 against the branch: the 2,108 calls unchanged. The difftest counts are
  unchanged; the clang-tidy baseline is unchanged (14 findings, 0 new, 0 gone; it held none of this check).
- **Not in this issue** (medium). tests/ and tools/ keep their C casts (as in #35 and #40: src/ only).

## The gotos: restructured, and the one shared exit kept (Patella-Modern issue #42)

Settled by Claude Opus 5.5, November 4, 2026. The eleventh change of M3 (plan section 7), by hand, one function per
commit: src/ has one `goto` left, ProcessEvents' jump to the binary's shared exit block, with a NOLINT that names the
block; `cppcoreguidelines-avoid-goto` is enabled in .clang-tidy.

- **25 gotos in five functions, not 18** (high). The plan counted 18 before the Vulkan engine (issue #19) copied
  DX9's WriteMaskedImage: TBattleWindowBody::Draw (0x43a2c0, ContentWindows.cpp, 3), ProcessEvents (0x4498a0,
  Control.cpp, 1), TSurfaceGL::WriteMaskedImage (0x4fbbb0, 7), TSurfaceDX9::WriteMaskedImage (0x4f2040, 7) and
  TSurfaceVulkan::WriteMaskedImage (x64 only, 7). src/Extern has none.
- **What counts as the original's cleanup path** (high). Read from the decompiled listings (decompilation/decomp/
  clean): a goto stays when the binary has one exit block that the end of a loop and a jump from inside the loop
  both reach, and C++ has no other way to leave the loop from where the jump is. Only ProcessEvents qualifies: its
  loop condition and case 0x40 (logout while connected, after AddMessageFirst(0x40)) both jump to 0x44a2f5, the
  walk, Redraw and return before the catch. Inside a switch inside the loop, `break` leaves only the switch. A flag
  (`bool LoggingOut`, tested in the loop condition) gives the same behaviour (ModalWindow is null on that path, so
  the dialog placement after the switch is skipped), but MSVC compiles ProcessEvents differently with it (measured
  on the object), so the goto stays: `goto Done; // NOLINT(cppcoreguidelines-avoid-goto): the binary's shared exit
  block 0x44a2f5 (#42)`. Its code is unchanged.
- **The rest jump to code that several branches share, not to an exit** (high). In Draw, case 1 of the party flag
  inside a party and every unknown flag jump past the shield (switchD_0043a85b_default), and an unknown skull flag
  past the skull; in the three WriteMaskedImage, every mask or colour test that fails jumps to the copy of an
  untinted pixel (Copy), the tint jumps over it (Next). Restructured:
  - Draw: `int Skin = 0;` means none (the skins are 240-251, never 0), each switch sets it or leaves it, and the
    drawing (with the Offset update) runs `if (Skin != 0)`. Same calls in the same order.
  - WriteMaskedImage (all three): the tests leave the tint's colour in `Index`, or -1 for a copy; each failing test
    is the old `goto Copy` condition negated (De Morgan), and one if/else tints or copies. The pixel code is the
    same text.
  No form that keeps the generated code was found: the x86 code of Draw and of the GL and DX9 functions changes
  (Draw 3,200 to 3,208 bytes, GL 1,063 to 1,061, DX9 1,225 to 1,199; block order and registers), so behaviour is
  proved by difftest instead. On x64 MSVC emits the same code for the new Draw (the x64 images are identical).
- **Coverage in difftest** (high). The counts of every gate are unchanged (16,848,752 / 67 / 1,134,513 / 3,201).
  Which paths ran was measured once with temporary counters in the four files (not committed; the run's total was
  again 16,848,752 checks, 0 failures): Draw (contentwindowstest.cpp, 52,506 checks plus 168,764 in its error part;
  the creatures get random party and skull flags 0-5 and IsPlayerInParty is faked) drew 5,978 battle rows: party
  flag 1 inside a party 167, flag 1 otherwise 145, flags 2-4 993, unknown flags 373; skull flags 1-4 1,335, unknown
  301. TSurfaceGL::WriteMaskedImage (surfacegltest.cpp, case 21, 53,082 checks in SurfaceGL) and
  TSurfaceDX9::WriteMaskedImage (surfacedx9test.cpp, 58,682 checks in SurfaceDX9), pixels per path, GL / DX9: with
  a mask copied 137,334 / 136,601, tinted with colour 0-3 17,248 17,140 17,092 17,088 / 22,706 22,629 22,913
  22,838; without a mask copied 48,760 / 78,403, tinted 114,289 / 182,733. Each test compares the textures (and
  the thrown errors) with Tibia.exe's functions. ProcessEvents (controltest.cpp, every event type, against
  0x4498a0) is unchanged code.
- **TSurfaceVulkan::WriteMaskedImage: no difftest** (high). It is x64 only, and neither tests_vulkan nor the unit
  tests call it. Its classification is the same text as GL's and DX9's; the old (goto) and the new classification,
  copied verbatim into a scratch program (build/logs/issue-42/scripts/eq.cpp, not committed), give the same Index
  for all 2^24 mask values (exactly one value per colour 0-3 is tinted) and for 2^24 unmasked colours against two
  Colors[1] each: 50,331,648 cases, 0 differences. Its code grows from 807 to 885 bytes (and its unwind funclet's
  frame offset changes). The reference compare and compare_rendering draw sprites through it (orchestrator's live
  check).
- **How the images were compared** (high). Every commit was built at the same path (oracle-x86 Patella.exe,
  windows-x64 Release Patella.exe and PatellaTest.exe) and compared with the commit before it. pecompare reports
  every shifted address after a function that changes size, so three uncommitted scripts (build/logs/issue-42/
  scripts) took over: fndiff2.py lists the functions from the PDB (dbghelp) and compares each function's bytes,
  allowing a difference only where a relocated or RIP-free address names the same function+offset in both images
  or where the instruction (dumpbin) is the same once its addresses are written as function+offset; datacheck.py
  does the same for every other section (pointers and RVAs in .rdata, .data, .pdata, .didat) and compares the
  relocation sites as function+offset; stubcheck.py follows the pointers to the EH stubs and funclets that have no
  symbol (x86 `__ehhandler$`, `.text$x`) and compares their code. Results, per commit:
  - Draw: oracle, only TBattleWindowBody::Draw differs (of 11,427 functions); .rdata/.data: 47,001 pointers and
    17,806 EH stub targets moved to equal code, and the POGO record's `.text$x` size (+0x20, Draw's funclets);
    the relocations differ only inside Draw; headers: entry point, .text size, .reloc size. x64: identical.
  - GL: oracle, only TSurfaceGL::WriteMaskedImage (alignment absorbs its 2 bytes; nothing else moves). x64:
    unchanged (not built there).
  - DX9: oracle, only TSurfaceDX9::WriteMaskedImage; 38 .rdata pointers to the same function+offset. x64: unchanged.
  - Vulkan: oracle identical (not built there); x64 Patella.exe and PatellaTest.exe, only
    TSurfaceVulkan::WriteMaskedImage and its unwind funclet (of 16,277 / 16,696 functions); every other difference
    is a moved RVA or pointer to the same function+offset, apart from that function's own .pdata end and its FH4
    EH information.
  - The ProcessEvents comment and the NOLINT: all images identical apart from the link stamps and PDB record.
  macrocheck f2b3374 against the branch: the 2,108 calls unchanged (no goto was inside a macro argument).
- **cppcoreguidelines-avoid-goto in .clang-tidy** (medium). It reports the kept goto (the check accepts a jump out
  of a loop, not out of a switch inside one), hence the NOLINT; nothing else on windows-x64's sources. The oracle's
  own sources (SurfaceGL.cpp, surfaceDX9.cpp, the DirectDraw files) have no goto left. The clang-tidy baseline is
  unchanged (14).
- **Nothing odd found** (medium). The behaviour kept as it is: party flag 1 draws no shield while the player is in a
  party, and a skull flag the switch does not know adds no offset.

## MSVC-only spellings behind portable functions (Patella-Modern issue #43)

Settled by Claude Opus 5.5, November 4, 2026. The twelfth change of M3 (plan section 7), by hand: src/ uses no
spelling of the Microsoft CRT or compiler outside src/compat/ apart from the Windows platform's own (below). It
prepares M8 (the client on Linux); nothing is ported here.

- **What was there** (high). A scan of src/ (not src/Extern) for the Microsoft CRT's underscore functions and the
  compiler's keywords, pragmas and macros: `_snprintf` 52 (Help.cpp 30, Dialogs.cpp/.h 8, Control.cpp 5, Map.cpp 3,
  Status.cpp 2, ContentWindows, ContextMenus, Preferences, Script 1 each), `_stricmp` 6 (Player.cpp's command line,
  Creatures.cpp's buddy lookup, Dialogs.h's qsort comparison), `_strdup` 2, `_access` (sysinc.cpp FileExists),
  `_eof(_fileno())` (Script.cpp TReadBinaryFile::eof), `_execl` (Player.cpp, patch.exe), `__time32_t` 9 with
  `_time32`, `_ctime32` and `_localtime32` (the 4-byte times of Creatures.h and Talk.h, TestHooks.h's
  PATELLA_TIME32), `_time64` (the test mode's system clock), `_dupenv_s` 2 and `_putenv_s` (the Vulkan engine's
  environment), `__cdecl` 13 (the patcap frame observers' types: SurfaceVulkan.h, TestHooks.h, TestModeWin32.cpp),
  `#pragma warning` 25 (ConsoleWindow, ContentWindows, MainFrm, Player, Talk) and Talk.cpp's two `_MSC_VER` tests,
  `<io.h>` and `<process.h>`. No `__int64`, `__forceinline`, `__declspec`, `_vsnprintf`, `_strnicmp` or `_itoa`.
- **Where the line is: the Windows platform stays** (high). Not MSVC-only spellings but the Windows API and its
  configuration, which M8 replaces with SDL3 rather than wraps: WINAPI/CALLBACK/APIENTRY on callbacks and imports
  (their convention is part of the type on x86), `_CRT_SECURE_NO_WARNINGS` and `_WINSOCK_DEPRECATED_NO_WARNINGS`
  in common.h (settings of the Windows SDK's headers, before MFC), `_M_IX86` (common.h's original-layout test),
  sysinc.cpp's `#pragma comment(lib, "winmm.lib")` (a file of Windows calls: timeGetTime, the clipboard) and the
  test mode's `_wfopen_s` (4 calls in TestModeWin32.cpp, the Windows half of the test mode: UTF-16 path names of
  std::filesystem::path; the Linux test mode is its own file in M8). The comment in DialogElements.cpp that says
  the binary's varargs constructor is __cdecl describes the binary; it is not a spelling in use.
- **Three headers in src/compat/** (high). crt71.h is MSVC 7.1's CRT where the UCRT differs and mfc_shim.h a
  stand-in for MFC; neither is this. `compat/msvc.h` (included by common.h, IWYU export) has the names,
  `PATELLA_CDECL`, `PATELLA_MSVC_WARNING(...)` (`__pragma(warning(...))` on MSVC, nothing elsewhere; in
  .clang-format's StatementMacros) and `Time32`; `compat/msvc_io.h` `MsvcAccess` and `MsvcEof` (below);
  `compat/msvc_portable.h` the implementations with the C library in `patella::compat` (`PortableVsnprintf`,
  `PortableSnprintf`, `PortableStricmp`, `PortableStrdup`, `PortableTime32`, `PortableCtime32`,
  `PortableLocaltime32`), which msvc.h includes only off Windows. The names say whose semantics they keep:
  `MsvcSnprintf`, `MsvcVsnprintf`, `MsvcStricmp`, `MsvcStrdup`, `MsvcTime32`, `MsvcCtime32`, `MsvcLocaltime32`,
  `MsvcFileno`, `MsvcExecl(Path, Argument)`, `MsvcDupenv`, `MsvcPutenv`, `MsvcAccess`, `MsvcEof`. clang-format has
  no column limit, so no call was rewrapped.
- **The Microsoft semantics, not the standard ones** (high). From the UCRT's sources (Windows Kits 10.0.26100.0):
  `_snprintf`/`_vsnprintf` (stdio/output.cpp with LEGACY_VSPRINTF_NULL_TERMINATION): a text shorter than the count
  gets its NUL and returns its length; exactly the count, no NUL and the count; longer, the first count characters,
  no NUL, -1; a null buffer with count 0 returns the length. `_stricmp` (string/stricmp.cpp, the "C" locale: the
  client never calls setlocale): A-Z folded to lower case only when a pair differs, bytes compared unsigned, the
  difference returned ("abc_" < "ABCD"; bytes from 0x80 not folded). `_strdup(NULL)` is NULL. `_time32` -1 after
  2038-01-19 03:14:07; `_ctime32` and `_localtime32` NULL for a time before 1970. `_eof` compares the descriptor's
  own position with lseek(SEEK_END), equal only, and puts it back (lowio/eof.cpp; not the FILE's position, which the
  buffer may have read ahead). `_putenv_s` with an empty value removes the variable; `_dupenv_s` gives NULL and 0
  for a variable that is not set. `_access`'s modes 0, 2, 4, 6 are POSIX's F_OK, W_OK, R_OK.
- **On Windows each name is the Microsoft function's own: a macro** (high). The issue suggested forwarders that
  inline away; measured, they do not keep the images:
  - a variadic template forwarding to `_snprintf` changes the code even inlined: MSVC x64 copies SetCharacter's
    four uint8 arguments before their promotion to int, 16 more bytes of Status.cpp code with the arguments by
    value, 32 by reference. A C varargs function cannot pass its arguments on to `_snprintf` at all.
  - plain inline forwarders inline away, but their declarations in every translation unit move data: MSVC emits a
    translation unit's globals in an order that depends on the names it declares, and with the forwarders (or the
    portable implementations) in common.h, Surface.cpp's globals came out rotated (its .bss in the oracle,
    `curEngine` after `Data71D0D8`; its .data on x64, `CommandLineEngine` after `CacheSurfaceHeight`). Adding one
    unrelated inline function to the header rotated them back: an artefact of the compiler's tables, not of the
    code. Surface.cpp itself did not change.
  So on Windows (MSVC and mingw-w64, whose CRT is Microsoft's) msvc.h defines `#define MsvcSnprintf _snprintf` and
  so on (`MsvcExecl(Path, Argument)` adds the terminating nullptr): every call is the call it was, and the client's
  translation units declare nothing new apart from the alias `Time32`. Off Windows the names are inline functions
  over the portable implementations and POSIX.
- **<io.h> only where it was** (high). With `<io.h>` included through common.h, MSVC x64 emitted the `<charconv>`
  digit table (`std::_Digit_pairs<char>`, 200 bytes, a COMDAT in every object that includes `<string>`) first among
  Communication.obj's constants instead of 154th, which moved it and every constant after it in the x64 images
  (pecompare: about 104,000 differing ranges; issue #40's move of the same table, the other way round). So `_access`
  and `_eof` are in `compat/msvc_io.h`, which includes `<io.h>` on Windows and is included by sysinc.cpp and
  Script.cpp exactly where they included `<io.h>`. Player.cpp includes compat/msvc.h where `<process.h>` was. No
  file's line count changed except Talk.cpp's, which lost its two `#if defined(_MSC_VER)` pairs.
- **How it was found** (high). pecompare reported the differences; a byte-alignment scan of `.text` found where code
  grew (the PDB named Status.cpp's neighbourhood), dbghelp named the moved globals, and single translation units
  compiled by hand with the preset's command lines (`ninja -t commands`) against the bc7417c sources (`git archive`)
  and variants of the header showed which part of the header moved what: the section order of
  `std::_Digit_pairs<char>` in Communication.obj, the symbol offsets of Surface.obj's globals (scripts not committed).
- **Calling conventions** (high). `PATELLA_CDECL` is `__cdecl` on MSVC x86 and nothing elsewhere (x64 and Linux
  have one convention; a unit test asserts that the observer types are then the plain function pointer types). The
  frame observers are called through GetProcAddress pointers of patcap.dll (x64 only today), so their type keeps the
  DLL's convention wherever there is more than one. No qsort or atexit callback in src/ carried `__cdecl`
  (Dialogs.h's CompareNames has the default convention, which is cdecl on x86).
- **Renderer.cpp's environment without `#if defined(_WIN32)`** (medium). TDriverFiles::Get and Set had a Windows
  branch (`_dupenv_s`, `_putenv_s`) and a POSIX one (getenv, setenv/unsetenv); with MsvcDupenv and MsvcPutenv both
  are one. On Windows the code is the same text. Off Windows Set now removes a variable whose saved value was empty
  (`_putenv_s`'s meaning, which the Windows client has), where the old POSIX branch set it empty; Renderer.cpp is
  not built off Windows yet (patella_render_vulkan is Windows-only until M8).
- **The test mode's system clock** (high). TestModeWin32.cpp's `_time64(nullptr)` is `std::time(nullptr)`: the
  UCRT's `time` is an inline function calling `_time64` (64-bit time_t on x64), so PatellaTest.exe is unchanged.
- **Unit tests: tests_core's core_msvc_test.cpp** (high). Eight cases: `_snprintf`'s outcomes (fits, exactly the
  count, longer, count 0 with and without a text, a null buffer) and the client's own formats (Control.cpp's frame
  rate with a float, Status.cpp's address, chars); `_vsnprintf` through a va_list; `_stricmp` against exact
  differences (lower-case folding, '[' against 'a', Latin-1 0xC4 against 0xE4, 0x80); `_strdup` and NULL; the 32-bit
  time (2038, before 1970, ctime's text and localtime's fields); `_access` and `_eof` on an unbuffered temporary
  file (the position before, at and after the end, and kept); `_dupenv_s`/`_putenv_s` (set, read, removed by an
  empty value); PATELLA_CDECL and PATELLA_MSVC_WARNING. On Windows every formatting, comparison, copy and time case
  runs both the Microsoft function and the portable implementation against the same expected values, so the
  portable code is held against the UCRT itself; on Linux (GCC, and Clang with ASan and UBSan) the Msvc* names are
  the portable code, with three more EINVAL checks that the UCRT would send to its invalid parameter handler (no
  case passes such an argument on Windows, so no dialog can open). `_execl` has no test (it replaces the process).
  tests_core: 25 cases / 216 assertions before, 33 / 340 on Windows (MSVC and mingw-w64) and 33 / 343 on Linux.
- **clang-tidy** (high). CI's clang-tidy (22.1.8 in the image, linux-x64-clang's commands, which compile the headers
  through patella_core) reported the three `va_list` declarations of the portable formatting functions
  (cppcoreguidelines-pro-type-vararg): NOLINT with the reason, `_snprintf`'s interface. The baseline is unchanged
  (14 findings, 0 new, 0 gone). Visual Studio's clang-tidy on windows-x64's commands of the changed files reports no
  include-cleaner finding; the `_snprintf` calls are vararg calls as before (the Windows targets are not in the
  baseline).
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built from
  main bc7417c and from the branch at the same path, differ only in the link time stamps and the PDB record
  (pecompare: 0 unexpected differences, all three). macrocheck bc7417c issue-43: the 2,108 calls unchanged (no
  spelling was inside a TASSERT/TERROR/TEND argument, so none waits for M5). The difftest counts are unchanged.
- **Not done** (medium). tests/ and tools/ keep their own spellings (src/ only, as in #35 to #42). The format strings
  that go through MsvcSnprintf use only `%s`, `%d`, `%c` and `%.02f`, which the UCRT and glibc format alike; the
  test mode's `%lld` and `%llx` are standard.

## File-scope static to anonymous namespaces (Patella-Modern issue #44)

Settled by Claude Opus 5.5, November 4, 2026. The thirteenth change of M3 (plan section 7), one hand pattern: every
file-scope `static` of src/ (not src/Extern) is in an anonymous namespace, but three, which keep the images' data.

- **What was there** (high). 498 file-scope `static` declarations: 349 in 30 .cpp files (Status.cpp 52,
  Preferences.cpp 48, Creatures.cpp 39, Map.cpp 36, Network.cpp 22, ...; 39 of them functions, the rest data, among
  them the ten `TCallback NoCallback` of the window files) and 149 in headers (Text.h's 147 text pointers, Status.h's
  LinkPattern, TRSA.h's HeaderExponent). The test mode, src/settings and src/vulkan's other files already used
  anonymous namespaces; SurfaceVulkan.cpp had 9 statics. Static data members and function-local statics are not
  file-scope statics and stay.
- **Who looks the names up** (high). Checked before the change:
  - tools/client_state.py: `ORIGINAL_GLOBALS` (lines 65-99) names 33 globals that `Pdb.global_symbol` (line 576)
    finds with SymEnumSymbols by exact name; 30 of them were statics (Creatures.cpp's PlayerState ... PlayerID and
    Creatures, Inventory.cpp's Body, Containers, OwnOffer, CounterOffer, Map.cpp's MapX/Y/Z and Fields, Status.cpp's
    ConnectionState, Characters, AccountCharacters). compare_traffic.py, compare_files.py (line 494's
    `mem.var("PlayerID")`), compare_rendering.py, compare_network.py, testmode_check.py and reference_record.py reach
    them only through client_state.py.
  - The test mode (src/testmode/ClientView.h, ClientStateWin32.cpp, the state dump) reads them through the View
    functions at the end of Creatures.cpp, Inventory.cpp, Map.cpp and Status.cpp: C++ names in the same translation
    unit, no lookup.
  - tools/patch_client.py finds the server list by its bytes, not by a symbol; reference.py's compare reads the test
    build's dumps; patcap.c resolves only its own exports.
  - The difftest: no `extern` declaration and no `/alternatename` names a static (its alternatenames are the
    constructors of classes); networktest.cpp and mainfrmtest.cpp `#include` Network.cpp and MainFrm.cpp and use
    their statics by name in the same translation unit, which an anonymous namespace keeps visible. The typed calls
    into the image use the image's addresses.
  So nothing needed a static's decorated name, and nothing became an exception for a tool.
- **The PDB keeps the data's names** (high). Contrary to the expectation in the issue, MSVC (14.51, x86 and x64)
  writes a variable of an anonymous namespace into the PDB under its plain name (`PlayerState`, `Hotkeys`,
  `TextPoisoned`, S_LDATA32 as before); only functions get `` `anonymous namespace'::Name ``. client_state.py's
  lookups therefore find the same symbols, and `client_state.py --layout` prints the same table for the oracle
  Patella.exe, the x64 Patella.exe and PatellaTest.exe as main's builds (33 globals, 105 fields, 0 problems). No
  tool changed; a tool that one day looks up a static function by name has to add the prefix.
- **How** (high). A script (not committed) took each `static` at column 0 to the end of its declaration or function
  body, removed the keyword and put each run of such items, with the comment lines directly above it, into
  `namespace {` ... `} // namespace`. A run ends at anything that is not a blank or comment line, so an external
  global inside a data table (Talk.cpp's DefaultTextBuffer, GUI.cpp's SkinProperties) stays where it is, between
  two namespace blocks, and every table keeps its order. `static inline` became `inline`. clang-format then
  realigned the trailing comments. No TASSERT/TERROR/TEND argument contained a changed line.
- **Three stay static: MSVC orders the globals by their names** (high). As issue #43 found, MSVC emits a translation
  unit's globals in an order that depends on their decorated names, and the anonymous namespace changes those
  (`?Name@?A0x<hash>@@...`). With everything converted the images differed in moved data only, the same in the
  oracle and on x64, found by comparing every symbol's address in the two PDBs (dbghelp, script not committed):
  - GUI.cpp: Hotkeys came before SkinProperties in .bss (on x64 NumberOfHotkeys moved with it);
  - Talk.cpp: TemporaryIgnoreList and IgnoreList changed places;
  - TRSA.h: TRSA.obj's HeaderExponent came before TRSA.cpp's PublicExponent (the image's order, but not the builds').
  Each was tried apart: Hotkeys static (NumberOfHotkeys and Timers in the namespace), the two lists static
  (TalkThrottle, BuddyExemptionList and ConsoleFilterList in the namespace) and HeaderExponent static give images
  identical to main's. Rather than keep whole files static, only these keep the keyword, each with a comment and,
  in the .cpp files, `NOLINT(misc-use-anonymous-namespace)`. Every other file, Text.h and Status.h included, moved
  nothing.
- **The headers** (high). Text.h's table and Status.h's LinkPattern are in an anonymous namespace: internal linkage
  as before, so every object that includes them still has its own copy, in the same order, as the image has
  (16 copies of the text table, 17 of LinkPattern). The ODR is as it was: Dialogs.h's inline TReconnectBody::SetTime
  reads TextTryingToReconnect, a different object in every translation unit, with `static` as with the namespace
  (below).
- **clang-tidy** (high). `misc-use-anonymous-namespace` is in .clang-tidy, so a new file-scope static is reported. It
  offers no fixes and does not look at headers. Visual Studio's clang-tidy on windows-x64's commands (`--all`, the
  check alone) reports exactly the three kept statics before their NOLINTs; the oracle-only engines (SurfaceDX.cpp,
  surfaceDX9.cpp, SurfaceGL.cpp) are not in any compile_commands.json and were checked by a scan for `static` at
  file scope. CI's clang-tidy (22.1.8, the image): 14 findings, 0 new, 0 gone; the baseline is unchanged (its
  bugprone-throwing-static-initialization entries name PublicExponent and HeaderExponent without their namespace).
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built from
  main 9312a61 and from the branch at the same path, differ only in the link time stamps and the PDB record
  (pecompare: 0 unexpected differences, all three; every symbol at the same address). macrocheck 9312a61 issue-44:
  the 2,108 calls unchanged. The difftest counts are unchanged.
- **Not done** (medium). tests/ and tools/ keep their statics (src/ only, as in #35 to #43). The inline
  TReconnectBody::SetTime in Dialogs.h, which reads Text.h's per-object TextTryingToReconnect, refers to a different
  object in every translation unit (an ODR violation since the reconstruction, harmless: all copies hold the same
  pointer); moving it out of the header is not this issue's. clang-tidy's
  misc-use-internal-linkage (not enabled) reports Network.cpp's TSocket class and Renderer.cpp's ListGpus and
  BestGpu, which have no `static` to move.

## The local clang-tidy check that CI agrees with (Patella-Modern issue #51)

Settled by Claude Opus 5.5, November 4, 2026. Issue #39 found that Visual Studio's clang-tidy 22.1.3 on the Windows
targets' commands missed 11 misc-const-correctness findings in src/testmode that the hosted job (clang-tidy 22.1.8 of
the Docker image, linux-x64-clang's commands) reported. tools/tidy_ci.py now runs the hosted job itself.

- **The cause is the compile commands, not the version** (high). At 4fe3e83 (the commit before #39's fix) the image
  reports the 11 (`std::size_t` locals in InputScript.cpp, PacketLog.cpp, Png.cpp, ServerReplay.cpp); 22.1.3 on
  windows-x64's commands of the same four files reports none. A probe (build/logs/issue-51, not committed) run in
  both: `std::size_t End = Text.find('\n');` is reported by 22.1.8 with libstdc++ and not by 22.1.3 with MSVC's
  headers, while the same local as `unsigned long long` is reported by both. Without any header, a typedef brought
  into a namespace by a using-declaration (`typedef unsigned long long G; namespace ns { using ::G; }`, a local
  `ns::G End`) is reported by neither 22.1.3 nor 22.1.8, the same typedef declared in the namespace by both. MSVC's
  `<cstddef>` has `using _CSTD size_t;` (`using ::size_t;`), libstdc++ `typedef __SIZE_TYPE__ size_t;` in namespace
  std. So misc-const-correctness skips a local whose type is named through a using-declaration, in both versions,
  and on MSVC's headers that is every `std::size_t` local. Other checks may differ the same way (the Windows targets
  also see the `#ifdef _WIN32` branches the image never compiles); the version difference itself was not found to
  matter.
- **tools/tidy_ci.py, not the build.py command** (high). The documented `python -I tools/build.py --env
  linux-x64-clang bash -c "cmake --preset linux-x64-clang && python3 -I tools/tidy.py ..."` ran the right clang-tidy
  on the right commands, but not exactly the job: it checks the working tree (untracked files too), reuses
  build\linux-x64-clang with its CMakeCache.txt from earlier runs, and uses the locally tagged image, which may
  predate a change of the Dockerfile. tidy_ci.py does what the job does: it builds the image from the commit's
  docker/linux-x64 (`git archive` of that directory as the context, untagged; Docker's cache makes it seconds),
  streams `git archive <rev>` into /work of a new container (case-sensitive, no build tree, no CipSoft file) and
  runs the script of the job's tidy.py step, read from the commit's hosted.yml, so the two cannot drift apart.
  `--worktree` checks uncommitted changes the same way (a temporary index of the files git knows). `--hosted <run
  id>` fetches the job's log (GitHub API, available as soon as the job is done) and compares the clang-tidy version
  line, every finding line and every tidy.py summary line, in order; `--compare` does it on two saved outputs. No
  new dependency: Docker and gh were already used. A pinned Windows clang-tidy 22.1.8 was not chosen: it would be a
  new download, and it would still run on MSVC's headers, i.e. not report what the job reports.
- **Both checks, for different files** (high). The per-target Windows runs (11 targets, Visual Studio's clang-tidy)
  stay the only check of the code Linux does not compile (MFC, DirectX, OpenGL, Win32, the oracle) and of the Windows
  branches of shared files; an M3 step runs them for its check and fixes what they report. tidy_ci.py is the gate a
  change must pass before it is pushed: it is the hosted job. A finding of either is fixed (or, for the image, put in
  the baseline on purpose).
- **Proof** (high). On 4fe3e83 (pushed as issue-51-scratch, hosted run 41758724203, its clang-tidy job failed) and on
  issue-51's head (hosted run below), `python -I tools/tidy_ci.py --rev <commit> --hosted <run>` reported "identical
  to the hosted job": at 4fe3e83 58 lines (the 14 baseline findings and the 11 new ones, their repetition in the "new"
  list, the summary: 25 findings, 11 new), at the head the 14 findings, 0 new, 0 gone. Comparing the head's output
  with 4fe3e83's hosted log reports the difference (exit 3), so the comparison is not vacuous. Logs in
  build/logs/issue-51 (not committed).

## One presentation window per device (Patella-Modern issue #52)

Settled by Claude Opus 5.5, November 4, 2026, after the user's repro, a full dump of the frozen client and the
orchestrator's live inspection. notes/vulkan.md, "One window per device (issue #52)" has the facts.

- **What froze** (high). Not the client: in the dump and live it drew about 1,000 frames a second, pumped its
  messages and every Present succeeded on the one swapchain it had (the window's client size, not stale). The screen
  kept the last frame of the AMD Radeon iGPU, whose swapchain presents cross-adapter on this machine; resizing the
  window stretched that picture. "X does nothing" is the original's in-game refusal with a dialog open; the
  "Application Hang" reports came from Task Manager's End task. My first two readings (a cross-process wait, then an
  endless loop in the map drawing) were wrong: the hang reports' XProc key and the frozen FPS counters misled them.
- **Why** (medium: the binding is outside the process and cannot be inspected from it). Every device presented into
  the main window. The Radeon's device, swapchain, surface and instance were destroyed in order after
  vkDeviceWaitIdle, and nothing of them was left in the process, but its presentation stayed bound to the window, and
  the compositor showed it over every later device's swapchain there. No validation layers on this machine to check
  the teardown with; the teardown order matches the specification.
- **The change** (high). One child window per device, which the device's surface uses, never the main window
  (src/vulkan/PresentWindow.h). Destroying a window releases every presentation bound to it, by whichever driver, so
  this needs no knowledge of the AMD driver's leftover and holds for any device and any order of switches. The old
  device's window is kept until the new device's first frame (its last picture stays on screen meanwhile, no blank),
  then destroyed; a window that never showed a frame goes at once. The device test uses a hidden window of its own,
  so it binds nothing to a window that shows frames. Mouse messages pass through (HTTRANSPARENT), so the client's
  input is unchanged.
- **Not taken** (high). A wait (DwmFlush or a sleep) between devices: it would bet on the compositor releasing in
  time, and a hang must not be hidden by a timeout. Destroying and recreating the main window: MFC's frame, its
  position, the client's input and the test mode hang on it. Keeping one device across clicks (only the last one
  applied): the user's choice is applied at once (issue #31), and the Radeon's leftover would still cover a later
  device. Present fences (VK_EXT_swapchain_maintenance1) before destroying the swapchain: the leftover is not ours to
  wait for.
- **Behaviour** (high). The frames, the files and the packets are unchanged (nothing reads the window but the
  swapchain); a single switch draws the same frames; on screen, the old picture stays until the new device's first
  frame. Not in the oracle-x86 build (src/vulkan): the oracle Patella.exe is unchanged (pecompare).
- **The test** (high). tests_vulkan's "quick device switches" runs the client's switch sequence through
  TPresentWindows on every device of the machine, including switches before the device exists. It cannot reproduce
  the stale picture (a hidden window is not composed); it checks what makes it impossible. The live check is the
  user's repro.
- **The lost scripted click** (medium: the desktop event itself was not seen). The reference compare after the
  change lost one scripted click (session traffic, "Graphics"). The device windows cannot take it: the test mode
  dispatches to the main window, and a held click survives every window operation of the engine (tests_vulkan,
  "scripted clicks reach the main window while device windows come and go": no WM_CAPTURECHANGED). The click is
  lost when the main window loses the capture between press and release; the desktop does that (another window
  activated; the run before had met the user's own client still running), and the test mode let it through, as
  it is a sent message, not queued input. With a script the test mode now skips OnCaptureChanged
  (PATELLA_TEST_CAPTURE_CHANGED; nothing in Patella.exe and the oracle, pecompare). Not taken: WS_DISABLED or
  WS_EX_TRANSPARENT device windows (they change nothing for dispatched input), the test mode hit-testing (it does
  not), forwarding messages from the device windows (they receive none).
- **Not done** (medium). Each CPU-driver device leaves about 56 process handles (lavapipe's ended threads); its own
  issue (#53: lavapipe's and NVIDIA's, not ours; "Handles the drivers leave" below).

## Handles the drivers leave (Patella-Modern issue #53)

Settled by Claude Opus 5.5, November 4, 2026. notes/vulkan.md, "Handles the drivers leave (issue #53)" has the
facts and the census.

- **Whose** (high). None of ours: the census by object type shows only Thread handles growing with CPU-driver
  devices and Event handles with NVIDIA devices; every other type stays flat over 25 devices. The Thread handles are
  of ended threads that vulkan_lvp.dll started (their start address in its image, 24 per device, 4 with
  `LP_NUM_THREADS=4`) and one handle of the calling thread per lavapipe instance (also when only listing its
  device); Mesa 26.2.5's source shows why (lp_rast.c's `lp_rast_destroy` never joins its threads on Windows,
  threads_win32.c's `thrd_current` duplicates a handle closed only at that thread's end). The NVIDIA driver's 3
  events per device: our code is the same for every device, the Radeon and lavapipe leave none, and they stay with
  the implicit layers disabled. A handle trace (cdb `!htrace -diff`) gave no stacks, only that all were opened on
  the creating thread; the census is the evidence.
- **Not worked around** (high). The cost is small (a handle and an ended thread's object, a few KB, per lavapipe
  thread; 50 handles per switch to the CPU driver, far below the per-process limit) and every workaround changes
  what the plan settles or is fragile:
  - keeping one lavapipe instance and device alive across switches: TestEngine and the surfaces' device are two
    devices (and the device test must test a new one); a kept device keeps its 49 threads and its memory while a GPU
    is in use, and it would not end the device test's leak. It changes the switch's lifetime design (#31, #52);
  - keeping vulkan_lvp.dll loaded (a LoadLibrary of our own): it would save only the one `thrd_current` handle per
    instance (the thread-local state survives), not the rasterizers' 24;
  - `LP_NUM_THREADS=0` (rasterizing on the calling thread): no rasterizer threads, but the CPU device becomes
    many times slower; a behaviour change of the device the user chose;
  - closing the ended threads' handles ourselves after the device is gone (those started in the unloaded
    vulkan_lvp.dll): closing handles we do not own, found through ntdll's undocumented handle list;
  - patching or upgrading the driver: the user's decision (the pin), and the fix belongs upstream (Mesa's
    lp_rast.c: close the handle, e.g. `thrd_detach` after the wait, on Windows). Not reported upstream from here.
- **The test** (high). tests_vulkan's "switching to and from the CPU driver leaves no handles of ours" (headless,
  no window): 5 rounds of the CPU driver and each GPU and back, after one round of warm-up; the drivers' handles
  are counted apart and bounded (the ended threads all lavapipe's and the same number per device, at most 32; at
  most one handle of this thread per lavapipe instance; at most 3 events per NVIDIA device), everything else flat
  within 4 (system objects such as ALPC ports varied by 2 with no device in between; below the 5 rounds, so one
  handle of ours per device fails). Checked by injecting a leaked event per device: the test failed (25 more
  than the drivers').
- **Behaviour** (high). Nothing in src/ changed; the oracle-x86 build compiles nothing that changed.

## Each header includes what it uses: the headers checked on their own (Patella-Modern issue #49)

Settled by Claude Opus 5.5, November 4, 2026. The second half of issue #40: every header of src/ (not src/Extern)
compiles on its own, the build checks it on every preset, and misc-include-cleaner is clean on each header as its
own main file, on every target's settings. The images are unchanged.

- **The compile check: generated one-line sources in the build** (high). cmake/HeaderCheck.cmake writes
  `build/<preset>/header-check/<target>/<header>.cpp` (`#include "<header>"`, file(CONFIGURE), rewritten only when
  it changes) and compiles them in object libraries that are never linked, with the settings of a target that uses
  the header, once per target whose definitions change what the header says (common.h's PATELLA_CORE,
  TIBIA_MFC_SHIM, PATELLA_VULKAN and PATELLA_TEST_MODE): header_check_core (patella_core's),
  header_check_cxx26_standin (PATELLA_CXX26_STANDIN), header_check_testmode and header_check_render (tests_render's)
  on every product preset; header_check_client (Patella.exe's, patella_client_compile_settings) and
  header_check_test_build on windows-x64, with PATELLA_CLIENT_COMPILE_ONLY too; header_check_oracle (the oracle
  Patella.exe's properties copied: real MFC, C++17, x86, /Zc:strictStrings-) and header_check_oracle_shim
  (mfc_shim_check's) on oracle-x86. They are in the default build, so tools/build.py's workflows and the hosted Linux
  and windows-msvc jobs build them; the hosted windows-test-build job, which builds named targets, builds the
  client's two in a step of its own. No test: a header that does not compile fails the build, as any source does,
  and its warnings fail it too.
- **Every header on a list** (high). The lists name the headers by the targets that include them; the configure fails
  on a header of src/ that is on none (and on a listed header that does not exist), so a new header is checked from
  its first commit. PATELLA_HEADER_CHECK_EXCEPTIONS (a reason each) is empty: no header of src/ is an inline
  fragment or .inc file, and the shaders' SPIR-V headers (ColorKeyFrag.h and the others) are generated by glslang
  into the build tree (cmake/Vulkan.cmake), not headers of src/.
- **Not self-contained before, fixed** (high). TestHooks.h (test mode: DWORD, HWND, MSG, Time32 and PATELLA_CDECL
  came from common.h before it) includes `<afxwin.h>` (MFC, as common.h does; the test build has real MFC, never the
  shim) and compat/msvc.h in its PATELLA_TEST_MODE branch. SurfaceDX.h: real MFC's d3d9types.h sets
  DIRECT3D_VERSION to 0x0900 unless it is defined, so the header defines 0x0700 itself before its first include when
  the includer has not (SurfaceDX.cpp and Surface.cpp still define it first; the #error stays for another value).
  compat/msvc_portable.h off Windows: it included compat/msvc.h for PATELLA_PRINTF_FORMAT and Time32, and msvc.h
  includes it there before its Msvc* functions, which call it; included first, the functions came before what they
  call. msvc.h's off-Windows Msvc* functions are now at the end of msvc_portable.h (behind `#if !defined(_WIN32)`,
  with the POSIX headers they need), msvc.h includes it there with `IWYU pragma: export`, and msvc.h's Windows text
  is as it was. A first fix moved the spellings and Time32 into a header of their own (compat/msvc_base.h, exported
  by msvc.h): the client's translation units then declared the same names in the same order from another file, and
  PatellaTest.exe's .data still moved (pecompare: 287 ranges; Patella.exe unchanged), so that split was dropped.
- **Includes missing from the headers, added** (high), after the header's own includes as #40 placed them (no header
  comes earlier in any translation unit than before): ConsoleWindow.h graphics.h (DefaultFontColor) and `<cstddef>`
  (the NULL of a TASSERT argument: the include is added, the argument is untouched); DialogElements.h, Script.h and
  Status.h `<cstring>`; Script.h and Talk.h `<cstdio>` (FILE, fclose); DialogWindow.h graphics.h (TRect); MapWindow.h
  Objects.h and graphics.h; TRSA.h, SurfaceDX.h, SurfaceGL.h and surfaceDX9.h common.h (uint8, uint32);
  vulkan/Renderer.h `<cstddef>`; vulkan/SurfaceVulkan.h common.h, vulkan/Quads.h (TQuad, TFilter), `<cstddef>` and
  `<cstdint>` (src/vulkan: includes only); compat/msvc_io.h off Windows `<cstdio>` (SEEK_*) and `<stdio.h>` for off_t
  (NOLINT(modernize-deprecated-headers): POSIX declares it there, include-cleaner's mapping asks for it and does
  not take `<sys/types.h>`); compat/msvc_portable.h off Windows `<stdio.h>` and `<stdlib.h>` (fileno, setenv,
  unsetenv: POSIX's, in the C headers; NOLINT likewise).
- **Includes a header does not use, removed** (high): Communication.h `<sstream>`; ContentWindows.h Container.h and
  Objects.h; Creatures.h Preferences.h and Objects.h; GUI.h, Preferences.h and Status.h Container.h; Talk.h
  `<ctime>`; compat/msvc.h `<unistd.h>` (its users moved to msvc_portable.h); compat/msvc_io.h `<sys/types.h>`; and
  common.h in DialogWindow.h, Help.h, Inventory.h, MainWindow.h, Preferences.h, Skins.h and sysinc.h, which use
  nothing of it and compile alone without it. Two .cpp files relied on that: sysinc.cpp (its `<io.h>` and
  `<mmsystem.h>` came after sysinc.h's common.h; without it the Windows SDK's mciapi.h does not parse) includes
  common.h itself, moved from after `<mmsystem.h>` to before compat/msvc_io.h; Skins.cpp, whose only include was
  Skins.h, includes common.h first (below). The difftest's guitest.cpp, which uses Container.h's vector template,
  includes Container.h itself (GUI.h did). No removed include led to Text.h or Status.h, so no object gains or
  loses their per-object tables.
- **Kept for the images, each measured** (high). Every changed file was bisected on the x64 Patella.exe and
  PatellaTest.exe (main's version of every changed file but one, built at the same path, pecompare; build/logs/issue-49,
  bisect-x64.log and bisect-x64test.log, not committed), then the oracle compared:
  - Skins.h's common.h: without it Skins.obj has no MFC and loses MFC's and ATL's per-object statics and initialisers
    (.data, .CRT$XCU, ATL$__a/z; dumpbin /summary), and the images' .data moves. Skins.cpp includes common.h before
    Skins.h (the translation unit as before) with `IWYU pragma: keep` and the reason; Skins.h, which uses nothing of
    it, does not.
  - Communication.h's Talk.h: without it Communication.obj's .bss comes out in another order (OutfitRequested after
    the ATL statics; the x64 .data 16 bytes longer, 18,265 differing ranges), the artefact issue #43 found: MSVC
    orders an object's globals by the names its translation unit declares. Kept with `IWYU pragma: keep` and that
    reason; its `<sstream>` goes (Talk.h kept: identical).
  - common.h's `<string>`, `<cstring>`, `<cstdio>`, `<cstdlib>`: every file that uses them includes them itself (#40),
    but without them in common.h the test build's objects place their globals otherwise (PatellaTest.exe: 62 ranges;
    Patella.exe and the oracle identical). Kept with `IWYU pragma: keep` and a comment.
- **Kept because include-cleaner cannot see the use** (high): Control.h's common.h (HWND: MFC and the Windows headers,
  which IgnoreHeaders leaves out, so the tool calls the include unused; the compile check fails without it);
  ContentWindows.h's common.h (first: without it `<climits>` comes before _CRT_SECURE_NO_WARNINGS and MFC, and
  DialogElements.h's strcpy warns C4996); compat/msvc.h's eight C headers (on Windows the Msvc* names are macros for
  the CRT's functions, and the tool does not look into a macro's body); compat/cxx26.h's `<version>` (the
  feature-test macros of its #if lines).
- **GCC's -Wshadow in Container.h** (high). Container.h alone warns at GCC's full level (`-Wshadow`: the constructors'
  parameters min, max, xmin... shadow members), which is why core_container_test.cpp is on the GCC baseline;
  renaming them would touch TASSERT arguments. Container.h is on PATELLA_WARNING_BASELINE_GCC, and
  patella_target_warnings gives a header check's source (source property PATELLA_HEADER) its header's level. MSVC
  /W4 and Clang build every header at the full level.
- **clang-tidy on the headers: tools/tidy.py --headers** (high). clang-tidy checks the includes of the main file only,
  and the commands it interpolates for a header parse some as C or without the Windows headers. tidy.py --headers
  takes each header check's command from compile_commands.json (the Release one), puts the header in its source's
  place (`-x c++-header` for GCC and Clang; MSVC's commands say /TP), writes a compilation database per check target
  (build/<preset>/header-check/<target>/compile_commands.json) and runs clang-tidy on the header with it. Only
  misc-include-cleaner runs there: the other checks see each header already through its includers
  (HeaderFilterRegex), and on a header alone they also report what no source of the build includes or instantiates
  (Container.h's seven cppcoreguidelines-owning-memory, compat/cxx26.h's bugprone-exception-escape on Linux), which
  would grow the baseline with findings that are not this issue's. The hosted job runs `--all --headers`
  (hosted.yml; tools/tidy_ci.py reads the script from there).
- **Before and after** (high). On main, the headers as main files: 51 findings with windows-x64's header checks
  (Patella's and PatellaTest's settings; one more inside a TASSERT argument), 47 with the oracle's, 13 with
  linux-x64-clang's; after: 0, 0 and 0, none inside a TASSERT/TERROR/TEND argument. misc-include-cleaner on the
  sources of all 11 targets (one compile_commands.json per target, as in #40; the oracle's with `--extra-arg=-m32`):
  0 findings, MainFrm.cpp's two clang-diagnostic errors (the MFC message map, as on main) aside. CI's job
  (tools/tidy_ci.py): 14 findings, 0 new, 0 gone; the baseline is unchanged.
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built from
  main 968ec47 and from the branch at the same path, differ only in the link time stamps and the PDB record
  (pecompare: 0 unexpected differences, all three). macrocheck 968ec47 issue-49: the 2,108 calls unchanged. The
  difftest counts are unchanged.
- **Not done** (medium). What the other checks would report on headers alone (Container.h's owning pointers,
  cxx26.h's ContractViolation) is M4's. The headers of tests/ (tests/golden) are not in the header check (src/ only,
  as #40 to #44).

## The display modes of a reference session (Patella-Modern issue #48)

Settled by Claude Opus 5.5, November 4, 2026. notes/testmode.md, "The display modes (Patella-Modern issue #48)"
has the mechanism.

- **Which list Tibia.exe saw** (high). Tibia.exe recorded every session with Engine = 2 (Tibia.cfg.start), so its
  dialog listed DirectDraw's EnumDisplayModes, which on this machine is EnumDisplaySettings' list filtered as the
  dialog filters it (issue #20's probe: the same 24 modes in the same order). The traffic session's frame 3 shows
  the first seven of them; the options session wrote the first (FullscreenMode = (640,480,32)); on November 4 the
  machine reports the same 24 again, without the 1176x664 mode of November 3. So the 24 are the recording's list,
  certain for the seven shown and for the first, very likely for the rest (nothing in a recording shows them).
- **A fixed list in the test build, from the session** (high). The comparison asks "does the client draw what
  Tibia.exe drew"; the display modes are an input to that, like the server's packets, so the test client gets the
  recording's input (`--test-display-modes`, the hook PATELLA_ENUM_DISPLAY_SETTINGS) and the frame stays exact.
  Recording the list in the state dump and calling a difference an environment difference (the issue's other
  option) would leave the frame and the options session's Tibia.cfg unchecked whenever the machine differs; it
  would also change the state dump's form, which Tibia.exe's dumps (client_state.py) must share. A fixed set in
  the test mode's code for every script: it would be wrong for a session recorded on another machine and would
  hide the real list from the live checks (testmode_check.py) that never asked for one.
- **Where the list lives** (high). In the session's meta.json (`display_modes`), written by the record step from
  EnumDisplaySettings (read before and after the run; a change is a recording problem). The key is optional and
  the sessions without it (the whole set of November 3) get FORMAT1_DISPLAY_MODES, the 24 modes above: no new
  recording, and the format stays `patella-reference 1` (a new format would have made the set stale, and a
  reader before #48 just ignores the key; the compare step is the only reader). The test mode reads a small text
  file rather than meta.json: the client has no JSON reader, and the file is the option's whole contract.
- **What the hook keeps** (high). Only EnumDisplaySettings(nullptr, Index) is answered from the file; the desktop's
  mode (ENUM_CURRENT_SETTINGS, the fullscreen code) and other devices stay the machine's. Without the option the
  test build calls EnumDisplaySettingsA as before, and Patella.exe and the oracle compile the call itself
  (pecompare). The fields set are those GetFullscreenModes reads, plus the frequency (60) and dmFields.
- **A client that ignores it** (high). PatellaTest.exe from before #48 would ignore the unknown option and list the
  machine's modes; the compare step requires the driver's log line "testmode: display modes ..., N modes" when it
  passed the option, and reports its absence or another count as a difference.
- **compare_rendering.py and compare_traffic.py** (high). Their live modes run Tibia.exe and the x64 client on the
  same machine at the same time, so both list the same modes: unchanged. Their `record`/`compare` words go
  through tools/reference.py and get the list there.
- **The test** (high). tests_testmode "DisplayModes: ...": the file's parser, and the dialog's loop
  (GetFullscreenModes' filter) over EnumerateDisplayMode with a made-up machine that reports a 1176x664 mode, a
  16-bit mode, a mode below 640x480 and duplicates: with the fixed list the dialog lists exactly the file and the
  machine is never asked; without it the dialog lists the machine's modes. `reference.py --self-test`: the default
  for sessions without the key (the traffic frame's seven first), meta.json's list winning, the file, malformed
  lists refused, the driver-log check.

## const pointer parameters (Patella-Modern issue #50)

Settled by Claude Opus 5.5, November 4, 2026. Split from issue #39: `readability-non-const-parameter` is clean on src/
and enabled in .clang-tidy. The images are unchanged.

- **What the check reports: four parameters** (high). Run as #39 ran its checks: tools/tidy.py once per target on
  that target's Release compile commands (oracle-x86's Patella, mfc_shim_check, difftest, communicationtest,
  sysinctest and vlongtest with `--extra-arg=-m32`, the preset configured with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`;
  windows-x64's Patella, PatellaTest, patella_core, patella_testmode and patella_render_vulkan). Each of the six
  targets that compile Dialogs.cpp and DialogElements.cpp reported the same four, the other five nothing, and none
  inside a TASSERT/TERROR/TEND argument: CreateTextDialog's `ObjectData` (0x481370), TOutfitDialog's `Data`
  (0x47cf70), TNewChannelDialog's `_ChannelIDs` (0x4733a0) and TOutfitIcon::SetColors's `ColorData` (0x4575a0), all
  `int *` read only. clang-tidy 22's check looks only at pointers to integer and floating types (not to classes or
  pointers), and it takes a parameter passed on to another function or constructor as possibly written, so the
  count is small. A second run after the fix (to catch a function that qualifies once its callee does) reported
  nothing: CreateOutfitDialog and CreateNewChannelDialog, which only hand their `int *` to the two constructors,
  are not reported (the check does not follow the argument into the constructor) and stay as they are.
- **The fix: tidy.py --fix, then clang-format** (high). The check changes the definition and the header's
  declaration together; clang-format re-aligned DialogElements.h's two trailing comments. No `int const` (the
  check writes `const int *`). No virtual function and no API callback is among the four; none has an overload,
  so no call resolves to another function. The parameters' type is not part of any layout.
- **The difftest** (high). It binds image functions by address, not by type: the fake of CreateTextDialog
  (commworld.cpp's `F_CreateTextDialog`, put in the image's place through `F(CreateTextDialog, 0x481370)`, which
  takes both as `void *`) takes `const int *` too, so that it keeps the reconstructed function's type; the tests that
  call the image's SetColors (`CallI<void>(0x4575a0, O, (int *)Colors)`) pass the same pointer as before. Its
  `/alternatename`s name constructors none of the four is (THelpBubble, the context menus, the dialog elements).
- **The decorated names change; nothing looks them up** (high). `PAH` becomes `PBH` in the x86 names (`PEAH` to
  `PEBH` on x64), in the PDBs only. As in #39: client_state.py and the test mode take data by plain names and
  classes by RTTI names, no tool reads a function's decorated name (tools/ has none but RTTI's `.?AV`).
- **Proof** (high). The oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe, built from
  main a338fa8 and from the branch at the same path, differ only in the link time stamps and the PDB record
  (pecompare: 0 unexpected differences; `.text`, `.data`, `.pdata` and `.reloc` identical). macrocheck a338fa8
  issue-50: the 2,108 calls unchanged. All 11 targets with the whole .clang-tidy: no finding of
  readability-non-const-parameter, misc-const-correctness or readability-make-member-function-const.

## The golden corpus: settings, scripts and player data (Patella-Modern issue #26)

Settled by Claude Opus 5.5, November 4, 2026. tests/golden/ (status_tests.cpp, preferences_tests.cpp,
script_tests.cpp, player_tests.cpp, help_tests.cpp, inventory_tests.cpp, talk_tests.cpp, container_tests.cpp,
client_stubs.cpp; golden.h's helpers), tests/golden/CMakeLists.txt, cmake/OracleX86.cmake. notes/reconstruction.md,
"Checking", describes the use; the section on issue #24 above the format.

- **Ported** (high). Nine of the issue's ten tests, each with the oracle's count and 0 failures: Status 565,809,
  Preferences 102,060, Script 458,232, PreferencesFile 112,155, Player 18,442, Help 70,631, Inventory 158,400, Talk
  1,214,929, Container 192,832 (2,893,490 checks). The runner now replays 3,267,008 of difftest's 16,848,752 checks
  and sysinctest's 67. The old test files are gone from tools/difftest (playertest.cpp keeps difftest.exe's link
  stubs of Player.cpp and MainFrm.cpp); every CHECK is the old one, every count and random number too.
- **Errors is not ported** (needs a later issue). Its our side is Map.cpp, Creatures.cpp, Objects.cpp (Tibia.dat),
  graphics.cpp and the engines' start-up besides this issue's files, and it runs after difftest's Objects, Map,
  Creatures, Graphics and GUI tests, whose patches of our functions it reads: "250 comparisons skipped (functions
  redirected by earlier tests)" is part of its count. The runner can replay it only together with those tests (#27,
  #28) or after it is made independent of them.
- **Our side in the runner** (high). The tests call the reconstructed files: golden_client compiles Status, Preferences,
  Script, Talk, Player, MainFrm, Surface, Help and Inventory against the MFC shim with the product's settings
  (cmake/Warnings.cmake, Hardening.cmake), as the oracle's difftest compiles them against the shim. MFC's own headers
  switch C4201 (nameless struct, graphics.h) off for every includer (afx.h); the shim does not, so the runner's
  client targets do (/wd4201). Surface.cpp is compiled with PATELLA_VULKAN, as Patella.exe's (its getters are the
  same code either way). The functions of the files not linked are aborting stubs (client_stubs.cpp), except the two
  constructed at start-up (CWinApp for Player.cpp's theApp, TCallback for MainFrm.cpp's NoCallback; the latter is
  GUI.cpp's body). Considered: the real x64 MFC (the client's: static MBCS MFC needs the static CRT the runner and
  patella_core do not use); every client file (the stubs then grow to the GUI's fakes of difftest).
- **sysinc.cpp's four recorders** (high). difftest.exe keeps sysinc.cpp out because its tests define GetMilliseconds,
  RemoveFile, SetClipboardText and FileExists as recorders; the runner has sysinctest's test and these in one
  program, so its golden_sysinc compiles sysinc.cpp with those four renamed (preprocessor definitions; sysinc.cpp
  unchanged). Considered: a second runner executable for sysinctest.
- **By value, again** (high; plan 4.3). The original's objects never cross as memory images:
  - numbers and texts through `ORIGINAL_VALUE` / `ORIGINAL_TEXT` (golden::TText keeps NULL apart from "");
  - a pointer the original was given back (the mouse-over text, a message's parameters) or one into its own objects
    (a talkbox, a buffer it created, a line) as which of the test's objects it is, which the runner turns into its
    own pointer;
  - an object with pointers field by field: Script's readers and writers (File as NULL or not, Bytes as an offset
    from String), Talk's buffers (fields, the cursor and the last line as indices, the lines by identity: a line
    recorded in full the first time its address holds it), Container's elements through a layout projection (next
    point);
  - a file name relative to the test's TEMP directory (TEMP differs between recording and replay);
  - big buffers by their changes since the last record (golden::OutputChanges: a reader's 1,000-byte string, the
    error reports, the talkboxes, Container's entries), which took Status from 110 MB to 8 MB.
  A whole-object memcmp of the old test is the same comparison field by field where both objects keep NULL in their
  pointers wherever they were compared (Script's constructors and close); Talk's base constructor check compares the
  fields and the bytes no field covers (the layouts differ; with both sides untouched, 0x55).
- **Container's layouts** (high). Container.h's test compares template instances byte for byte in the original's
  layout, and seven of its element types (and vector<T>'s header, which holds `T init`) have pointers. Each type is
  described field by field (int, pointer, bytes with an alignment; a type without pointers as its bytes), laid out
  with 4-byte pointers and with the build's. Our objects are projected to the original's layout for the comparison
  (each field and the padding after it, which our padding after the same field holds; a pointer's upper half has no
  counterpart), the random entries go into ours the other way (a pointer zero-extended), and the random numbers are
  drawn for the original's sizes. In the oracle both layouts are the original's, both mappings copy every byte and
  the comparisons are the old memcmps; a layout that does not fit stops the test (the oracle checks the original's
  size against its own classes). Our new[] memory gets the test's pattern through the runner's replacement
  operator new[] while the test runs (the oracle patches its CRT's operator new[] as before). Considered: stand-in
  element types of the original's sizes (the runner would not test the client's instances).
- **Talk's clock** (high). Both sides' _time32 is fixed. The oracle patches its static CRT's code; the runner's CRT
  is ucrtbase's, out of a rel32 jump's reach, so the runner points its own import slot of _time32 at the same fake
  for the length of the test (our Talk.cpp calls _time32 through that slot). The time zone must be the recording's
  (_localtime32 on both sides): the same machine.
- **Size** (form). The corpus is 190 MB (Container 82 MB, Talk 46 MB, Script 29 MB); local only, as before.
- **Commits** (form). One commit per test, Preferences and PreferencesFile together (one source, shared helpers); the
  first has the runner's new infrastructure. Each commit's runner and difftest were built and its tests replayed.

## The golden corpus: the game world (Patella-Modern issue #27)

Settled by Claude Opus 5.5, November 4, 2026. tests/golden/ (objects_tests.cpp, map_tests.cpp, creatures_tests.cpp,
golden_patch.h; golden.h's DataFile and OutputRuns), tests/golden/CMakeLists.txt, cmake/OracleX86.cmake.
notes/reconstruction.md, "Checking", describes the use; the sections on issues #24 and #26 above the format and the
earlier helpers.

- **Ported** (high). Three of the issue's six tests, each with the oracle's count and 0 failures: Objects 327,886, Map
  639,877, Creatures 2,610,987 (3,578,750 checks). The runner now replays 6,845,758 of difftest's 16,848,752 checks
  and sysinctest's 67. The old files are gone from tools/difftest; every CHECK is the old one, every count and random
  number too. golden_client compiles Objects.cpp, Map.cpp and Creatures.cpp (no new stubs: their callees are linked
  or were stubbed already; GetCreature and GetPlayerID left client_stubs.cpp, NoObject too).
- **Not ported: MainFrm, MapWindow, MainWindow** (needs a later issue). What blocks them:
  - their error rounds (TErrorRound, issue #34: MainFrm's 18,152 checks after its 31,424, MapWindow's 12,359): the
    comparison (TThrown, CompareThrown with its line pairs) lives in imageeh.cpp next to the image's exception
    machinery, and the fakes throw our side's assertion through GUI.cpp's TComponent::SetSize
    (ThrowSideAssertion), which golden_client does not compile. The runner needs a portable error-round layer: the
    comparison copied for the runner, the original's TThrown recorded by value (kind, type, message, origin, code),
    our assertion thrown by the same code as in the oracle. Errors (#26), the window tests and the surface tests
    (#28, #29) need the same layer;
  - MainFrm compiles MainFrm.cpp into its test file and defines the MFC shim's members as its fakes; the runner
    compiles MainFrm.cpp in golden_client and has those members as aborting stubs (client_stubs.cpp), so both move
    with the port; its user32 and shell32 fakes go into the runner's import slots (golden::FindImport), and its logs
    print a pointer with %p (SetTimer's), whose width differs on x64;
  - MapWindow and MainWindow compare their windows byte for byte from +0x14 (TMapWindow is 0x92d8 bytes after
    TComponent's pointers): a layout projection as Container's; their fakes are elementfakes.cpp's, testsurface.h's
    and GUI.cpp's and graphics.cpp's classes, and they run after the Graphics, GUI and window tests (#28, #29),
    whose redirects stay (MainWindow's after MapWindow's).
- **Tibia.dat in the corpus** (high). golden::DataFile: the oracle records whether the file exists next to Tibia.exe
  and its contents (176 KB, once per test that loads it); the runner writes them to %TEMP%\golden-data and loads that
  copy. So the runner needs no data files, now and after M6, and a corpus is never replayed against another
  Tibia.dat. The corpus is derived from CipSoft's files already: local only (build/ is ignored), never committed,
  pushed or uploaded. Considered: the runner looking for Tibia.dat (a cache variable, the source tree as the oracle
  does): one more local input to keep in step with the recording.
- **By value, again** (high; plan 4.3).
  - Objects: per table its presence and bounds, per entry its fields without the sprite pointer and the sprite numbers
    it points to. TProperties has no padding in either layout, so the old memcmp with both pointers cleared is the same
    comparison field by field.
  - Map: objects (three ints), colours, the effect texts, automap marks (TMark: ints and a char array, 0x70 bytes on
    both sides; compared byte for byte as before), FindPath's results, the creatures the original reported removed
    (the new ones since the last record) and the automap files it wrote (their bytes).
  - Creatures: the creature table and the VIP list (TCreature 0x9c and TBuddyEntry 0x2c bytes on both sides, no
    pointers; static_assert) by their runs of changed bytes (golden::OutputRuns; the start is the original's tables
    before the test), still compared byte for byte, padding included: our x64 code leaves the padding as the x86 code
    does. A creature the original returns is its slot; the hints, monster hints and path finder calls its fakes
    recorded, by value. Where the old test's && did not call the original's second getter (CheckPlayer's skill
    progress), the runner reads no record either; the IDs' getters have no side effects and are all recorded.
- **Our functions redirected** (high). Creatures redirects our Map.cpp's and Help.cpp's functions to its fakes for
  good, as difftest does (Errors relies on it: "functions redirected by earlier tests"). golden::RedirectOwn writes
  the oracle's 5-byte rel32 jump: the function and the fake are in the runner's image, within reach. golden_runner
  links with /OPT:NOICF, so no function folded with a patched one shares its code; in the Debug build the
  incremental-link thunk is patched, which every call goes through. The clock is _time32's import slot, as Talk's
  (FindImport moved to golden_patch.h; Talk uses it). Considered: a 14-byte absolute jump (it would overwrite the next
  function after a small one); link-time fakes (Map.cpp must be the real one for the Map test in the same runner).
- **Errors (#26)** (needs a later issue). In place now: Objects, Map and Creatures run in the runner in difftest's
  order with Creatures' redirects of our functions, golden_client has Objects.cpp, Map.cpp and Creatures.cpp, and
  Tibia.dat comes from the corpus. Still blocking: the Graphics and GUI tests (#28, #29) and their redirects, which
  are part of Errors' count; graphics.cpp and the engines' start-up in the runner; the error-round layer above.
- **Size** (form). The corpus is 243 MB (Container 82 MB, Talk 46 MB, Creatures 35 MB, Script 29 MB, Map 17 MB,
  Objects 4 MB); local only, as before.
- **Commits** (form). One commit per test; the first has DataFile, the third OutputRuns and golden_patch.h. Each
  test's corpus was recorded and replayed after its port; the intermediate commits were not built on their own.

## The golden corpus: error rounds (Patella-Modern issue #54)

Settled by Claude Opus 5.5, November 4, 2026. tests/golden/ (golden_eh.h, golden_eh.cpp, mainfrm_tests.cpp,
client_stubs.cpp), tools/difftest/imageeh.h and imageeh.cpp, tests/golden/CMakeLists.txt, cmake/OracleX86.cmake.
notes/reconstruction.md, "Checking", describes the use; the sections on issues #24, #26 and #27 above the format and
the earlier helpers.

- **One comparison for both sides** (high). difftest's error rounds (issue #34) compare what a call threw on our
  side and on the original's. The portable part moved from imageeh.h/.cpp to tests/golden/golden_eh.h/.cpp
  unchanged: TThrown, ThrownOf, OurThrow, CompareThrown with its line pairs (WithoutLines, CompareLines, the line
  map), TErrorRound. The oracle (difftest.exe, communicationtest.exe) and the runner compile the same file, so the
  checks (one per compared call, one per line pair of a call that matched otherwise) are the same code and the
  counts cannot drift. imageeh keeps what needs the image: ImageThrow and its filter, Enable/DisableImageExceptions
  (TErrorRound's constructor and Finish call them in the oracle only), ROUND_COMPARE, ImageSideVtable, Redirected.
  ThrownOf names our type ".?AVTMyException@@" as a literal: what MSVC's typeid(...).raw_name() returned, the same
  text on every compiler. Considered: a copy of the comparison for the runner (two copies to keep in step, which
  issue #54 asked to avoid: "compares the same way").
- **The original's exception by value** (high; plan 4.3). `TThrown Original = ORIGINAL_THROWN("thrown", the
  original's call);` runs the call inside ImageThrow in the oracle and records its kind and PROGRAM_ERROR value
  (two ints), type, message and origin (texts; golden::OutputThrown); the runner gets the recorded TThrown. That is
  everything CompareThrown reads, so the line pairs (their number comes from the original's message) are counted
  alike. TErrorRound::Finish records the round's three counts (checks, calls compared, calls in which the original
  threw); the runner prints the oracle's next to its own and fails when they differ. That is a failure, not a CHECK:
  the oracle's counts stay difftest's. Every error round of difftest records them, also the not yet ported tests'
  (in their own .bin files; no count changes).
- **Our side's assertion in the runner** (high). The fakes throw "an assertion of the calling side" through
  ThrowSideAssertion, in the oracle our GUI.cpp's TComponent::SetSize (guitest.cpp), which the runner does not link
  (GUI.cpp needs graphics.cpp and the window code, #28/#29). The runner's ThrowSideAssertion throws
  OurSideAssertion(Value): our TMyException with SetSize's TASSERT1(1092, _Width>=0, _Width) arguments, what the
  macro passes. The oracle checks before a round that uses it (VerifyOurSideAssertion, called by the ported test)
  that it throws exactly what SetSize throws; a difference is a failure, again not a check. When GUI.cpp is in the
  runner, ThrowSideAssertion can call the real SetSize there too. The runner's version aborts when asked for the
  original's side (there is none). Considered: compiling GUI.cpp into golden_client now (its callees reach graphics,
  the skins and every window class: #28/#29's work).
- **MainFrm ported** (high). 49,576 checks, the oracle's count, 0 failures: 31,424 in its rounds and 18,152 in its
  error round (5,504 calls compared, 535 in which the original threw, both equal to the oracle's). The test moved
  from tools/difftest/mainfrmtest.cpp to tests/golden/mainfrm_tests.cpp; every CHECK, random number and operation
  is the old one. What the corpus holds per comparison: the original's log (text), its state (TState: the frame's
  members, the file's statics, the world its fakes keep, 420 bytes without pointers or padding, by their runs of
  changed bytes, golden::OutputRuns; the dump is formatted from it on both sides), in the error round what its call
  threw and how many fake calls it made (whether its n-th fake call was reached, which ends the trial). Inputs: the
  seed of every round and every error trial. MainFrm.cpp left golden_client and is compiled into the test, as in
  the oracle (its statics are in an unnamed namespace); the MFC shim's members it calls left client_stubs.cpp and are
  the test's fakes, as in the oracle (AfxGetApp, AfxGetInstanceHandle, CFrameWnd's constructor and destructor, the
  CWnd members, wndTopMost/wndNoTopMost). In the runner the user32 and shell32 fakes go into the runner's import
  slots by name (golden::FindImport), our functions the file calls (client_stubs.cpp's Control.cpp and Network.cpp
  stubs, Preferences.cpp's, Status.cpp's, Creatures.cpp's, AddMessage, InitMainWindow) are redirected with the
  test's own rel32 jump (now with golden_patch.h's reach check), and all of it is undone at the end, as before.
- **The logs on x64** (high). SetTimer's fake printed its callback with %p, eight hex digits on x86 and sixteen on
  x64: it prints the callback's name now (CallbackName, as InitWindow's), on both sides. The dump prints ClassName
  with %.32s (the 32-byte array, terminated or not); every other value of the logs and the dump was printed through
  an unsigned or int already (HWND, HCURSOR, LPARAM, the hex of WINDOWPLACEMENT and MINMAXINFO, the same 44 and 40
  bytes on both targets).
- **/EHsc in the runner** (high). difftest builds with /EHa, the runner (golden_client) with the product's /EHsc,
  as both real clients. MainFrm's comment explains why the user32 fakes never throw (Win32Tick does nothing): under
  /EHsc the compiler drops the try blocks that call only C functions (AddFrameSize, ExitProgram). Every TEND the
  round reaches is reached through the MFC and game fakes, C++ calls, which both models keep; the equal counts
  show it.
- **The layer catches a difference** (high). With OurSideAssertion's line set to 1093 for a run, the runner reported
  1,070 failures in MainFrm's error round ("our GUI.cpp line 1093, the original's 1092"), its other checks passed;
  reverted.
- **Not ported** (needs later issues): Errors (40,408; #26) after the Graphics and GUI tests (#28, #29) and their
  redirects; MapWindow (127,529, error round 12,359) and MainWindow (57,314) need a layout projection of TMapWindow
  and TComponent and the GUI and graphics fakes. Their error rounds use this layer as MainFrm does: OurThrow for our
  call, ORIGINAL_THROWN for the original's, TErrorRound::Compare, and VerifyOurSideAssertion in the oracle before the
  round when the test throws through ThrowSideAssertion.
- **Size** (form). MainFrm.bin is 3 MB (88,304 records); the corpus 246 MB, local only, as before.

## The golden corpus: Network (Patella-Modern issue #30)

Settled by Claude Opus 5.5, November 4, 2026. tests/golden/ (network_tests.cpp, network/CMakeLists.txt,
client_stubs.cpp, runner.cpp, golden_test.h), tests/golden/CMakeLists.txt, cmake/OracleX86.cmake.
notes/reconstruction.md, "Checking", describes the use; the sections on issues #24, #26, #27 and #54 above the
format and the earlier helpers.

- **Network ported** (high). 113,806 checks, the oracle's count, 0 failures: 74,928 in its rounds and 38,878 in its
  error round (7,393 calls compared, 556 in which the original threw, both equal to the oracle's). The runner now
  replays 7,009,140 of difftest's 16,848,752 checks and sysinctest's 67. tools/difftest/networktest.cpp moved to
  tests/golden/network_tests.cpp; every CHECK, random number and operation is the old one: the oracle's recording
  of Network was byte for byte the same before and after the change below ("The order of Random()"). A runner-only
  change of one fake's log text gave 239 failures (reverted).
- **By value** (high; plan 4.3). Per comparison the corpus holds the original's log (text), its dump (the text of
  the old Dump, in a 4,096-byte buffer by its runs of changed bytes, golden::OutputRuns), its ReceiveData (16 KB)
  and SendData (2 KB), both by OutputRuns, and in the error round what its call threw (ORIGINAL_THROWN) and how many
  fake calls it made (whether its n-th was reached, which ends a trial). Inputs: the seed of every round and every
  error trial. The dump printed the three handlers InitNetwork stores with %p (the oracle's addresses, not the
  runner's): it prints their names now (CallbackName, as MainFrm's), on both sides. Network.bin is 7.9 MB; the
  corpus 253 MB, local only, as before.
- **Our side on x64** (high). Network.cpp is compiled into the test, as in the oracle; its stubs left
  client_stubs.cpp (MainFrm's redirects of its functions now reach the real ones, as in the oracle). What the old
  test took to be the same on both sides is ours where it is: CAsyncSocket's m_hSocket (+4 in the image, the shim's
  member in ours: +8 on x64), TXTEA's key (after the vtable pointer: +4, +8 on x64), and the HOSTENT the host
  lookup's fake writes (the image's in the x86 layout as before, ours with 8-byte pointers on x64; in the oracle both
  are the old x86 layout). WinSock: the runner links no ws2_32 (dumpbin: no import), its `__imp_` symbols are the
  test's slots through /alternatename (undecorated on x64), as difftest.exe's; nothing reaches a network. rand(): the
  runner's import slot of ucrtbase's (golden::FindImport/SetImport, restored at the end). Status.cpp's and
  Preferences.cpp's functions: the test's rel32 Redirect (now with the reach check), restored at the end.
- **/EHs for this test** (high). The error round makes the WinSock functions and rand() throw, as the oracle does.
  Under the product's /EHsc the compiler assumes that a C function never throws, and our x64 code let such an
  exception pass Network.cpp's TEND blocks where the original's catch it (21 failures in the first run, all "ours
  TMyException ..., original Network.cpp <line>: exception occurred ..."). No C function throws in either client,
  so only the fakes see it. network_tests.cpp compiles in tests/golden/network with /EHs (synchronous C++
  exceptions, from C functions too): that directory replaces /EHsc in CMAKE_CXX_FLAGS for itself. Considered: the
  source property /EHc- (D9025, "overriding '/EHc' with '/EHc-'", a warning, which fails the build); fakes that do
  not throw (MainFrm's user32 fakes; here it would change the oracle's count); /EHa (the same override, and further
  from the product's model).
- **The order of Random()** (high). Five expressions of the old test called Random() twice or more without a
  sequence point between them (RandomIP's snprintf arguments, RandomErrorCode's `Random() - (Random() << 8)`, the
  XORs of SendOps, RandSeed and the error trials' seed). The order is unspecified: the x86 oracle and the x64
  Release runner agreed, the x64 Debug runner evaluated RandomErrorCode's difference the other way (failures in
  Debug only). Each is now its calls in the oracle's order (XOR: the left operand first; the difference: the right
  one first; snprintf: its arguments right to left), proven by the oracle's unchanged recording and by the runner
  in Release and Debug. Other ported tests have such expressions too (MainFrm's `Random() ^ (Random() << 16)`);
  they pass in both builds, so their compilers agree today; not changed here.
- **Communication and Control are not ported** (needs a later issue). communicationtest.exe's whole count is these
  two tests: 1,134,513 = Communication 856,366 (364,041 + 492,325 in its error round) + Control 278,147 (114,595 +
  163,552). The runner already reads its manifest as a group of its own ("communicationtest" in runner.cpp's table,
  with the oracle's per-test counts) and lists both as not ported, so no check is unaccounted for. Recording them
  by value would be simple (commworld.cpp's CompareRun compares texts: the logs, DumpCreature with TCreature's
  bytes, 0x9c on both targets, DumpBuffer, DumpWorld, what was thrown). What blocks them is our side in the runner:
  - their world's windows are our GUI.cpp classes: TCtlWindow derives from TWindow (static_assert 0x2c, the x86
    size), TCtlMainWindow mirrors TMainWindow's x86 offsets (Console +0x3c, InGame +0x48), and the faked dialogs,
    context menus and help bubble build such windows; GUI.cpp and the window files are not in the runner (#28,
    #29, as for MapWindow and MainWindow);
  - WorldFakes redirects about 280 of our functions in Network, Map, Creatures, Inventory, Help, Talk, Status,
    Preferences, Objects, graphics, GUI, MainWindow, ConsoleWindow, ContextMenus, Dialogs, Skins, sysinc and Surface;
    those not in golden_client need the real files or stubs to redirect;
  - x86-only mechanics: 27 member-function fakes are `__fastcall (This, void *, ...)` stand-ins for thiscall (on
    x64 the dummy parameter takes the first argument), constructors are found through x86-mangled /alternatename
    names (`??0THelpBubble@@QAE@PBD@Z`), ThisOfFirstCall finds Network.cpp's buffers by decoding x86 code
    (`mov ecx, imm32; call`), FollowJumps and CheckRoom read x86 thunks, FillStack's pattern stands for locals read
    before they are written (x64 frames differ);
  - communicationtest.cpp's own definitions collide with the runner's: CAsyncSocket's members (traps there,
    network_tests.cpp's fakes here) and MainFrm.cpp's PacketHeader and DisplayChanged (defined there; the runner
    has MainFrm.cpp). A port shares them with the runner's or builds a second runner executable for the group.
- **Commits** (form). Network's port, then the order of its Random() calls, then these notes. Each test's corpus was
  recorded and replayed after its port, in Release and Debug.

## The golden corpus: the GUI, graphics and the window files (Patella-Modern issue #28)

Settled by Claude Opus 5.5, November 4, 2026. tests/golden/ (graphics_tests.cpp, gui_tests.cpp, testsurface.h,
client_stubs.cpp, golden_patch.h's SaveOwn/RestoreOwn, golden_eh), tests/golden/CMakeLists.txt, cmake/OracleX86.cmake.
notes/reconstruction.md, "Checking", describes the use; the sections on issues #24, #26, #27, #54 and #30 above the
format and the earlier helpers.

- **The foundation** (high). golden_client compiles graphics.cpp, GUI.cpp and every window file (DialogWindow,
  ContextMenus, ConsoleWindow, ContentWindows, DialogElements, Dialogs, MapWindow, MainWindow), as difftest.exe links
  them. What they call below them and the runner does not link is in client_stubs.cpp, aborting (55 symbols the
  linker asked for): Communication.cpp's Send functions, Control.cpp's ChangeEngine, CancelPatch, and the Vulkan
  device queries of the advanced graphics dialog (GetNumberOfGpus, GetCpuDriverManifest, GetDeviceName, ButtonOf).
  The runner's stand-ins of GUI.cpp (TCallback's constructor), MainWindow.cpp (InitMainWindow) and graphics.cpp
  (InitVideoCache) left the stubs: the real ones are linked. A window test can build our window classes in the runner
  and redirect what they call (golden::RedirectOwn works on a stub as on any of our functions). Considered: compiling
  only graphics.cpp and GUI.cpp (each later window test would add its file and more stubs; with all of them the 55
  stubs are the whole list now).
- **The window objects' layouts** (high). The ported tests do not project the x86 objects: the original's objects
  stay in the oracle, and what the comparisons read from them is recorded field by field: a component's parent, next
  child, first child and capture as the test's node indices, its offsets and size; a TTimer's Remaining, Active and
  getters. That is what the old comparisons read (GUI's CompareTrees, the timer checks), so no layout of TComponent or
  TWindow enters the corpus. The runner has our objects alone; a node's original object is a placeholder pointer
  there, never dereferenced (every use of the original's side is inside ORIGINAL), which CollectDeletions clears
  when the original's recorded log reports the deletion, as it cleared the oracle's. The later window tests can do
  the same where their dumps are texts (the original's dump is then recorded as a log is); where they memcmp objects
  (MapWindow's TMapWindow from +0x14) they need Container's kind of projection.
- **Graphics ported** (high). 570,954 checks, the oracle's count, 0 failures; the summary line (10,962 sprites and 567
  picture tiles decoded, 115,306 tiles written to the video cache, 132,506 blits) is the oracle's, so the inputs are
  too. tools/difftest/graphicstest.cpp moved to tests/golden/graphics_tests.cpp; every CHECK and random number is the
  old one. By value: the sprite table (TSprite, no pointers) and the fonts (TFontInfo) as bytes, a picture as its
  TilesX, TilesY, colour key and offsets array, a memory cache item as its Data's offset in the cache's data and its
  nine ints (TMemoryCacheItem has a pointer; field by field is the old memcmp with Data cleared, which has no padding
  on x86), the video cache (TVideoCacheItem, 0x5c bytes without pointers on both targets) by its changed bytes
  (golden::OutputRuns), the hash tables and lists as ints, the file names relative to the data directory
  ("<data>/Tibia.spr": the runner's directory differs). Tibia.spr and Tibia.pic come from the corpus, as Tibia.dat
  does (golden::DataFile); the runner's ReloadSprites and InitGraphics read them in its copy's directory. Our
  GetSurface is redirected to the recording surface for good (golden::RedirectOwn), as in the oracle (MapWindow takes
  the same object from it).
- **The tile dictionary** (high). The original's surface log held 115,306 tiles of 3,072 bytes (354 MB as they are),
  the memory cache's data changes by whole tiles, and every decoded sprite and picture tile is a tile as well.
  RecordTile records a tile as the index of an equal tile recorded before, or as its bytes (which get the next
  index); both sides keep the same list. The log is recorded as its bytes without the tiles, where they were, and the
  tiles through the dictionary; the runner puts it together again and compares it whole, as before. Graphics.bin is
  91 MB (Tibia.spr 16 MB, the decoded sprites 34 MB). Considered: a hash of the log (a failure could no longer show
  where the logs differ); the logs as they are (several hundred MB).
- **The order of Random() in Graphics** (high). RandomColor's `TRGBColor(Range(...), Range(...), Range(...))` calls
  in an unspecified order (#55): now three statements in the oracle's order (the constructor's arguments right to
  left, as x86 MSVC evaluated them); the oracle's summary line and every check of the runner agree. The file's other
  expressions were sequenced already (braced initialisers, one declarator per call).
- **GUI ported** (high). 5,277,938 checks, the oracle's count, 0 failures: 5,246,029 in its rounds and 31,909 in its
  error round (2,750 calls compared, 2,154 in which the original threw, both equal to the oracle's).
  tools/difftest/guitest.cpp moved to tests/golden/gui_tests.cpp; every CHECK and random number is the old one, the
  oracle's lines too ("each of the 30 window operations at least 236 times"). Per comparison the corpus holds the
  original's log (text), its tree (eight ints per node, by its changes), its help state, the results of its calls
  (queries, mouse events, element defaults, CheckHotkey), its timers' fields and the timer vector's min, its skin
  properties (0x1528 bytes, twice; no pointers, and the test's own check of sizeof holds on x64), and in the error
  round what each call threw (ORIGINAL_THROWN, through GUI_ROUND_COMPARE: imageeh.h's ROUND_COMPARE by value). Our
  DrawPicturePart and DrawPicturePartClipped go to the test's recorders through golden::RedirectOwn while it runs and
  come back through the new golden::SaveOwn and RestoreOwn. GUI.bin is 56 MB.
- **Our side's assertion is the real one** (high). ThrowSideAssertion (golden_eh.h) is gui_tests.cpp's on both sides:
  our TComponent::SetSize, which the runner links now. Issue #54's stand-in, OurSideAssertion, stays for the oracle's
  VerifyOurSideAssertion (MainFrm, Network), which checks it against SetSize; MainFrm's and Network's error rounds in
  the runner throw through SetSize and pass with the oracle's counts.
- **x86-only mechanics** (high). These two tests call the original's member functions through `__fastcall (This,
  void *, ...)` stand-ins for thiscall, copy the image's vtables and patch the image with PatchJump: all of that is
  the original's side, inside ORIGINAL or `#if GOLDEN_ORACLE`, never compiled into the runner. The runner's side uses
  portable means that were the runner's already: golden::RedirectOwn (our functions to the recorders, GetSurface),
  golden::DataFile, golden::OutputRuns. No /alternatename, no decoding of x86 code, no FollowJumps, CheckRoom or
  FillStack is involved.
- **Not ported** (needs later issues). DialogWindow 34,068, ContextMenus 41,660, ConsoleWindow 318,882, ContentWindows
  221,270, DialogElements 1,362,116, Dialogs 299,174 (2,277,170 checks). What blocks them is the shared fakes of the
  DialogElements.cpp classes, tools/difftest/elementfakes.{h,cpp}: mirror classes with the real classes' x86 offsets
  (`uint8 Data2C[0x38 - 0x2c]`, a TRadioButtonGroup pointer at +0x38, a tooltip at +0x3c) that both sides build,
  because PatchElementFakes redirects the image's constructors and ours (through their x86-decorated names) to them;
  our window code then reads them as our classes, which have other offsets on x64. A port needs fakes with our layout
  for our side (the original's side keeps its own), so each test's fake world splits by side. Besides: `__fastcall`
  stand-ins on our side too (FakeUpdateUpButton for TContainerWindowBody::UpdateUpButton, found with `Member(&...)`,
  a member pointer read as a code address), the hooks of difftest.h (DialogFakes, ElementCreated, ReportElement) that
  the tests define in each other's files, and for Dialogs the x64 variant of Dialogs.cpp (PATELLA_VULKAN: the device
  buttons of issue #31 in the advanced graphics dialog, where the oracle compares the engine buttons).
- **What is possible now** (high). The window classes are in the runner, which the later tests here, #27's MapWindow
  and MainWindow, #26's Errors and #30's Communication and Control were waiting for. Errors (#26): Graphics and GUI
  run before it in the runner in difftest's order (GUI's redirects undone, GetSurface's kept, as in the oracle) and
  graphics.cpp is linked; still missing are the tests between GUI and Errors in difftest's order (the six above and
  SurfaceDX, DX9, GL), whose redirects its "250 comparisons skipped" counts, and a portable form of its Redirected
  (a function that starts with a jump, decoded as x86 code). MapWindow and MainWindow (#27): MapWindow.cpp,
  MainWindow.cpp and the recording surface are in the runner; they need the element fakes above and TMapWindow's
  projection. Communication and Control (#30): their world's windows (GUI.cpp's TWindow, the window files) link now;
  the rest of #30's list (27 `__fastcall` stand-ins, x86-decorated /alternatename constructors, ThisOfFirstCall,
  FollowJumps, CheckRoom, FillStack, the CAsyncSocket and MainFrm.cpp definitions) is unchanged.
- **Size** (form). The corpus is 394 MB (Graphics 91 MB, Container 82 MB, GUI 56 MB, Talk 46 MB); local only, as
  before.
- **Commits** (form). The foundation with Graphics, then GUI, then these notes. Each test's corpus was recorded and
  replayed after its port; the first commit was not built on its own (the second's tree was, in Release and Debug).

## M3's exit (Patella-Modern, November 4, 2026)

Decided by the user, November 4, 2026; recorded by Claude Opus 5.5. The plan's M3 exit says "clang-tidy's
`modernize-*` clean outside the error macros". After issues #32 to #44 and #49 to #51 every check M3's table names is
clean on src/ and enabled in `.clang-tidy`, so CI keeps it clean. Running every `modernize-*` check on the windows-x64
sources (`python -I tools/tidy.py -p build/windows-x64 --all --checks='-*,modernize-*'`, main 1363438) still reports
3,289 findings, all in src/. The exit is read as "the named checks are clean, and each other `modernize-*` check has a
decision":

| Check | Findings | Decision |
|---|---:|---|
| `modernize-use-trailing-return-type` | 1,764 | Not adopted: style only; the code keeps the original's declarations. |
| `modernize-use-designated-initializers` | 702 | Not adopted for the original's aggregates (their initialisers follow the binary's field order); new code may use them. |
| `modernize-avoid-c-style-cast` | 249 | Later: the functional and arithmetic casts `cppcoreguidelines-pro-type-cstyle-cast` (#41) does not report; a separate pass when the arithmetic is reviewed (M7). |
| `modernize-avoid-c-arrays` | 241 | M4 ("C arrays in structures to `std::array`", same layout). |
| `modernize-use-auto` | 186 | Not adopted: style only. |
| `modernize-loop-convert` | 39 | Later (M7): range-for over the original's index loops, where the index is not used otherwise. |
| `modernize-use-integer-sign-comparison` | 34 | Not mechanical: `std::cmp_*` can change results where the original compares signed and unsigned; each case by hand with difftest, in M7 or not at all. |
| `modernize-use-nodiscard` | 31 | Later: these are what the check reports with its default options; `.clang-tidy`'s configuration (#38) is clean. |
| `modernize-use-default-member-init` | 24 | Later (M7), with the constructors. |
| `modernize-use-ranges`, `modernize-return-braced-init-list`, `modernize-use-bool-literals`, `modernize-avoid-variadic-functions` | 12 | Later (M7). |
| `modernize-use-std-print` | 4 | M4 ("Formatting and I/O"). |

M3 is therefore done; the M4 issues follow the plan's M4 section.

## The golden corpus: the window tests (Patella-Modern issue #28, second part)

Settled by Claude Opus 5.5, November 4, 2026. tests/golden/ (dialogwindow_tests.cpp, contextmenus_tests.cpp,
consolewindow_tests.cpp, contentwindows_tests.cpp, dialogelements_tests.cpp, elementfakes.{h,cpp} moved from
tools/difftest, golden.h's OutputTextChanges, golden_eh.h's GOLDEN_ROUND_COMPARE, golden_test.h's GOLDEN_THISCALL and
GOLDEN_THIS), tests/golden/CMakeLists.txt, cmake/OracleX86.cmake, tools/difftest/difftest.h. notes/reconstruction.md,
"Checking", describes the use; the sections on issues #24, #26, #27, #54, #30 and #28 above the format and the
earlier helpers.

- **Ported** (high). Five of the six window tests left by the first part, each with the oracle's count and 0
  failures in Release and Debug, its error round's counts equal to the oracle's: DialogWindow 34,068 (9,501 in its
  error round), ContextMenus 41,660 (8,572), ConsoleWindow 318,882 (287,934), ContentWindows 221,270 (168,764),
  DialogElements 1,362,116 (1,226,638): 1,977,996 checks. The runner replays 14,836,028 of difftest's 16,848,752
  checks and sysinctest's 67. Every CHECK, random number and operation is the old one; the summary lines ("each of
  the 24 operations at least 463 times", "each window at least 275 times, each action at least 91 times", "12 marks
  found" ...) are the oracle's in the runner too. A runner-only change of one fake's log text gave 431 failures in
  DialogWindow (reverted).
- **The element fakes on both sides** (high). elementfakes.h/.cpp moved to tests/golden and are compiled into both.
  The oracle keeps the mirror classes with the x86 offsets (the image's code and our x86 code read them alike); the
  runner has the same classes with our x64 layout: each named member at the offset of the real member it stands for
  and each fake as large as its real class (static_asserts against DialogElements.h's classes; TFakeEditLine's
  Active byte is `Data30[EditActive]`, an offsetof difference, 0x28 on x86 as before). The runner redirects our
  constructors through their x64 decorated names (/alternatename, as the oracle does with the x86 ones; the runner
  is built with MSVC x64 only) to plain stand-ins: on x64 a constructor takes this as its first argument, as a plain
  function does, so a stand-in has the constructor's own parameter types (bool where it has bool) with this first.
  Each fake is defined in the ported test that introduced it, on both sides; elementfakes.cpp has the runner's
  definitions of those whose test is not ported (mainwindowtest.cpp's text button and label constructors, which only
  report the object outside MainWindow's test; the border and the blinking button stop the runner). Considered: the
  real classes on our side with their state recorded by value (the fakes' logs and the test vtables they get are
  part of the comparison; real classes would log nothing).
- **Member-function stand-ins** (high). The `__fastcall (This, void *, ...)` fakes of member functions are
  `R GOLDEN_THISCALL Fake(GOLDEN_THIS, ...)` (golden_test.h): __fastcall with the dummy for EDX in the oracle, a
  plain function with this first in the runner. FakeUpdateUpButton is declared per side in elementfakes.h, with
  the other hooks difftest.h had (DialogFakes, ElementCreated, ReportElement), so the ported tests and the oracle's
  remaining ones share them. None of these fakes returns a structure by value (on x64 a member's hidden return
  pointer comes after this, a function's before its first argument).
- **By value** (high; plan 4.3). Per comparison the corpus holds the original's log (text), its dump (the old
  Dump's text by its changes since the last one: golden::OutputTextChanges records the bytes kept from the last
  text's start and end and those between, which keeps DialogElements' 1.2 million dumps at 96 MB), the results of
  its calls, the original's answers where the old test read the original outside its calls (whether it has a
  window or a part, the sub-window that is gone), the number of logged fake calls the original's call made (the
  error rounds draw the failing call from it) and what it threw (GOLDEN_ROUND_COMPARE, ORIGINAL_THROWN). The
  original's side (its constructors, members, text buffers, vtables) is inside ORIGINAL or `#if GOLDEN_ORACLE`;
  DialogElements' 123 member calls go through its CallI, which in the runner is never reached (Both runs our side
  only) and stops the runner if it were. Pointers printed into a compared text are printed as the x86 oracle printed
  them (ContextMenus' mark window, a number of the test's; DialogElements' unset tooltip, the 0xab pattern), eight
  hex digits on both sides.
- **The runner's patches** (high). Our functions go to the fakes through golden::RedirectOwn and back through
  RestoreOwn, as in the oracle; operator new and new[] to the tests' pattern the same way (the runner's operator new
  is in its image); our _time32 through its import slot (as Talk's), and ContentWindows' printf (the automap's
  MouseHover; inline on x64) through the import slot of __stdio_common_vfprintf while our side runs. The Send
  functions and IsClipboardEmpty/GetClipboardText the oracle's tests define for our side are client_stubs.cpp's and
  golden_sysinc's in the runner, redirected the same way.
- **No incremental linking** (high). golden_runner links with /INCREMENTAL:NO: an /alternatename alias of a
  constructor got an incremental-link thunk of its own in Debug, and patching it did not reach the calls (the
  constructor stayed real). Without incremental linking every address is the function's code, in Debug as in
  Release.
- **A deleted child read in TDialogWindow::SetSize** (found, not changed). DialogWindow.cpp's SetSize (as the
  original's 0x4857d0) sizes each child and follows the child's Next; a sub-window's SetSize can rearrange the
  column and delete children, so the loop reads a deleted child. The x86 heap and the x64 Release heap leave the
  freed object as it was; the Debug CRT fills it, and the Debug runner crashed. The runner's operator delete keeps
  memory freed during DialogWindow's test until its end (dialogwindow_tests.cpp), so both builds read what the oracle
  read. src/ is unchanged (the behaviour is the original's); the game may never reach it (the test drives sizes the
  game does not); left for a later issue.
- **The order of Random()** (high; #55). Expressions with two or more calls in one expression are now sequenced in
  the x86 oracle's order, right to left: the TRGBColor and TRect constructors of RandomSkinProperties
  (DialogWindow, ContextMenus) and DialogElements' colours, ContextMenus' NewLine(Random(), Modes[Random() % 6],
  ...), ConsoleWindow's `new TTalkLine(Random() % 1000, Mode, Random() % 2 ? ... : ..., ...)`. A probe compiled with
  the oracle's flags (/O2 /EHa, x86) gave the same values for the old and the sequenced forms.
- **ODR** (high). The tests' own definitions are in a namespace per test (DialogWindowTest ...): with
  dialogwindowtest.cpp moved, the linker kept mainwindowtest.cpp's `struct TInfo` destructor for both files (the
  same name, another layout) and difftest crashed in DialogWindow; the old order had hidden it.
- **Dialogs is not ported** (needs the user). The x64 Dialogs.cpp (PATELLA_VULKAN) has the graphics device buttons of
  issue #31 in the advanced graphics dialog where the oracle compares the engine buttons; the runner's Dialogs.cpp
  is that one (as Patella.exe's), and its device queries are client_stubs.cpp's aborting stubs. The dialog's checks,
  counted with a temporary instrumentation of the oracle (not committed; the log is local): 3,263 in the rounds that
  open it and 5,806 in its error rounds, 9,069 of Dialogs' 299,174. Porting the other 290,105 needs the runner to
  skip those rounds and stay in step: a record marking each round's end with the seed after it, a reader that skips
  to it, and the runner's count of the skipped checks next to the replayed ones. Options for the 9,069: replay them
  against Dialogs.cpp compiled without PATELLA_VULKAN (the engine buttons, code the x64 client no longer has), or
  retire them with the oracle (M6), the device buttons being covered by the Vulkan tests and the reference
  recordings. Not decided here (the device buttons are a settled decision; what the corpus does with these checks
  is the user's).
- **What is possible now** (high). The element fakes, the member-function stand-ins and the hooks have a runner
  form, so MapWindow and MainWindow (#27) need only their own parts: MainWindow's fakes (the border, the text
  button's first constructor, the label's font constructor are the runner's report-only versions in
  elementfakes.cpp today) and TMapWindow's projection for MapWindow's byte compare. Errors (#26) still waits for
  Dialogs, SurfaceDX, DX9 and GL in difftest's order (its "250 comparisons skipped" counts their redirects) and a
  portable Redirected. Communication and Control (#30): GOLDEN_THISCALL covers their 27 member-function fakes;
  their x86-decorated constructors can use x64 names as elementfakes.cpp does; ThisOfFirstCall, FollowJumps,
  CheckRoom and FillStack remain x86-only.
- **Size** (form). The corpus is 609 MB (DialogElements 96 MB, Graphics 91 MB, Container 82 MB, ConsoleWindow 69 MB,
  GUI 56 MB, ContentWindows 48 MB, Talk 46 MB); local only, as before. A full recording takes about four minutes.
- **Commits** (form). The element fakes with DialogWindow, then one commit per test, then these notes. Each test's
  corpus was recorded and replayed after its port in Release and Debug; the first commit's tree was built as part of
  the second's.

## TFixedString<N> for the fixed buffers (Patella-Modern issue #57)

Settled by Claude Opus 5.5, November 4, 2026. The first change of M4 (plan section 7, "Safety inside the frozen
layout"): the client's text buffers are written through a bounded type, and src/ has no strcpy, strcat, sprintf,
vsprintf or gets any more (167 calls on main b061421, 0 now; a grep gate in the hosted CI keeps it so).

- **The type** (high). `src/types/FixedString.h`: `TFixedString<N>`, a struct whose only member is the original's
  `char Chars[N]`: size N, alignment 1, standard layout, trivially copyable, trivially default-constructible, an
  aggregate (`IsFixedStringLayout<N>` asserts all of it; a local is as uninitialised as the array, `= {}` and
  `= {"Text"}` work). It converts to `char *` and `const char *` as the array decayed, so every reader (strlen,
  strcmp, MFC and Windows calls, indexing, and above all the TASSERT expressions whose text is frozen until M5,
  `strlen(Address)<100`) is unchanged. Writes: `Assign` (strcpy), `Append` (strcat), `Format` (sprintf) and
  `AppendFormat` (sprintf at the end), each returning the length; `Length`/`View` stop at the first NUL or at N;
  `Capacity`. `TStringBuffer(Pointer, Size)` has the same writes for a buffer known only by a pointer (a
  parameter, an allocation, a table row), with the size its callers allocate; `TFixedString::Buffer()` is its own.
  The directory src/types/ is for the fork's M4 types (the next: TFile); it is on cmake/HeaderCheck.cmake's core
  list and in the clang-format file list.
- **The contract: the C functions' bytes, cut where they overflowed** (high). A write puts the text and its NUL
  exactly where the C function did and touches nothing else, so the bytes after the NUL keep what they were (the
  golden tests compare the talkboxes, the VIP list and TrimString's buffer with memcmp; a structure written whole to
  a file stays byte-identical). Where the text does not fit, which in the original wrote past the buffer (undefined
  behaviour), the first N - 1 characters and the NUL are kept: no new error path, no new message. Where the
  original asserts a length (`strlen(Address)<100` and the 13 others below), the assertion stays in front of the
  write, as it was. Where it truncated with `_snprintf`, the call stays `MsvcSnprintf` (#43) on the buffer:
  `_snprintf`'s result and its missing NUL are kept exactly (a unit test holds this on a TFixedString), and those
  formats are mostly the text table's runtime strings (Text.h, TextTryingToReconnect), which std::format cannot
  take without rewriting the data. A null source faults as strcpy's did (no check added). A source inside the
  buffer is allowed (memmove; TrimString may trim in place).
- **std::format_to_n, and a stand-in for the oracle's C++17** (high). The plan asks for `Format` through
  std::format_to_n, but the oracle-x86 preset compiles with /std:c++17 until M6 (plan section 5: "keeps exactly
  today's flags"), and MSVC's <format> needs C++20. Raising the oracle's standard was not this issue's to decide.
  So `src/compat/format.h` gives `PATELLA_HAS_FORMAT` (1 where `__cpp_lib_format` is there: every product preset)
  and `patella::compat::FormatToNStandIn`, std::format_to_n for the part of the format language the client uses
  (automatic fields, `{{`/`}}`, fill/align, sign, `0`, width, precision, the types d x X f s c; bool, char, the
  integers with unsigned char and signed char as numbers, float, double, C strings, string_view, string and any
  type with a View()), its digits from std::to_chars as in the standard libraries; anything else throws
  `TFormatError`. The product presets take std::format_to_n with compile-time checked formats; the oracle the
  stand-in, with the same call sites (`Text.Format("{:03} ", Value)`). The unit tests compare the two directly for
  every count from 0 to past the length (integers to their limits, the client's float format `{:2.2f}` among 16
  doubles and infinities, strings with widths, a char array with bytes after its NUL), and run every TFixedString
  test once with each path (tests_unit and tests_unit_cxx26_standin, which gets `PATELLA_FORMAT_STANDIN`). So
  difftest proves the call sites through the stand-in, and the unit tests prove the stand-in equal to
  std::format_to_n. A TFixedString passed to Format is its View() (MSVC's std::format takes a class that converts
  to `const char *` for a C string and rejects its own std::formatter, so Format maps the argument).
- **What the formats became** (high). `%d`, `%u`, `%s`, `%c` are `{}`; `%03d`/`%03u`/`%02d` are `{:03}`/`{:02}`
  (std::format pads negative numbers after the sign, as printf does); `%2.2f` is `{:2.2f}` (both exact). Arguments
  that are `unsigned char` (the colours' R, G, B; uint8) format as numbers in both. The two runtime formats that
  were literals chosen at run time became a format with the word as an argument: a talkbox's first line
  (`"%s says:"` and the others: `"{}{}"` with " says:", " whispers:", " yells:", ":") and the context menus'
  `"Unignore %s"`/`"Ignore %s"` (`"{} {}"`); the same text. A sprintf without arguments became Assign.
- **Which members changed, and their layout** (high). The members written by the replaced calls became
  TFixedStrings: TServer::Address, TBuddyEntry::Name, TContainer::Name, TOffer::Name, TCharacter::Name and World,
  TBuddy::Description, the hotkeys (Preferences.cpp), TEffect::Text (inside its union), the sprite and picture
  file names, the status message, TTalkbox::Text (`TFixedString<40> Text[10]`), TBattleWindowBody::MouseOverText,
  TTextButton::Text, TDialog::Caption, TIgnoreEntry::Name and THotkeyDialog::Texts. Each structure asserts its size
  and the changed offsets: unconditionally where it has no pointers (the same on x64: TServer 0x70, TBuddyEntry
  0x2c, TCharacter 0x54, TBuddy 0x8c, TEffect 0x44, TTalkbox 0x1c0, TIgnoreEntry 0x1f), under
  PATELLA_ORIGINAL_LAYOUT in the .cpp where it has pointers or a vtable (TContainer 0x1ec, TOffer 0x4d4,
  TBattleWindowBody 0x58, TTextButton 0x78, TDialog's Caption 0x50, THotkeyDialog's Texts 0x2c). The members
  nothing replaced writes stay arrays (TCreature::Name, TTalkLine::Addressee, TMark::Text, TCharacter::IPAddress,
  the strncpy'd error reports): the exit criterion is the calls, and each conversion is a layout claim.
- **Kept arrays** (high). Script.cpp's TReadScriptFile::Filename and TWriteScriptFile::Filename and
  InstallPatch's Name stay `char[1024]`, written through `TStringBuffer(Filename, sizeof(Filename))`:
  TERROR_REASON names them, and its reason is a `const std::string &`, which a TFixedString cannot reach in one
  implicit conversion; the argument text is frozen until M5 (then `Filename.Chars`, or the member converts).
- **The varargs trap** (high). A TFixedString passed through `...` is copied as its N bytes, not as a pointer,
  and nothing warns (it is trivially copyable; MSVC's C4477 checks only literal printf formats). Every varargs call
  of src/, tests/golden and tools/difftest with a converted member or local was looked at: src/'s three
  MsvcSnprintf calls in Map.cpp pass `FileName.Chars`; tools/difftest/contentwindowstest.cpp passed the battle
  list's MouseOverText to its `Add(Out, "...%.31s", ...)` and crashed difftest (reading 0x9f9f9f00, the empty
  text and the fill bytes) until it passed `.Chars` (forced by the change; the only one). Also a stream: `Out <<
  Fixed` would print the pointer (operator<<(const void *)), so the stream writes (Communication.cpp's RSA bytes,
  Control.cpp's and MainFrm.cpp's packet types) use View().
- **Truncation or assertion, case by case** (high, except where marked). Asserted before the write (unchanged):
  SetTestServerAddress and SetServerIPAddress (<100), AddBuddy (<30), OpenContainer, SetOwnOffer, SetCounterOffer
  (<30), SetCharacter's Name and World (<30), SetHotkey (<HOTKEY_LENGTH), AddBuddy's Description
  (<BUDDYDESCRIPTION_LENGTH), AddTextualEffect (<10 into 12), LoadSprites and LoadPictures (<PATH_MAX),
  SetStatusMessage (<STATUSMESSAGE_LENGTH), TTextButton (<50), TDialog (<100); the automap file names (In() on the
  block coordinates: at most 12 characters in 16) and FormatNumber (Number>=0: at most 10 digits in 11). Sized by
  their own allocation (no cut possible): the MotD, the talk lines and their time stamps, the console tabs'
  captions, the labels', links' and list boxes' copies, the report and help texts, the patch messages, the
  ignore and buddy-exemption texts (first pass), GetSelectedText, TrimString (the caller's buffer holds Source) and
  GetErrorMessage (every caller passes new char[1024]; the longest message has about 300 characters). Bounded by
  their data, never cut in practice: the context menus' and dialogs' lines (names of at most 29 characters, the
  protocol's GetString(Name, 30)), the battle and VIP lists' copies, the talkbox's first line (29 + " whispers:"
  fills the 40 exactly), the packet and number texts. Two that the original could overflow (medium): the buddy
  descriptions read from Tibia.cfg ("copied without a length limit", so a hand-edited file with a description of
  128 characters or more wrote past TBuddy; now cut at 127) and a name of exactly 30 characters typed into the
  ignore or white list editor (its TEditLine allows 30; strcpy put the NUL into TIgnoreEntry::Permanent, which the
  next statement overwrites, so the entry kept all 30 characters, followed by the Permanent byte; now 29). No
  Tibia name has 30 characters (the client asserts strlen(Name)<30 everywhere else), and the list applies the
  edit lines' texts, not the entries, so the stored lists are the same; the entry's own copy differs only in the
  dialog after deleting another entry. difftest's PreferencesFile and Dialogs tests pass with unchanged counts, so
  they do not reach either case (both overflowed in the original).
- **Two test harness fixes forced by the moved data** (high). communicationtest's CheckRoom (tools/difftest/
  commworld.cpp) scanned the first five bytes of a faked function for a ret byte (C3) and found one in the operand
  of `mov eax,[0x2066c378]` once the globals moved (HelpHandle's new address): four Help.cpp functions "too short
  for a jump", 8 failures. A function that starts on a 16-byte boundary (/Gy) has room up to the next one; the scan
  now runs only for the others. And contentwindowstest.cpp's `.Chars` above.
- **The grep gate** (high). `tools/banned_calls.py` lists every call of strcpy, strcat, sprintf, vsprintf and gets
  in src/ outside src/Extern (Henry Spencer's regexp keeps its own code), skipping comments and string, character
  and raw literals (so a comment may say what a line replaced), and fails on one; `--self-test` checks the scanner.
  The hosted workflow runs both in the job "banned C string calls" (no Docker: python3 of the runner). On main it
  finds the 167 calls, on the branch 0. Not banned: strncpy (its own semantics, the error reports), MsvcSnprintf
  and snprintf (bounded), fprintf to files (M4's TFile).
- **Proof** (high). The images change (new code, moved data), so pecompare is no longer the proof; behaviour is:
  difftest 16,848,752, sysinctest 67, communicationtest 1,134,513, vlong 3,201 checks, 0 failures, after each of
  the three conversion commits and on the final head (`--fresh`); golden_runner with a corpus recorded from the
  branch; the unit tests above; compare_resources.py 0 differences (oracle build). macrocheck b061421 issue-57:
  2,108 calls, 0 differences. clang-tidy: the image's baseline 13 (Utils.cpp's sprintf vararg gone, 0 new); Visual
  Studio's on windows-x64's commands of the changed files against main's: one new finding (MainFrm.cpp's
  `<cstdio>`, removed), the rest moved lines; `--headers` 0 on windows-x64 and oracle-x86. The x64 live checks
  (compare_traffic.py, compare_files.py, the reference sessions, compare_rendering.py) are the orchestrator's.
- **The tools that read the images** (high; found by the first live `reference.py compare` after the merge, which
  stopped before any client started). tools/patch_client.py found the login port's stores as the immediate
  7171 in the 0x200 code bytes after the first reference to "server.tibia.com": with TServer::Address a
  TFixedString the x64 InitServerList copies the four names (a bounded loop and memcpy each) before the four
  stores, and the last store's immediate lay 6 bytes past the window ("expected 1 or 4 immediates, found 3"). It
  now finds them structurally where the image has a PDB: the RVA of `Servers`, sizeof(TServer) and the offset of
  TServer::Port (tools/client_state.py's dbghelp reader) give each server's Port field, and the code must hold
  exactly one `mov dword ptr [field], 7171` per server (C7 05, the absolute address on x86 or a RIP-relative
  disp32 on x64, the immediate), the four within 0x1000 bytes. Not InitServerList's symbol: the older builds
  inline it into its caller, and their PDBs have none. Tibia.exe (no PDB) keeps the old search. After writing it
  reads the file back as the client will (each name up to its NUL is --host, the four stores found again with
  the new port, the RSA text), and `--self-test` checks the store matcher on made-up x86 and x64 code. On Tibia.exe,
  the pre-#57 oracle and x64 Patella.exe and PatellaTest.exe (issue-28's build) and the new oracle it writes the
  same bytes as before (cmp); on the new x64 Patella.exe and PatellaTest.exe dumpbin shows the four stores with the
  new port at Servers + 0x70 i + 0x64. The names need nothing new: TFixedString::Assign copies up to the NUL, so
  a NUL-padded host reads short as before. tools/client_state.py's layout check expected `char[30]` for
  TContainer::Name and TCharacter::Name and got `TFixedString<30>` from the PDB, so every state reading of a new
  image (compare_traffic, compare_files, the reference compare's dumps) would have failed: Pdb.kind reads a
  `TFixedString<N>` of N bytes as `char[N]`. `--layout` passes on the new oracle (equal to the original, 0
  mismatches) and the new x64 images (every field, size, vtable slot and code pattern the same as issue-28's x64
  build; the globals moved). compare_rendering.py's add_import patches the new x64 images (patcap.dll and
  PatcapLoaded imported), compare_imports.py reports the new oracle as the old one, and the self-tests of
  reference.py, testmode_check.py and client_state.py pass. No client and no server were started.

## The golden corpus: Dialogs and retired parts (Patella-Modern issue #28, third part)

Settled by Claude Opus 5.5, November 4, 2026; the retirement decided by the user the same day. tests/golden/
(dialogs_tests.cpp moved from tools/difftest/dialogstest.cpp, golden.h/.cpp's RetiredBegin and RetiredEnd,
golden_eh's TErrorRound, runner.cpp), tests/golden/CMakeLists.txt, cmake/OracleX86.cmake. notes/reconstruction.md,
"Checking", describes the use; the sections on issues #24, #26, #27, #54, #30 and #28 above the format and the earlier
helpers.

- **Retired with the oracle** (decided by the user, November 4, 2026). The rounds of Dialogs that open the advanced
  graphics dialog compare the DirectX5, DirectX9 and OpenGL buttons, which the x64 Dialogs.cpp replaces by the device
  buttons of issue #31 (its creator asks the Vulkan device queries, its tree has the device line). They are not
  replayed: they retire with the oracle (M6), as issue #29's engine tests do. The device buttons are covered by the
  Vulkan tests and the reference recordings.
- **Retired parts** (high). golden::RetiredBegin(Name, Checks) and RetiredEnd(Checks) mark a part of a test that the
  runner does not replay. The oracle runs it and records two ordinary values around it: the input "retired" (the
  part's name) before, the output "retired end" (its checks and the number of values it recorded) after. The runner
  skips the part's values by their sizes up to "retired end", checks that their number is the recorded one, and adds
  the part's checks to golden::RetiredChecks(). EndReplay expects the oracle's checks of a test to be the replayed ones
  plus the retired ones; TErrorRound::Finish counts a round's retired checks as the oracle does (its printed and
  recorded counts stay the oracle's) and prints the replayed and retired ones apart; the runner prints each test's
  replayed and retired checks and fails when their sum is not the oracle's. The format version stays 1 (no new tag).
  Considered: a tag of its own with the part's length (the recorder writes sequentially; patching the length in
  afterwards gains nothing over skipping by sizes).
- **Staying in step** (high). After a retired part the test passes, as outputs of the original, what the later rounds
  read of it: Random()'s seed, the operation counts (the summary line), the error round's fake-call counts (MaxTicks,
  LastTicks: the failing calls of later rounds), the exceptions the part compared (ThrownCompared, ThrownThrew: the
  error round's summary), the original's last dump (OutputTextChanges records the next dump against it; without it
  2,603 checks failed after the first retired round) and the clock of our side's last UpdateTimers call (GUI.cpp's
  function-local LastTime, from which the next call measures the elapsed time; without it 59 timer checks failed). The
  runner calls UpdateTimers once at that clock: no timer is left between rounds, so the call updates none and only
  sets LastTime. The world, the arguments and the clock are new in every round, and the table of blocks is empty
  between rounds, so nothing else is needed; the passing replay shows it.
- **The retired checks: 9,129, not 9,069** (high; reported to the user). The parts: the 41 main rounds that open the
  dialog (3,263 checks), its 190 error-round rounds (5,806) and, found here, the three direct calls on a newly opened
  advanced graphics dialog in the assertions' second loop ("remove a button" and an unknown event, 60 checks). The
  second part's measurement (3,263 + 5,806, a temporary instrumentation) counted the first two; the third opens the
  same dialog, whose x64 creator differs, so its checks cannot be replayed with the oracle's results either. The
  runner replays 290,045 of Dialogs' 299,174 checks and retires 9,129.
- **The dump by member** (high). The old dump printed every 32-bit word of each object from +0x0c to its x86 size and
  named each word by where it would point: a tree component, a string, a timer, a block of the round
  (`H<size>+<offset>`), a module (`S`), the stack (`K`), else a number. The runner's x64 objects have no such words,
  and the words depended on the oracle's address space: text in char arrays (the hotkeys, the caption, a text
  button's text) read as `S` where the bytes fell into a DLL, an object number or house identifier (Random()) as `S`
  or `K`, block sizes in x86 bytes. Now each class's members are printed by their types, the same text from the
  original's x86 objects (through our x86 classes, which have its layout), ours in the oracle and ours in the runner:
  numbers as numbers, bools and bytes as their byte (an unset one shows the fill pattern), a char array as its text and
  the bytes after it in runs (the old words compared those too), a pointer by where it points as before, with a block
  as `H` or `D` and the offset when it is not the start (no x86 size), the fill pattern as `ab`; the radio and focus
  groups, the vectors (fields and entries), a label's lines, a multi-state button's states, the graphics dialog's
  modes and the channel IDs by their contents. Every member of every class the test builds is printed (DialogElements.h
  and Dialogs.h's member lists). The oracle's counts are unchanged (Dialogs 299,174: 98,889 + 200,285 in its error
  round, 61,097 exceptions compared, 4,786 calls that threw; difftest 16,848,752, 0 failures). Considered: projecting
  our x64 objects onto the x86 words (it would need the oracle's module and heap ranges for every non-pointer word, and
  the x86 sizes of every block).
- **By value** (high; plan 4.3). Per comparison the corpus holds the original's log, its dump (by its changes), its
  fake-call count, whether it has a frame (the runner's Frame[ORIGINAL] is a placeholder that is never read, or NULL)
  and in the error round what it threw. The runner redirects its own functions to the fakes with golden::RedirectOwn,
  among them operator new and new[] (to the pattern), operator delete and delete[] with their sized forms (to the
  table of blocks), the Send functions and CancelPatch (client_stubs.cpp's; the oracle's are this file's, as before)
  and IsClipboardEmpty and GetClipboardText (sysinc.cpp's).
- **The order of Random()** (high; #55). RandomArguments' `sprintf(Caption, "%s %d", Random() % 2 ? ... : ...,
  Random() % 100)` is two statements in the x86 oracle's order (the number first); the captions of all 1,733 creator
  dumps of a full run are the old ones.
- **Mutation check** (high). A runner-only change of one fake's log text (SetActionState's) gave 16,433 failures in
  Dialogs (reverted).
- **Size** (form). Dialogs.bin is 52 MB; the corpus is 660 MB, local only, as before.
- **Commits** (form). The retired parts, then Dialogs with the runner's reporting, then these notes.

## The ownership map, and std::unique_ptr for owning members and globals (Patella-Modern issue #58)

Settled by Claude Opus 5.5, November 4, 2026. The second change of M4 (plan section 7, "Ownership"), in two parts: the
map, notes/ownership.md (its own commit, no code), then `std::unique_ptr<T>` for the owning members and globals where
that changes nothing the comparisons see, one subsystem per commit. The issue stays open for the rest (ownership.md,
sections 3, 4 and 6).

- **The map** (high). Every `new`, `new[]`, `new (std::nothrow)`, `malloc` and `MsvcStrdup` of src/ (546 sites; the
  plan's 608 counted the word) and every `delete`, `delete[]` and `free` (212), listed by a scan that skips comments
  and literals, then read where each goes. Kinds of owner: the child list (379 GUI components: `TWindow::~TWindow`
  deletes its children through DeleteChild), members, conditional members (a child or the member's, by `IsChild`),
  init/exit globals, the event queue (`INT_PTR` parameters freed by type in ProcessEvents and ClearMessages), the
  caller, locals, until exit. It names the original's real leaks (message 0x1c's `TTalkLine::Line`; queued marks in
  ClearMessages; ExitStatus' dangling `TaintedClipboardText` and `LinkRegexp`) and its mismatched releases (`free` of
  `new` memory in Talk.cpp, Preferences.cpp, sysinc.cpp, MainWindow.cpp; `delete[]` of `TTalkLine` and `MsvcStrdup`
  blocks; SurfaceGL.cpp's `delete` of arrays): all kept.
- **The rule for a conversion** (high). A `T *` becomes `std::unique_ptr<T>` (`<T[]>` for arrays) when it is the only
  owner, the pointee is released with the matching `delete`/`delete[]`, and:
  - the pointee's destructor cannot throw. `unique_ptr::reset()` and its destructor are `noexcept`; every `TComponent`
    (`noexcept(false)`, issue #34: the TBEGIN/TEND destructors rethrow) and `TTalkBuffer` would turn an exception
    from their destructors into `std::terminate` instead of the original's error report. So `TTabBar::LeftTabs`,
    `RightTabs`, `TConsoleWindow::History` and every component member stay raw, although they are exclusive owners.
  - the original's release sites can be kept: each `delete` becomes an explicit `reset()` in the same place and order
    (the destructors' bodies keep their order, Color before Part, Attack before Chase, the label's three buffers),
    each `new` a `reset(new ...)` (the same new-expression: `make_unique` would value-initialise the arrays and the
    `TRadioButtonGroup`s, whose `Selected` the original leaves uninitialised; `make_unique_for_overwrite` is C++20,
    the oracle is C++17). Where the original deleted the old timer before creating the new one, `reset()` then
    `reset(new TTimer(...))`: a single `reset(new ...)` would register the new timer before unregistering the old.
    Where it cleared an owning global without freeing it (InitCommunication, InitObjects), `release()` keeps the
    leak. Where it freed without clearing (ExitStatus' `TaintedClipboardText`), no conversion: the dangling value
    cannot be kept.
  - the layout stays: `std::unique_ptr` with the default deleter is one pointer on MSVC x86/x64, GCC and Clang (empty
    base). Each converted class asserts its size and the member's offset under `PATELLA_ORIGINAL_LAYOUT` (the
    original's values, read from main's oracle PDB with client_state.py's reader), each file asserts `sizeof` and
    `alignof` of its `unique_ptr`s against the raw pointer. No converted class is copied bytewise, written to a file
    or a packet, or compared by memcmp; the tests' fakes (tests/golden/elementfakes.h) keep their raw pointers at the
    same offsets, and the inline TMultiStateButton::Reset reads a fake's raw `ButtonUps` through the `unique_ptr`
    representation, which is the pointer.
  - TASSERT/TERROR/TEND arguments: untouched. `PatchData==NULL` and `PatchData!=NULL` compile against a `unique_ptr`
    as written (`NULL` is a null pointer constant); no converted pointer is a value argument (`(int)(V1)`).
  - Globals: a `unique_ptr` global has a destructor; at a normal exit the Exit functions have reset it already, but
    FatalError ends the client with `exit(1)` with everything still set, and the destructors then run. Only arrays and
    Container.h tables whose destruction frees memory and nothing else were converted (`Characters`, `PatchData`, the
    four object tables); `ModalWindow`, `MainWindow`, `Socket` (an MFC CAsyncSocket after MFC's module state) and
    `Surface` stay raw.
- **What was converted** (high). 28 owning pointers: 22 members of 17 classes (the console's selection and tab
  captions, the combat groups and the VIP hover text, the dialogs' timers, focus and radio groups and channel IDs, the
  label's full text, the link's destination, the multi-state button's arrays) and 6 globals (`Characters`,
  `PatchData`, the four object tables); the table in ownership.md, section 6.
- **Behaviour** (high, except where marked). Unchanged where the client keeps running. Two differences on paths that
  end it (medium: no comparison reaches them, and both end in FatalError): a constructor that throws after allocating a
  converted member frees it while unwinding (the original leaked it, and a TReconnectWindow's timer stayed
  registered); a converted global still set at `exit(1)` is freed during static destruction.
- **Tools and tests** (high). tools/client_state.py's layout check got `std::unique_ptr<char [0],std::default_delete<char
  [0]> >` for TLabel.FullText (and the global Characters) from the PDB and reported a problem (`--layout` with the
  tool of main on the new x64 Patella.exe: "PROBLEM field TLabel.FullText"); Pdb.kind reads a `std::unique_ptr` with the
  default deleter as `ptr` (its length is still checked against the image's pointer size). The test mode's state dump
  reads FullText through `.get()`. Forced test changes: `.get()` where tests/golden's dumps pass a converted pointer
  through varargs or to a function (consolewindow_tests.cpp, contentwindows_tests.cpp, dialogelements_tests.cpp),
  consolewindow_tests.cpp's SaveChannel error round takes the tab caption out with `release()` and puts it back with
  `reset()` only when it took it, objects_tests.cpp's OurTable returns the held pointer, tools/difftest/errortest.cpp's
  table list takes `.get()`. tests/golden's Dialogs port (the other #28 work) reads `Timer == NULL`, which compiles
  unchanged.

## The golden corpus: MapWindow, MainWindow and the retired engine tests (Patella-Modern issue #27, second part)

Settled by Claude Opus 5.5, November 4, 2026. tests/golden/ (mapwindow_tests.cpp and mainwindow_tests.cpp moved from
tools/difftest, elementfakes.{h,cpp}, golden_test.h's GOLDEN_THIS_NAMED, runner.cpp's RetiredTests),
tests/golden/CMakeLists.txt, cmake/OracleX86.cmake. notes/reconstruction.md, "Checking", describes the use; the
sections on issues #24, #26, #27, #54, #30 and #28 above the format and the earlier helpers.

- **Ported** (high). MapWindow 127,529 (12,359 in its error round: 1,776 exceptions compared, 1,394 calls that threw)
  and MainWindow 57,314 (43,150 in its error round: 9,974 compared, 229 threw), each the oracle's count with 0
  failures in Release and Debug; nothing retired. The runner replays 15,310,916 of difftest's checks and sysinctest's
  67. Every CHECK, random number and operation is the old one; both summary lines ("each of the 24 operations at least
  236 times; entries: ...", "at least 219 times") are the oracle's in the runner too. A runner-only change of one
  fake's log text gave 19 failures in MapWindow and 2,767 in MainWindow (reverted).
- **TMapWindow by member, no projection** (high; plan 4.3). The old test compared the windows' bytes from +0x14 to
  HintButton without TWindow's children: TComponent's OffsetX, OffsetY, Width, Height and every member of
  TMapWindow up to HintButton. None of those is a pointer, and their bytes, padding included, are the same on x86 and
  x64 (static_asserts on the members' distances); only where they start differs (TComponent's and TWindow's
  pointers before them). So the corpus holds those members as bytes in the x86 order, by their runs of changed
  bytes (golden::OutputRuns), ours are read by member, and the comparison and its message (the first differing x86
  offset, the ints there) are the old ones. The same for TMapFrame's geometry and view, its map view and its drawing
  list (TDrawing: seven ints). GetCacheField's result is compared as the x86 offset (the tile's place in the cache
  plus the cache's 0x29d4). The error round's save and restore of the window state copies our members from OffsetX
  to HintButton, as the old 0x14..0x92d4. Considered: a layout projection as Container's (unneeded: no pointer among
  the compared members).
- **MainWindow's fakes** (high). The console and the side-bar windows are this file's classes as large as the real
  ones (`sizeof(TConsoleWindow) - sizeof(TWindow)` instead of the x86 0x6c - 0x2c; static_asserts); our constructors
  are redirected through their x64 decorated names; the constructor and member stand-ins are GOLDEN_THISCALL
  (GOLDEN_THIS_NAMED where they read this). The border, the first text button constructor and the label's font
  constructor (elementfakes.h) are this file's on both sides; elementfakes.cpp's runner versions (report-only and
  stopping) are gone, and no fake is "not ported" any more. The dump was text already; it is recorded by its changes
  (OutputTextChanges), the state of the original's fakes (action and connection state, open dialogs, fullscreen
  mode) and whether it had a trade window by value. The check inside the original's call of case 23 ("0x4ac090
  leaves a window") is recorded as not reached, failed or passed and made after the calls on both sides (the count
  is the same; only its place among the round's checks moved). After the original's call Side is the original's in
  the runner too, so what our code logs between the calls goes where it went in the oracle. The console's text
  buffer was printed with %p (eight digits on x86, sixteen on x64): it is printed with %08X, the same text.
- **The order of Random()** (high; #55). Expressions with two calls were sequenced in the x86 oracle's order, found
  with a probe built with the oracle's flags (/std:c++17 /O2 /EHa, x86): the arguments of TRGBColor's constructor
  (MapWindow's colours, MainWindow's skin properties) and of SetWidth right to left, but the sum
  `(Random() % 2 ? MAPSIZE_X : MAPSIZE_Y) + (int)(Random() % 5)` left operand first. A first attempt with the sum
  sequenced right first changed the error round (12,361 checks, 1,395 that threw): the oracle's counts caught it;
  with the probe's order every count, summary and entry line is the old one.
- **#29's engine tests are retired** (decided by the user, #29 closed "not planned"; plan M6). SurfaceDX (1,368,981),
  SurfaceDX9 (63,616) and SurfaceGL (55,702) are whole tests retired with the oracle: runner.cpp's RetiredTests
  names them with the reason "#29"; the runner prints them as retired, counts their checks with the retired ones
  (1,497,428 with Dialogs' 9,129) and no longer as not ported. Its last line gives replayed, retired and not ported
  checks: 15,310,983 + 1,497,428 + 1,174,921 = 17,983,332, the oracle's totals.
- **Errors (#26)** (needs a later issue). In place now: every difftest test before Errors in difftest's order is
  replayed or retired; MapWindow and MainWindow, which run after it, are ported. What that means for its port: the
  engine tests will never run in the runner, so Errors cannot rely on what they leave behind; their patches are
  restored, but its "250 comparisons skipped (functions redirected by earlier tests)" counts the redirects present
  when it runs. Its port must record that set (or the skipped comparisons) by value in the oracle instead of finding
  it in the runner (a portable Redirected; the x86 decoding of a jump stays the oracle's), record whatever state of
  our side the engine tests leave that Errors reads (curEngine, the surface), and run in the runner's table before
  Network, MainFrm, MapWindow and MainWindow, whose redirects (MapWindow's and MainWindow's for good) come after it
  in the oracle. Still open: errortest.cpp's SurfaceInitInstance part (its "Statics" checks) compares the x86
  Surface.cpp's engine selection (CreateSurfaceDX9, curEngine 0 to 2), which the x64 Surface.cpp replaces by the
  Vulkan engine: as Dialogs' engine buttons, it can only be retired or replayed against code the x64 client does not
  have, a decision for the user; and its skips are found with ImageRedirected/Redirected, which decode x86 jumps.
- **Size** (form). MapWindow.bin is 182 MB (its logs: 5.7 million map-object calls), MainWindow.bin 9 MB; the corpus
  is 843 MB, local only, as before.
- **Commits** (form). MapWindow, MainWindow, the runner's summary, these notes. The MapWindow commit's tree was built
  (oracle and x64) and replayed on its own, the MainWindow commit's too.

## ctime's text written by the client: FormatCRT71Time (Patella-Modern issue #61)

Settled by Claude Opus 5.5, November 4, 2026. The third change of M4 (plan section 7, "crt71.h"): the client writes
ctime's text itself, in MSVC 7.1's form, and src/ calls neither `ctime` nor `_ctime32` any more (comments name them).

- **What was there** (high). Two calls: Player.cpp HandleException `CRT71Time(ctime(&Now))` (the 64-bit time_t of
  time(): `_ctime64` on x64 and in the oracle, glibc's ctime on Linux) and ConsoleWindow.cpp SaveChannel
  `CRT71Time(MsvcCtime32(&Now))` (the Time32 of `_time32`). `CRT71Time` changed the 9th character of ctime's static
  buffer from ' ' to '0' (issue #21). `MsvcCtime32` (#43: `_ctime32` on Windows, `PortableCtime32` over `std::ctime`
  elsewhere) had no other caller.
- **The function** (high). `src/compat/crt71.h`: `const char *FormatCRT71Time(TCRT71TimeText &Text, std::int64_t
  Time)`, `TCRT71TimeText` = `TFixedString<26>` (25 characters and the NUL). It writes `"{} {} {:02}
  {:02}:{:02}:{:02} {:02}{:02}\n"`: the day and month names, then every number in two digits, the year as `19 +
  tm_year / 100` and `tm_year % 100`, exactly the fields of MSVC 7.1's asctime (0x517f6d, store_dt: `'0' + v / 10`,
  `'0' + v % 10`), and returns Text. It formats through `TFixedString::Format`: `std::format_to_n` on the product
  presets, the C++17 stand-in of compat/format.h in the oracle (#57), one call site for both. A Time32 converts to
  the parameter unchanged, so neither caller's time type changed (#62).
- **No text where 7.1's ctime gave none** (high). nullptr for a time before 1970: MSVC 7.1's localtime (Tibia.exe
  0x513eb1, decompiled with REA) returns NULL only for a negative time, as `_ctime32` does. nullptr after 3000-12-31
  23:59:59 UTC (`CRT71LastTime`, the UCRT's `_localtime64_s` limit), checked before the C library on every platform
  so that Linux formats the same times. The callers: SaveChannel wrote `fprintf("%s", NULL)`, which MSVC 7.1's, the
  UCRT's and glibc's printf all write as "(null)"; it now passes "(null)" itself (no null `%s`). HandleException
  wrote `Out << (char *)NULL`, a crash in MSVC's STL (and in 7.1's); it now writes nothing. Neither is reachable
  with a clock after 1970.
- **Local time: the C library's localtime, not std::chrono's zones** (high). The original's fields are MSVC 7.1's
  localtime: the zone of TZ or of Windows' GetTimeZoneInformation, with that rule applied to every year. The UCRT
  does the same (with TZ, the United States' rules of before and after 2007 by year), and the client used it before
  this change through `_ctime32`/`_ctime64`. `std::chrono::current_zone()` (on Windows ICU's zones, with their
  history) ignores TZ and would give other hours for past years whose rules changed, and for every run that sets TZ:
  the reference compare runs both clients with TZ=UTC0 (tools/reference.py TIME_ZONE), and difftest and the golden
  ConsoleWindow test redirect the image's localtime and ctime to the UCRT's. So `patella::compat::CRT71LocalTime`
  calls `_localtime32_s` on Windows for a time up to `UCRTLastTime32` (exactly what `_ctime32` used),
  `_localtime64_s` after it, and `localtime_r` elsewhere (glibc: TZ, else /etc/localtime; on Linux the original's
  CRT does not exist, the system's zone is the meaning). std::chrono's calendar is the unit tests' independent
  reference instead (below); the oracle's C++17 has no chrono calendar either.
- **The last 3 h 14 min of 32 bits** (high; found here). The UCRT's 32-bit localtime stops at 0x7fffd27f,
  2038-01-18 23:59:59 UTC (`time_traits::max_time_t`, ucrt/inc/corecrt_internal_time.h), so `_ctime32` gave NULL up
  to INT32_MAX, where MSVC 7.1's localtime converts every time up to INT32_MAX (it adjusts the tm itself in the first
  and last three days). FormatCRT71Time takes `_localtime64_s` there and writes the original's text. On Windows
  `_time32` itself returns -1 after 0x7fffd27f, so SaveChannel still writes "(null)" then (unchanged); only a
  64-bit time (HandleException, or Linux) reaches the window. The three CRTs adjust near the ends in slightly
  different places (whether daylight saving time is tested on UTC or on local standard time); that matters only at
  a change of daylight saving time within those days (none in 15 to 19 January 2038 in the zones tested).
- **Unit tests** (high). tests/unit/crt71_test.cpp in tests_unit and tests_unit_cxx26_standin (both format paths),
  TZ set for each case and restored (compat/msvc.h's `MsvcDupenv`/`MsvcPutenv`, then `_tzset`/`tzset`): texts in UTC
  computed apart with Python (every month and weekday, leap days, 1970, INT32_MAX, 2100, 2400, 3000); every day of a
  month (the two-digit day); nullptr below 0 and above the year 3000; PST8PDT around 2026's changes and the first
  second of 1970 (still 1969); std::chrono's `"{:%a %b %d %H:%M:%S %Y}"` (two-digit day too) for a prime step from
  1970 to 3000 and every day of 2024 to 2028 in UTC, and in PST8PDT from `year_month_weekday` rules around both
  changes of every year 2007 to 2037; on Windows the UCRT's `_ctime32` with its day made two-digit (the old
  client's text, which compare_files.py and difftest held against Tibia.exe) for every hour of 1970 to 2038 and the
  second before it, and every 61st second of the last four days, in UTC0, PST8PDT (the UCRT's rules of both eras),
  IST-5:30 and the machine's own zone, with `_ctime64` in the last 3 h 14 min. Windows (MSVC): 7 cases, 95
  assertions in each program; Linux (GCC 16, Clang 22 with ASan and UBSan): 6 cases, 86 (no `_ctime32`).
  core_msvc_test.cpp lost its 8 `_ctime32` assertions with `MsvcCtime32`: tests_core 33/332 (33/335 on Linux).
- **The UCRT keeps the system's daylight bias under TZ** (high; found by the hosted CI). The first hosted run failed
  6 PST8PDT checks and the chrono comparison on Windows: GitHub's runners are in UTC, and the UCRT's TZ parser
  (ucrt/time/tzset.cpp, tzset_from_environment) sets `_daylight` and `_timezone` but keeps `_dstbias` from the last
  reading of the system's zone, 0 there, so "PST8PDT" had a daylight saving time of 0 minutes (here, in a zone with
  daylight saving time, -3600). The `_ctime32` comparison passed (both sides read the same state): it is the CRT's
  state, which the client gets as before, not FormatCRT71Time's. The test's TTimeZone sets the one hour of a TZ value
  through `__dstbias()` (the UCRT's `_set_dstbias` is not public) and restores the bias after, and the fixed-zone case
  sets the bias to 0 first, as on the runner (without the fix it fails the same 6 checks here). The client does not
  set TZ; the reference compare sets TZ=UTC0, which has no daylight saving time.
- **Forced test change** (high). tools/difftest/errortest.cpp gave the image's ctime (0x5121d1) and our `_ctime64`
  one fixed UTC text, "Thu Jun 08 12:34:56 2006". Ours no longer calls `_ctime64`; it formats the faked clock
  (1149770096) in local time. The image's ctime now returns the UCRT's `_ctime32` of its time with the day made
  two-digit, which on 8 June is the two-digit-day case itself; the Errors counts are unchanged. tests/golden's
  ConsoleWindow test keeps its ImageCtime (`_ctime32` unpatched: at its fixed clock, 1700000000, the day is 14 or
  15 in every zone).
- **Headers** (high). compat/crt71.h is portable now: on cmake/HeaderCheck.cmake's core list (it was on the client
  list). It includes types/FixedString.h.
- **For #62** (medium). The time types are untouched: SaveChannel's Time32 (`_time32`: -1 after 0x7fffd27f on
  Windows), HandleException's time_t, Talk.h's and Creatures.h's 4-byte fields. FormatCRT71Time takes 64 bits
  already, so #62 changes only the callers. Found: compat/msvc_portable.h's `PortableTime32` returns -1 after
  INT32_MAX, the UCRT's `_time32` after 0x7fffd27f, 3 h 14 min 8 s earlier (#43's comment says INT32_MAX for both):
  the platforms disagree in that window; #62 removes it with the 32-bit time.

## The golden corpus: Errors (Patella-Modern issue #26, second part)

Settled by Claude Opus 5.5, November 4, 2026; the retirement decided by the user the same day. tests/golden/
(errors_tests.cpp moved from tools/difftest/errortest.cpp, runner.cpp, golden_test.h), tests/golden/CMakeLists.txt,
cmake/OracleX86.cmake. notes/reconstruction.md, "Checking", describes the use; the sections on issues #24, #26, #27,
#54, #30 and #28 above the format and the earlier helpers.

- **Retired with the oracle** (decided by the user, November 4, 2026). The Statics round's SurfaceInitInstance part
  compares the x86 Surface.cpp's engine selection (IsDirect3D9Installed, CreateSurfaceDX9, GetEngine, curEngine 0 to
  2), which the x64 Surface.cpp replaces by the Vulkan engine (curEngine always 2): it cannot be replayed against the
  x64 client's code. The oracle runs and records it between golden::RetiredBegin and RetiredEnd ("SurfaceInitInstance");
  the runner skips it: **36 checks** (12 calls, each a "did not throw" check, the exception comparison and the engine
  check; every call throws PROGRAM_ERROR 12, so no line pairs). After it the test passes the exceptions the part
  compared (the round's summary) and ErRandom()'s state through golden::Output, as Dialogs does.
- **Ported** (high). Errors 40,408 = 40,372 replayed + 36 retired, 0 failures, Release and Debug: the main part 31,912
  (9,020 exceptions compared, 7,795 calls that threw, 250 comparisons skipped) and the Statics round 8,496 (8,460
  replayed; 2,832 compared, 2,832 threw), each printed with the oracle's next to it; the runner fails when they differ
  (not a check). Every CHECK, random number and operation is the old one: an ER_TRACE=1 run of DIFFTEST_ONLY=Errors
  (every compared exception with its message) is line for line the pre-port oracle's, except the report's date (UTC
  now, below). The runner replays 15,351,355 of the oracle's 17,983,332 checks, retires 1,497,464 and lists 1,134,513
  (Communication, Control) as not ported. A runner-only change of the error hint fake's text gave 277 failures
  (reverted).
- **The redirected functions by value** (high). Errors runs after tests that redirect the original's functions and ours
  for good; it skips such a comparison ("250 comparisons skipped") by finding a jump at the function's start
  (imageeh.cpp's Redirected, x86 code). That set includes what the engine tests (SurfaceDX, DX9, GL), which never run in
  the runner, leave; and an x64 function may start with a jump of its own (a tail call). So the oracle's answer is
  recorded per comparison ("redirected", one byte) and the runner skips exactly those; TestWrapping's three "the callee
  is real" flags, the talkbox getters' redirect and the statics' getters are the same answer. The comparisons that are
  not skipped call our functions in the runner; all pass, so our side's redirect state matches the oracle's where it
  matters. Errors runs in the runner's table after Dialogs and before Network, MainFrm, MapWindow and MainWindow, as in
  the oracle (MapWindow's and MainWindow's redirects are for good). Considered: a portable Redirected (an x64 jmp at
  the start is not always a test's patch).
- **What the engine tests leave** (high). Nothing Errors reads: TestPlayer sets curEngine before each report, the
  graphics counts are set and restored by TestGraphics, GetSurface's redirect is the Graphics test's (in the runner
  too), and the engine tests restore their patches. The original's talkbox count is recorded (ORIGINAL_VALUE), ours is
  the runner's: equal, so the talkbox getters run in both.
- **Our statics in the runner** (high). The Statics round sets NumberOfBuddies, NumberOfPlayerSteps,
  AccountCharacters and the offers' counts, which are in unnamed namespaces. The oracle finds them by decoding the
  getters' x86 code (FindStatic); the runner by the getters' x64 code: `mov r32, [rip+disp32]` (8b /r, mod 00, r/m
  101, a REX prefix without W allowed), each candidate probed as the oracle's (the getter must see a change). Whether
  the oracle found both sides' is recorded; a runner that cannot find ours fails.
- **TestPlayer in the runner** (high). Our CPlayerApp::FatalError, OutputControlErrorInformations (client_stubs.cpp),
  GetErrorText and GetErrorHint (sysinc.cpp) go to the fakes with golden::RedirectOwn (SaveOwn/RestoreOwn), _time64 and
  GetVersionExA through the runner's import slots; all is undone where the oracle's DisableImageExceptions undoes its
  patches (before the Statics round), so sysinctest's later replay sees sysinc.cpp's own functions. The fake
  FatalError must not return: the oracle's raises an SEH exception (the image's frames are in the call too), the
  runner's throws a C++ exception of its own type, caught around the call (under the runner's /EHsc an SEH exception
  would skip our destructors). The original's error.txt, FatalError texts and Terminating are recorded by value.
- **Machine-independent texts** (high). Since issue #61 the report's date is the C library's local time on both sides
  (our FormatCRT71Time, the image's ctime fake through the UCRT's _ctime32), so the recorded error.txt depended on
  the recording machine's time zone: TestPlayer now writes the reports with TZ=UTC0 on both sides (_putenv_s and
  _tzset, the earlier TZ back afterwards), in the oracle and the runner alike. The script errors name their file in
  %TEMP%\errortest, and TEMP differs between the recording and the replay: both sides' messages have the directory
  replaced by "%TEMP%\errortest" before the comparison (in the oracle the same replacement on both, so the
  comparison is the old one; the line pairs are not where a path is).
- **The order of ErRandom()** (high; #55). Calls whose arguments called ErRandom() more than once (SetPlayerSkill,
  SetHotkey, InsertMemoryItem, InsertVideoItem) are statements in the x86 oracle's order (arguments right to left);
  RandomValue's `ErRandom() ^ (ErRandom() << 16)` the left operand first; the trace comparison above proves the order.
  The old COMPARE's two sides were one call's arguments: ours runs first now (they share no state).
- **Size** (form). Errors.bin is 2.7 MB; the corpus 846 MB, local only, as before.
- **Commits** (form). The port with the runner's table, then these notes.

## 64-bit time everywhere (Patella-Modern issue #62)

Settled by Claude Opus 5.5, November 4, 2026. The fourth change of M4 (plan section 7, "Times"): the client reads the
wall clock in 64 bits only, and src/ calls no 32-bit time function. The original's two 4-byte time fields keep
their size (frozen until M6) and convert at one place each.

- **What was there** (high). `PATELLA_TIME32` (TestHooks.h: `MsvcTime32`, i.e. `_time32` on Windows and
  `PortableTime32` over `std::time` on Linux; the test build's `WallTime32`) in SetSkull and UpdateSkulls
  (Creatures.cpp), the battle list's skull (ContentWindows.cpp), the two TTalkLine constructors (Talk.cpp, with
  `MsvcLocaltime32` for the stamp) and SaveChannel (ConsoleWindow.cpp); `PATELLA_TIME` (`time()`, 64 bits) in
  HandleException only; `CRT71LocalTime` (compat/crt71.h, #61) took `_localtime32_s` up to 0x7fffd27f.
- **The original's time()** (high; decompiled with REA). Tibia.exe 0x5121ea (MSVC 7.1's `time`):
  `GetSystemTimeAsFileTime`, minus the 1601-to-1970 bias, divided by 10^7 in 64 bits (`__aulldiv`), returned as its
  low 32 bits. No -1 at 2038: the value wraps to negative at 2038-01-19 03:14:08 UTC. Neither the UCRT's `_time32`
  (-1 after 0x7fffd27f, 2038-01-18 23:59:59 UTC) nor #43's Linux stand-in (-1 after INT32_MAX) was that; #43's
  comment said INT32_MAX for both. **Resolved**: both are gone, and the one rule for a 4-byte field is the
  original's own truncation, the same on every platform (src/types/Time32.h `StoreTime32`).
- **src/types/Time32.h** (high). `Time32` = `std::int32_t` (4 bytes, alignment 4, static_assert; on Windows it was
  `__time32_t`, `long`, same size and representation), `StoreTime32(int64)` = the low 32 bits as a signed number
  (the time itself from 0 to INT32_MAX), `LoadTime32(Time32, Now)` = the time nearest Now with those 32 bits (from
  Now - 2^31 to Now + 2^31 - 1): the field itself when field and Now are both from 0 to INT32_MAX, so a comparison
  in 64 bits gives the original's 32-bit result up to 2038. Portable, constexpr, on HeaderCheck.cmake's core list.
- **The boundary** (high). `TCreature::SkullTime` (+0x98, static_assert in Creatures.cpp, active in the oracle):
  written only in SetSkull (`StoreTime32(PATELLA_TIME(nullptr))`), compared only in `TCreature::SkullKept(Now)`
  (Creatures.h, ours: set, and Now from LoadTime32 of it to four seconds after), which UpdateSkulls and the battle
  list call; MapWindow and the battle list test it against 0. `TTalkLine::Time` (+0x3c in the oracle,
  static_asserts in Talk.cpp): written only in the two constructors (`StoreTime32(LineTime)`, LineTime =
  `PATELLA_TIME(nullptr) + DiffTime` in 64 bits; StoreTime32 of the sum is the original's wrapping 32-bit sum),
  never read by the client (the golden Talk test compares it). The stamp comes from LineTime through
  `CRT71LocalTime`; SaveChannel formats its 64-bit time (FormatCRT71Time). Every read of the clock is
  `PATELLA_TIME` (`time()`: `_time64` on Windows); `PATELLA_TIME32`, `WallTime32`, `MsvcTime32`,
  `MsvcLocaltime32`, `PortableTime32` and `PortableLocaltime32` are removed (core_msvc_test.cpp's case with them).
- **Before 2038: unchanged** (high). The fields hold the same values (StoreTime32 is the identity there), the skull
  comparisons give the same results (time32_test.cpp holds `SkullKept` against the original's 32-bit check on every
  pair near 1970, 2023 and 2038), and the texts are the same: `CRT71LocalTime` now takes `_localtime64_s` for every
  time on Windows, which crt71_test.cpp's comparison with `_ctime32` (every hour of 1970 to 2038 and every 61st
  second of the last four days, in UTC0, PST8PDT, IST-5:30 and the machine's zone) holds equal to `_localtime32_s`.
  Two cases the original handled differently only with a clock before 1970 (impossible on Windows) or a server
  offset that makes a talk line's time leave 1970..2038: there the original's localtime gave NULL and the
  constructor crashed (a null read); now the stamp is the 64-bit time's, or "00:00" where the C library gives no
  local time (before 1970, after 3000). And `SkullTime > 0` / `< 1` became `!= 0` / `== 0`, the same for every time
  the clock gives after 1970.
- **After 2038: works as before 2038; differs from the original** (decided here, the user may reverse it). The
  original from 2038-01-19 03:14:08 UTC: a talk line crashed the client (localtime of a negative time is NULL);
  SaveChannel wrote "Channel saved at (null)"; the kept skull did not blink (MapWindow's and the battle list's
  `> 0` took the negative field for none) though it still went after four seconds; and in the last four seconds of
  32 bits its `SkullTime + 4` wrapped, so a skull removed then went at once. Now: the stamps and the saved channel's
  text are the real local time, and a kept skull blinks for four seconds, until the year 2106 and after it
  (LoadTime32 reads across the wrap). The 4-byte fields still hold exactly the original's bits. The only places
  the original's 32-bit value shows: at 2106-02-07 06:28:16 UTC (and 2^32 seconds later) StoreTime32 is 0, which
  SkullTime reads as "no skull" (that second's removed skull goes at once, as in the original); and a field read
  more than 68 years after it was written comes back 2^32 seconds off (no field lives that long). The fork's x64
  client before this change: `_time32` returned -1 from 2038-01-19 00:00:00 UTC (Linux from 03:14:08), so its
  talk line crashed too ("_localtime32(-1)" is NULL), its saved channel wrote "(null)", and its skull stayed
  forever (-1 + 4 was never passed).
- **Forced test change** (high). tests/golden's Talk, Creatures, ConsoleWindow and ContentWindows tests faked our
  clock by patching our `_time32` (the oracle's static CRT) or the runner's import slot of `_time32`; ours is
  `_time64` now, so they patch `_time64` (the oracle) and its import slot (the runner) with a 64-bit fake of the
  same fixed time (1700000000). The image's side is unchanged (0x5121ea still goes to the 32-bit fake). The Errors
  port (#26, tools/difftest/errortest.cpp) is not affected: errortest already fakes our `_time64` and the image's
  `time` the same way.
- **Unit tests** (high). tests/unit/time32_test.cpp, in tests_unit and tests_unit_cxx26_standin (portable, no
  platform branch): the type, StoreTime32 against the original's truncation from 1970 to 2300 and against its
  wrapping talk-line sum, the UCRT's and the old stand-in's end values, LoadTime32's identity on a grid of the
  32-bit range and its round trip within 68 years past 2038 and 2106 with the window's ends, the skull check
  against the original's on every pair near 1970, 2023 and 2038, and the skull kept four seconds at 2038's and
  2106's wraps; also `static_assert(sizeof(std::time_t) == 8)`.
- **Gates** (high). oracle-x86 difftest counts unchanged (16,848,752 / 67 / 1,134,513 / 3,201), golden_runner
  15,310,983 replayed, 0 failures, unit tests as in notes/progress.md, "Last difftest". The x64 images change (the
  call sites); the oracle's too (its code calls `_time64` and `_localtime64_s` now): no tier A or reference
  compare was run here (no client may be started by this change's worker); see the issue's report for the
  commands.

## The golden corpus: Communication, and Control's engine change (Patella-Modern issue #30, second part)

Settled by Claude Opus 5.5, November 4, 2026. tests/golden/ (communication_tests.cpp, commworld.h, commworld.cpp and
commseeds.h moved from tools/difftest; golden_patch.h, network_tests.cpp, client_stubs.cpp, runner.cpp,
golden_test.h), tests/golden/comm/CMakeLists.txt, tests/golden/CMakeLists.txt, cmake/OracleX86.cmake. The section on
issue #30's first part above lists what blocked the group; notes/reconstruction.md, "Checking", describes the use.

- **Communication ported** (high). 856,366/856,366 replayed (364,041 + 492,325 in its error round, 35,252 exceptions
  compared, 8,008 calls that threw, printed next to the oracle's), 0 failures, Release and Debug. A runner-only change
  of one fake's log (GetMapLevel's value) gave 5,295 failures (reverted). The runner replays 16,207,721 of the oracle's
  17,983,332 checks, retires 1,497,464 (unchanged) and lists Control's 278,147 as not ported.
- **By value** (high). commworld.cpp's CompareRun compares texts and creature bytes; the oracle records the original's
  side of each comparison while a ported test records (RecordComparisons: Communication only, so Control's corpus stays
  empty): what it threw (golden::OutputThrown), its log, each text buffer's dump and its world's dump
  (OutputTextChanges: the error round runs one frame failing at each call in turn, so consecutive logs share most of
  their text), and its six creature records as bytes (OutputRuns; TCreature is 0x9c bytes of ints and chars on both
  targets, static_assert), which go into the runner's original world, so the creature checks are the old code. Also by
  value: the readers' ReadCreature result, the first byte of the image's shared empty Exit (0x496160) and Tibia.dat's
  flag and waypoint tables (LoadObjectFlags; the runner needs neither Tibia.dat nor Objects.cpp's state). The runner
  builds the original's world as the oracle does (the test sets both), but no original code runs on it. The
  communicationtest corpus is 41 MB (Communication.bin; Control.bin has no records).
- **One runner** (high). The group's definitions that clash with the runner's are the oracle's only: PatchImage
  (status_tests.cpp's), CAsyncSocket's members and AfxSocketInit (network_tests.cpp's fakes), PacketHeader and
  DisplayChanged (mainfrm_tests.cpp compiles MainFrm.cpp). The world's own globals did not clash (the other tests keep
  theirs in unnamed or test namespaces). A second executable would duplicate golden_client and the CTest test (13
  tests stay 13) for no gain: the runner already runs the group after difftest's and sysinctest's tests, and what
  those leave is undone (below).
- **x86-only mechanics replaced** (high). The 27 `__fastcall (This, void *, ...)` fakes are `GOLDEN_THISCALL
  (GOLDEN_THIS_NAMED(...), ...)` (25 in commworld.cpp, TRSA::Encrypt's in the test; the 26th and 27th, the image's
  GetByte and vlong constructor, are the oracle's only); the ten in-place constructors (THelpBubble, the nine context
  menus) are reached through their x64 decorated names (the oracle keeps the x86 ones); ThisOfFirstCall and
  FollowJumps stay in the oracle, the runner gets Network.cpp's ReceiveBuffer and SendBuffer by name from
  network_tests.cpp (which compiles Network.cpp) and resets them to their start state; our functions go to the fakes
  with golden::RedirectOwn (CheckRoom's scan reads bytes, valid on x64 too; the runner has no incremental-link thunks),
  rand() and Sleep() through the runner's import slots (the oracle patches its static CRT's rand); TCtlMainWindow's
  members are static_asserted at TMainWindow's offsets on every target and TCtlWindow no larger than each class built
  in place; the console's CurrentConsole and TextWindow are written at offsetof of our classes (0x34 and 0x2c in the
  oracle, asserted there). FillStack is portable and kept.
- **Nothing left patched** (high). golden_patch.h remembers the first bytes of every function RedirectOwn redirected
  and the first value of every import slot SetImport bound; RestoreAllOwn and RestoreAllImports undo them all.
  communicationtest.exe starts with nothing of ours patched; difftest's tests leave patches for good in the runner
  (MapWindow, MainWindow), so BeginWorld undoes them first, and EndWorld undoes the test's own at its end, where the
  oracle's DisableImageExceptions does.
- **Communication.cpp and Control.cpp in the runner** (high). Communication needs Control.cpp: ReceivePackets is
  Control.cpp's dispatcher (EnterGame and HandleEvents are faked). Both compile, the x64 client's (PATELLA_VULKAN), in
  tests/golden/comm with the test and its world; their stubs left client_stubs.cpp; the device-choice functions of
  Control.cpp's x64 ChangeEngine are aborting stubs (Communication never calls it).
- **/EHa for tests/golden/comm** (high). The error round raises structured exceptions that both sides' catch (...)
  catch: the failing TRSA::Encrypt's RaiseException, and ReceivePatchBlock's division by the size of a zero-sized
  patch. VC7.1's catch (...) catches them; an x64 catch (...) does so only under /EHa. The oracle builds with /EHa;
  the directory replaces CMake's /EHsc with /EHa (as tests/golden/network replaces it with /EHs). Note for the client:
  under the product's /EHsc the x64 client does not catch that division by zero where the original logs it (a server
  that sends a block of an empty patch); not changed here (src/).
- **No /RTC1 in Debug for tests/golden/comm** (high). Control.cpp's ReceivePackets passes `int Container;`, not yet
  assigned, to ReceiveOpenContainer and ReceiveCloseContainer, whose TEND prints it when their first GetByte throws.
  The test fills the stack with 0x5a5a5a5a before each side's call, so the original and our Release build print
  that; Debug's /RTCs filled the local with 0xcccccccc (4 failures; and the first-byte check of ExitCommunication saw a
  Debug prologue). The directory drops /RTC1 from its Debug flags; Debug then passes like Release.
- **Control not ported: its engine change is not the x64 client's** (needs the user). Control.cpp's ChangeEngine is
  the x86 engine selection (DirectX 5, OpenGL, DirectX 9, curEngine 0 to 2) in the oracle and the device choice of
  issue #31 under PATELLA_VULKAN (the x64 client's, which the runner compiles): different calls, different state, and
  its callees are not in the runner. CheckSurface calls ChangeEngine(0) whenever the surface's Init returns false (a
  quarter of the test's worlds), and Redraw, ProcessEvents and the event handlers call CheckSurface; Step 11 calls
  ChangeEngine directly. Measured on the oracle's full run (a local instrumentation, not committed: a comparison
  reaches ChangeEngine when it is Step 11 or its log has "Surface.Init(...)=0"): **1,518 of Control's comparisons
  reach it, 21,979 checks** (1,088 comparisons with 14,249 checks in the main part, 430 with 7,730 in the error round),
  and the Control.cpp statics compared right after them are 2,176 checks more (CompareStatics, main part). Options:
  (a) retire the 21,979 with the oracle (RetiredBegin per comparison, decided by the oracle after both sides ran, with
  Control.cpp's statics, curEngine and the random state recorded after each so the replay stays in step; the 2,176
  replayed against the restored statics, or retired too: 24,155), port the other 256,168 (or 253,992); (b) replay all
  278,147 against Control.cpp compiled without PATELLA_VULKAN in the runner (the x86 ChangeEngine on x64: code of src/
  the x64 client does not have); (c) leave Control unported until M6 retires the oracle. Control is untouched until
  the user decides (only its include of the moved commworld.h changed).
- **Commits** (form). Communication's port, then these notes.

## TFile, std::format in the writes and the formats (Patella-Modern issue #64)

Settled by Claude Opus 5.5, November 4, 2026. The fifth change of M4 (plan section 7, "Formatting and I/O"): the
client's files have an owner, and src/ has no `FILE`, fopen, fclose or printf outside that owner (a grep gate).

- **What was there** (high). Script.h's four classes (TReadScriptFile, TWriteScriptFile, TReadBinaryFile,
  TWriteBinaryFile: Tibia.cfg, the `.map` files, the patch files) with a `FILE *File` member (frozen: +0x004 and
  +0x000) and fopen "rb"/"wb", getc/ungetc, fputc, fread, fwrite, fseek and `_eof(_fileno())`; SaveChannel's
  `fopen("<caption>.txt", "at")` and fprintf, TTalkBuffer::Write(FILE *) with fprintf "%s\n"; MouseHover's
  `printf("%s\n")` on stdout; the test mode's packets.log ("wb"), `--test-log` ("w", text), frames.txt (fprintf) and
  messages.txt ("ab") through `_wfopen_s`. error.txt is a std::ofstream/std::ifstream (Player.cpp) and stays one.
  clang-tidy's modernize-use-std-print reported the four printf/fprintf calls (Windows-only files, so not in the
  Linux image's baseline).
- **The type** (high). `src/types/File.h`: `TFile`, whose only member is the `FILE *` (`IsFileLayout`: its size,
  alignment and standard layout; Script.h asserts the four classes' sizes and offsets under
  PATELLA_ORIGINAL_LAYOUT), closed by its destructor, movable, not copyable. Its functions are the C library's,
  with their results: `Open(Name, TAccess::Read/Write/Append, TContent::Binary/Text)` is fopen (false where fopen
  returned NULL, errno from the library; an open TFile is not opened again: EBUSY), `Close` is fclose, `Read`/`Write`
  fread/fwrite (whole items; `Write(std::string_view)` for a text), `PutChar`/`GetChar`/`UngetChar`
  fputc/getc/ungetc, `Seek` (also for rewind, which clang-tidy's bugprone-unsafe-functions rejects), `Flush`, `Descriptor` (_fileno: TReadBinaryFile::eof keeps
  `_eof(descriptor)`, whose answer depends on what the FILE's buffer read ahead, as in the original),
  `OpenTemporary` (tmpfile), `Address` (the FILE's address: TTalkBuffer::Write's catch value `bytes`, which the
  original's report shows), and comparisons with nullptr, so `File == NULL` and TASSERT(1672, `F!=NULL`), whose text
  is frozen, compile unchanged. Nothing throws or reports: the callers raise their TERRORs as before (the texts and
  lines unchanged; macrocheck 0). Header-only (no new source in the oracle's or difftest's lists).
- **Open modes and the bytes on both platforms** (high). Binary is "b": the bytes as written (Tibia.cfg is binary;
  its CR LF come from writeLn). Text is the original's "t" (SaveChannel's "at"; the test mode's "w"): the Microsoft
  C runtime writes every '\n' as "\r\n" and changes nothing else (a "\r\n" in the data becomes "\r\r\n"; "at" opens
  write-only, so the runtime's Ctrl-Z trimming, which needs read-write, never runs). On Windows TFile uses the
  runtime's own text mode, so the bytes are the original's by construction. POSIX has no text mode, and an "at" file
  on Linux would have kept bare '\n'; there TFile opens the descriptor itself (O_APPEND or O_TRUNC, 0666 like
  fopen) and gives a glibc `fopencookie` stream whose write function adds the '\r' before each '\n' and writes to
  the descriptor. The translation is in the FILE, not in TFile's functions, so TFile stays one pointer, every write
  (Write, PutChar, Print) and every flush point (a Flush puts the bytes into the file: the test mode's log) behave
  as on Windows, and a TTalkBuffer::Write into a binary file (the golden test's tmpfile) still writes bare '\n'.
  Reading in text mode is refused (EINVAL): the client reads only binary files. tests/unit/file_test.cpp checks
  the same expected bytes on Windows, mingw-w64 and Linux, and on Windows the C runtime's own "wb", "wt", "w" and
  "at" against them.
- **Sharing and buffering** (high). fopen is `_fsopen(Name, Mode, _SH_DENYNO)` on Windows, which is what the UCRT's
  and MSVC 7.1's fopen call (and it has no deprecation warning outside common.h's _CRT_SECURE_NO_WARNINGS); the
  UTF-16 overload of the Windows-only test mode is `_wfopen_s`, not shared, as its four calls had. The buffering is
  the C library's default, as before; the flush points are the callers' (the test mode flushes after each line;
  SaveChannel closes before it reports).
- **Print and fprintf** (high). `TFile::Print(Format, Values...)` formats with std::format's language (std::format
  on the product presets; in the oracle compat/format.h's stand-in, measured and then written, as TFixedString's
  Format) and writes the text with fwrite: fprintf's result (the characters) or -1, no exception for a failed write.
  The three formats, proved character for character against snprintf in file_test.cpp: "%s\n" (TTalkBuffer::Write,
  the line's text: never null), "\nChannel saved at %s" (FormatCRT71Time's text or the literal "(null)"), and
  frames.txt's "%d %u %d %u %d %d %lld\n", whose arguments have exactly those types (int, std::uint32_t, int,
  std::uint32_t, int, int, long long), so `{}` gives printf's digits (checked at INT_MIN, UINT32_MAX, LLONG_MIN).
- **Not std::print** (high; a deviation from the plan's wording, "std::print for diagnostics"). std::print and
  std::println throw std::system_error where the write fails (C++23 [print.fun]; MSVC checks fwrite's result), where
  printf returns quietly. The client is a GUI program without a valid stdout unless redirected, so MouseHover's
  printf leftover as std::println would throw on every mouse move over the automap, inside TBEGIN/TEND2(433): a
  TMyException and error.txt. On Windows std::print also writes UTF-16 through WriteConsoleW to a console, where
  printf wrote the bytes. So stdout's printf is `src/compat/print.h`'s `patella::compat::PrintLine("{}", ...)`
  (std::format or the stand-in, then fwrite to stdout: printf's bytes and its silence), and the test mode's
  diagnostics on stderr stay fputs and fflush (the standard streams are not files TFile owns; Fail ends the
  process right after). compat/cxx26.h's existing std::print(stderr) in the contract-violation handler, which
  terminates anyway, stays. modernize-use-std-print reports nothing now (no printf or fprintf is left to report).
- **One difference on an error path** (medium). SaveChannel opened the file before TASSERT(2324, TextBuffer!=NULL)
  and leaked the FILE when it threw (the golden test's comment says so: "leaves it open when it throws"); the
  TFile is closed when the exception leaves the function. The file's bytes are the same (nothing was written before
  the assertion, and the leaked FILE's empty buffer was all the runtime would have flushed at exit); only the
  handle is gone, so the file can be removed and appended to as before. Likewise a TTalkBuffer::Write that threw
  halfway would have left its lines in the leaked FILE's buffer, written at a normal exit; now at once (nothing in
  Write can throw: the lines' text is never null).
- **MsvcSnprintf with constant formats** (high). 52 calls on main; 44 converted where the text always fits, so
  _snprintf wrote it and its NUL exactly as Format does: Help.cpp's 30 help texts (`TStringBuffer(HelpText,
  sizeof(HelpText)).Format`; "%d" became "{}" in the 9 with arguments, all literal ints; a script checked that no
  text has another conversion or a brace and that the longest formats to 1,297 characters of 2,048), Control.cpp's
  engine names (`TFixedString<4>` and Assign: three characters), Dialogs.cpp's four descriptions without an author
  (texts of the text table, at most 49 characters of 255), Map.cpp's three messages (the file name has at most 12
  characters: TFixedString<256> and Format), Preferences.cpp's hotkey "%s\n" (a hotkey is a
  TFixedString<HOTKEY_LENGTH>, so at most 256 characters with the '\n' in 257), Status.cpp's account number ("%d", at
  most 11 characters in 12; the original's `[11] = 0` stays) and dotted address (four uint8, which std::format and
  the stand-in print as numbers: at most 15 in 16). Kept, 8: the text table's runtime formats (TextPercentToGo,
  the context menu's speaker formats, TextTryingToReconnect: 2 calls), Dialogs.cpp's two with the book's author (a
  server string of any length: _snprintf leaves no NUL at 255 characters, the type would cut at 254), Script.cpp's
  error message (a 1,023-character file name overflows the 1,054), and Control.cpp's frame rate ("%s %.02f fps":
  printf writes NaN as "-nan(ind)" or "nan", std::format "nan", and 11 digits fill the 20 without a NUL).
- **Removed** (high). compat/msvc.h's `MsvcFileno` (TFile::Descriptor is the only use; core_msvc_test.cpp's _eof
  case takes the descriptor itself, unbuffered as before); `TWriteScriptFile`'s writers are no longer `const` (they
  write through the member). Script.h, Talk.h and the test mode no longer include <cstdio> for FILE.
- **The grep gate** (high). tools/banned_calls.py also bans, outside src/types/File.h, the type `FILE` and the calls
  fopen, _fsopen, _wfopen, _wfopen_s, fopen_s, freopen, fdopen, tmpfile, tmpfile_s, fopencookie, fclose, printf,
  fprintf, vprintf and vfprintf (self-test extended); 0 in src/. fputs/fwrite to the standard streams are allowed.
- **Forced test change** (high). tests/golden/talk_tests.cpp passed a tmpfile's `FILE *` to TTalkBuffer::Write: our
  side is now `TFile OurFile; OurFile.OpenTemporary()` with Seek/Read/Close (the image's side unchanged). And
  tests/golden/contentwindows_tests.cpp logged our MouseHover output by faking our printf (the oracle's static CRT
  function, the runner's import of __stdio_common_vfprintf) while our side runs: difftest's first run had 403
  ContentWindows failures ("logs differ": the original's "PrintfMark 384 of block 0,0" missing on ours). It now
  fakes our fwrite instead (the oracle's with a jump, other streams through _fwrite_nolock; the runner's import
  slot "fwrite", other streams to the saved one) and logs what goes to stdout the same way, "Printf" and the
  text; the image's side (0x512292, FakePrintf) is unchanged. Nothing in tools/difftest changes.
- **Gates** (high). oracle-x86 difftest counts unchanged (16,848,752 / 67 / 1,134,513 / 3,201, 0 failures),
  golden_runner 15,351,355 replayed and 1,497,464 retired, 0 failures, the unit tests and presets as in
  notes/progress.md, "Last difftest". The images change (TFile's and Format's inline code); no tier A, reference
  or compare_files.py run here (no client may be started by this change's worker): compare_files.py is the live
  check this change needs (Tibia.cfg, the `.map` files, the saved channel, the patch files).

## The golden corpus: Control (Patella-Modern issue #30, third part)

Settled by Claude Opus 5.5, November 4, 2026; the retirement decided by the user the same day. tests/golden/
(control_tests.cpp moved from tools/difftest/controltest.cpp, commworld.h, commworld.cpp, runner.cpp, golden_test.h),
tests/golden/comm/CMakeLists.txt, cmake/OracleX86.cmake. The section on the second part above describes the world
and Communication's port; notes/reconstruction.md, "Checking", the use.

- **Retired with the oracle** (decided by the user, November 4, 2026). Control.cpp's ChangeEngine is the x86 engine
  selection in the oracle and the graphics device choice of issue #31 in the x64 client (PATELLA_VULKAN): **24,155
  checks**, the 1,518 comparisons that reach it (21,979 checks: 14,249 in the main part, 7,730 in the error round;
  Step 11's call, or CheckSurface's ChangeEngine(0) after the surface's Init returned false, which the original's
  log shows as "Surface.Init(...)=0") and the 2,176 checks of Control.cpp's statics compared right after them.
- **Per comparison** (high). Only the oracle knows after both sides ran whether a comparison reached ChangeEngine.
  It records that flag first (no record is written while the sides run), then the comparison's checks and values
  between golden::RetiredBegin("ChangeEngine") and RetiredEnd; the runner reads the flag before it runs our side and,
  for a retired one, neither calls our code (which would reach the x64 ChangeEngine) nor compares. A Step ends the
  part after its own checks on the comparison's results (CloseRequest's result, curEngine; DeferRetiredEnd); the
  statics' checks right after a retired comparison are a retired part of their own.
- **In step after a retired part** (high). The state later comparisons read goes through golden::Output after
  RetiredEnd, the original's values in the oracle, set on our side in the runner: Control.cpp's statics
  (LastPacketType, LastPacketIndex, LastPacketTypes, ReconnectTimeout, Reconnect, LastMouseMove, ShowFPS: the
  image's at their addresses; ours by name, since the runner compiles Control.cpp into control_tests.cpp, as
  network_tests.cpp compiles Network.cpp), curEngine, the error round's exception counters, Random()'s seed and the
  rand() fake's state; the callbacks and the window handle only ever hold InitWindow's values and are compared by
  name, not set. The texts recorded by their changes (log, buffer and world dumps) restart from empty on both
  sides. Nothing else carries over: every scenario ends with FreeWorlds, and no Init, Exit or other comparison of a
  scenario follows a retired one (InitControl and ExitControl never reach ChangeEngine). A runner-only change that
  leaves the statics unset gave 2,131 more failures; a runner-only fake-log change 4 more (both reverted).
- **The statics search** (high). FindStatics runs our code to find our statics by value; its ProcessEvents redraws and
  can reach our ChangeEngine, compared nowhere (in the oracle it leaves only curEngine, which every scenario sets
  again before a comparison reads it). The runner finds the statics by name after the same calls (so they leave the
  same state) and stands ChangeEngine in for the search only (a logging fake, golden::SaveOwn/RestoreOwn).
- **Replayed** (high). 253,992 + 24,155 retired = 278,147, the oracle's, Release and Debug; the error round's counts
  (163,552 checks, 10,980 exceptions compared, 9,474 that threw) equal the oracle's. The runner replays 16,461,713 of
  the oracle's 17,983,332 checks, retires 1,521,619, lists none as not ported.
- **60 failures: an x64 defect of src/Control.cpp** (fixed in its own commit, below). ProcessEvents' event 0x12
  reads its first parameter as `INT_PTR *` (Data[0] the count, &Data[1] the IDs), but Communication.cpp's
  ReceiveChannelList builds an `int[]` (count, then the IDs). On x86 both are 4 bytes; on x64 the count is read from
  8 bytes (its low half, so right) and the IDs start one entry late: the new-channel dialog of the x64 client shows
  each channel with the next one's ID and reads one int past the array. All 60 failures (20 in the main part, 40 in
  the error round, Release and Debug alike) are this call's log ("CreateNewChannelDialog(2,24:...,311:...)", the
  original's "(2,88:...,24:...)").
- **The fix** (high; the orchestrator's decision: it restores the original's behaviour on x64). Event 0x12 reads
  Param1 as the `int *` ReceiveChannelList builds (`Data[0]` the count, `&Data[1]` the IDs). The oracle's Patella.exe
  is unchanged (pecompare of 8d48db9's and the fix's, built at the same path: .text identical, 0 unexpected
  differences: INT_PTR is int on x86); the runner replays Control with 0 failures in Release and Debug. The golden
  test is the check that catches it on x64 (control_tests.cpp's BuildEvent builds event 0x12 as the client does).
  The reference recordings do not open the channel list (no session sends Ctrl+O or the channels button), so the x64
  client's dialog needs a live check: the "Open a channel" dialog's entries open the channel they name.
- **The same class of defect, audited** (high). Every message of the event queue (Status.cpp's TMessage keeps both
  parameters as INT_PTR; FetchMessage's only reader is Control.cpp's ProcessEvents; ClearMessages frees them) whose
  producer builds a block and whose reader reads it, element type against element type: event 6 (MainWindow.cpp's
  INT_PTR[4], read as INT_PTR[]), 7 (Control.cpp's CloseRequest, INT_PTR[4] / INT_PTR[]), 14 (Communication.cpp's
  ReceiveEditText: uint32[3] / int *, char *[2] / INT_PTR[] and ClearMessages' char **), 15 (ReceiveEditHouseText:
  uint32[2] / int *), 16 (ReceiveOutfit: int[7] / int *), 0x12 (ReceiveChannelList: int[] / INT_PTR[], the defect),
  0x1c (ContextMenus.cpp's new TTalkLine / TTalkLine *), 0x1d and 0x1e (TCoordinate *, TMark *), 0x24, 0x3a, 0x3b,
  0x3c (window and menu-data pointers / the same pointers), and every block of text (char * both sides). The scalar
  parameters (counts, IDs, line numbers, the patch size; 5, 9, 10, 0x18, 0x29, 0x30, 0x31, 0x35, 0x36, 0x37,
  0x39) are ints or uint32 widened to INT_PTR and narrowed back to the same type: no difference on x64. Outside the
  queue: MainFrm.cpp's OnDisplayChange and OnHostResolved read LPARAM through LOWORD/HIWORD and WSAGETASYNCERROR
  (WinSock's 32-bit packing, both sides Windows'), OnTimer's UINT_PTR is MFC's, Player.cpp's LPARAMs are Windows',
  Talk.cpp's Write prints its FILE * through an INT_PTR (the original's catch value, a pointer). No other pair is a
  defect; none was unclear.
- **Commits** (form). Control's port, these notes, the fix of event 0x12, then its notes.

## The machine's folders (folder layout, November 4, 2026)

- November 4, 2026, the user's decision: everything under C:\Tibia. This repository C:\Patella-Modern becomes
  C:\Tibia\Patella-Modern, upstream Patella's C:\Tibia (the original Tibia.exe and data files, decompilation\)
  becomes C:\Tibia\Patella, C:\remeres-map-editor-redux becomes C:\Tibia\remeres-map-editor-redux,
  C:\Tibia\gameserver stays, C:\Tibia\Assets (D14) comes later; difftest's C:\Tibia\Tibia.exe fallbacks gave
  way to a required argument, the test server's folder became `PATELLA_TESTSERVER` (issue #2) and
  `PATELLA_CIPSOFT_DIR`'s default C:\Tibia\Patella (notes/reconstruction.md, "The machine's folders"). The
  paths in the entries above are as they were then.

## std::span parameters: the packet buffers, TXTEA and TRSA (Patella-Modern issue #59)

Settled by Claude Opus 5.5, November 4, 2026. The sixth change of M4 (plan section 7, "Views"): the packet buffers',
TXTEA's and TRSA's interfaces take spans, and no pointer and length pair is left in their signatures.

- **The type** (high). `src/compat/span.h`: `patella::TSpan<T, Extent>` is `std::span<T, Extent>` where the library has
  it (`PATELLA_HAS_SPAN` 1: every product preset), and a C++17 stand-in in the oracle, which compiles with /std:c++17
  until M6, as compat/format.h does for std::format_to_n (#57). The stand-in has the part of std::span's interface the
  code uses (empty, pointer and count, arrays, std::array, the conversions to const and between extents with
  std::span's explicitness, data/size/size_bytes/empty, operator[], front/back, begin/end, first/last with a count or a
  fixed one, subspan) and, like std::span without a hardened library, checks no precondition. `IsSpanLayout` asserts the
  size on both paths (a pointer for a fixed extent, a pointer and a std::size_t for the dynamic one; std::span's in
  MSVC's, libstdc++'s and libc++'s library), the alignment and trivial copying; a TSpan is a parameter or a local,
  never a member of a frozen structure. tests/unit/span_test.cpp (5 cases, compile-time checks of every conversion's
  explicitness) runs in tests_unit with std::span and in tests_unit_cxx26_standin with the stand-in
  (`PATELLA_SPAN_STANDIN`, also on header_check_cxx26_standin). The header is on cmake/HeaderCheck.cmake's core and
  oracle lists.
- **What takes spans** (high). Utils.h: `TReadBuffer(TSpan<uint8>)`, `TWriteBuffer(TSpan<uint8>)` (explicit),
  `GetString(TSpan<char>)`, `GetBytes(TSpan<uint8>)`, `PutBytes(TSpan<const uint8>)`. TXTEA.h: `SetKey(TKey)` and
  `EncryptBlock`/`DecryptBlock(TBlock)`, `TKey = TSpan<const uint8, 16>`, `TBlock = TSpan<uint8, 8>` (the sizes the
  original's pointers implied). TRSA.h: `Encrypt(TSpan<uint8, 128>)`. And Network.h's wrappers of the packet buffers,
  which Communication.cpp calls: `SendBytes(TSpan<const uint8>)`, `GetString(TSpan<char>)`, `GetBytes(TSpan<uint8>)`.
- **The macros' text** (high). Each body defines the original's parameters as locals from the span, under the names
  the assertions' text uses (`data`, `size`, `Buffer`, `MaxLength`, `Count`): the pointer is `data()`, the int is
  `static_cast<int>(size())`. So `TASSERT(286, data!=NULL)`, `TASSERT2(372, Position<=Size-Count, ...)`,
  `TEND1(470, MaxLength)` and the others are as written (macrocheck db0e223 issue-59: 2,108 calls, 0 differences),
  and the int keeps the original's meaning: a span with a null pointer fails the same assertion; a size above INT_MAX
  reads as the original's negative size (lines 287, 388) or negative MaxLength (no limit). No caller of the client
  makes such a span; the tests do (below). TXTEA and TRSA keep their null checks on `data()` ("Data pointer is NULL").
- **Kept as C strings** (high). `TWriteBuffer::PutString(const char *s)` and Network's `SendString(const char *Text)`:
  their assertions (437, 316) report a null pointer, which a std::string_view cannot carry (constructing one from a
  null C string faults before the assertion; difftest's error round passes one). `TRSA::SetPublicKey(const char *)`:
  the digits end at the first NUL, and tools/patch_client.py's `--rsa` writes a shorter modulus into the literal's
  place padded with NULs; a std::string_view's length would be the literal's 309, and DecimalToVlong would read the
  padding (atoi of "" is 0, a digit). None of the three is a pointer and length pair; the issue's std::string_view
  part has no clean target in these interfaces.
- **The callers** (high). Communication.cpp's 22 `GetString(Name, 30)`: the limit is the protocol's and the arrays are
  larger (Name[32] to Text[632]), so `GetString(patella::TSpan<char>(Name).first(30))`, and `TSpan<char>(Name, 30)`
  where the buffer is a new[] of that size or a row of one (`Names + i * 30`). `TWriteBuffer Buffer(Data)`,
  `PutBytes(Key)`, `SendBytes(Data)`: the arrays' sizes, 128, 16 and 128, the counts the original passed. The patch
  block: `TSpan<uint8>(PatchData.get() + PatchDataCount, BlockSize)`. Network.cpp: `SendBuffer(SendData)` (sizeof, as
  before), `ReceiveBuffer({ReceiveData, 0})`, the packet header `TSpan<uint8>(&ReceiveData[ReceivePacketEnd], 2)`, the
  blocks `TXTEA::TBlock(&SendData[i], 8)`, SetSymmetricKey's key `TXTEA::TKey(Key, 16)`. Same pointers and counts as
  before, so the same bytes; with a hardened library (Debug libstdc++, `_GLIBCXX_ASSERTIONS`) `first(30)` checks that
  the array holds 30.
- **The images** (high). They change: a span is passed by value (on x86 a dynamic span is the same two stack words as
  the original's pointer and int, a fixed one a pointer; the x64 ABI passes a 16-byte span through a stack temporary,
  so each caller stores the pointer and the count there). pecompare of db0e223 and issue-59, built at the same path,
  with the functions' sizes from both PDBs: the oracle's Patella.exe resizes exactly 12 functions (GetBytes,
  GetString, SendLogin, SendGameLogin, SetSymmetricKey, TRSA::Encrypt, TWriteBuffer::PutBytes, TXTEA's SetKey,
  EncryptBlock and DecryptBlock, the dynamic initialisers of SendBuffer and ReceiveBuffer; .text +0xf0), the x64
  Patella.exe 38 (those of them not inlined, Communication.cpp's 16 callers of GetString and GetBytes, the
  TReadBuffer/TWriteBuffer constructors, TSocket::OnReceive and the callers' catch funclets; .text +0x2b0); no
  function added or removed, no text of .rdata changed. The other byte differences are the moved code's addresses and relocations. The x64 Release callers'
  `first(30)` has no check (the disassembly of ReceiveBuddy stores the array's address and 0x1e and calls
  GetString).
- **Tests** (high). Forced: tests/golden's core, errors and network tests and commworld.cpp call the new signatures;
  golden_test.h's `OriginalSpan(Pointer, int Count)` makes the original's arguments a span, also those no valid span
  holds that the error comparisons pass (a null pointer with a count, a negative count as a std::size_t). Such a span
  is never read (the assertion fires first, or the size is only compared); Clang's UBSan run over the unit tests'
  equivalents is clean. network_tests.cpp's table of our functions takes lambdas with the original's signatures for
  SendBytes, GetString and GetBytes (the image's side unchanged). The unit tests' NULL cases became spans with a null
  pointer. New: span_test.cpp, core_utils_test.cpp's "the original's pointer and length as spans" (null spans, sizes
  above INT_MAX, MaxLength cut and no limit), core_txtea_test.cpp's block inside a packet.
- **Not done** (found). Other pointer and length parameters, for later issues: vlong's ByteArrayToVlong and
  VlongToByteArray (`uint8 *`, uint16), Script.h's readBytes and writeBytes, TestHooks.h's PacketSent and
  PacketReceived, Status.h's CheckClipboardText, Utils.h's CopyString (Size), CompareStrings (Count) and ShortenString
  (MaxLength) and FormatNumber's unsized buffer, Network.h's SetSymmetricKey (`const uint8 *`, 16 bytes;
  communicationtest fakes it by address with that signature) and GetErrorMessage's unsized buffer, Dialogs.h's
  CreateUpdateDialog/TUpdateDialog (a file name and a size, not a pair).

## std::array for the C arrays, and spans for vlong and the packet hooks (Patella-Modern issue #60)

Settled by Claude Opus 5.5, November 5, 2026. The seventh change of M4 (plan section 7, "C arrays in structures"): the
client's arrays are `std::array` or `TFixedString`, and clang-tidy's modernize-avoid-c-arrays reports nothing on
windows-x64's sources but the owners of #58 (`std::unique_ptr<T[]>`) and the arrays marked below.

- **The rule** (high). A member, global or local `T X[N]` became `std::array<T, N>`, a `T X[A][B]`
  `std::array<std::array<T, B>, A>` (TAutomapBlock's Color and Speed, TMapWindow's Light, CoverLevel and Cache), a
  text `char X[N]` `TFixedString<N>` (#57's type: it converts to `char *` as the array decayed, so the C string
  calls and the TASSERT texts that name it are unchanged). Uninitialised stays uninitialised (both are aggregates
  default-initialised as the array was); `= {}`, `= {0}` and `= {"text"}` initialise as before. MSVC's
  `std::array` is a struct whose only member is the array, so size, alignment and offsets are the array's; libstdc++
  and libc++ likewise.
- **The layouts** (high). Each converted structure asserts its size and the converted members' offsets, the values
  read from main's (d9b3cfd) oracle PDB with tools/client_state.py's reader: unconditionally where the structure has
  no pointers (the x64 PDB has the same values: TCreature, TMapPoint, TMark, TErrorReport, TTalkbox, TFontInfo,
  TVideoCacheItem, TCacheField, the skins' TSkinProperties288, B08, F8C, TCombatControlsProperties and
  TSkinProperties), under PATELLA_ORIGINAL_LAYOUT where it has pointers or a vtable (TContainer, TOffer,
  TAutomapBlock, TXTEA, TTalkLine, the private and rule violation buffers, TTalkFilter, TReadScriptFile, TMapWindow,
  TContextMenuEntry, TOutfitIcon, TReconnectWindow, TOutfitDialog, TEditBuddyDialog, TRuleViolationDialog,
  TEditMarkDialog, THotkeyDialog). CMainFrame derives from CFrameWnd, whose size differs between MFC and the shim of
  difftest and the golden tests: it asserts KeyStates' place among its own members (after Placement, 0x100 bytes
  before KeyFlags). The globals assert their sizes. `client_state.py --layout` on the new oracle: 105 fields, 33
  globals, 0 mismatches, equal to the original; on the new x64 image every field and size as main's.
- **Where the array decayed** (high). Outside the macros, a pointer is `.data()` (memset, memcpy, CAsyncSocket::Send,
  DrawOutfit's and SendOutfit's colours, TXTEA's key, FindPath's steps, ShadeRectangles, WriteImage,
  WriteMaskedImage, the test mode's views) and a TFixedString's is `.Chars` where a conversion cannot reach it: a
  `TSpan<char>` (GetString's 22 callers), snprintf's and the golden tests' varargs (`%s`: a TFixedString there would be
  its N bytes; MSVC's C4477 found snprintf's and sprintf's, and every variadic call of src/, tests/golden and
  tools/difftest with a converted name was read), and the two stream writes (MoveCreature's error text, the service
  pack in error.txt: MSVC chose `operator<<(const char *)` for a TFixedString in both standards, tested, so the text
  is the same; `.Chars` says so). No TASSERT/TERROR/TEND argument changed (macrocheck: 2,108 calls, 0 differences).
- **The arrays that stay** (high), each with `NOLINT(modernize-avoid-c-arrays)` and its reason on the line: the four
  Script.h `Filename` buffers and InstallPatch's `Name` (TERROR_REASON's reason is a `const std::string &`, which
  neither type reaches in one conversion), TReadScriptFile::error's `Message` (TMYEXCEPTION(84)'s argument, the same),
  TProperties::Flags (TASSERT1(67)'s value is `*(int*)Entry.Flags`), all until M5; TFixedString's own `Chars`; the
  skin table (Skins.cpp: an unnamed type sized by its entries; std::to_array is C++20, the oracle C++17); MFC's
  BEGIN_MESSAGE_MAP; CreateNewChannelDialog's and TNewChannelDialog's `const char (*)[30]` (the golden tests fake
  CreateNewChannelDialog by address with that signature; a span would change it). Not marked: the 16 findings (15 lines) on
  `std::unique_ptr<T[]>` (#58's owners of new[] and their static_asserts), which are not C arrays; and the
  oracle-only engine sources (SurfaceDX.cpp's `uint8 Bytes[4]`), which windows-x64 does not compile.
- **Count** (high). `python -I tools/tidy.py -p build/windows-x64 --all --checks='-*,modernize-avoid-c-arrays'`: 241 at
  M3's exit, 202 on main d9b3cfd, 16 now (the unique_ptrs above). The check is not added to `.clang-tidy`: the
  remaining unique_ptr findings would need a NOLINT each.
- **Spans** (high). vlong.h's ByteArrayToVlong and VlongToByteArray take `TSpan<const uint8>` and `TSpan<uint8>`; the
  bodies define the original's `pData` and `Size` (cut to uint16, as the original's parameter) from the span, so a
  span without data throws "pData pointer is NULL" as before (new unit test). TestHooks.h (test build only):
  RsaBlock takes `TSpan<const unsigned char, 128>`, PacketSent and PacketReceived the whole SendData and ReceiveData as
  spans, the payload and padding are subspans. Not done (risky or larger than this change): Script.h's readBytes and
  writeBytes, Status.h's CheckClipboardText, Utils.h's CopyString, CompareStrings, ShortenString and FormatNumber,
  Network.h's GetErrorMessage (the golden core and error tests call them with the original's arguments, null pointers
  and negative sizes included: each needs #59's OriginalSpan treatment) and SetSymmetricKey and
  CreateNewChannelDialog (faked by address with their signatures).
- **Tools and tests** (high). tools/client_state.py's Pdb.kind reads a `std::array<T, N>` (one data member `_Elems` at
  offset 0, the same length) as `T[N]`; main's tool on the new x64 image reported Body, Containers, TMapPoint.Objects,
  TCreature.Colors and five more as problems. tools/patch_client.py needs nothing (Servers' RVA, sizeof(TServer),
  TServer::Port): it patches the new oracle and x64 images. tests/golden/runner.cpp: in an MSVC Debug build the
  library's checks (a std::array index out of range) report to stderr and end the run with exit code 3 instead of the
  CRT's message box. Forced test changes: `.data()`/`.Chars` in tests/golden (script, graphics, context menus,
  MainFrm, content windows, map window, network, control), dialogs_tests.cpp's Numbers and Pointers for std::array
  members, communication_tests.cpp's and tools/difftest/playertest.cpp's own definitions of PacketHeader,
  TitleCode and ClassNameCode.
- **Proof** (high). The images change (code only), so behaviour is the proof: difftest 16,848,752, sysinctest 67,
  communicationtest 1,134,513, vlong 3,201, 0 failures (`oracle-x86 --fresh`, and again recording the branch's
  corpus); golden_runner 16,461,713 replayed, 1,521,619 retired, 0 failures in windows-x64 and windows-x64-debug (the
  Debug library checks each std::array index: none out of range). pecompare of d9b3cfd and issue-60 at the same path,
  with the functions' sizes from both PDBs: the oracle resizes 83 functions (.text -0x950) and adds out-of-line
  copies of four std::array::operator[] (ClearMap calls std::array<TEffect, 200>'s instead of unrolling its loop) and
  the TFixedString Format instantiations that replace the char-array ones; x64 resizes 75 (.text +0x800), adds a
  dynamic initialiser for Containers (std::array<TContainer, 16>: the same zeroed values before main) and the same
  kind of instantiations; no string of .rdata changed. The x64 test mode's packet log and state dump are the live
  checks (reference.py compare).

## Static analysis of the core library (Patella-Modern issue #63)

Settled by Claude Opus 5.5, November 5, 2026. The eighth change of M4 (plan section 7, "Static analysis"): MSVC
`/analyze`, clang-tidy's `bugprone-*` and GCC `-fanalyzer` on patella_core (cmake/Core.cmake: vlong.cpp, TRSA.cpp,
TXTEA.cpp, TMyException.cpp, Utils.cpp, Skins.cpp, settings/Settings.cpp and the regexp, with the headers of src/
they include) report nothing that is not suppressed in the code with its reason, and each runs in the hosted CI.

- **How they run** (high). cmake/Core.cmake compiles patella_core with `/analyze /analyze:external-` (MSVC: the
  headers in angle brackets, SDK and standard library, are not analysed) and `-fanalyzer` (GCC), for C and C++, in
  every configuration, under the option PATELLA_STATIC_ANALYSIS (on by default). Their findings are warnings, so /WX
  and -Werror (cmake/Warnings.cmake) fail the build on one, and tools/build.py's warning scan would too: the hosted
  windows-x64 job (Release and Debug) runs /analyze, the linux-x64-gcc job (Release and Debug) -fanalyzer, and so does
  every local windows-x64, windows-x64-gcc and linux-x64-gcc build. Clang has no analyzer step (clang-tidy is
  Clang's). The analysis does not change the objects, and the client and the oracle compile the same sources
  themselves without it (cmake/WindowsX64.cmake, OracleX86.cmake), so no image changes through the flags. Cost
  (patella_core alone, one job at a time, measured): MSVC 5.8 s to 11 s, GCC 5.8 s to 14 s per configuration.
- **bugprone-* at zero** (high). `.clang-tidy` had bugprone-* already (issue #8), and the hosted clang-tidy job
  checked it against the baseline, which held six bugprone findings. tools/tidy.py's new `--zero <prefix>` fails on
  any finding of those checks whatever the baseline says, inside TASSERT/TERROR/TEND arguments included (none are);
  the job passes `--zero bugprone-`, and tools/tidy_ci.py runs the job's own script, so it checks the same. The
  baseline is 7 findings (cppcoreguidelines-owning-memory and -use-enum-class), from 13.
- **Measured** (high), before the change: MSVC 14.51 /analyze 114 findings (vlong.cpp C6246 1; regexp.c C28278 108,
  C6001 5), Release and Debug alike; GCC 16.2 -fanalyzer 2 (vlong.cpp and regexp.c -Wanalyzer-null-dereference,
  Release; Debug measured after the change only); clang-tidy 22.1.8 bugprone-* with every bugprone check on, linux-x64-clang's commands (the
  CI job's: patella_core's sources and src/testmode's and src/vulkan's portable ones) 26: the six of the baseline and
  20 of the four checks .clang-tidy leaves off (easily-swappable-parameters 17, reserved-identifier 2,
  implicit-widening-of-multiplication-result 1, narrowing-conversions 0). After: 0, 0 and 0 outside the
  suppressions below.
- **Fixed** (high), each without a change of behaviour: vlong.cpp's fast_mul named its inner bound `min` like the
  outer one (C6246; now `jmin`, the same code); GetOutfitColor's switch has a `default: break;` (a negative Color
  keeps S and V at 1.0, as without it); Settings.h's MaxFileSize multiplies in `std::size_t` (the same constant).
- **Suppressed** (high), in the code, each with its reason:

  | Analyzer | Finding | Where | Kind | Suppression | Reason |
  |---|---|---|---|---|---|
  | clang-tidy | bugprone-unhandled-self-assignment | vlong.cpp, vlong::operator= | the original's bug | NOLINT | Barwood's operator=, as at 0x50c610: a self-assignment of an unshared value deletes it before taking it again. Nothing in the client assigns a vlong to itself; the fix changes the image's function, kept until M6 |
  | clang-tidy | bugprone-unchecked-string-to-number-conversion | vlong.cpp, DecimalToVlong | the original's | NOLINT | the binary calls atoi (0x51262a) on a one-digit string; a non-digit counts as 0 |
  | clang-tidy | bugprone-throwing-static-initialization | TRSA.cpp PublicExponent, TRSA.h HeaderExponent | the original's | NOLINT | the binary's globals (0x71f004 and each object's copy), constructed before main; vlong allocates, and an allocation failure there ends the program as in the original |
  | clang-tidy | bugprone-integer-division | Utils.cpp, GetOutfitColor | false positive | NOLINT | `Color / 19` is the palette row, an integer on purpose (the binary's) |
  | clang-tidy | bugprone-easily-swappable-parameters | 17 (HSVToRGB, vlong's exp, modinv, modexp, Settings' Parse, testmode, vulkan) | style | off in .clang-tidy | a heuristic on parameter lists, most of them the original's; no bug |
  | clang-tidy | bugprone-reserved-identifier | 2 in the core (common.h's `_CRT_SECURE_NO_WARNINGS`, `_WINSOCK_DEPRECATED_NO_WARNINGS`) | false positive | off in .clang-tidy | the CRT's and the SDK's own macro names; the client's sources have several hundred `_Name` parameters (the reconstruction's convention), which the per-target Windows runs would report |
  | clang-tidy | bugprone-narrowing-conversions, -implicit-widening-of-multiplication-result | 0 in the core (the one widening fixed above) | - | off in .clang-tidy | the client's integer types are the original's (issue #8); not turned on, as the per-target Windows runs would report the client's |
  | MSVC /analyze | C28278 (no prototype in scope) | regexp.c, 108 | false positive | `#pragma warning(disable: 28278 6001)` in the file | K&R C (src/Extern, kept until M6): no function has a prototype |
  | MSVC /analyze | C6001 (uninitialized memory 'flags') | regexp.c, 5 (regcomp, reg, regbranch, regpiece) | false positive | the same pragma | without prototypes the analysis cannot see that reg, regbranch, regpiece and regatom set `*flagp` before they return non-NULL |
  | GCC -fanalyzer | -Wanalyzer-null-dereference | vlong.cpp, flex_unit::set | false positive | PATELLA_GCC_DIAGNOSTIC push/ignored/pop around the function | the path needs `i + 1` to wrap to 0 in reserve; i is below a source value's n |
  | GCC -fanalyzer | -Wanalyzer-null-dereference | regexp.c, regmatch's STAR/PLUS | false positive | `#pragma GCC diagnostic ignored` in the file (GCC only) | regnext of a STAR or PLUS node is never NULL: regcomp links each to the node that follows |

  The GCC pragmas go through compat/msvc.h's new PATELLA_GCC_DIAGNOSTIC (`_Pragma`, GCC only: Clang knows the pragma
  but not the analyzer's warnings); regexp.c is C and includes none of src/'s headers, so it keeps its own
  `#ifdef _MSC_VER` and `__GNUC__` pragmas, as for C4018 (issue #7). The pragmas change no object.
- **Not done** (found). vlong::operator='s self-assignment (above) is a real use after free, kept with the original's
  code; a guard (`if (this == &x)`) would change the oracle's and x64's function, for M6. The rest of the client
  (MFC, Win32, the engines, the test mode on Windows) is not analysed by /analyze or -fanalyzer: only patella_core
  is the plan's scope, and the client's sources are not in the library yet. clang-tidy's bugprone-* sees what
  linux-x64-clang compiles; the Windows-only sources only in the per-target Windows runs, which are not gated.
- **Proof** (high). Behaviour: difftest 16,848,752, sysinctest 67, communicationtest 1,134,513, vlong 3,201, 0
  failures (`oracle-x86 --fresh`, and again recording the corpus); golden_runner 16,461,713 replayed, 1,521,619
  retired, 0 not ported, 0 failures in windows-x64 and windows-x64-debug; the unit tests' counts unchanged on every
  preset. Images: pecompare of bf3e6df and issue-63 at the same path, oracle and x64 Release Patella.exe, differ in
  the link stamps only (.text, .data and .rdata identical): the pragmas, NOLINTs, renamed local and the new default
  change no code. An x64 image of bf3e6df built first in a new build directory differed from every later clean
  build of the same tree in one function, `std::_Integer_to_chars<unsigned char>` (register widths, same address
  and size); two clean builds of bf3e6df made afterwards agree with each other and with issue-63, so the comparison
  is against those. macrocheck: 2,108 calls, 0 differences.

## The remaining pointer and length parameters to spans (Patella-Modern issue #69)

Settled by Claude Opus 5.5, November 5, 2026. These are the parameters #59 and #60 left (their "Not done" lists): the
pointer and length pairs, and the buffers passed without a size, now take `patella::TSpan` (src/compat/span.h).
Two parameters stay, and why is given below.

- **What takes spans** (high). Script.h: `TReadBinaryFile::readBytes(TSpan<uint8>)`,
  `TWriteBinaryFile::writeBytes(TSpan<const uint8>)`. Status.h: `CheckClipboardText(TSpan<const char>)` (the copied
  characters, which need no NUL). Utils.h: `CopyString(TSpan<char> DestinationBytes, const char *Source)` (the
  original's Destination and Size) and `FormatNumber(int, TSpan<char>)`. Network.h:
  `GetErrorMessage(TSpan<char, ERROR_MESSAGE_SIZE>, int, int)`. graphics.h: `DrawOutfit(..., TSpan<const int, 4>
  Colors, ...)` (TCreature's and TOutfitIcon's `std::array<int, 4>`; outfit 0 reads only Colors[0], as before).
- **The macros' text** (high). As in #59, each body defines the original's parameters as locals from the span, under
  the names the assertions' text uses: CopyString's `Destination` and `Size` (`TASSERT(124, Destination!=NULL)`),
  FormatNumber's `Buffer` (467), GetErrorMessage's `Message` (544), readBytes' `Buffer` and `Count` (line 664's error
  names Count), writeBytes' and CheckClipboardText's own. The int is `static_cast<int>(size())`, so a null span fails
  the same assertion and a size above INT_MAX is the original's negative size. macrocheck 7ad3e31 issue-69: 2,108
  calls, 0 differences.
- **Fixed extents for the two that the golden tests fake by address** (high). GetErrorMessage (mainfrm_tests.cpp,
  commworld.cpp) and DrawOutfit (the content windows, dialog elements and dialogs tests) are replaced, on our side and
  in the image, by one fake with the original's signature (`char *`, `int *`). A fixed-extent span holds only the
  pointer and is trivially copyable (`IsSpanLayout`, asserted next to each declaration), so MSVC x86 pushes it as the
  pointer's stack word and MSVC x64 passes it in the pointer's register: the fakes need no change, and the sizes are
  the ones the callers always had (new char[1024]; the four colours). A dynamic span is two words on x86 (the
  pointer's and the int's places) but goes through a stack temporary on x64, so CheckClipboardText, also faked by
  address (0x4ea370), gets a second fake for our side that takes the span and calls the first
  (dialogelements_tests.cpp, dialogs_tests.cpp: `FakeOurCheckClipboardText`; the image keeps the old one, the logs
  are the same). SetSymmetricKey and CreateNewChannelDialog keep their signatures (#59, #60).
- **FormatNumber writes no further than its span** (high). The text is built in a local `TFixedString<14>` (10
  digits, 3 commas, the NUL) and copied into the caller's buffer with `TStringBuffer::Assign`. The original wrote
  nothing after the final NUL (the digits' NUL moves right with each comma), so the caller's bytes are the original's
  wherever the result fits: every caller's buffer holds 14 (ContentWindows' `TFixedString<20>`, the tests' 16, 32 and
  64). Where it does not, which overflowed in the original, the text is cut (#57's contract), and an empty span is
  not written. New unit test: tests/unit/core_utils_test.cpp's "the original's buffer and size as spans".
- **Kept** (high). `CompareStrings(const char *s1, const char *s2, int Count)`: Count limits a comparison of two C
  strings and -1 means no limit; it is not the size of a buffer (the callers pass -1 or a token's length).
  `ShortenString(char *Text, int MaxLength)`: MaxLength is the length the text is cut to (a font's fitting length),
  and the buffer is the text's own (strlen(Text) + 1, longer than MaxLength). Neither is a pointer and length pair;
  a span would describe a buffer neither function has (Utils.h says so). TrimString (`char *Destination`, no size)
  was not in the issue: its callers' buffers hold Source (#57).
- **The callers** (high). A TFixedString destination is `.Chars` (the array converts to the span with its N, which is
  the count the original passed: 30, 50, 20, 100, MAX_IDENT_LENGTH, HOTKEY_LENGTH); the ignore and white lists' 29 is
  `TSpan<char>(Name.Chars).first(29)`; a pointer and a computed count (the console's selected text, TFormattedText's
  pieces, the clipboard's ranges, readString's and writeString's lengths, the automap block's two tables) is
  `TSpan<T>(Pointer, static_cast<std::size_t>(Count))`; GetErrorMessage's seven callers wrap their new char[1024] in
  `TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE)`. Same pointers and counts, so the same bytes.
- **Tests** (high). Forced: tests/golden's core, errors and script tests pass the original's arguments through
  golden_test.h's `OriginalSpan` (CopyString with a null pointer and 3, FormatNumber with a null pointer, readBytes
  with a null pointer and 2); network_tests.cpp's table of our functions takes a lambda for GetErrorMessage (also
  called with NULL); the CheckClipboardText fakes above. tests_core gains one case: 35/337 to 36/352 on Windows and
  with mingw-w64, 35/340 to 36/355 on Linux; Clang's ASan and UBSan run is clean.
- **The images** (high). pecompare of 7ad3e31 and issue-69, built at the same path, with the functions' sizes from
  both PDBs: no function added or removed in either, no text of .rdata changed. The oracle resizes 20 functions,
  .text +0x2b0: the callers that build the spans (CopyString's in AddAutomapMark, AddTalkFilter, InitPreferences,
  TAutomapWindow::MouseHover, the context menu entry's constructor, TEditMarkDialog::HandleEvent, the ignore list
  dialog's HandleEvent and LeftButtonUp, the talk buffers' and TTalkLine's constructors, GetSelectedText; readBytes'
  and writeBytes' in LoadAutomapBlock and SaveAutomapBlock; CheckClipboardText's in TEditLine::HandleEvent;
  FormatNumber's in TSkillsWindowBody::Draw; DrawOutfit's in TBattleWindowBody::Draw and TOutfitIcon::Draw),
  FormatNumber's new body and DrawOutfit (the stand-in's operator[]). CopyString, readBytes, writeBytes,
  CheckClipboardText and GetErrorMessage keep their sizes on x86 (a dynamic span is the pointer's and the int's
  words, a fixed one the pointer's). x64 resizes 51 (.text +0x2c0, .rdata +0x1b0 of unwind and EH data): those
  callers and their catch funclets, the bodies of CopyString, CheckClipboardText, readBytes, writeBytes, readString,
  CopyToClipboard, GetErrorMessage and TFormattedText::Arrange (the span through a temporary), the ignore list
  dialog's constructor and its unwind funclets (CopyString's first(29)), and FormatNumber, whose one unwind funclet
  is renumbered (dtor$20 to dtor$10). GetErrorMessage's callers are unchanged on both (the fixed span is the
  pointer).
- **Proof** (high). difftest 16,848,752, sysinctest 67, communicationtest 1,134,513, vlong 3,201, 0 failures
  (`oracle-x86 --fresh`, and again recording the branch's corpus); golden_runner 16,461,713 replayed, 1,521,619
  retired, 0 not ported, 0 failures in windows-x64 and windows-x64-debug. tools/patch_client.py and
  tools/client_state.py read nothing this changes (no structure, global or code pattern they use).
