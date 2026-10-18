// The TARDIS as the player experiences it: spawning, boarding, piloting, flight, float, handbrake, and
// remembering where it was left. Everything goes through Update(); the world and save file are ports.
#pragma once
#include <array>
#include "input.h"
#include "ports.h"
#include "../domain/flight_model.h"
#include "../domain/pilot_camera.h"
#include "../domain/tardis_state.h"

struct TardisSettings
{
	FlightTuning flight;
	float mouseSensitivity = 8.0f;
	float exitOffset = 1.4f; // metres in front of the door
	bool legend = true;      // on-screen list of the TARDIS keys
	std::array<std::string, (size_t)Action::Count> keyNames; // as bound, for the legend
};

class Tardis
{
public:
	Tardis(World &world, Store &store, Audio &audio, TardisSettings settings);

	void Start(); // restore the exterior from the save, if there is one
	void Update(const FrameInput &in, float dt);

	bool piloting() const { return piloting_; }
	std::vector<LegendLine> Legend() const; // what the on-screen legend shows right now
	const TardisState &state() const { return state_; }

private:
	void ToggleSpawn();
	void SetPiloting(bool on);
	void PilotControls(const FrameInput &in);
	void Save();
	void Sounds(Pose ext, V3 vel, float dt);

	World &world_;
	Store &store_;
	Audio &audio_;
	TardisSettings settings_;
	FlightModel flight_;
	PilotCamera camera_;
	TardisState state_;
	V3 angVel_;
	bool piloting_ = false;
	bool owned_ = false;   // flight/float driving the body last frame
	bool frozen_ = false;
	float sinceSave_ = 0;
	float lastVelZ_ = 0;         // for landing sounds
	float sinceLanding_ = 99;
};
