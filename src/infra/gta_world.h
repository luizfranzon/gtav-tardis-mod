// World and input adapters over ScriptHookV natives. Only this layer includes the SDK.
#pragma once
#include <array>
#include <string>
#include "../app/input.h"
#include "../app/ports.h"

class GtaWorld : public World
{
public:
	explicit GtaWorld(std::string exteriorModel) : model_(std::move(exteriorModel)) {}

	// Once per frame: keeps a vehicle exterior silent and undamaged.
	void Maintain();

	bool SpawnExterior(Pose at) override;
	void DespawnExterior() override;
	bool HasExterior() override;
	Pose ExteriorPose() override;
	V3 ExteriorVelocity() override;
	void DriveExterior(Quat rot, V3 vel) override;
	void SetExteriorPhysics(bool gravity, bool frozen) override;
	void SetExteriorAngularVelocity(V3 w) override;

	Pose PlayerPose() override;
	bool PlayerInVehicle() override;
	void Board() override;
	void Unboard(V3 exit) override;

	float GroundZ(V3 near) override;
	V3 CastToWorld(V3 from, V3 to) override;
	View GameplayView() override;

	void ShowCamera(std::optional<CameraShot> shot) override;
	void LockControls() override;
	void BlockPhone() override;
	void Hint(const std::string &text) override;
	void DrawLegend(const std::vector<LegendLine> &lines) override;
	void Notify(const std::string &text) override;

private:
	std::string model_;
	int ext_ = 0; // Object or Vehicle handle
	int cam_ = 0;
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
