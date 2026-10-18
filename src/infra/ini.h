// TARDIS.ini (settings and key binds) and TARDIS_state.ini (where the TARDIS was left).
#pragma once
#include <array>
#include <string>
#include "../app/input.h"
#include "../app/ports.h"
#include "../app/tardis.h"

struct ModConfig
{
	TardisSettings tardis;
	std::string exteriorModel = "maverick";
	float soundVolume = 1.0f; // 0 mutes the TARDIS
	std::array<int, (size_t)Action::Count> keys{}; // virtual-key codes
};

ModConfig LoadConfig(const std::string &iniPath);

// GMod key name ("R", "LSHIFT", "MOUSE_RIGHT", "KEY_F5"...) -> Windows virtual-key code, 0 if unknown
int KeyCode(std::string name);
// GMod key name -> what the legend shows ("LSHIFT" -> "Shift")
std::string KeyLabel(std::string name);

class IniStore : public Store
{
public:
	explicit IniStore(std::string path) : path_(std::move(path)) {}
	std::optional<SavedTardis> Load() override;
	void Save(const std::optional<SavedTardis> &t) override;

private:
	std::string path_;
};
