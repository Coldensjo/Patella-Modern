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

Linux: run bin/Patella from the folder with your data files, or start it with --data <folder>; or the same with the
AppImage (the AppImage needs FUSE and bash, or run it with --appimage-extract-and-run). --xdg-config keeps Patella.ini in
$XDG_CONFIG_HOME/patella instead.

You need:
  - a C library of glibc 2.38 or newer (the program's newest needed symbol version; a distribution from 2023 or later);
  - the Vulkan loader, libvulkan.so.1, and a Vulkan 1.3 driver (a GPU's, or Mesa's lavapipe from your distribution's
    mesa-vulkan-drivers for "CPU");
  - X11 or Wayland (their client libraries, which any desktop has).

A build from this project's CI has neutral placeholder cursors and a neutral window icon: the original pictures are CipSoft's.
Licences: share/doc/patella (LICENSE, NOTICE, licenses).
