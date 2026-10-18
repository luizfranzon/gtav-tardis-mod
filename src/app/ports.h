// What the TARDIS needs from the outside. GTA (src/infra) and the test fakes (tests/) adapt these.
#pragma once
#include <optional>
#include <string>
#include <vector>
#include "../domain/math.h"
#include "../domain/pilot_camera.h"

struct LegendLine
{
	std::string key;  // "R", "Shift", or "" for a heading/status line
	std::string text;
};

class World
{
public:
	virtual ~World() = default;

	// The exterior. Spawn fails (false) when its model can't be loaded.
	virtual bool SpawnExterior(Pose at) = 0;
	virtual void DespawnExterior() = 0;
	virtual bool HasExterior() = 0;
	virtual Pose ExteriorPose() = 0;
	virtual V3 ExteriorVelocity() = 0;
	virtual void DriveExterior(Quat rot, V3 vel) = 0; // used while flight/float owns it
	virtual void SetExteriorPhysics(bool gravity, bool frozen) = 0;
	virtual void SetExteriorAngularVelocity(V3 radPerSec) = 0; // world space; game physics carries it on

	// The player. Board hides them inside the exterior; unboard puts them back out at `exit`.
	virtual Pose PlayerPose() = 0; // rot is the ped's heading
	virtual bool PlayerInVehicle() = 0;
	virtual void Board() = 0;
	virtual void Unboard(V3 exit) = 0;

	virtual float GroundZ(V3 near) = 0;           // near.z if there is no ground
	virtual V3 CastToWorld(V3 from, V3 to) = 0;   // first map hit, or `to`
	virtual View GameplayView() = 0;

	// Per-frame presentation. Camera nullopt hands the view back to the game.
	virtual void ShowCamera(std::optional<CameraShot> shot) = 0;
	virtual void LockControls() = 0; // this frame only
	virtual void BlockPhone() = 0;   // this frame only: no ringing, no calls
	virtual void Hint(const std::string &text) = 0; // this frame only
	virtual void DrawLegend(const std::vector<LegendLine> &lines) = 0; // this frame only
	virtual void Notify(const std::string &text) = 0;
};

// TARDIS Rewrite's exterior sounds (interiors/base.lua, templates_default_exterior.lua)
enum class Sound { Spawn, Delete, FlightLand, FlightFall };
enum class LoopSound { Flight };

struct LoopState
{
	V3 at, vel;          // world position and velocity (m, m/s), for distance and doppler
	float pitch = 1;     // 1 = normal
	float volume = 1;
};

class Audio
{
public:
	virtual ~Audio() = default;
	virtual void Play(Sound s, V3 at) = 0;
	// Call every frame while it should sound; nullopt stops it.
	virtual void Loop(LoopSound s, std::optional<LoopState> state) = 0;
};

struct SavedTardis
{
	Pose pose;
	bool handbrake = false;
};

class Store
{
public:
	virtual ~Store() = default;
	virtual std::optional<SavedTardis> Load() = 0;
	virtual void Save(const std::optional<SavedTardis> &t) = 0;
};
