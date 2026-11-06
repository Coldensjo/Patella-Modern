# Orchestrator handover: Patella-Modern, during M4, after the folder move (November 4, 2026, 22:30)

Copy everything below the line into a new coding-agent session started in `C:\Tibia\Patella-Modern`.

---

You are the **orchestrator** for **Patella-Modern** (`C:\Tibia\Patella-Modern`, GitHub `Coldensjo/Patella-Modern`,
now **public**). Read, in this order: `notes/orchestrator-prompt.md` (sections 1, 2, 5–9: the base brief), then
`notes/orchestrator-handover-m3.md` (its section 2, the user's rules, and section 3, the mechanics), then
`notes/orchestrator-handover-m4.md` (its sections 3 and 4: the lessons and the counts). **This file overrides all
three where they differ.** Old paths in those files (`C:\Patella-Modern`, `C:\Tibia\Tibia.exe`) mean
`C:\Tibia\Patella-Modern` and `C:\Tibia\Patella\Tibia.exe` now.

You do not write code yourself: one subagent per issue (T3 `delegate_task`, `isolation: "worktree"`; call
`orchestrator_capabilities` once first), a complete brief, check the report against the gates, merge, verify the
merged tree, push, close. Small fixes of a merge (e.g. `.get()` on a member another branch converted, a header on
HeaderCheck.cmake's lists) you may make yourself.

## 1. State

- `main` = `origin/main` = **`0752e0f`** ("Merge folders: the folder layout …"). Hosted CI green on it (run
  41902039965, all jobs), which also covers the commits merged on local gates during the billing block (8e76abc,
  8d48db9, ea69506, fedf7a5). The working tree has only the untracked handover notes (`notes/handover.md`,
  `notes/orchestrator-*.md`); leave them uncommitted unless the user says otherwise.
- **The folder move is done** (November 4): C:\Tibia\Patella (upstream Patella repository, Tibia.exe and its data,
  the decompilation workspace), C:\Tibia\Patella-Modern (this repository), C:\Tibia\gameserver (test server),
  C:\Tibia\remeres-map-editor-redux. Verified after the move: the 38 worktrees under
  `C:\Users\Chris\.t3\worktrees\Patella-Modern` point at the new `.git`; the REA MCP server is registered at local
  scope for C:\Tibia\Patella-Modern and C:\Tibia\Patella; Claude's memory loads from `C--Tibia-Patella-Modern`;
  oracle-x86, windows-x64 and windows-x64-gcc were configured with `--fresh` and reproduce every count of section 4
  of the M4 handover (logs `build\logs\move-*.log`); `build\manual-test\Patella.exe` was rebuilt from 0752e0f
  (patched to 127.0.0.1 with `C:\Tibia\gameserver\tibia.pem`) and its .cmd files use the new path;
  `reference.py compare` matches on all 8 sessions. The one-off build folders `build\f2same`, `build\mainoracle`,
  `build\issue-5\measure`, `build\issue-7\*` still have old CMake caches: unused; configure with `--fresh` if ever
  needed. The Linux presets build under Docker's `/work` and were not affected.
- **M3: done.** **M4:** done #57 (TFixedString), #61 (FormatCRT71Time), #62 (64-bit time), #64 (TFile/PrintLine).
  Part done #58 (ownership map + 28 unique_ptr owners; component members wait: noexcept(false) destructors). Open
  #59 (std::string_view/std::span parameters), #60 (std::array), #63 (static analysis to zero or documented
  suppressions).
- **Golden corpus (M6): complete** (16,461,713 replayed + 1,521,619 retired = 17,983,332, 0 failures). #30 is open
  only for its live check (2.3).
- **Other open issues:** #56 (live checks on a hidden desktop; the user wants it), #66 (golden runners share
  %TEMP%), #68 (compare_files/compare_traffic: wait for the world; WindowedMode's desktop position), #55
  (reproducible recordings, evaluation order), #65 (TDialogWindow::SetSize use after free, M5/M7), #67 (zero-sized
  patch block crashes x64 under /EHsc, M5), #47 (final, M7), #45 (Patella.ini only, M6), #3 (go public).

## 2. Waiting on the user

1. **Going public (#3):** the repository **is public now** (the user switched it; checked 22:25 November 4). Still
   open: ask the user before you (a) protect `main` (branch protection or a ruleset: no force pushes, no deletion;
   required checks only if they want PRs), (b) check Settings → Actions → General → "Require approval for all
   outside collaborators" (recommended earlier), (c) close #3 with a comment. PATELLA_SELF_HOSTED must stay unset
   while public, and no self-hosted runner may be registered without the user.
2. **The upstream Patella repository at C:\Tibia\Patella still names the old paths:** `CLAUDE.md` lines 3, 25,
   38–40 and `notes/reconstruction.md` 43–44 (`C:\Tibia\Tibia.exe`, `C:\Tibia\rea-out\...`), `notes/agent-prompt.md`
   3, 10 (`C:\Tibia`). They should become `C:\Tibia\Patella\...`; its `C:\Tibia\gameserver` paths are right. It also
   holds the user's uncommitted work (`notes/testserver.md`, `tools/build-patella.cmd`,
   `tools/testserver-gm-account.sql`): do not touch, stage or commit those. Change the paths only when the user says so.
3. **#30's live check (event 0x12 fix):** log into the test server with the x64 Patella.exe (`build\manual-test\start.cmd`
   is ready), open the channel list (Ctrl+O), check each entry joins the channel it names (before the fix it joined
   the next one); compare with Tibia.exe. The reference compare does not cover the channel dialog. Ask before
   starting anything that opens a window.
4. **T3 projects:** a project for `C:\Tibia\Patella-Modern` exists. The stale ones ("Patella-Modern" at
   `C:\Patella-Modern`, "remeres-map-editor-redux" at `C:\remeres-map-editor-redux`, "Tibia" at `C:\Tibia`) are the
   user's to archive or repoint; do not delete them unasked.
5. **Mesa report** (#53): https://gitlab.freedesktop.org/mesa/mesa/-/work_items/16777 ; a scheduled T3 task checks it
   at 23:50 November 4 and should disable itself after that run; check it did.

## 3. Machine notes

- A `tibia-server.exe` was running during the move's checks (not started by this session). Before any live check,
  `tasklist` for Patella/Tibia clients and tibia-server.exe; `testserver.py stop` does not stop a server it did not
  start; never kill processes you did not start.
- Run only the gates a change can affect (memory: "Gates proportional to change"); tools-only changes get a quick check.

## 4. Next steps

1. Ask about 2.1–2.3 (one message, the questions numbered).
2. M4: #59 (views), #60 (std::array, with modernize-avoid-c-arrays), #63 (static analysis), #58's second part.
   One at a time in src/, beside one tools/tests issue when the file sets are disjoint (#56, #66, #68 are tools).
   Verify every merge of parallel branches on the merged tree (oracle-x86, a fresh `golden_corpus`, windows-x64).
3. When M4's exit holds (no strcpy/strcat/sprintf/gets: done; sanitizers clean over all replayed sessions on both
   platforms; difftest unchanged), open the M5 issues from the plan, then stop and report.
