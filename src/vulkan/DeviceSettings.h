// DeviceSettings.h: the x64 client's graphics device choice (Patella-Modern issue #31; plan 6.3, D12;
// notes/vulkan.md, "Choosing the device").
//
// The advanced graphics options' three buttons choose the Vulkan engine's device: "Automatic", "GPU"
// (the next GPU on every click when there are several) and "CPU" (the shipped CPU driver). The choice is
// Patella.ini's Device (src/settings/Settings.h), next to Tibia.cfg; the environment variable
// PATELLA_VULKAN (the same words: auto, gpu, gpuN, cpu) overrides it at start-up, for the comparisons and
// the tests, and is never written. Surface.cpp (SurfaceInitInstance), Control.cpp (ChangeEngine),
// Dialogs.cpp (TAdvancedGraphicsDialog) and SurfaceVulkan.cpp use it. Windows x64 client only; no Vulkan
// header.

#pragma once

#include "vulkan/DeviceChoice.h"

#include <string>

// At start-up (SurfaceInitInstance), before any device exists: the choice (PATELLA_VULKAN when it is
// set and not empty, a value it does not know meaning automatic; else Patella.ini's Device; else
// automatic), the GPUs (ListGpus) and the CPU driver next to the executable.
void LoadDeviceChoice();

patella::vulkan::TDeviceChoice GetDeviceChoice();
void SetDeviceChoice(const patella::vulkan::TDeviceChoice &Choice);

// Writes the choice as Patella.ini's Device; the file's other lines stay as they are, a new file gets a
// comment line first. False when the file cannot be written, or is there but cannot be read (it is then left
// alone); the choice still holds for this run.
bool SaveDeviceChoice();

// The GPUs found at start-up (ListGpus' order, the one GpuIndex counts in), the one "the best GPU" is,
// and a GPU's name ("" for an index out of range).
int GetNumberOfGpus();
int GetBestGpu();
const char *GetGpuName(int Index);

// The CPU driver's manifest next to the executable (lvp_icd.x86_64.json); empty when it is not there.
const std::string &GetCpuDriverManifest();

// The advanced options' engine buttons send the original's engine numbers (MainWindow.cpp: DirectX5 0,
// DirectX9 2, OpenGL 1); the button in each of those places.
patella::vulkan::TDeviceButton DeviceButtonOfEngine(int Engine);

// SurfaceVulkan.cpp: the name of the device the engine runs on ("" while it has none).
const char *GetDeviceName();
