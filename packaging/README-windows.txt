Patella-Modern, a modernized reconstruction of the Tibia 7.72 client.

This package has no game data. You need your own copy of the Tibia 7.72 files: Tibia.dat, Tibia.spr and Tibia.pic,
and a server to connect to. Tibia.cfg is optional (its options are imported once). They belong to CipSoft GmbH and
are not distributed here.

The client looks for the data files, in this order:
  1. --data <folder> on the command line
  2. the working folder, if it has Tibia.dat
  3. the folder of the program, if it has Tibia.dat
  4. the DataFolder key of a Patella.ini (the working folder's, then the program's)
Patella.ini, the settings file, is written next to the data files.

Windows: run Patella.exe. Put your data files next to it, or start it with --data <folder>.

You need a Vulkan 1.3 driver and the Vulkan loader (vulkan-1.dll), which every GPU driver with Vulkan support installs.
Without a working GPU, choose "CPU" in the advanced graphics options: Mesa's lavapipe, in this folder, still needs that
loader.

A build from this project's CI has neutral placeholder cursors and a plain icon: the original pictures are CipSoft's.
Licences: LICENSE, NOTICE and the folder licenses.
