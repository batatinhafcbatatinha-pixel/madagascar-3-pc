// DreamWorks Madagascar 3 for the in-game PC settings (pc_settings.h).
#pragma once

#include "pc_settings.h"

const pcs::Theme* PcMadagascarTheme();  // pc_theme.cpp

inline pcs::GameInfo PcGameInfo() {
  pcs::GameInfo info;
  info.title = L"MADAGASCAR 3";
  info.window_title = "Madagascar 3: The Video Game";
  info.ini_name = L"madagascar3-pc.ini";
  // The game asks for its language itself at the first boot (seven languages).
  info.internal_resolution = true;
  info.msaa = true;
  // One fixed step per frame, presented every second vblank (gpu_hooks.cpp paces the flips):
  // more frames would speed the game up.
  info.real_time = false;
  info.theme = PcMadagascarTheme();
  return info;
}
