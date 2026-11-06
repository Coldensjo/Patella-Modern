# Orchestrator handover: Patella-Modern, during M4 (November 4, 2026, evening)

Copy everything below the line into a new coding-agent session started in `C:\Patella-Modern`, or read it
again in the same session.

---

You are the **orchestrator** for **Patella-Modern** (`C:\Patella-Modern`, GitHub `Coldensjo/Patella-Modern`,
**private** until the user switches it, see 6). Base brief: `notes/orchestrator-prompt.md` (sections 1, 2, 5–9).
The M2→M3 handover `notes/orchestrator-handover-m3.md` still holds for the user's rules (its section 2) and the
mechanics (its section 3); **this file overrides both where they differ.** You do not write code yourself: one
subagent per issue (T3 `delegate_task`, `isolation: "worktree"`), a complete brief, check the report against the
gates, merge, verify the merged tree, push, close. Small fixes of a merge (e.g. `.get()` on a member another branch
converted, a header on HeaderCheck.cmake's lists) you may make yourself.

## 1. State

- `main` = **`0752e0f`**: the last merge is "Merge folders: the folder layout …" on top of fedf7a5 (#30's
  Control, static part). Pushed. Hosted CI works again (billing resolved; the `folders` run was green on all 7 jobs).
- **The folder move (the user's layout, November 4):** C:\Tibia\Patella (today's C:\Tibia: the Patella repository,
  Tibia.exe and its data), C:\Tibia\Patella-Modern (this repository), C:\Tibia\gameserver (unchanged),
  C:\Tibia\remeres-map-editor-redux, C:\Tibia\Assets (later, D14). The repository's paths are ready (merge
  0752e0f; notes/reconstruction.md "The machine's folders"). The user moves the folders with
  `%USERPROFILE%\Desktop\move-tibia-folders.ps1` (dry run, then `-Apply`, with T3/Claude/VS Code/Docker closed).
  **Done November 4 (late) except (7):** worktrees repaired, REA at local scope for
  C:\Tibia\Patella-Modern and C:\Tibia\Patella, oracle-x86 / windows-x64 / windows-x64-gcc reconfigured with
  --fresh and green with the counts of 4, manual-test rebuilt from 0752e0f, reference compare: every session matches. The original checklist:
  **If you start in C:\Tibia\Patella-Modern after the move, finish it:** (1) `git worktree repair` if the script
  did not (it does); (2) re-register the REA MCP server at local scope for this folder (and for C:\Tibia\Patella)
  as CLAUDE.md "REA" describes; (3) Claude's memory was copied to `C--Tibia-Patella-Modern`; check it loads;
  (4) every `build\<preset>`: configure with `--fresh` (CMakeCache paths), keep build\reference, build\golden,
  build\manual-test (rebuild manual-test's client from main; its start.cmd has the old path); (5) the helper
  scripts `build\issue-8\tools\macrocheck.py` (ROOT) and the `$TEMP/resolve.py` habit; (6) run the gates once
  (oracle-x86, windows-x64 with golden, a reference compare after asking); (7) the upstream Patella repository at
  C:\Tibia\Patella has its own old paths (CLAUDE.md lines 3, 25, 38–40; notes/reconstruction.md 43–44;
  notes/agent-prompt.md 3, 10) and the user's uncommitted work (notes/testserver.md, tools/build-patella.cmd,
  tools/testserver-gm-account.sql): update those paths only with the user. The T3 project must point at the new
  folder (the user does it, or `t3_project_update`).
- **M3: done** (the user decided its exit: the named checks clean, a decision per other modernize-* check;
  notes/decisions.md "M3's exit"). M4 issues #57–#64 were opened.
- **M4:** done #57 (TFixedString), #61 (FormatCRT71Time), #62 (64-bit time), #64 (TFile/PrintLine). Part done
  #58 (ownership map + 28 unique_ptr owners; component members wait: noexcept(false) destructors). Open #59
  (views), #60 (std::array), #63 (static analysis).
- **Golden corpus (M6): complete.** golden_runner replays 16,461,713 of the oracle's 17,983,332 checks, 1,521,619
  retired with the oracle by the user's decisions (#29's engine tests; Dialogs' advanced dialog 9,129; Errors'
  SurfaceInitInstance 36; Control's ChangeEngine 24,155), 0 not ported, 0 failures. Closed: #26, #27, #28, #29,
  #54; #30 open only for its live check (below).
- **Open M2 bugs/tools:** #56 (live checks on a hidden desktop, user wants it), #66 (golden runners share
  %TEMP%), #68 (compare_files/compare_traffic: wait for the world; WindowedMode's desktop position), #55
  (reproducible recordings, evaluation order), #65 (TDialogWindow::SetSize use after free, M5/M7), #67 (zero-sized
  patch block crashes x64 under /EHsc, M5), #47 (final, M7), #45 (Patella.ini only, M6), #50/#49/#51 closed.

## 2. Waiting on the user

1. **Hosted CI** was blocked by GitHub billing from ~16:30 to ~21:00 November 4; it works again. Commits merged
   on local gates in that window: 8e76abc, 8d48db9, ea69506, fedf7a5 (their issue comments say so); the push of
   0752e0f runs hosted CI over all of them: check that run.
2. **Going public (#3):** the user asked; the answer given: the conditions hold (README; no CipSoft file, golden
   data or recording ever committed on any ref; upstream Patella is already public with the same old history;
   no self-hosted runner registered, PATELLA_SELF_HOSTED unset). Recommended first: Settings → Actions → General →
   "Require approval for all outside collaborators". The user may switch it themselves or say "do it" (then
   `gh repo edit Coldensjo/Patella-Modern --visibility public --accept-visibility-change-consequences`), then
   protect main, rerun hosted CI (public repos have free Actions minutes, which also ends the billing block), close #3.
3. **#30's live check (event 0x12 fix):** log into the test server with the x64 Patella.exe, open the channel list
   (Ctrl+O), check each entry joins the channel it names (before the fix it joined the next one); compare with
   Tibia.exe. The reference compare does not cover the channel dialog. Ask before starting anything that opens
   a window.
4. **Mesa report** (#53): https://gitlab.freedesktop.org/mesa/mesa/-/work_items/16777 ; a scheduled T3 task
   checks it at 23:50 November 4 and should disable itself after that run.

## 3. Lessons from this stretch (add to the mechanics)

- **Verify every merge of two parallel branches on the merged tree**: oracle-x86, a fresh `golden_corpus`, then
  windows-x64 (golden replays it). Twice a semantic conflict appeared only there (#28's Dialogs test vs #58's
  unique_ptr members; new headers vs #49's HeaderCheck lists).
- **Live checks**: `reference.py compare` (~5 min, no server) is the user's usual choice; tier A on request.
  Check `tasklist` for Patella/Tibia clients and `tibia-server.exe` first (a manual-test server from start.cmd once
  blocked compare_files; `testserver.py stop` does not stop a server it did not start). compare_files may fail on
  a cold server (#68): one rerun.
- `tools/patch_client.py` and `tools/client_state.py` read the images through the PDB; type changes in src/
  (TFixedString, unique_ptr) needed tool fixes before the live compare could run.
- Subagents' golden runners share %TEMP% (#66): file-writing golden tests can fail when two run at once; rerun
  alone. A subagent once killed another worktree's build by a too-wide process filter: briefs say "never kill
  processes you did not start (filter by your worktree's path)".
- `build\manual-test\` holds a patched x64 client (start.cmd / stop-server.cmd / dump-hang.cmd) for the user's
  manual tests; rebuild it from the current main when they want to test again.

## 4. Counts on main (every later build must reproduce them)

- difftest.exe 16,848,752, sysinctest.exe 67, communicationtest.exe 1,134,513, vlong 3,201, 0 failures; oracle-x86
  CTest 4/4.
- golden_runner: 16,461,713 replayed + 1,521,619 retired + 0 not ported = 17,983,332, 0 failures.
- Unit tests (Windows): tests_unit 52/10,027, tests_unit_cxx26_standin 52/10,029, tests_core 32/313,
  tests_testmode 35/709, tests_render 14/359, tests_vulkan 14/1,309; Linux tests_unit 51/10,009 (Clang 10,007),
  standin 51/10,011, tests_core 32/316. CTest windows-x64/-debug 13/13, windows-x64-gcc and Linux 11/11.
- clang-tidy baseline 13, 0 new (`tools/tidy_ci.py`); `tools/banned_calls.py` 0 (strcpy/strcat/sprintf/gets, FILE,
  fopen/printf families outside src/types/File.h).

## 5. Next steps (when the user continues)

1. Ask about 2.1–2.3 above.
2. M4: #59 (views), #60 (std::array, with modernize-avoid-c-arrays), #63 (static analysis), #58's second part.
   One at a time in src/, beside a tools/tests issue if file sets are disjoint (#56, #66, #68 are tools).
3. When M4's exit holds (no strcpy/strcat/sprintf/gets: done; sanitizers clean over all replayed sessions on both
   platforms; difftest unchanged), open the M5 issues from the plan and stop and report.
