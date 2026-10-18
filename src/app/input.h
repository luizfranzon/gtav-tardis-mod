// What the player did this frame, in TARDIS terms. Bind ids and default keys are TARDIS Rewrite's.
#pragma once
#include <bitset>

enum class Action
{
	Spawn,
	FlightToggle, FloatToggle, Handbrake,
	FlightForward, FlightBackward, FlightLeft, FlightRight, FlightUp, FlightDown, FlightBoost, FlightRotate, FlightSpinDir,
	FloatForward, FloatBackward, FloatLeft, FloatRight, FloatBoost, FloatRotate, FloatBrake,
	Count
};

struct ActionInfo
{
	const char *id;  // TARDIS.ini [Keys] name, same as GMod's bind id
	const char *key; // default, GMod key name
};

inline constexpr ActionInfo kActions[] = {
	{"spawn", "F5"},
	{"flight-toggle", "R"}, {"float-toggle", "T"}, {"handbrake", "J"},
	{"flight-forward", "W"}, {"flight-backward", "S"}, {"flight-left", "A"}, {"flight-right", "D"},
	{"flight-up", "SPACE"}, {"flight-down", "LCONTROL"}, {"flight-boost", "LSHIFT"}, {"flight-rotate", "LALT"},
	{"flight-spindir", "MOUSE_RIGHT"},
	{"float-forward", "W"}, {"float-backward", "S"}, {"float-left", "A"}, {"float-right", "D"},
	{"float-boost", "LSHIFT"}, {"float-rotate", "LALT"}, {"float-brake", "SPACE"},
};
static_assert(sizeof kActions / sizeof *kActions == (size_t)Action::Count);

struct FrameInput
{
	std::bitset<(size_t)Action::Count> down, pressed;
	bool use = false;            // GTA's context key (E), pressed this frame
	float lookX = 0, lookY = 0;  // mouse look delta
	int wheel = 0;               // +1 per notch zooming in

	bool Down(Action a) const { return down[(size_t)a]; }
	bool Pressed(Action a) const { return pressed[(size_t)a]; }
};
