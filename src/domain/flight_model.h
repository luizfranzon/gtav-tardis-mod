// How the TARDIS moves while flight or float owns it. All of TARDIS Rewrite's sh_flight.lua / sh_float.lua
// physics lives behind Step(); Source applies those per 66 Hz tick and they are converted to per-second rates.
#pragma once
#include "math.h"
#include "tardis_state.h"

struct Body
{
	Quat rot;
	V3 vel;    // m/s
	V3 angVel; // world space, rad/s
};

// What the pilot is holding this frame. Flight and float read the same directions (GMod binds them
// to the same keys); brake is float's Space, up/down are flight's Space/Ctrl.
struct Stick
{
	bool forward = false, backward = false, left = false, right = false;
	bool up = false, down = false, boost = false, rotate = false, brake = false;
};

struct FlightTuning
{
	float boostSpeed = 2.5f;  // GMod "boost-speed"
	float stabilizeK = 40.0f; // how hard it rights itself / leans into flight
};

class FlightModel
{
public:
	explicit FlightModel(FlightTuning t) : t_(t) {}

	// True when flight or float owns the body: gravity off and Step() drives it. Otherwise game physics does.
	static bool Owns(const TardisState &s) { return s.flight || s.floating; }

	// Advance one frame. `pilot` is null when nobody is steering (the TARDIS keeps its motion).
	Body Step(const TardisState &s, Body b, const Stick *pilot, View view, float dt) const;

private:
	void Flight(const TardisState &s, Body &b, const Stick *pilot, View view, float dt) const;
	void Float(Body &b, const Stick *pilot, View view, float dt) const;
	FlightTuning t_;
};
