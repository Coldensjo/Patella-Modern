# Release checklist

What to do, in order, to release a commit of Patella-Modern (issue #136, plan M9). Nothing here is automatic: a release, a tag
or anything else outward is the owner's to make. A release never contains CipSoft files (`Tibia.exe`, `Tibia.dat`, `Tibia.spr`,
`Tibia.pic`, `Tibia.cfg`), the golden corpus or the reference recordings, nor anything derived from them (plan D17).

Note the commit first and run every step on that one commit, with a clean working tree (`git status`).

## 1. The gates

Each must finish with exit code 0 and no warning (`tools/build.py` fails a build on any). Logs go to `build\logs\release-<version>\`.

1. Windows: `python -I tools/build.py windows-x64`, `windows-x64-debug`, `windows-x64-asan`, each with the golden corpus at
   15,739,165 checks replayed + 1,576,875 retired = 17,316,040, 0 failures. Use `--fresh` the first time on the commit,
   so that every file is really compiled and checked for warnings.
2. The other compilers: `python -I tools/build.py windows-x64-gcc`, `linux-x64-gcc`, `linux-x64-clang`, the Linux ones with the
   same golden numbers.
3. The behaviour, against the original program (notes/testmode.md, notes/traffic.md, notes/written-files.md, notes/rendering.md):
   - `python -I tools/reference.py compare --reference C:/Tibia/Patella-Modern/build/reference`
   - `python -I tools/compare_traffic.py`, `python -I tools/compare_files.py`, `python -I tools/compare_rendering.py` (Windows
     Sandbox, never `--desktop`)
   - `python -I tools/testmode_check.py`
4. Static checks: `python -I tools/tidy_ci.py`, `python -I tools/banned_calls.py`, and clang-format with the full command of
   CLAUDE.md, "Code style".
5. Hosted CI on the commit, green (`.github/workflows/hosted.yml`).
6. Reproducible builds: `python -I tools/repro_check.py windows-x64 linux-x64-gcc linux-x64-clang` exits 0: two builds of the
   commit, in different folders, give byte-identical `Patella` and `PatellaTest` binaries (notes/decisions.md, "#136", says
   what is and is not identical). `--show-strings` helps when it fails, and its output stays local.

## 2. The changelog

Move the content of "Unreleased" in `CHANGELOG.md` under a heading with the version and the date, with anything added since
the last release in plain words, and start a new empty "Unreleased".

## 3. The packages

**Pending #135 (M9, CPack packages): nothing to run yet.** The step will be a Windows zip, a Linux tarball and AppImage or Flatpak
(plan M9, D4), built from the commit with neutral placeholder pictures or none, the user pointing the client at their own data
files. Before a package goes anywhere, list its contents and check that no CipSoft file or picture derived from one is inside
(the local builds embed pictures from the local `Tibia.exe`, cmake/Resources.cmake: such a binary is never published).

## 4. The tag

The owner creates the annotated tag on the commit and pushes it. Only after every step above: tags are not moved or re-used
once pushed.

## 5. Afterwards

Record the release in `notes/progress.md` (version, commit, date) and the gates' results, and open the next "Unreleased".
