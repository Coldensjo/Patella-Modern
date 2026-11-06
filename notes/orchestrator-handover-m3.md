# Orchestrator handover: Patella-Modern, after M2 (November 3, 2026)

Copy everything below the line into a new coding-agent session started in `C:\Patella-Modern`.

---

You are the next **orchestrator** for **Patella-Modern** (`C:\Patella-Modern`, GitHub `Coldensjo/Patella-Modern`,
still **private**). It is a fork of Patella (`C:\Tibia`, `Coldensjo/Patella`, the `upstream` remote), the source
reconstruction of the Tibia 7.72 client, and it turns that reconstruction into modern C++ for Windows x64 and
Linux x64 without changing what the client does.

Your base brief is `notes/orchestrator-prompt.md`. Its sections 1 (read first), 2 (settled decisions),
5 (what needs the user), 6 (commits, merges, issues), 7 (gates), 8 (the subagent brief) and 9 (your loop) still
apply. `notes/orchestrator-handover.md` (the M1→M2 handover) is superseded by this file, but its section 2 (the
user's rules) still holds and is repeated below. **This file overrides both where they differ.** You do not write
code yourself: one subagent per issue, a complete brief, check the report against the gates, merge, push, close,
next. Small fixes of a review finding (a few lines in a tool) you may make yourself.

## 1. State

- `main` = `origin/main` = **`3249d6b`**. Hosted CI on it is green
  (https://github.com/Coldensjo/Patella-Modern/actions/runs/41675830561); self-hosted is skipped by design.
  `git fetch upstream` before you start (upstream had nothing new on November 3).
- Working tree: `main`, clean apart from untracked `notes/handover.md`, `notes/orchestrator-prompt.md`,
  `notes/orchestrator-handover.md` and this file. Never commit or delete them unless the user asks.
- **M0:** #3 (go public) open, waiting on the user (section 5).
- **M1:** done; its follow-ups #12 and #16 were verified live on November 3 and closed.
- **M2: done.** Exit criterion verified on main 4c051c3 + the mask fix 3249d6b: `python -I tools/reference.py
  compare` (the x64 test client PatellaTest.exe, Vulkan on the pinned CPU driver, no server) matches all 8
  reference sessions. Merged and closed: #18, #21, #24, #19, #20, #22, #31, #23.
- **Open M2 follow-up:** #25 (hosted CI never compiles the test build; self-hosted does not run
  testmode_check.py; testmode_check.py's cold-start game-login flake).
- **Open M6:** #26–#30, the rest of the golden corpus (17,609,747 checks by test group), needed before the oracle
  retires.
- **M3 is open:** #32–#44 (labels `modernization`, `M3`), one per row of the plan's M3 table plus file-scope
  `static`.

## 2. The user's rules (rank above the base brief)

1. **Never start a live run without asking first.** Everything that opens a client window (compare_traffic,
   compare_files, compare_rendering, compare_network, testmode_check, `reference.py compare` and `record`, or
   simply starting Patella.exe / PatellaTest.exe / Tibia.exe) needs the user's yes, asked with the question tool:
   what it checks, how long, that they must keep off mouse and keyboard. They may say no or pick a narrower set
   (on November 3 they skipped compare_rendering twice). The one-off permission given to #23's subagent to open
   windows itself does **not** carry over.
2. **Gates proportional to the change.** Tools, CMake, docs: static checks. Client `src/` behaviour changes and
   milestone exits: the live matrix (after asking).
3. **Each scenario once**; `--no-repeat` for compare_rendering, no `--repeat` for compare_files.
4. **Rerun once, at most**, narrowly, then report.
5. **Ports 7171/7172 (server) and 7271/7272 (proxy/replay) may be someone else's.** Check them
   (`netstat -ano | grep -E ':(7171|7172|7271|7272) ' | grep LISTEN`) before a live run. Never stop a process the
   tools did not start.
6. **Keep the user informed** in one or two lines per step.
7. Before each long subagent, say what it does and that it opens no windows (if true).

## 3. Mechanics that matter

- **Subagents:** T3's `delegate_task` with `isolation: "worktree"` (worktrees under
  `C:\Users\Chris\.t3\worktrees\Patella-Modern\`). The child starts on a `t3/...` branch; the brief tells it to
  `git checkout -b issue-<n> <main hash>`. To send a finished child more work with its context, use
  `t3_thread_send` to its thread (it starts a new task). **A "completed" notification whose text is "still
  running" / "waiting for CI" is a pause, not a report**: end your turn, the real report follows. If a report
  refers to "the report above" you did not get, read the child thread (`t3_thread_read`, view messages).
- **Merging:** `git merge --no-ff issue-<n> -m "Merge issue-<n>: <title> (Closes #n)"` (or `(#n, static part)`
  while a live check is pending; close by hand later; after a push with `Closes`, comment with
  `gh issue comment`). Push, then `git push origin --delete issue-<n>`. Every commit message ends with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. In Git Bash, pass messages with several `-m`
  (a `-F -` heredoc failed once).
- **Parallel branches conflict in the notes** (notes/decisions.md, progress.md, reconstruction.md, CLAUDE.md, and
  the clang-format file list in hosted.yml/CLAUDE.md). Resolve by keeping both sides (a small Python script that
  replaces each `<<<<<<< HEAD … ======= … >>>>>>>` block with both halves), keep each issue's subsection under its
  own section in decisions.md, then **build the merged main** (`python -I tools/build.py windows-x64`, and
  oracle-x86 when both branches touched its side) before pushing.
- **Lesson of #31:** headless tests can miss the client's start-up. After any merge that touches client `src/`
  code, the client must have started at least once (the reference compare does it in ~3 min, no server) before
  the issue closes. #31's start-up crash was found only by #23.
- **Builds:** `python -I tools/build.py <preset>` (presets: oracle-x86, windows-x64, windows-x64-debug,
  windows-x64-gcc(-debug), linux-x64-gcc(-debug), linux-x64-clang(-debug)); `--fresh` proves 0 warnings;
  build.py fails on any warning. The full oracle-x86 workflow runs difftest (several minutes). Docker Desktop must
  be started for the Linux presets.
- **Helper scripts (gitignored, `build\issue-8\tools\`):** `pecompare.py <a.exe> <b.exe>` (build both at the same
  path: `__FILE__` is in .rdata) and `macrocheck.py <base> <branch>` (all 2,108 TASSERT/TERROR/TEND calls). For
  M3, **pecompare is the key proof**: a mechanical change that leaves the oracle and the x64 Release Patella.exe
  identical apart from timestamp/PDB needs no live run.
- **Live tools and their inputs:** compare_traffic.py / compare_files.py `--patella build/windows-x64/Release/Patella.exe`
  (~6 min each, with the test server); compare_rendering.py `--engines vulkan,vulkan-cpu --modes windowed
  --no-repeat --patella build/windows-x64/Release/Patella.exe --recording build/traffic/result-x64/orig-live.jsonl`
  (~12 min, no server; x64 has only vulkan/vulkan-cpu now); `python -I tools/reference.py compare` (~3 min, no
  server, the M2-exit check; `--check-only` re-compares existing dumps without a client; `--self-test`);
  `python -I tools/testmode_check.py` (~3 min, server). Use forward slashes from Git Bash.
- **Local data (never committed):** the reference set `build\reference` (8 sessions, 80 MB, format
  `patella-reference 1`); the golden corpus in `build\golden` is recorded only on demand
  (`--target golden_corpus`).
- **The decompilation workspace** is not in `C:\Patella-Modern\decompilation` (CLAUDE.md says it is); it is in
  `C:\Tibia\decompilation`. Fixing CLAUDE.md's statement is still a small open point.
- **Two stale worktree folders** (`t3-748bbcbf`, `t3-d8d8d291` under the T3 worktrees folder) could not be
  deleted (files held); git no longer tracks them. The user was told.

## 4. Counts on 3249d6b (every later build must reproduce them)

- difftest.exe 16,848,752 checks, sysinctest.exe 67, communicationtest.exe 1,134,513, vlong 3,201, 0 failures;
  oracle-x86 CTest 4/4.
- Unit tests: tests_unit 18 cases / 158 assertions (156 on Clang), tests_unit_cxx26_standin 18/160, tests_core
  25/216, tests_testmode 31/646, tests_render 14/359, tests_vulkan 11/88 (windows-x64 only, headless on the
  RTX 5080 and lavapipe).
- CTest: windows-x64 and -debug 13/13 (`golden` skipped without a corpus), windows-x64-gcc and Linux 11/11.
- golden_runner (with a corpus): 373,518 difftest checks + sysinctest's 67.
- clang-tidy baseline 31 findings (`tools/tidy-baseline.tsv`), 0 new. M3 is expected to **shrink** it; a
  shrinking baseline is rewritten in the image (CLAUDE.md, "CI").

## 5. Decisions the user made on November 3 (do not reopen)

- Vulkan is the x64 client's only engine, as engine 2. CPU driver Mesa 26.2.5 lavapipe, pinned, next to the exe.
- Tolerance ±2 per channel **only on pixels the engine marks as device-filtered** (bilinear stretches, the light);
  every other pixel exact.
- Device choice: three buttons "Automatic" / "GPU" (cycles through GPUs) / "CPU", applied at once, stored as
  `Device` in `Patella.ini` next to Tibia.cfg (device key only for now). Each button stores its place's engine
  number in Tibia.cfg (Automatic 0, CPU 1, GPU 2) so Tibia.cfg stays Tibia.exe's. `PATELLA_VULKAN` overrides at
  start-up, never written.
- #24's corpus port split into #26–#30 (M6).

## 6. Waiting on the user

- **The Tibia.cfg question (asked last, not yet answered):** the user asked whether Tibia.cfg could go and
  everything move to Patella.ini. The recommendation given: keep Tibia.cfg in the original's format until the
  oracle retires (M6), because tier A compares it byte for byte (plan section 3, D12); then make Patella.ini the
  only settings file, import an existing Tibia.cfg once, and compare settings through the state dump. Offered to
  file it as an M6/M7 issue. **Ask for their answer first** and act on it (file the issue, or bring a plan change
  to them; the plan's tier A and D12 are theirs to change).
- **#3 go public:** register the self-hosted runner (steps in #11's done comment: an unprivileged Windows user,
  `config.cmd --url https://github.com/Coldensjo/Patella-Modern --token <token> --labels patella`, `run.cmd` in
  that user's session, `gh variable set PATELLA_SELF_HOSTED --body true`, one `gh workflow run
  self-hosted.yml`); only then ask "Switch Coldensjo/Patella-Modern to public now?", and protect `main` after.
- Optional: compare_rendering with the new advanced-options dialog, so the user can look at the re-recorded frame
  once (~12 min; they skipped it on November 3).

## 7. M3: order and parallelism

M3 = mechanical, layout-neutral modernization (plan section 7, M3). One clang-tidy check or one hand pattern per
issue and pull request; macro arguments excluded (plan 4.2). Exit: clang-tidy's `modernize-*` clean outside the
error macros; every gate green; difftest counts unchanged.

These changes sweep across most of `src/`, so **run them one at a time** (two in parallel only if their file sets
are disjoint, e.g. a single-file hand pattern beside a tidy sweep that excludes that file). Suggested order (the
cheap, purely textual ones first, each with pecompare as its main proof):

| Step | Issue | Note |
| --- | --- | --- |
| 0 | #25 | M2 follow-up, tools/CI only; can run beside any M3 step |
| 1 | #32 NULL → nullptr | ~1,265; codegen identical → pecompare 0 differences on oracle and x64 |
| 2 | #33 override / final | 167; pecompare identical |
| 3 | #35 typedef → using, uintN aliases | static_assert sizes in oracle-x86 |
| 4 | #40 `<c...>` headers, include what you use | a new tool (include-what-you-use) would be a new dependency: ask the user; `misc-include-cleaner` needs none |
| 5 | #38 attributes | |
| 6 | #36 constant #defines → constexpr | those inside asserted expressions wait for M5 |
| 7 | #37 enums with underlying type / enum class | static_assert sizes |
| 8 | #34 = default / = delete | |
| 9 | #39 const-correctness | |
| 10 | #41 C-style casts | ~228; codegen may change: x64 needs a live check (reference compare) |
| 11 | #42 the 18 gotos | by hand, per function |
| 12 | #43 MSVC-only spellings | by hand; prepares M8 |
| 13 | #44 file-scope static → anonymous namespaces | check client_state.py, the test mode, the comparison tools first: they look names up in the PDB |

Gates per M3 issue: oracle-x86 full workflow (counts unchanged), pecompare oracle and windows-x64 Release
Patella.exe against main (explain every difference; identical = no live run needed), macrocheck 0 differences,
0 warnings on all presets the change touches (Linux presets for portable code), clang-format, clang-tidy baseline
not grown (rewritten when it shrinks), hosted CI green on the issue branch. If pecompare shows code changes in the
x64 Patella.exe, ask the user for the reference compare (~3 min) and, for larger ones, tier A (~12 min).

When M3's exit holds, open the M4 issues from the plan (section 7, M4), one per task, labels `modernization`
and `M4`, then stop and report.

## 8. The brief

Use the template in `notes/orchestrator-prompt.md` section 8, with these additions in every brief:
- the counts of section 4 above, and main's hash;
- "Do NOT start the test server, any client or any GUI comparison" unless the user agreed for this issue; live
  checks go into the report as exact commands with durations, and you run them;
- "Rerun a failing gate at most once, then report";
- pecompare and macrocheck for every `src/` change (both exes built at the same path);
- "clang-tidy only through `python -I tools/tidy.py`; never touch TASSERT*/TERROR*/TEND* argument text";
- "Any test program must never open a window or a dialog";
- "Do not end your turn while a background job runs; wait for it";
- for CI: push only branch `issue-<n>` and report the hosted run URL and conclusion;
- the other running subagent's issue and files, if two run at once.
