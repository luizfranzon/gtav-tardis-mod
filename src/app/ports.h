// What the TARDIS needs from the outside. GTA (src/infra) and the test fakes (tests/) adapt these.
#pragma once
#include <optional>
#include <string>
#include <vector>
#include "../domain/math.h"
#include "../domain/lamp.h"
#include "../domain/pilot_camera.h"

// The pilot's console panel, already in the player's language. The world adapter decides how it looks.
struct PanelRow
{
	enum class Kind
	{
		System,  // a light: label, key, on, value (its status word)
		Action,  // label and key; while busy, value is the phase and progress 0..1
		Reading, // label and value
		Footer,  // key and label, under a rule
	};
	Kind kind;
	std::string label, key, value;
	bool on = false;
	float progress = -1; // -1: not busy
};

struct Panel
{
	std::string title;
	std::vector<PanelRow> rows;
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
	// used while flight/float owns it; it turns by angular velocity (world space, rad/s) rather than being set to a
	// rotation, so the game still resolves collisions instead of pushing other vehicles into it
	virtual void DriveExterior(V3 angVel, V3 vel) = 0;
	virtual void SetExteriorPhysics(bool gravity, bool frozen) = 0;
	virtual void SetExteriorAngularVelocity(V3 radPerSec) = 0; // world space; game physics carries it on
	virtual void PlaceExterior(Pose at) = 0;     // teleport it there, at rest, its base resting on at.pos
	virtual void SetExteriorAlpha(int alpha) = 0; // 0 invisible .. 255 opaque
	virtual void ShowLamp(LampLight lamp) = 0;   // every frame: the roof light as it shines this frame

	// The player. Board hides them inside the exterior; unboard puts them back out at `exit`.
	virtual Pose PlayerPose() = 0; // rot is the ped's heading
	virtual bool PlayerInVehicle() = 0;
	virtual void Board() = 0;
	virtual void Unboard(V3 exit, bool parachute) = 0; // parachute: stepping out in the air, with one on the back

	virtual float GroundZ(V3 near) = 0;           // highest ground below near, or near.z if there is none (yet)
	virtual std::optional<V3> SafeSpot(V3 near) = 0; // nearest walkable outdoor spot on the ground, once streamed
	virtual V3 RoadNode(V3 near) = 0;             // nearest road, or near if there is none
	virtual void ClearArea(V3 at, float radius) = 0; // removes peds and parked vehicles
	virtual void Preload(std::optional<V3> at) = 0;  // collision and navmesh around `at`, keeping the map where the player is
	virtual void FocusOn(std::optional<V3> at) = 0;  // stream the whole map in around `at`; nullopt: around the player again
	virtual bool FocusLoaded() = 0;                  // the map around the FocusOn point has streamed in
	virtual void FadeScreen(bool out) = 0;           // to black and back, half a second each
	virtual std::optional<V3> Waypoint() = 0;     // the map waypoint, if one is set
	virtual V3 CastToWorld(V3 from, V3 to) = 0;   // first map hit, or `to`
	virtual View GameplayView() = 0;

	// Per-frame presentation. Camera nullopt hands the view back to the game.
	virtual void ShowCamera(std::optional<CameraShot> shot) = 0;
	virtual void LockControls() = 0; // this frame only
	virtual void BlockPhone() = 0;   // this frame only: no ringing, no calls
	virtual void HidePlayer() = 0;   // this frame only: invisible, unarmed, can't fire (the game can undo it any frame)
	// GMod env_explosion on the exterior: its flash, bang and shake; magnitude in GMod units, 0 harms nothing
	virtual void Explode(float magnitude) = 0;
	virtual void ClearWanted() = 0;    // the wanted level is gone at once (it can be earned again)
	virtual void HideFromPolice() = 0; // this frame only: no wanted level, none can be earned, police ignore the player
	// this frame only: a train running into the exterior stops dead and stays stopped; once this isn't called it
	// goes on. True on the frame one first hits.
	virtual bool HoldTrains() = 0;
	virtual void ShakeCamera() = 0;
	// this frame only: every attack on the TARDIS goes back to its attacker, lethally and as the player's crime, and
	// anything but the player (and their vehicle) entering the bubble is thrown out. visible: the bubble flashes where hit.
	virtual void Shield(bool visible) = 0;
	virtual void FlashShield() = 0; // the whole bubble, once
	virtual void Hint(const std::string &text) = 0; // this frame only
	virtual void DrawPanel(const Panel &panel) = 0; // this frame only
	virtual void Notify(const std::string &text) = 0;
	virtual void Trace(const std::string &text) = 0; // the log file only
};

// TARDIS Rewrite's exterior sounds (interiors/base.lua, templates_default_exterior.lua)
enum class Sound { Spawn, Delete, FlightLand, FlightFall, Demat, Mat, DematFail, CloakOn, CloakOff, ShieldOn, ShieldOff, Interrupt };
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
	// attached: it follows the exterior (MoveAttached), like GMod's ent:EmitSound
	virtual void Play(Sound s, V3 at, bool attached = false) = 0;
	virtual void MoveAttached(V3 at) = 0; // every frame
	virtual void StopAttached() = 0;      // cuts the ones following the exterior (GMod StopTeleportSounds)
	// Call every frame while it should sound; nullopt stops it.
	virtual void Loop(LoopSound s, std::optional<LoopState> state) = 0;
};
