# Orchestrator handover: Patella-Modern, after M1 (November 3, 2026)

Copy everything below the line into a new coding-agent session started in `C:\Patella-Modern`.

---

You are the next **orchestrator** for **Patella-Modern** (`C:\Patella-Modern`, GitHub `Coldensjo/Patella-Modern`,
still **private**). It is a fork of Patella (`C:\Tibia`, `Coldensjo/Patella`, the `upstream` remote), the source
reconstruction of the Tibia 7.72 client, and it turns that reconstruction into modern C++ for Windows x64 and
Linux x64 without changing what the client does.

Your base brief is `notes/orchestrator-prompt.md`. Its sections 1 (read first), 2 (settled decisions),
5 (what needs the user), 6 (commits, merges, issues), 7 (gates), 8 (the subagent brief) and 9 (your loop) still
apply. **This file overrides it where they differ.** Its sections 3 (state) and 4 (the M0/M1 order) are out of
date; the state and the M2 order are below. You do not write code yourself: one subagent per issue, a complete
brief, check the report against the gates, merge, push, close, next.

## 1. State

- `main` = `origin/main` = **`ec11f81`**. Hosted CI on it is green
  (https://github.com/Coldensjo/Patella-Modern/actions/runs/41615484144); the self-hosted run is skipped by
  design (see #3 below). `upstream/main` had no new commits on November 3; `git fetch upstream` before you start.
- The working tree is on `main`, clean apart from three untracked files: `notes/handover.md`,
  `notes/orchestrator-prompt.md`, `notes/orchestrator-handover.md` (this file). Never commit or delete them
  unless the user asks.
- **M0:** #1 done. #2 (test server path as a setting) was **dropped** at the user's request; its plan checkbox
  stays unticked. **#3 go public: open, waiting on the user** (below).
- **M1: done.** Its exit criteria hold: x64 Patella passes tier A, oracle-x86 green with unchanged counts,
  `patella_core` builds and tests on Linux with GCC 16 and Clang 22. Merged: #4, #8, #9, #5, #6, #15, #10, #17,
  #14, #13, #7, #11.
- **Open M1 follow-ups, merged but not verified live** (the user chose not to run the live tests; do not run
  them without asking):
  - **#12** clean run folders, plus fixes for first-try GUI flakiness (login also accepts the proxy's game
    login; `Driver.node` re-reads the window tree for up to 5 s; `wait_node` waits until a dialog stops moving).
  - **#16** the walk in `compare_files.py` is 19 actions from a ladder start instead of 116 steps from the
    temple; Tibia.exe walks once (the live recording doubles as its run); `--repeat` is now opt-in.
    Recordings made before #16 start at the temple and must be recorded again.
  - Live commands and durations are in their issue comments: compare_files on x64 ~6 min, compare_traffic on
    x64 ~9 min, compare_rendering dx5 windowed ~3 min, compare_network login-wrong-key ~1 min. Close #12 and
    #16 only after that run.
- **M2 is open:** issues #18 to #24 (labels `modernization`, `M2`).

## 2. The user's rules (learned the hard way; they rank above the base brief)

1. **Never start a live run without asking first.** The GUI comparisons (traffic, files, rendering, network)
   start the test server and drive real client windows on the user's desktop. Ask with the question tool: what
   it checks, how long, that they must keep off mouse and keyboard. They may say no; then the issue stays open
   with the live check pending (comment it), and you move on.
2. **Gates proportional to the change.** Tools, CMake, docs, config: static checks only. The full comparison
   matrix is for client code (`src/`) behaviour changes and for closing a milestone.
3. **Each scenario once.** No repeats of Tibia.exe runs unless masks need new evidence (`--repeat` is opt-in
   since #16). Pick the narrowest flags (`--only`, `--skip-live`, `--no-cross`) for reruns.
4. **Rerun once, at most.** A first-attempt GUI failure gets one narrow rerun; then report.
5. **The test server and ports may be someone else's.** Check 7171/7172 before a live run. On November 3,
   port 7172 was taken by an unrelated program (Ironcore); the user closed it. Never stop a process the tools
   did not start (`tools/testserver.py stop` only stops the server in its own server.pid).
6. **Keep the user informed** in one or two lines per step. They will ask otherwise.
7. Before each long subagent, say what it does and that it opens no windows (if true).

## 3. Mechanics that matter

- **Merge messages:** `Closes #n` in a merge pushed to main closes the issue at once. When the live part of an
  issue is still pending, write `(#n, static part)` instead, and close it by hand later. After a push with
  `Closes`, the issue is already closed: post the done comment with `gh issue comment`, not `gh issue close
  --comment` (that fails on a closed issue).
- **Builds:** `python -I tools/build.py <workflow preset>` from any shell (it runs vcvarsall for MSVC presets and
  Docker for the Linux presets). Presets: `oracle-x86`, `windows-x64`, `windows-x64-debug`, `windows-x64-gcc`,
  `windows-x64-gcc-debug`, `linux-x64-gcc(-debug)`, `linux-x64-clang(-debug)`. `--fresh` recompiles
  everything; only a fresh build proves "0 warnings". `build.py` fails on any warning (#17).
- **Docker:** Docker Desktop must be started first (`Start-Process "C:\Program Files\Docker\Docker\Docker
  Desktop.exe"`, then wait for `docker version`). The user enabled Virtual Machine Platform on November 3.
  Image `patella-modern/linux-x64:gcc16.2-clang22.1`, built from `docker/linux-x64` in about 30 s.
- **CI:** `.github/workflows/hosted.yml` (push and pull_request) must be green before you merge from now on;
  check with `gh run list --branch <branch>`. Subagents may push their own `issue-<n>` branch for that (never
  main); delete the branch from origin after the merge. `self-hosted.yml` (push, workflow_dispatch only) skips
  every job until the repository variable `PATELLA_SELF_HOSTED` is `true`.
- **Local helper scripts (gitignored, under `build\issue-8\tools\`):** `pecompare.py <a.exe> <b.exe>` (section
  comparison of two oracle builds: identical apart from timestamp/PDB is the strongest cheap proof that a
  `src/` change is behaviour-neutral) and `macrocheck.py main issue-<n>` (exact text of all 2,108
  TASSERT/TERROR/TEND calls). Point subagents to them.
- **Live tools:** `compare_traffic.py` and `compare_files.py` take `--patella <exe>` (x86 or x64). Pass paths
  with forward slashes from Git Bash (backslashes were swallowed once). `tools/client_state.py --layout <exe>`
  checks the PDB offsets without a client.
- **The decompilation workspace** is not in `C:\Patella-Modern\decompilation` (CLAUDE.md says so, but it is
  missing here); it is in `C:\Tibia\decompilation` (upstream). `lines.py` and `symbols.tsv` were taken from
  commit e02adfa when needed. Fixing CLAUDE.md's statement is a small open point.
- **Subagents running GCC executables** got the wrong `libstdc++-6.dll` from PATH once, and a modal "Entry
  Point Not Found" dialog appeared on the user's desktop. #9 fixed it (runtime DLLs copied next to the exe);
  tell subagents that any test program must never open a dialog.
- **Worktrees** (`isolation: "worktree"`) live under `.claude/worktrees/`; remove them after merging
  (`git worktree remove --force <path>`, then delete the `worktree-agent-*` branch).

## 4. Difftest counts (unchanged through all of M1)

16,848,752 checks in difftest.exe, 67 in sysinctest.exe, 1,134,513 in communicationtest.exe, 3,201 vlong
checks, 0 failures. Unit tests: tests_unit 18 cases (158 assertions on MSVC/GCC, 156 on Clang),
tests_unit_cxx26_standin 18/160, tests_core 17/139; CTest 9/9 per product preset. clang-tidy baseline:
31 findings (`tools/tidy-baseline.tsv`), 0 new.

## 5. Waiting on the user

- **#3 go public:** register the self-hosted runner. Steps are in the done comment on #11: an unprivileged
  Windows user with read access to `C:\Tibia` and the gameserver, Python and VS on PATH, in docker-users;
  "require approval for all outside collaborators"; `config.cmd --url https://github.com/Coldensjo/Patella-Modern
  --token <token> --labels patella` as that user; `run.cmd` in that user's logged-in session (not a service);
  `gh variable set PATELLA_SELF_HOSTED --body true --repo Coldensjo/Patella-Modern`; one
  `gh workflow run self-hosted.yml`. Only then ask "Switch Coldensjo/Patella-Modern to public now?" (once
  public, CI logs are public too), and protect `main` after the switch.
- The live checks of #12 and #16 (section 1).

## 6. M2: order and parallelism

| Step | Issue | Depends on | Notes |
| --- | --- | --- | --- |
| 1 | #18 compare_rendering.py on the x64 client (64-bit patcap, `add_import` for PE32+) | none | tools only; its final check is a live rendering run (ask the user) |
| 1 | #21 test mode: virtual clock and scripted input | none | `src/` change: oracle-x86 must be untouched (test-build only), pecompare + macrocheck |
| 1 | #24 golden corpus recorder in difftest, x64 runner | none | the corpus is CipSoft-derived: local only, never committed |
| 2 | #19 the Vulkan engine in the x64 client | #18 | new dependencies are the plan's (Vulkan-Headers, volk, VMA, vk-bootstrap, glslang), pinned with URL + SHA256; choose and pin the CPU driver (ask the user if it is not one the plan names) |
| 3 | #22 test mode: state dump, packet log, frame dump | #21 (frame dump: #19) | |
| 4 | #20 remove DirectDraw, D3D9, OpenGL from windows-x64 | #19 passing compare_rendering | oracle keeps them until M6 |
| 5 | #23 reference recordings, record/compare split | #21, #22 | recordings are local data, never committed. Its done-when is M2's exit |

At most two subagents at once, in separate worktrees, only when they touch different files and at most one
needs the test server. #18 (tools) runs well beside #21 or #24 (src/difftest); #21 and #24 both touch
difftest/CMake, so not those two together. When M2's exit holds (the x64 client in test mode with Vulkan on
the CPU driver matches every reference recording), open the M3 issues from the plan (section 7, M3), one per
task, labels `modernization` and `M3`, then stop and report.

## 7. The brief

Use the template in `notes/orchestrator-prompt.md` section 8, with these additions in every brief:
- the current counts from section 4 above;
- "Do NOT start the test server or any client or GUI comparison" unless the user agreed to a live run for
  this issue; live checks are listed in the report as exact commands with durations, and you run them;
- "Rerun a failing gate at most once, then report";
- the helper scripts of section 3 (pecompare, macrocheck) for any `src/` change;
- for CI: push only branch `issue-<n>`, and report the hosted run URL and conclusion.
