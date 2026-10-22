#include "tardis.h"
#include <algorithm>
#include <cstdio>

namespace
{
constexpr float kBoardRange = 3.0f;   // metres; E this close to the exterior starts piloting
constexpr float kAirExit = 3.0f;     // metres above the ground; higher, the player steps out into the air
constexpr float kInterruptKick = 8.0f; // m/s up when the handbrake breaks off a demat (~3 m)
constexpr float kParkRange = 150.0f;  // beyond this the map collision under it isn't streamed
constexpr float kProbeZ = 1000.0f;    // metres; the destination's ground is searched from above the map's top
constexpr float kSafeGrace = 3.0f;    // seconds from H the walkable spot gets to stream in before the bare ground will do
constexpr float kSeekLimit = 15.0f;   // seconds from H; then it lands on the nearest road
constexpr float kClearRadius = 2.0f;  // metres cleared of peds and parked cars before it lands
constexpr float kNearTrip = 300.0f;   // metres; closer than this both places are streamed at once
constexpr float kFadeTime = 0.5f;     // seconds to black on a far trip
constexpr float kLoadLimit = 8.0f;    // seconds a far trip waits in black for the map before landing anyway
constexpr float kFlightVolume = 0.75f, kDuckedVolume = 0.2f, kDuckTime = 0.5f;
constexpr float kCloakSpeed = 255 / 3.0f; // alpha per second: it fades out, or back in, over 3 s

std::string Fmt(const char *fmt, V3 a, float x = 0) // a's xyz, then x
{
	char buf[160];
	snprintf(buf, sizeof buf, fmt, a.x, a.y, a.z, x);
	return buf;
}

// sh_falling.lua: a vertical hit faster than 100 u/s thuds, faster than 1500 u/s crashes; 2 s apart
constexpr float kLandSpeed = 100 * kUnit, kCrashSpeed = 1500 * kUnit, kLandGap = 2.0f;

float YawDeg(Quat q) { V3 f = q.Forward(); return std::atan2(-f.x, f.y) * 180 / kPi; }

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

Tardis::Tardis(World &world, Audio &audio, TardisSettings settings)
	: world_(world), audio_(audio), settings_(settings), flight_(settings.flight),
	  camera_(settings.mouseSensitivity, [&world](V3 a, V3 b) { return world.CastToWorld(a, b); })
{
}

void Tardis::Update(const FrameInput &in, float dt)
{
	if (!piloting_ && in.Pressed(Action::Spawn)) ToggleSpawn();
	if (!world_.HasExterior())
	{
		piloting_ = false;
		return;
	}

	Pose ext = world_.ExteriorPose();
	Pose player = world_.PlayerPose();
	float dist = (ext.pos - player.pos).Len();

	if (piloting_)
	{
		if (in.use && !tp_.Busy()) SetPiloting(false); // no stepping out mid-journey
	}
	else if (dist < kBoardRange && !world_.PlayerInVehicle())
	{
		world_.Hint(settings_.text("msg.hint"));
		if (in.use) SetPiloting(true);
	}

	if (piloting_)
	{
		world_.LockControls();
		world_.BlockPhone(); // nobody rings you inside the TARDIS
		world_.HidePlayer();
		if (state_.cloak) world_.HideFromPolice(); // the box is unseen; the player on foot is not
		PilotControls(in, dt);
		world_.ShowCamera(camera_.Update(ext, in.lookX, in.lookY, in.wheel));
	}

	if (tp_.Busy()) Travel(dt);
	if (secondBlast_ > 0 && (secondBlast_ -= dt) <= 0) world_.Explode(60);
	Cloak(dt);

	bool hidden = tp_.Hidden(); // between places: held still out of sight
	bool owns = FlightModel::Owns(state_) && !hidden && !state_.physlock; // GMod: flight under physlock goes nowhere
	bool frozen = hidden || state_.physlock || (!owns && !piloting_ && dist > kParkRange);
	if (owns != owned_ || frozen != frozen_)
	{
		world_.SetExteriorPhysics(!owns, frozen);
		if (owned_ && !owns)
		{
			world_.SetExteriorAngularVelocity(angVel_); // keep spinning on momentum, like Source physics does
			angVel_ = {};
		}
		owned_ = owns;
		sinceDriftLog_ = 0; // no expected rotation from before it took over
		frozen_ = frozen;
	}
	if (owns)
	{
		Stick stick = ReadStick(in, state_.flight);
		// SET_ENTITY_ANGULAR_VELOCITY's frame is assumed (world, rad/s): log how far the game strays from the model
		if ((sinceDriftLog_ += dt) >= 2)
		{
			float d = std::fabs(ext.rot.x * expectRot_.x + ext.rot.y * expectRot_.y + ext.rot.z * expectRot_.z + ext.rot.w * expectRot_.w);
			char line[96];
			snprintf(line, sizeof line, "flight: turned %.2f deg off the model's rotation this frame", 2 * std::acos(std::min(d, 1.0f)) * 180 / kPi);
			world_.Trace(line);
			sinceDriftLog_ = 0;
		}
		Body b = flight_.Step(state_, {ext.rot, world_.ExteriorVelocity(), angVel_}, piloting_ ? &stick : nullptr, camera_.view(), dt);
		angVel_ = b.angVel;
		expectRot_ = b.rot;
		world_.DriveExterior(b.angVel, b.vel);
	}

	if (state_.shield && !hidden) world_.Shield(!state_.cloak); // cloaked, it still reflects, without the flash

	// frozen alone stops cars; a train runs on through anything, so it is stopped by hand
	if (state_.physlock && world_.HoldTrains())
	{
		audio_.Play(Sound::FlightFall, ext.pos);
		world_.ShakeCamera();
		world_.Trace("physlock: a train ran into it and stopped");
	}

	// lamp: on while it flies or travels; it doesn't show once it has vanished or while cloaked
	clock_ += dt;
	bool travelling = tp_.Busy() && !hidden;
	world_.ShowLamp(Lamp(state_.floating && !hidden, travelling, state_.cloak, settings_.lampAlwaysOn && !hidden, clock_));

	Sounds(world_.ExteriorPose(), world_.ExteriorVelocity(), dt);
	if (piloting_ && settings_.panel) world_.DrawPanel(CurrentPanel()); // after this frame's keys, so it shows the new state
}

void Tardis::Cloak(float dt)
{
	float target = state_.cloak ? 0.0f : 255.0f, step = kCloakSpeed * dt;
	cloakAlpha_ = cloakAlpha_ < target ? std::min(target, cloakAlpha_ + step) : std::max(target, cloakAlpha_ - step);
	int a = (int)std::min(cloakAlpha_, tp_.alpha);
	if (a != shownAlpha_) world_.SetExteriorAlpha(shownAlpha_ = a);
}

void Tardis::Sounds(Pose ext, V3 vel, float dt)
{
	audio_.MoveAttached(ext.pos);
	// flight loop: GMod pitch 95 + speed/250 (capped at +15), volume 0.75; ducked while travelling
	float duckTarget = tp_.Busy() ? kDuckedVolume : kFlightVolume, step = (kFlightVolume - kDuckedVolume) / kDuckTime * dt;
	flightVolume_ = flightVolume_ < duckTarget ? std::min(duckTarget, flightVolume_ + step) : std::max(duckTarget, flightVolume_ - step);
	if (state_.flight && !tp_.Hidden() && !state_.cloak) // GMod ShouldTurnOffFlightSound
	{
		float p = std::min(vel.Len() / kUnit / 250.0f, 15.0f);
		audio_.Loop(LoopSound::Flight, LoopState{ext.pos, vel, (95 + p) / 100.0f, flightVolume_});
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

Panel Tardis::CurrentPanel() const
{
	const Text &t = settings_.text;
	auto key = [this](Action a) { return settings_.keyNames[(size_t)a]; };
	using A = Action;
	using K = PanelRow::Kind;
	auto light = [&](const char *label, A a, bool on) { return PanelRow{K::System, t(label), key(a), t(on ? "panel.on" : "panel.off"), on}; };

	Panel p{t("panel.title"), {
		light("panel.flight", A::FlightToggle, state_.flight),
		// in flight `floating` is always on (flight brings float); the pilot's own toggle is floatfirst, as the notification says
		light("panel.float", A::FloatToggle, state_.flight ? state_.floatfirst : state_.floating),
		light("panel.handbrake", A::Handbrake, state_.handbrake),
		light("panel.cloak", A::CloakToggle, state_.cloak),
		light("panel.physlock", A::PhyslockToggle, state_.physlock),
		light("panel.shield", A::ShieldToggle, state_.shield),
	}};
	PanelRow travel{K::Action, t("panel.travel"), key(A::TeleportDemat)};
	if (tp_.Busy())
	{
		auto ph = tp_.phase;
		travel.value = t(ph == Teleport::Phase::Demat ? "panel.demat" : ph == Teleport::Phase::Mat ? "panel.mat" : "panel.transit");
		travel.progress = tp_.Progress();
	}
	p.rows.push_back(travel);
	if (state_.flight)
	{
		char kmh[32];
		snprintf(kmh, sizeof kmh, "%+d km/h", state_.speedOffset);
		// "Num 8/Num 5" -> "Num 8/5": one short cap like the others
		std::string up = key(A::SpeedUp), down = key(A::SpeedDown);
		if (up.rfind("Num ", 0) == 0 && down.rfind("Num ", 0) == 0) down.erase(0, 4);
		p.rows.push_back({K::Reading, t("panel.speed"), up + "/" + down, kmh});
		p.rows.push_back({K::Reading, t("panel.spin"), key(A::FlightSpinDir), SpinName()});
	}
	p.rows.push_back({K::Footer, t("panel.leave"), "E"});
	return p;
}

std::string Tardis::SpinName() const
{
	int d = state_.SpinDir();
	return settings_.text(d < 0 ? "spin.anticlockwise" : d > 0 ? "spin.clockwise" : "spin.none");
}

void Tardis::Demat()
{
	if (tp_.Busy()) return; // CanDemat
	auto wp = world_.Waypoint();
	Pose ext = world_.ExteriorPose();
	if (!wp) { world_.Notify(settings_.text("msg.travel.no_waypoint")); return; }
	if (state_.handbrake) // ShouldFailDemat
	{
		audio_.Play(Sound::DematFail, ext.pos);
		world_.Notify(settings_.text("msg.travel.handbrake"));
		return;
	}
	dest_ = *wp;
	spot_.reset();
	travelT_ = seekT_ = vanishedT_ = landingT_ = 0;
	float dist = V3{dest_.x - ext.pos.x, dest_.y - ext.pos.y, 0}.Len();
	far_ = dist > kNearTrip;
	world_.Preload(dest_); // navmesh and collision only: the map here stays while it fades out
	world_.Trace(Fmt(far_ ? "travel: waypoint %.2f %.2f %.2f (t=0.0 s, far: %.0f m)" : "travel: waypoint %.2f %.2f %.2f (t=0.0 s, near: %.0f m)", dest_, dist));
	tp_.Start();
	if (!state_.cloak) audio_.Play(Sound::Demat, ext.pos, true); // GMod ShouldPlayDematSound
	world_.Notify(settings_.text("msg.travel.demat"));
}

void Tardis::Travel(float dt)
{
	travelT_ += dt;
	char line[160];
	switch (tp_.Step(dt))
	{
	case Teleport::Event::Vanished: // stays put, invisible and frozen, until the spot is ready
		vanishedT_ = travelT_;
		snprintf(line, sizeof line, "travel: vanished (t=%.1f s)", travelT_);
		world_.Trace(line);
		if (far_) // black out, then stream the destination in place of this map
		{
			world_.FadeScreen(true);
			world_.FocusOn(dest_);
			seekT_ = 0;
		}
		break;
	case Teleport::Event::Landing: // physics back on, fading in
		landingT_ = travelT_;
		world_.Notify(settings_.text("msg.travel.mat"));
		break;
	case Teleport::Event::Done:
		snprintf(line, sizeof line, "travel: arrived (t=%.1f s)", travelT_);
		world_.Trace(line);
		world_.Notify(settings_.text("msg.travel.arrived"));
		break;
	default: break;
	}
	// a second into the fade-in, where physics has settled it (or the pilot has flown it off)
	if (landingT_ > 0 && travelT_ - landingT_ >= 1)
	{
		V3 at = world_.ExteriorPose().pos;
		snprintf(line, sizeof line, "travel: landed at %.2f %.2f %.2f (t=%.1f s, %.2f m from spot)", at.x, at.y, at.z, travelT_, (at - *spot_).Len());
		world_.Trace(line);
		landingT_ = 0;
	}
	// a near trip looks from H on; a far one only once the destination is being streamed in
	bool seeking = tp_.phase == Teleport::Phase::Seek || (tp_.phase == Teleport::Phase::Demat && !far_);
	if (seeking) seekT_ += dt;
	if (!spot_ && seeking)
	{
		// the nearest safe walkable spot, else the bare ground under the waypoint, else the nearest road.
		// The waypoint's own z is never trusted: it can be under the terrain.
		float z = world_.GroundZ({dest_.x, dest_.y, kProbeZ});
		bool ground = z != kProbeZ;
		const char *via = nullptr;
		if ((spot_ = world_.SafeSpot({dest_.x, dest_.y, ground ? z : dest_.z}))) via = "safe-coord";
		else if (ground && seekT_ > kSafeGrace) spot_ = V3{dest_.x, dest_.y, z}, via = "ground";
		else if (seekT_ > kSeekLimit) spot_ = world_.RoadNode(dest_), via = "road-node";
		if (spot_)
		{
			// every source answers a little differently (a ped's root is ~1 m up): settle it onto the ground below
			float gz = world_.GroundZ({spot_->x, spot_->y, spot_->z + 2});
			if (gz != spot_->z + 2) spot_->z = gz;
			snprintf(line, sizeof line, "travel: landing spot via %s %.2f %.2f %.2f (t=%.1f s, %.0f m from waypoint)", via,
				spot_->x, spot_->y, spot_->z, travelT_, V3{spot_->x - dest_.x, spot_->y - dest_.y, 0}.Len());
			world_.Trace(line);
		}
	}
	// a far trip also waits for the black screen and, for a few seconds at most, the map around it
	bool ready = !far_ || (seekT_ >= kFadeTime && (world_.FocusLoaded() || seekT_ > kLoadLimit));
	if (spot_ && ready && tp_.phase == Teleport::Phase::Seek)
	{
		snprintf(line, sizeof line, "travel: placed at spot (t=%.1f s, waited %.1f s invisible%s)", travelT_, travelT_ - vanishedT_,
			!far_ ? "" : world_.FocusLoaded() ? ", map loaded" : ", map NOT loaded");
		world_.Trace(line);
		world_.Preload(std::nullopt);
		world_.ClearArea(*spot_, kClearRadius);
		world_.PlaceExterior({*spot_, Quat::Yaw(YawDeg(world_.ExteriorPose().rot))});
		if (far_)
		{
			world_.FocusOn(std::nullopt); // back on the player, who is over there now
			world_.FadeScreen(false);
		}
		if (!state_.cloak) audio_.Play(Sound::Mat, *spot_, true);
		world_.ClearWanted(); // it kept the wanted level on the way; here, materialising, it is gone
		tp_.Arrive();
	}
}

// sh_physlock.lua ExplodeIfFast: a shake over 50 u/s, a bang over 1600 u/s (~110 km/h), harmful only over 2500
void Tardis::ExplodeIfFast()
{
	float v = world_.ExteriorVelocity().Len() / kUnit;
	if (v > 50) world_.ShakeCamera();
	if (v > 1600) world_.Explode(std::max((v - 2500) / 5, 0.0f));
}

// sh_tp_interrupt.lua InterruptTeleport: the journey's sounds cut to the interrupt one, it is back at once where it
// is, blows up twice a second apart, and drops with flight and float off (GMod also cuts the power, which this port
// doesn't have). Unlike GMod, dematerialising it blows up too and is thrown up (the user's call)
void Tardis::InterruptTravel()
{
	Pose ext = world_.ExteriorPose();
	audio_.StopAttached();
	audio_.Play(Sound::Interrupt, ext.pos, true);
	world_.Preload(std::nullopt);
	if (far_ && tp_.phase == Teleport::Phase::Seek) // still blacked out on the destination
	{
		world_.FocusOn(std::nullopt);
		world_.FadeScreen(false);
	}
	spot_.reset();
	landingT_ = 0;
	world_.Trace("travel: interrupted by the handbrake");
	if (tp_.Interrupt()) world_.DriveExterior({}, world_.ExteriorVelocity() + V3{0, 0, kInterruptKick});
	world_.Explode(60);
	world_.ShakeCamera();
	secondBlast_ = 1;
	state_.floatfirst = false;
	state_.SetFlight(false);
	state_.SetFloat(false);
}

void Tardis::PilotControls(const FrameInput &in, float dt)
{
	const Text &t = settings_.text;
	if (in.Pressed(Action::TeleportDemat)) Demat();
	if (in.Pressed(Action::FlightToggle))
	{
		bool ok = state_.ToggleFlight();
		world_.Notify(!ok ? t("msg.flight.fail") : t(state_.flight ? "msg.flight.on" : "msg.flight.off"));
		if (ok && state_.flight && state_.physlock) world_.Notify(t("msg.flight.physlock"));
	}
	if (in.Pressed(Action::FloatToggle))
	{
		bool ok = state_.ToggleFloat() || state_.flight;
		world_.Notify(!ok ? t("msg.float.fail") : t(state_.floatfirst ? "msg.float.on" : "msg.float.off"));
	}
	if (in.Pressed(Action::Handbrake))
	{
		bool flying = state_.flight;
		state_.ToggleHandbrake();
		world_.Notify(t(state_.handbrake ? "msg.handbrake.on" : "msg.handbrake.off"));
		// GMod HandbrakeToggled "vortex": a journey is interrupted, else flight is (InterruptFlight -> ExplodeIfFast)
		if (state_.handbrake && tp_.Busy()) InterruptTravel();
		else if (state_.handbrake && flying) ExplodeIfFast();
	}
	if (in.Pressed(Action::CloakToggle))
	{
		state_.cloak = !state_.cloak;
		audio_.Play(state_.cloak ? Sound::CloakOn : Sound::CloakOff, world_.ExteriorPose().pos, true);
		world_.Notify(t(state_.cloak ? "msg.cloak.on" : "msg.cloak.off"));
	}
	if (in.Pressed(Action::ShieldToggle))
	{
		state_.shield = !state_.shield;
		audio_.Play(state_.shield ? Sound::ShieldOn : Sound::ShieldOff, world_.ExteriorPose().pos, true);
		if (!state_.cloak) world_.FlashShield();
		world_.Notify(t(state_.shield ? "msg.shield.on" : "msg.shield.off"));
	}
	if (in.Pressed(Action::PhyslockToggle))
	{
		state_.physlock = !state_.physlock;
		world_.Notify(t(state_.physlock ? "msg.physlock.on" : "msg.physlock.off"));
	}
	if (state_.flight && in.Pressed(Action::FlightSpinDir))
	{
		state_.CycleSpinDir();
		world_.Notify(t("msg.spin", SpinName()));
	}

	// flight speed: Num 8 / Num 5, tapped or held; Num 0 back to GMod's
	int dir = state_.flight ? in.Down(Action::SpeedUp) - in.Down(Action::SpeedDown) : 0;
	bool pressed = dir > 0 ? in.Pressed(Action::SpeedUp) : in.Pressed(Action::SpeedDown);
	state_.speedOffset = std::max(FlightModel::kMinSpeedOffset, state_.speedOffset + dial_.Step(dir, pressed, dt));
	if (state_.flight && in.Pressed(Action::SpeedReset)) state_.speedOffset = 0;
}

void Tardis::SetPiloting(bool on)
{
	if (on == piloting_) return;
	piloting_ = on;
	if (on)
	{
		world_.Board();
		camera_.Reset(world_.GameplayView());
		world_.Notify(settings_.text("msg.pilot.on"));
		return;
	}
	world_.ShowCamera(std::nullopt);
	if (world_.HasExterior())
	{
		Pose ext = world_.ExteriorPose();
		V3 exit = ext.pos + ext.rot.Forward() * settings_.exitOffset;
		float ground = world_.GroundZ(exit + V3{0, 0, 2});
		bool air = exit.z - ground > kAirExit; // flying: out at its height, with a parachute
		if (!air) exit.z = ground;
		world_.Unboard(exit, air);
	}
	world_.Notify(settings_.text("msg.pilot.off"));
}

void Tardis::ToggleSpawn()
{
	if (world_.HasExterior())
	{
		SetPiloting(false);
		audio_.Play(Sound::Delete, world_.ExteriorPose().pos);
		audio_.Loop(LoopSound::Flight, std::nullopt);
		world_.DespawnExterior();
		state_ = {}; // cloak, physlock and speed included
		owned_ = frozen_ = false;
		angVel_ = {};
		cloakAlpha_ = 255;
		shownAlpha_ = 255;
		dial_ = {};
		world_.Notify(settings_.text("msg.removed"));
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
			world_.Notify(settings_.text("msg.spawned"));
		}
		else world_.Notify(settings_.text("msg.model_missing"));
	}
}
