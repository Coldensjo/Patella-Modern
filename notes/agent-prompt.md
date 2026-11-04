# Prompt: work one Patella issue

Copy everything below the line into a new coding-agent session started in `C:\Tibia\Patella`.
Replace `<ISSUE>` with an issue number, or leave it as it is to let the agent pick
the next one.

---

You are working on **Patella**, a source reconstruction of the Tibia 7.72 client
(`C:\Tibia\Patella\Tibia.exe`), in the repository at `C:\Tibia\Patella` (GitHub: `Coldensjo/Patella`,
private). Work on exactly **one** GitHub issue, finish it completely, then stop.

## 1. Pick the issue

- If an issue number is given here: **<ISSUE>**, work on that one.
- Otherwise run `gh issue list --repo Coldensjo/Patella --state open --limit 50` and take
  the lowest-numbered open issue that is not assigned and not labelled `blocked`, and
  whose prerequisites are done (a window file needs GUI.cpp, which is done; Dialogs.cpp
  needs DialogWindow.cpp and DialogElements.cpp; the verification issues need a working
  `Patella.exe`). If the lowest one is not ready, take the next ready one and say why.
- Assign it to yourself so no other session takes it:
  `gh issue edit <n> --repo Coldensjo/Patella --add-assignee @me`, and comment that work
  has started.

## 2. Read before you change anything

1. `CLAUDE.md` and the issue itself (`gh issue view <n> --repo Coldensjo/Patella`).
2. `notes/progress.md` (current state, "Notes for later files") and `git log --oneline -20`.
3. `notes/reconstruction.md` (the process, the tools, the status table).
4. `notes/decisions.md` (how earlier judgement calls were made and logged).
5. For a file issue: the closest finished files as style references (e.g. `src/Objects.cpp`,
   `src/Map.cpp`, `src/GUI.cpp`, `src/MainFrm.cpp`), `decomp/clean/<File>.c` and the
   `gap_*.c` files named in the issue.

## 3. Do the work

Follow `notes/reconstruction.md` exactly. In particular:

- Settle every gap named in the issue first, with evidence (data addresses, catch lines,
  vtables, static-initialiser order near 0x559fc0-0x55a400); record ranges in
  `file-ranges.tsv`, then rerun `python -I tools/triage.py` and `python -I tools/simplify.py`.
- Check catch types and try ranges with `python -I tools/ehinfo.py <address>`, switches
  against the binary's jump tables, and run `python -I tools/callcheck.py src/<File>.cpp`;
  explain every remaining difference.
- Every game function gets a real name (no `FUN_` names defined in your source) and a
  `names.tsv` row. Template instances go to `src/Container.h`; VC7.1 STL/MFC/compiler code
  gets a `names.tsv` row of kind `stl`, never a reimplementation. No shims or wrapper
  functions that the binary does not have.
- Declare every function and global in the header of the file that defines it (there is
  no catch-all header since issue #14) and include that header where it is used. Check each
  changed call site against `decomp/clean`.
- Behaviour must match the binary exactly, including odd behaviour: comment it, never
  fix it.
- Add every function that can run in the difftest child to a `tools/difftest/<file>test.cpp`
  (reuse `PatchImage` and the patterns of the other test files) and register it in
  `difftest.h`, `difftest.cpp` and `build.cmd`.
- Tabs for indentation. The Bash tool expands `\n` in heredocs and sed: write C/C++
  escapes with the Edit/Write tools.
- When something is unclear but has a reasonable default (a name, a type, a gap with
  weak evidence), choose it, mark it low confidence, log it in `notes/decisions.md`
  under a section for this issue, and keep going.
- Commit work in progress on `main` at sensible points (`WIP: #<n> ...`), so nothing is
  lost if the session ends early.

For non-file issues (build, verification, tooling), the issue's "Done when" list is the
definition of done; keep the same care: evidence for every claim, scripts that
reproduce every check, results recorded in the notes.

## 4. Check before you call it done

All of these must hold; run them, do not assume:

- `tools\compile-check.cmd`: no warnings, no errors.
- `tools\difftest\build.cmd`: 0 failures. Run it from PowerShell (`cmd /c ...`); from the
  Git Bash tool it can end with exit 1 and no output. If it fails without a message,
  kill `difftest.exe` and run it again before investigating.
- Every item of the issue's "Done when" list is met.
- `notes/reconstruction.md` (status row, last differential run), `notes/progress.md`
  (backlog, last difftest, notes for later files) and `notes/decisions.md` are updated.

## 5. Finish and stop

1. Commit on `main` with a message that names the issue ("Reconstruct DialogWindow.cpp",
   body with the main facts, `Closes #<n>`), ending with the attribution lines from your
   instructions.
2. Push: `git push origin main`.
3. Comment on the issue with a short summary (what was done, test totals, judgement
   calls worth a review, anything left over) and close it if the push did not.
4. If you found work that does not belong to this issue, open a new issue for it instead
   of doing it.
5. **Stop.** Do not start another issue.

## Stop and ask instead

Stop, leave the issue open with a comment, and ask the user when:

- the issue needs something installed (e.g. the Visual Studio MFC component for #16) or
  needs the original client to be run;
- the difftest fails and you cannot explain why after a real attempt, or the repository
  is in a state you cannot repair (broken history, merge conflicts you cannot resolve);
- the issue turns out to be much larger than described: then split it (open the new
  issues), finish the part you can, and say so.

Do not push anything other than `main`, do not change the GitHub repository settings,
and do not commit the client's files (`Tibia.exe`, `Tibia.dat`, `Tibia.spr`, `Tibia.pic`)
or the raw decompiler output; `.gitignore` excludes them.
