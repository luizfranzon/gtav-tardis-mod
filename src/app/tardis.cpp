#include "tardis.h"
#include <algorithm>

namespace
{
constexpr float kBoardRange = 3.0f;   // metres; stage 1 stand-in for the console (stage 2)
constexpr float kParkRange = 150.0f;  // beyond this the map collision under it isn't streamed
constexpr float kSaveEvery = 5.0f;    // seconds

// sh_falling.lua: a vertical hit faster than 100 u/s thuds, faster than 1500 u/s crashes; 2 s apart
constexpr float kLandSpeed = 100 * kUnit, kCrashSpeed = 1500 * kUnit, kLandGap = 2.0f;

std::string Status(const char *name, bool on, const char *yes = "enabled", const char *no = "disabled")
{
	return std::string(name) + " " + (on ? yes : no); // GMod TARDIS:StatusMessage
}

Stick ReadStick(const FrameInput &in, bool flight)
{
	using A = Action;
	Stick s;
	if (flight)
	{
		s.forward = in.Down(A::FlightForward); s.backward = in.Down(A::FlightBackward);
		s.left = in.Down(A::FlightLeft); s.right = in.Down(A::FlightRight);
		s.up = in.Down(A::FlightUp); s.down = in.Down(A::FlightDown);
		s.boost = in.Down(A::FlightBoost); s.rotate = in.Down(A::FlightRotate);
	}
	else
	{
		s.forward = in.Down(A::FloatForward); s.backward = in.Down(A::FloatBackward);
		s.left = in.Down(A::FloatLeft); s.right = in.Down(A::FloatRight);
		s.boost = in.Down(A::FloatBoost); s.rotate = in.Down(A::FloatRotate); s.brake = in.Down(A::FloatBrake);
	}
	return s;
}
}

Tardis::Tardis(World &world, Store &store, Audio &audio, TardisSettings settings)
	: world_(world), store_(store), audio_(audio), settings_(settings), flight_(settings.flight),
	  camera_(settings.mouseSensitivity, [&world](V3 a, V3 b) { return world.CastToWorld(a, b); })
{
}

void Tardis::Start()
{
	auto saved = store_.Load();
	if (!saved || !world_.SpawnExterior(saved->pose)) return;
	state_.handbrake = saved->handbrake;
	world_.SetExteriorPhysics(true, frozen_ = true); // thawed once the player is near
}

void Tardis::Update(const FrameInput &in, float dt)
{
	if (!piloting_ && in.Pressed(Action::Spawn)) ToggleSpawn();
	if (!world_.HasExterior())
	{
		piloting_ = false;
		if (settings_.legend) world_.DrawLegend(Legend());
		return;
	}

	Pose ext = world_.ExteriorPose();
	Pose player = world_.PlayerPose();
	float dist = (ext.pos - player.pos).Len();

	if (piloting_)
	{
		if (in.use) SetPiloting(false);
	}
	else if (dist < kBoardRange && !world_.PlayerInVehicle())
	{
		world_.Hint("Press ~INPUT_CONTEXT~ to pilot the TARDIS");
		if (in.use) SetPiloting(true);
	}

	if (piloting_)
	{
		world_.LockControls();
		world_.BlockPhone(); // nobody rings you inside the TARDIS
		PilotControls(in);
		world_.ShowCamera(camera_.Update(ext, in.lookX, in.lookY, in.wheel));
	}

	bool owns = FlightModel::Owns(state_);
	bool frozen = !owns && !piloting_ && dist > kParkRange;
	if (owns != owned_ || frozen != frozen_)
	{
		world_.SetExteriorPhysics(!owns, frozen);
		if (owned_ && !owns)
		{
			world_.SetExteriorAngularVelocity(angVel_); // keep spinning on momentum, like Source physics does
			angVel_ = {};
		}
		owned_ = owns;
		frozen_ = frozen;
	}
	if (owns)
	{
		Stick stick = ReadStick(in, state_.flight);
		Body b = flight_.Step(state_, {ext.rot, world_.ExteriorVelocity(), angVel_}, piloting_ ? &stick : nullptr, camera_.view(), dt);
		angVel_ = b.angVel;
		world_.DriveExterior(b.rot, b.vel);
	}

	Sounds(world_.ExteriorPose(), world_.ExteriorVelocity(), dt);
	if (settings_.legend) world_.DrawLegend(Legend()); // after this frame's keys, so it shows the new state
	if ((sinceSave_ += dt) >= kSaveEvery) Save();
}

void Tardis::Sounds(Pose ext, V3 vel, float dt)
{
	// flight loop: GMod pitch 95 + speed/250 (capped at +15), volume 0.75
	if (state_.flight)
	{
		float p = std::min(vel.Len() / kUnit / 250.0f, 15.0f);
		audio_.Loop(LoopSound::Flight, LoopState{ext.pos, vel, (95 + p) / 100.0f, 0.75f});
	}
	else
		audio_.Loop(LoopSound::Flight, std::nullopt);

	// landing: falling fast, then stopped by the ground while upright (not while flight/float hold it up)
	sinceLanding_ += dt;
	bool upright = ext.rot.Up().z > std::cos(Rad(30));
	bool hit = lastVelZ_ < -kLandSpeed && vel.z - lastVelZ_ > -lastVelZ_ * 0.5f;
	if (hit && upright && !FlightModel::Owns(state_) && sinceLanding_ > kLandGap)
	{
		audio_.Play(lastVelZ_ < -kCrashSpeed ? Sound::FlightFall : Sound::FlightLand, ext.pos);
		sinceLanding_ = 0;
	}
	lastVelZ_ = vel.z;
}

std::vector<LegendLine> Tardis::Legend() const
{
	auto key = [this](Action a) { return settings_.keyNames[(size_t)a]; };
	auto onOff = [](bool on) { return on ? "ON" : "OFF"; };
	using A = Action;
	if (!piloting_)
	{
		if (!world_.HasExterior()) return {{"", "TARDIS"}, {key(A::Spawn), "Spawn TARDIS"}};
		return {{"", "TARDIS"}, {key(A::Spawn), "Remove TARDIS"}, {"E", "Pilot (next to the TARDIS)"}};
	}

	int d = state_.SpinDir();
	std::vector<LegendLine> l = {
		{"", "TARDIS"},
		{"", std::string("Flight ") + onOff(state_.flight) + "   Anti-gravs " + onOff(state_.floating) +
		         "   Handbrake " + onOff(state_.handbrake)},
		{key(A::FlightToggle), "Flight"}, {key(A::FloatToggle), "Anti-gravs"}, {key(A::Handbrake), "Handbrake"},
		{"E", "Leave"}, {"Mouse", "Look"}, {"Wheel", "Zoom"},
	};
	if (state_.flight)
	{
		l.push_back({"", std::string("Flight - spin ") + (d < 0 ? "anti-clockwise" : d > 0 ? "clockwise" : "off")});
		l.push_back({key(A::FlightForward) + key(A::FlightLeft) + key(A::FlightBackward) + key(A::FlightRight), "Move"});
		l.push_back({key(A::FlightUp) + " / " + key(A::FlightDown), "Up / Down"});
		l.push_back({key(A::FlightBoost), "Boost"});
		l.push_back({key(A::FlightRotate) + " + " + key(A::FlightLeft) + "/" + key(A::FlightRight), "Rotate"});
		l.push_back({key(A::FlightSpinDir), "Spin direction"});
	}
	else if (state_.floating)
	{
		l.push_back({"", "Anti-gravs"});
		l.push_back({key(A::FloatForward) + key(A::FloatLeft) + key(A::FloatBackward) + key(A::FloatRight), "Tilt"});
		l.push_back({key(A::FloatRotate) + " + " + key(A::FloatLeft) + "/" + key(A::FloatRight), "Rotate"});
		l.push_back({key(A::FloatBoost), "Faster"});
		l.push_back({key(A::FloatBrake), "Brake"});
	}
	return l;
}

void Tardis::PilotControls(const FrameInput &in)
{
	if (in.Pressed(Action::FlightToggle))
		world_.Notify(state_.ToggleFlight() ? Status("Flight mode", state_.flight) : "~r~Failed to toggle flight mode");
	if (in.Pressed(Action::FloatToggle))
	{
		bool ok = state_.ToggleFloat() || state_.flight;
		world_.Notify(ok ? Status("Anti-gravs", state_.floatfirst) : "~r~Failed to toggle anti-gravs");
	}
	if (in.Pressed(Action::Handbrake))
	{
		state_.ToggleHandbrake();
		world_.Notify(Status("Time Rotor Handbrake", state_.handbrake, "engaged", "disengaged"));
	}
	if (state_.flight && in.Pressed(Action::FlightSpinDir))
	{
		state_.CycleSpinDir();
		int d = state_.SpinDir();
		world_.Notify(std::string("Spin direction set to ") + (d < 0 ? "anti-clockwise" : d > 0 ? "clockwise" : "none"));
	}
}

void Tardis::SetPiloting(bool on)
{
	if (on == piloting_) return;
	piloting_ = on;
	if (on)
	{
		world_.Board();
		camera_.Reset(world_.GameplayView());
		world_.Notify("You are now the pilot");
		return;
	}
	world_.ShowCamera(std::nullopt);
	if (world_.HasExterior())
	{
		Pose ext = world_.ExteriorPose();
		V3 exit = ext.pos + ext.rot.Forward() * settings_.exitOffset;
		exit.z = world_.GroundZ(exit + V3{0, 0, 2});
		world_.Unboard(exit);
	}
	world_.Notify("You are no longer the pilot");
}

void Tardis::ToggleSpawn()
{
	if (world_.HasExterior())
	{
		SetPiloting(false);
		audio_.Play(Sound::Delete, world_.ExteriorPose().pos);
		audio_.Loop(LoopSound::Flight, std::nullopt);
		world_.DespawnExterior();
		state_ = {};
		owned_ = frozen_ = false;
		angVel_ = {};
		world_.Notify("TARDIS removed");
	}
	else
	{
		Pose p = world_.PlayerPose();
		V3 at = p.pos + p.rot.Forward() * 4.0f;
		at.z = world_.GroundZ(at + V3{0, 0, 2});
		Quat facing = p.rot * Quat::Yaw(180); // door faces the player
		if (world_.SpawnExterior({at, facing}))
		{
			audio_.Play(Sound::Spawn, at);
			world_.Notify("TARDIS spawned");
		}
		else world_.Notify("~r~TARDIS model not found");
	}
	Save();
}

void Tardis::Save()
{
	sinceSave_ = 0;
	if (!world_.HasExterior()) { store_.Save(std::nullopt); return; }
	store_.Save(SavedTardis{world_.ExteriorPose(), state_.handbrake});
}
