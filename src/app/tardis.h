// The TARDIS as the player experiences it: spawning, boarding, piloting, flight, float, handbrake, cloak, physlock,
// flight speed and travelling to the map waypoint. Nothing outlives the game session. Everything goes through
// Update(); the world and the sound are ports.
#pragma once
#include <array>
#include "input.h"
#include "ports.h"
#include "text.h"
#include "../domain/flight_model.h"
#include "../domain/pilot_camera.h"
#include "../domain/speed_dial.h"
#include "../domain/tardis_state.h"
#include "../domain/teleport.h"

struct TardisSettings
{
	FlightTuning flight;
	float mouseSensitivity = 8.0f;
	float exitOffset = 1.4f; // metres in front of the door
	bool panel = true;       // the console panel on screen while piloting
	bool lampAlwaysOn = false; // GMod extlight-alwayson
	std::array<std::string, (size_t)Action::Count> keyNames; // as bound, for the panel
	Text text;               // in the player's language
};

class Tardis
{
public:
	Tardis(World &world, Audio &audio, TardisSettings settings);

	void Update(const FrameInput &in, float dt);

	bool piloting() const { return piloting_; }
	Panel CurrentPanel() const; // what the console panel shows right now
	const TardisState &state() const { return state_; }
	const Teleport &teleport() const { return tp_; }

private:
	void ToggleSpawn();
	void SetPiloting(bool on);
	void PilotControls(const FrameInput &in, float dt);
	void Cloak(float dt);
	std::string SpinName() const;
	void Sounds(Pose ext, V3 vel, float dt);
	void Demat();
	void Travel(float dt);
	void InterruptTravel();
	void ExplodeIfFast();

	World &world_;
	Audio &audio_;
	TardisSettings settings_;
	FlightModel flight_;
	PilotCamera camera_;
	TardisState state_;
	Teleport tp_;
	float secondBlast_ = 0; // seconds to the second explosion of an interrupted journey
	V3 dest_;          // the waypoint being travelled to
	std::optional<V3> spot_; // where it is set down there, once found
	float travelT_ = 0;      // seconds since H
	float seekT_ = 0;        // seconds spent looking for the spot (from H, or from vanishing on a far trip)
	bool far_ = false;       // too far for both places to stay streamed: the screen blacks out for the switch
	Quat expectRot_;         // where flight meant to turn it last frame, to log how well the game followed
	float sinceDriftLog_ = 0;
	float vanishedT_ = 0, landingT_ = 0; // travelT_ at those events, for the log
	float flightVolume_ = 0.75f; // ducked under the demat/mat sounds for the whole journey
	V3 angVel_;
	bool piloting_ = false;
	bool owned_ = false;   // flight/float driving the body last frame
	bool frozen_ = false;
	SpeedDial dial_;
	float cloakAlpha_ = 255; // fades 255 <-> 0 over 3 s; the exterior shows the lower of this and the journey's
	int shownAlpha_ = 255;
	float clock_ = 0; // seconds, for the lamp's pulse
	float lastVelZ_ = 0;         // for landing sounds
	float sinceLanding_ = 99;
};
