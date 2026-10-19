// World and input adapters over ScriptHookV natives. Only this layer includes the SDK.
#pragma once
#include <array>
#include <map>
#include <vector>
#include <string>
#include "../app/input.h"
#include "../app/ports.h"

// The Toyota exterior's light (templates_default_exterior.lua): warm yellow, a 200 u (5 m) dynamic light at brightness 2.5
struct LampLook
{
	float belowTop = 0.15f; // metres under the model's top: TardisV's lamp
	int r = 255, g = 231, b = 95;
};

class GtaWorld : public World
{
public:
	GtaWorld(std::string exteriorModel, LampLook lamp) : model_(std::move(exteriorModel)), lamp_(lamp) {}

	// Once per frame, after the TARDIS: keeps a vehicle exterior silent and undamaged, and hands the police and any
	// held train back to the game once the TARDIS stops asking for them.
	void Maintain();

	bool SpawnExterior(Pose at) override;
	void DespawnExterior() override;
	bool HasExterior() override;
	Pose ExteriorPose() override;
	V3 ExteriorVelocity() override;
	void DriveExterior(V3 angVel, V3 vel) override;
	void SetExteriorPhysics(bool gravity, bool frozen) override;
	void SetExteriorAngularVelocity(V3 w) override;
	void PlaceExterior(Pose at) override;
	void SetExteriorAlpha(int alpha) override;
	void ShowLamp(LampLight lamp) override;
	void Rotorwash(bool on) override;

	Pose PlayerPose() override;
	bool PlayerInVehicle() override;
	void Board() override;
	void Unboard(V3 exit) override;

	float GroundZ(V3 near) override;
	std::optional<V3> SafeSpot(V3 near) override;
	V3 RoadNode(V3 near) override;
	void ClearArea(V3 at, float radius) override;
	void Preload(std::optional<V3> at) override;
	void FocusOn(std::optional<V3> at) override;
	bool FocusLoaded() override;
	void FadeScreen(bool out) override;
	std::optional<V3> Waypoint() override;
	V3 CastToWorld(V3 from, V3 to) override;
	View GameplayView() override;

	void ShowCamera(std::optional<CameraShot> shot) override;
	void LockControls() override;
	void BlockPhone() override;
	void HidePlayer() override;
	void HideFromPolice() override;
	bool HoldTrains() override;
	void ShakeCamera() override;
	void Shield(bool visible) override;
	void FlashShield() override;
	void Hint(const std::string &text) override;
	void DrawPanel(const Panel &panel) override;
	void Notify(const std::string &text) override;
	void Trace(const std::string &text) override;

private:
	std::string model_;
	int ext_ = 0; // Object or Vehicle handle
	float baseZ_ = 0; // metres from the model's origin down to its base (TardisV: 1.01)
	float topZ_ = 0;  // metres from the origin up to its top (TardisV: 2.31)
	LampLook lamp_;
	int wash_ = 0;            // looped ptfx handle, 0 when off
	const char *washFx_ = nullptr; // which one is running
	int cam_ = 0;
	bool policeBlind_ = false, policeBlindNow_ = false; // HideFromPolice: last frame, this frame
	std::map<int, float> heldTrains_;                   // engine -> its cruise speed before it was stopped
	bool trainsHeldNow_ = false;

	// shield (gta_shield.cpp)
	V3 boxLo_, boxHi_; // the model's bounds, about its origin
	struct ShieldFlash { V3 at; float age; bool whole; };
	struct Strike { int target; float at; }; // finished off at game time `at` (s) unless the returned shot did it
	std::vector<ShieldFlash> flashes_;
	struct ShockArc { V3 from; int target; float age; };
	std::vector<ShockArc> arcs_;
	std::vector<Strike> strikes_;
	std::map<int, V3> lastImpact_;  // ped -> where its last shot landed, so each shot goes back once
	std::map<int, float> lastShot_; // ped -> when it last fired (s)
	std::map<int, float> pushedAt_; // entity -> when it was last thrown out (s)
	V3 ShieldCentre();
	float ShieldRadius();
	bool OnBox(V3 p);
	void DrawShield();
};

// Keyboard/mouse (only while GTA has focus) plus GTA's look, wheel and context controls
class GtaInput
{
public:
	explicit GtaInput(std::array<int, (size_t)Action::Count> keys) : keys_(keys) {}
	FrameInput Poll();

private:
	std::array<int, (size_t)Action::Count> keys_;
	FrameInput last_;
};
