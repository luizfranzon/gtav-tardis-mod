// TARDIS.ini: settings and key binds.
#pragma once
#include <array>
#include <string>
#include "../app/input.h"
#include "../app/ports.h"
#include "../app/tardis.h"
#include "gta_world.h"

struct ModConfig
{
	TardisSettings tardis;
	std::string exteriorModel = "tardis";
	float soundVolume = 1.0f; // 0 mutes the TARDIS
	LampLook lamp;
	std::array<int, (size_t)Action::Count> keys{}; // virtual-key codes
};

ModConfig LoadConfig(const std::string &iniPath);

// translations/en-US.ini, then <language>.ini over it ("pt-BR"): what that language lacks stays English
Text LoadText(const std::string &dir, const std::string &language);

// GMod key name ("R", "LSHIFT", "MOUSE_RIGHT", "KEY_F5", "KEY_PAD_8"...) -> Windows virtual-key code, 0 if unknown
int KeyCode(std::string name);
// GMod key name -> what the panel shows ("LSHIFT" -> "Shift", "PAD_8" -> "Num 8")
std::string KeyLabel(std::string name);
