# Modernizing Patella, explained simply

## What is Patella?

Tibia is an online game from 1997. Patella is a rebuild of version 7.72 of its game client (the program
players run on their computer), made from the 2006 program file. The rebuild has been checked piece by
piece against the original, so it behaves exactly like it.

That faithfulness comes at a price. The rebuilt code is written the way programs were written in 2006: it only
runs on Windows, as an old-style 32-bit program, draws its graphics with technology from the late 1990s, and
uses programming habits that are known today to cause crashes and security holes.

## What does "modernization" mean?

Think of restoring an old house. From the street it looks exactly the same as before, but inside the wiring,
plumbing and insulation are replaced with today's materials. It becomes safer, cheaper to maintain and ready
for the next decades, without becoming a different house.

Modernizing Patella is the same idea:

- **On the outside, the game stays the same.** It looks the same, plays the same, speaks the same language to
  the servers and uses the same game files. The few visible changes are listed below.
- **On the inside, almost everything is renewed:** the programming language version, the tools that build the
  program, the way it draws graphics, and the way it handles memory and errors.

## What players get

| Today | After modernization |
|---|---|
| Windows only | **Windows and Linux** |
| 32-bit program (an old standard, limited to 4 GB of memory) | **64-bit program**, the standard on every current computer |
| Graphics through DirectX 5, DirectX 9 or old OpenGL: technology from 1997 to 2004, some of it only emulated by today's Windows | **Vulkan**, today's standard graphics technology, on both Windows and Linux. Computers that can't run Vulkan on their graphics card (very old ones, virtual machines, remote desktop) get a version of Vulkan that runs on the main processor instead |
| Tiny picture on today's large, sharp screens | **Sharp enlargement** (2x, 3x...) that keeps the pixel-art look, and fullscreen without changing the monitor's resolution |
| Built to connect only to the original company's servers, which no longer exist | **Choose your server** in a settings file, no tinkering with the program file |
| Code habits that can let a malformed message from a server overflow memory | **Safer code** that checks sizes and cleans up after itself, tested with tools that feed it garbage on purpose |

## What is removed

- **The crash report file (`error.txt`).** When the old client crashed, it wrote a report, asked at the next
  start whether to send it, and sent it to the game server. The official servers for this version shut down
  long ago, and the servers people run today only file these reports away, so the client now just shows the
  error message and closes.

- **The built-in updater.** Any server you logged into could replace the game's files and make the client
  start a program. That is a security risk, and the servers it was made for are gone, so it is removed.
  Game files are updated by hand instead.

## How we make sure nothing breaks

This is the most important part of the project. The work happens in many small steps, and after every step
automatic tests compare the new program with the original 2006 one:

- **The same conversation with the server:** both programs play the same recorded game session, and every
  message they send is compared.
- **The same files:** settings, maps and saved chats must come out identical.
- **The same picture:** every frame both programs draw is compared pixel by pixel. The only exception is the
  soft light around torches and spells, which the graphics card itself draws, so even the original never
  looked quite the same on two computers.

A special test version of the client can also play a written script of key presses, mouse clicks and typed
text on a clock of its own, which moves only when the script says so. Run twice, it does exactly the same
thing both times, so a test does not depend on how fast the computer is or on someone touching the mouse. The
normal client contains none of this.

If any of these differ, the step is not accepted. The original program acts as an answer key that the new one
must keep matching.

## The steps, in plain words

1. **New building tools.** Set up modern tools that can build the program for both Windows and Linux.
2. **Better testing and new graphics.** Teach the client to record and replay game sessions by itself, so that
   tests can run on Linux too, without the original Windows program. Build the new Vulkan graphics engine and
   retire the three old ones.
3. **Tidy-up.** Let automatic tools update old-fashioned code to today's style, one kind of change at a time.
4. **Safety.** Replace the risky memory and text handling with safe versions.
5. **Simpler error handling.** Remove the old crash report system and the large amount of code that existed
   only to feed it.
6. **Cutting the cord.** Once the new tests cover everything the old ones did, stop requiring the program's
   insides to be arranged exactly like the 2006 original. From here on, the rebuild and the modern version go
   separate ways.
7. **Reorganising.** Group the code into clear parts (map, network, windows, settings) with clean
   connections between them.
8. **Linux.** Replace the remaining Windows-only parts (window, keyboard, mouse, network) with SDL3 and
   portable code, so the same program runs on Linux.
9. **Finishing.** Stress-testing, speed checks and ready-to-install packages.

## Words you may come across

- **C++:** the programming language the client is written in. It gets a new version every three years; "C++23"
  and "C++26" are the 2023 and 2026 versions.
- **Compiler:** the tool that turns the written code into a program you can run. We use Microsoft's on Windows
  and GCC on Linux.
- **CMake:** the tool that tells the compiler what to build and how, the same way on every system.
- **Renderer / graphics engine:** the part of the program that draws pictures on the screen.
- **Vulkan:** a modern, open standard for talking to graphics cards, supported on Windows and Linux (and, through
  a translation layer, on Macs).
- **SDL3:** a free, widely used library that handles windows, keyboard and mouse on many systems, so the game
  does not need separate code for each one.
- **64-bit:** the way all current computers and operating systems work; 32-bit programs are a leftover from
  before about 2010.

For the technical details, see the full plan: [notes/modernization-plan.md](../notes/modernization-plan.md).
