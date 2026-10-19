// Domain and application tests, no GTA needed. Build and run: tests\run_tests.bat
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "../src/app/tardis.h"

static bool Near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static const std::string kRepo = std::string(__FILE__).substr(0, std::string(__FILE__).find_last_of("\\/") + 1) + "../";

static Text LoadLang(const char *lang)
{
	Text t;
	std::ifstream f(kRepo + "translations/" + lang + ".ini");
	assert(f);
	t.Add(f);
	return t;
}

// what the player gets with the game in English
static TardisSettings En()
{
	TardisSettings s;
	for (size_t i = 0; i < s.keyNames.size(); i++) s.keyNames[i] = kActions[i].key;
	s.text = LoadLang("en-US");
	return s;
}

// ---------------------------------------------------------------- domain: GMod control rules

static void RulesTests()
{
	{ // flight brings float with it and takes it away again
		TardisState s;
		assert(s.ToggleFlight() && s.flight && s.floating);
		assert(s.ToggleFlight() && !s.flight && !s.floating);
	}
	{ // float asked for first survives flight turning off
		TardisState s;
		assert(s.ToggleFloat() && s.floating && s.floatfirst);
		s.ToggleFlight();
		s.ToggleFlight();
		assert(!s.flight && s.floating);
	}
	{ // T during flight flips floatfirst, so flight off then drops float
		TardisState s;
		s.ToggleFlight();
		s.ToggleFloat();
		s.ToggleFloat();
		s.ToggleFlight();
		assert(!s.floating);
	}
	{ // handbrake blocks flight, and engaging it mid-flight drops to float
		TardisState s;
		s.ToggleFlight();
		s.ToggleHandbrake();
		assert(!s.flight && s.floating && s.handbrake);
		assert(!s.ToggleFlight() && !s.flight);
	}
	{ // spin cycle: clockwise, none, anti-clockwise, none
		TardisState s;
		s.CycleSpinDir(); assert(s.SpinDir() == 1);
		s.CycleSpinDir(); assert(s.SpinDir() == 0);
		s.CycleSpinDir(); assert(s.SpinDir() == -1);
		s.CycleSpinDir(); assert(s.SpinDir() == 0);
	}
}

// ---------------------------------------------------------------- domain: flight model

static float CruiseSpeed(bool boost, bool spin)
{
	FlightModel m({2.5f, 40.0f});
	TardisState s;
	s.ToggleFlight();
	if (!spin) { s.CycleSpinDir(); s.CycleSpinDir(); }
	Stick st;
	st.forward = true;
	st.boost = boost;
	Body b;
	for (int i = 0; i < 60 * 30; i++) b = m.Step(s, b, &st, View{0, 0}, 1 / 60.0f);
	return b.vel.Len();
}

static void FlightTests()
{
	// GMod: 15 u/s per tick against 1% brake per tick -> ~1493 u/s = 37.9 m/s
	assert(Near(CruiseSpeed(false, true), 37.9f, 1.0f));
	assert(Near(CruiseSpeed(true, true), 37.9f * 2.5f, 2.5f));        // boost-speed 2.5 while spinning
	assert(Near(CruiseSpeed(true, false), 37.9f * 1.5f, 1.5f));       // max(1, 2.5 * 0.6) without spin

	FlightModel m({2.5f, 40.0f});
	{ // nobody steering: it coasts down
		TardisState s;
		s.ToggleFlight();
		Body b;
		b.vel = {0, 30, 0};
		for (int i = 0; i < 60 * 5; i++) b = m.Step(s, b, nullptr, {}, 1 / 60.0f);
		assert(b.vel.Len() < 30 * 0.05f);
	}
	{ // flying forward spins it (anti-clockwise by default = positive yaw) and stays upright-ish
		TardisState s;
		s.ToggleFlight();
		Stick st;
		st.forward = true;
		Body b;
		for (int i = 0; i < 60 * 10; i++) b = m.Step(s, b, &st, View{0, 0}, 1 / 60.0f);
		assert(b.angVel.z > 0);
		assert(b.rot.Up().z > std::cos(Rad(30)));
	}
	{ // float: brake bleeds the tumble
		TardisState s;
		s.ToggleFloat();
		Stick st;
		st.brake = true;
		Body b;
		b.angVel = {1, 0, 0};
		for (int i = 0; i < 60; i++) b = m.Step(s, b, &st, {}, 1 / 60.0f);
		assert(b.angVel.Len() < 0.05f);
	}
	{ // neither mode: the model leaves the body to the game
		TardisState s;
		Body b;
		b.vel = {1, 2, 3};
		Body out = m.Step(s, b, nullptr, {}, 1);
		assert(out.vel.x == 1 && out.vel.z == 3);
	}
}

// ---------------------------------------------------------------- domain: demat / mat

static void TeleportTests()
{
	const float dt = 1 / 60.0f;
	Teleport tp;
	tp.Start();
	for (int i = 0; i < 60 * 2; i++) assert(tp.Step(dt) == Teleport::Event::None);
	assert(tp.alpha == 255); // 2.5 s before the first fade
	float t = 2;
	Teleport::Event e = Teleport::Event::None;
	while (e == Teleport::Event::None) { e = tp.Step(dt); t += dt; }
	// 455 alpha of fading at 0.77 per tick, after the 2.5 s delay: ~11.5 s, as in GMod
	assert(e == Teleport::Event::Vanished && tp.alpha == 0 && tp.Hidden() && Near(t, 2.5f + 455 / Teleport::kSpeed, 0.1f));
	for (int i = 0; i < 600; i++) assert(tp.Step(dt) == Teleport::Event::None); // seeking waits for Arrive
	tp.Arrive();
	t = 0;
	while ((e = tp.Step(dt)) == Teleport::Event::None) t += dt;
	assert(e == Teleport::Event::Landing && Near(t, 8.5f, 0.05f) && !tp.Hidden());
	while ((e = tp.Step(dt)) == Teleport::Event::None) {}
	assert(e == Teleport::Event::Done && tp.alpha == 255 && !tp.Busy());
}

// ---------------------------------------------------------------- domain: pilot camera

static void CameraTests()
{
	Pose ext{{0, 0, 0}, {}};
	{ // 210 u behind and 60 u up by default, zoom clamps at 500 u
		PilotCamera c(8, [](V3, V3 to) { return to; });
		c.Reset({0, 0});
		CameraShot s = c.Update(ext, 0, 0, 0);
		assert(Near(s.pos.y, -210 * kUnit, 1e-3f) && Near(s.pos.z, 60 * kUnit, 1e-3f));
		for (int i = 0; i < 50; i++) s = c.Update(ext, 0, 0, -1);
		assert(Near(-s.pos.y, 500 * kUnit, 1e-3f));
	}
	{ // a wall pulls the camera in front of it
		PilotCamera c(8, [](V3 from, V3 to) { return from + (to - from) * 0.5f; });
		c.Reset({0, 0});
		CameraShot s = c.Update(ext, 0, 0, 0);
		assert(Near(s.pos.y, -105 * kUnit + 10 * kUnit, 1e-3f));
	}
}

// ---------------------------------------------------------------- application: the TARDIS through fake ports

struct FakeWorld : World
{
	bool modelOk = true, has = false, gravity = true, frozen = false, boarded = false, inVehicle = false;
	Pose ext, player{{0, 0, 0}, {}};
	V3 vel, unboardedAt, spin;
	int alpha = 255;
	std::optional<V3> waypoint;
	bool groundStreamed = true;
	float ground = 0;
	std::optional<V3> safeSpot;
	V3 road, cleared;
	std::vector<std::string> traces;
	bool cameraOn = false, locked = false, phoneBlocked = false;
	std::vector<std::string> notes;
	std::string hint;
	std::optional<Panel> panel;
	int policeHidden = 0;     // frames HideFromPolice was called
	bool trainHits = false;   // HoldTrains reports a hit once
	int trainsHeld = 0, shakes = 0;

	bool SpawnExterior(Pose at) override { if (!modelOk) return false; has = true; ext = at; alpha = 255; return true; }
	void DespawnExterior() override { has = false; }
	bool HasExterior() override { return has; }
	Pose ExteriorPose() override { return ext; }
	V3 ExteriorVelocity() override { return vel; }
	void DriveExterior(V3 w, V3 v) override { ext.rot = ext.rot.Integrate(w, 1 / 60.0f); vel = v; spin = w; } // tests step at 60 Hz
	void SetExteriorPhysics(bool g, bool f) override { gravity = g; frozen = f; }
	void SetExteriorAngularVelocity(V3 w) override { spin = w; }
	void PlaceExterior(Pose at) override { ext = at; vel = {}; }
	void SetExteriorAlpha(int a) override { alpha = a; }
	Pose PlayerPose() override { return player; }
	bool PlayerInVehicle() override { return inVehicle; }
	void Board() override { boarded = true; }
	void Unboard(V3 at) override { boarded = false; unboardedAt = at; }
	float GroundZ(V3 p) override { return groundStreamed ? ground : p.z; }
	std::optional<V3> SafeSpot(V3) override { return safeSpot; }
	V3 RoadNode(V3) override { return road; }
	void ClearArea(V3 at, float) override { cleared = at; }
	std::optional<V3> focus, preload;
	bool sceneLoaded = true, black = false, hidden = false;
	void Preload(std::optional<V3> at) override { preload = at; }
	void FocusOn(std::optional<V3> at) override { focus = at; }
	bool FocusLoaded() override { return sceneLoaded; }
	void FadeScreen(bool out) override { black = out; }
	void HidePlayer() override { hidden = true; }
	void Trace(const std::string &t) override { traces.push_back(t); }
	std::optional<V3> Waypoint() override { return waypoint; }
	V3 CastToWorld(V3, V3 to) override { return to; }
	View GameplayView() override { return {}; }
	void ShowCamera(std::optional<CameraShot> s) override { cameraOn = s.has_value(); }
	void LockControls() override { locked = true; }
	void BlockPhone() override { phoneBlocked = true; }
	void Hint(const std::string &t) override { hint = t; }
	void DrawPanel(const Panel &p) override { panel = p; }
	void HideFromPolice() override { policeHidden++; }
	bool HoldTrains() override { trainsHeld++; bool h = trainHits; trainHits = false; return h; }
	void ShakeCamera() override { shakes++; }
	int shieldFrames = 0, shieldFlashes = 0;
	bool shieldVisible = false;
	void Shield(bool visible) override { shieldFrames++; shieldVisible = visible; }
	void FlashShield() override { shieldFlashes++; }
	LampLight lamp;
	bool wash = false;
	void ShowLamp(LampLight l) override { lamp = l; }
	void Rotorwash(bool on) override { wash = on; }
	void Notify(const std::string &t) override { notes.push_back(t); }
};

struct FakeAudio : Audio
{
	std::vector<Sound> played;
	std::optional<LoopState> flight;
	V3 attachedAt;
	std::vector<bool> attached;
	void Play(Sound s, V3, bool a) override { played.push_back(s); attached.push_back(a); }
	void MoveAttached(V3 at) override { attachedAt = at; }
	void Loop(LoopSound, std::optional<LoopState> st) override { flight = st; }
};

static FrameInput Press(Action a) { FrameInput in; in.pressed[(size_t)a] = in.down[(size_t)a] = true; return in; }
static FrameInput Use() { FrameInput in; in.use = true; return in; }

// spawn a TARDIS 4 m ahead, then walk up to it
static void SpawnAndWalkUp(Tardis &t, FakeWorld &w)
{
	t.Update(Press(Action::Spawn), 1 / 60.0f);
	w.player.pos = {0, 2, 0};
}

static void TardisTests()
{
	const float dt = 1 / 60.0f;
	{ // F5 spawns 4 m ahead with the door facing the player, F5 again removes it
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		t.Update(Press(Action::Spawn), dt);
		assert(w.has && Near(w.ext.pos.y, 4, 1e-4f));
		assert(Near(w.ext.rot.Forward().y, -1, 1e-4f));
		assert(w.notes.back() == "TARDIS spawned");
		t.Update(Press(Action::Spawn), dt);
		assert(!w.has && w.notes.back() == "TARDIS removed");
	}
	{ // missing model: no exterior, the player is told
		FakeWorld w; FakeAudio au;
		w.modelOk = false;
		Tardis t(w, au, En());
		t.Update(Press(Action::Spawn), dt);
		assert(!w.has && w.notes.back().find("not found") != std::string::npos);
	}
	{ // E nearby boards and pilots; E again steps out through the door
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update({}, dt);
		assert(w.hint.find("pilot") != std::string::npos);
		t.Update(Use(), dt);
		assert(t.piloting() && w.boarded && w.cameraOn && w.locked && w.phoneBlocked);
		t.Update(Use(), dt);
		assert(!t.piloting() && !w.boarded && !w.cameraOn);
		assert(Near(w.unboardedAt.y, 4 - 1.4f, 1e-3f)); // in front of the door, towards the player
	}
	{ // too far or in a car: no boarding
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		w.inVehicle = true;
		t.Update(Use(), dt);
		assert(!t.piloting());
	}
	{ // R flies (gravity off), J mid-flight drops to anti-gravs, R is then refused
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update(Use(), dt);
		t.Update(Press(Action::FlightToggle), dt);
		assert(t.state().flight && !w.gravity && w.notes.back() == "Flight mode enabled");
		t.Update(Press(Action::Handbrake), dt);
		assert(!t.state().flight && t.state().floating && !w.gravity);
		t.Update(Press(Action::FlightToggle), dt);
		assert(!t.state().flight && w.notes.back() == "~r~Failed to toggle flight mode");
	}
	{ // R off mid-spin hands the spin to game physics instead of stopping it dead
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update(Use(), dt);
		t.Update(Press(Action::FlightToggle), dt);
		FrameInput fwd;
		fwd.down[(size_t)Action::FlightForward] = true;
		for (int i = 0; i < 60 * 5; i++) t.Update(fwd, dt);
		t.Update(Press(Action::FlightToggle), dt);
		assert(!t.state().flight && w.gravity && w.spin.z > 0.1f);
	}
	{ // flight keys do nothing unless piloting
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		t.Update(Press(Action::Spawn), dt);
		t.Update(Press(Action::FlightToggle), dt);
		assert(!t.state().flight);
	}
	{ // parked far away it freezes
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		t.Update(Press(Action::Spawn), dt);
		w.player.pos = {0, 500, 0};
		t.Update({}, dt);
		assert(w.frozen);
	}
}

static void TravelTests()
{
	const float dt = 1 / 60.0f;
	auto pilot = [](Tardis &t, FakeWorld &w) { SpawnAndWalkUp(t, w); t.Update(Use(), 1 / 60.0f); };
	{ // H without a waypoint asks for one
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update(Press(Action::TeleportDemat), dt);
		assert(!t.teleport().Busy() && w.notes.back().find("waypoint") != std::string::npos);
	}
	{ // the handbrake makes it fail, with the fail sound
		FakeWorld w; FakeAudio au;
		w.waypoint = V3{500, 600, 0};
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update(Press(Action::Handbrake), dt);
		t.Update(Press(Action::TeleportDemat), dt);
		assert(!t.teleport().Busy() && au.played.back() == Sound::DematFail);
	}
	{ // near: H fades it out where it is, waits there invisible for a safe spot, lands there, and fades back in
		FakeWorld w; FakeAudio au;
		w.waypoint = V3{50, 60, 0};
		w.groundStreamed = false;
		w.ground = 12;
		Tardis t(w, au, En());
		pilot(t, w);
		assert(w.hidden); // the ped is hidden every frame while piloting
		V3 origin = w.ext.pos;
		t.Update(Press(Action::TeleportDemat), dt);
		assert(t.teleport().Busy() && au.played.back() == Sound::Demat && au.attached.back());
		assert(w.preload && Near(w.preload->x, 50, 1e-3f) && !w.focus && !w.black); // near: the map here stays
		t.Update(Use(), dt);
		assert(t.piloting()); // no stepping out mid-journey
		while (!t.teleport().Hidden()) t.Update({}, dt);
		assert(w.alpha == 0 && w.frozen && Near((w.ext.pos - origin).Len(), 0, 1e-3f)); // never sent up into the sky
		for (int i = 0; i < 60; i++) t.Update({}, dt);
		assert(t.teleport().phase == Teleport::Phase::Seek);
		w.safeSpot = V3{55, 62, 12};
		t.Update({}, dt);
		assert(Near(w.ext.pos.x, 55, 1e-3f) && Near(w.ext.pos.z, 12, 1e-3f) && Near(w.cleared.x, 55, 1e-3f) && !w.preload);
		assert(au.played.back() == Sound::Mat && au.attached.back() && w.frozen);
		while (t.teleport().Hidden()) t.Update({}, dt);
		assert(!w.frozen);
		while (t.teleport().Busy()) t.Update({}, dt);
		assert(w.alpha == 255 && Near(w.ext.pos.y, 62, 1e-3f));
		const char *order[] = {"waypoint", "vanished", "safe-coord", "placed", "landed", "arrived"};
		assert(w.traces.size() == 6);
		for (int i = 0; i < 6; i++) assert(w.traces[i].find(order[i]) != std::string::npos);
		t.Update(Use(), dt);
		assert(!t.piloting());
	}
	{ // the spot is ready before it has faded out: it lands the moment it vanishes, settled onto the ground
		FakeWorld w; FakeAudio au;
		w.waypoint = V3{50, 60, 0};
		Tardis t(w, au, En());
		pilot(t, w);
		w.ground = 7;
		w.safeSpot = V3{51, 61, 8}; // a ped's root, ~1 m above the ground
		t.Update(Press(Action::TeleportDemat), dt);
		assert(w.traces.back().find("safe-coord") != std::string::npos && w.ext.pos.z < 7); // found, not moved yet
		while (t.teleport().phase == Teleport::Phase::Demat) t.Update({}, dt);
		assert(t.teleport().phase == Teleport::Phase::Premat && Near(w.ext.pos.z, 7, 1e-3f));
		assert(w.traces.back().find("waited 0.0 s") != std::string::npos);
	}
	{ // no walkable spot: the bare ground under the waypoint, once the spot had 3 s from H to stream in
		FakeWorld w; FakeAudio au;
		w.waypoint = V3{50, 60, 20};
		w.groundStreamed = false;
		w.ground = 31;
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update(Press(Action::TeleportDemat), dt);
		w.groundStreamed = true;
		for (int i = 0; i < 60 * 2; i++) t.Update({}, dt);
		assert(w.traces.back().find("waypoint") != std::string::npos); // still waiting for a walkable spot
		for (int i = 0; i < 60 * 2; i++) t.Update({}, dt);
		assert(w.traces.back().find("ground") != std::string::npos);
		while (!t.teleport().Hidden()) t.Update({}, dt);
		assert(t.teleport().phase == Teleport::Phase::Premat && Near(w.ext.pos.z, 31, 1e-3f));
	}
	{ // nothing streamed 15 s after H: the nearest road, never the waypoint's own z
		FakeWorld w; FakeAudio au;
		w.waypoint = V3{50, 60, 20};
		w.groundStreamed = false;
		w.road = {48, 59, 33};
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update(Press(Action::TeleportDemat), dt);
		for (int i = 0; i < 60 * 14; i++) t.Update({}, dt);
		assert(t.teleport().phase == Teleport::Phase::Seek);
		for (int i = 0; i < 60 * 2; i++) t.Update({}, dt);
		assert(t.teleport().phase == Teleport::Phase::Premat && Near(w.ext.pos.z, 33, 1e-3f));
		assert(w.traces[w.traces.size() - 2].find("road-node") != std::string::npos);
	}
	{ // far: the map here stays through the fade-out; then black, the destination streams in, and it lands there
		FakeWorld w; FakeAudio au;
		w.waypoint = V3{500, 600, 0};
		w.groundStreamed = false;
		w.sceneLoaded = false;
		w.safeSpot = V3{505, 602, 12};
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update(Press(Action::TeleportDemat), dt);
		while (t.teleport().phase == Teleport::Phase::Demat)
		{
			assert(!w.focus && !w.black && w.traces.back().find("far") != std::string::npos); // not even looking yet
			t.Update({}, dt);
		}
		assert(w.black && w.focus && Near(w.focus->x, 500, 1e-3f));
		for (int i = 0; i < 60; i++) t.Update({}, dt);
		assert(t.teleport().phase == Teleport::Phase::Seek && w.black); // spot found, the map isn't in yet
		w.sceneLoaded = true;
		t.Update({}, dt);
		assert(t.teleport().phase == Teleport::Phase::Premat && Near(w.ext.pos.x, 505, 1e-3f) && !w.black && !w.focus);
		assert(w.traces.back().find("map loaded") != std::string::npos);
	}
}

static void SoundTests()
{
	const float dt = 1 / 60.0f;
	{ // F5 plays the spawn and delete sounds
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		t.Update(Press(Action::Spawn), dt);
		t.Update(Press(Action::Spawn), dt);
		assert(au.played.size() == 2 && au.played[0] == Sound::Spawn && au.played[1] == Sound::Delete);
	}
	{ // flight loop only while flying, pitch 0.95 at rest rising with speed up to 1.10
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update(Use(), dt);
		t.Update({}, dt);
		assert(!au.flight);
		t.Update(Press(Action::FlightToggle), dt);
		assert(au.flight && Near(au.flight->pitch, 0.95f, 0.01f) && Near(au.flight->volume, 0.75f, 1e-4f));
		w.vel = {0, 1000, 0};
		t.Update({}, dt);
		assert(Near(au.flight->pitch, 1.10f, 0.01f));
		t.Update(Press(Action::FlightToggle), dt);
		assert(!au.flight);
	}
	{ // flying into a journey: the loop ducks to 0.2 over 0.5 s under demat/mat, and is back at 0.75 on arrival
		FakeWorld w; FakeAudio au;
		w.waypoint = V3{500, 600, 0};
		w.safeSpot = V3{500, 600, 0};
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update(Use(), dt);
		t.Update(Press(Action::FlightToggle), dt);
		t.Update(Press(Action::TeleportDemat), dt);
		for (int i = 0; i < 15; i++) t.Update({}, dt);
		assert(au.flight->volume < 0.75f && au.flight->volume > 0.2f); // ramping
		for (int i = 0; i < 30; i++) t.Update({}, dt);
		assert(Near(au.flight->volume, 0.2f, 1e-4f));
		while (t.teleport().phase != Teleport::Phase::Mat) t.Update({}, dt);
		t.Update({}, dt);
		assert(au.flight && Near(au.flight->volume, 0.2f, 1e-4f)); // still under the mat sound
		while (t.teleport().Busy()) t.Update({}, dt);
		for (int i = 0; i < 31; i++) t.Update({}, dt);
		assert(Near(au.flight->volume, 0.75f, 1e-4f));
	}
	{ // falling onto the ground: thud when slow, crash when fast, not twice within 2 s
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		t.Update(Press(Action::Spawn), dt);
		au.played.clear();
		w.vel = {0, 0, -10}; t.Update({}, dt);
		w.vel = {0, 0, 0};   t.Update({}, dt);
		assert(au.played.size() == 1 && au.played[0] == Sound::FlightLand);
		w.vel = {0, 0, -10}; t.Update({}, dt);
		w.vel = {0, 0, 0};   t.Update({}, dt);
		assert(au.played.size() == 1);
		for (int i = 0; i < 130; i++) t.Update({}, dt);
		w.vel = {0, 0, -50}; t.Update({}, dt);
		w.vel = {0, 0, 0};   t.Update({}, dt);
		assert(au.played.back() == Sound::FlightFall);
	}
}

static const PanelRow *Row(const std::optional<Panel> &p, const std::string &label)
{
	if (p) for (auto &r : p->rows) if (r.label == label) return &r;
	return nullptr;
}

static void PanelTests()
{
	const float dt = 1 / 60.0f;
	TardisSettings set = En();
	set.keyNames[(size_t)Action::SpeedUp] = "Num 8";
	set.keyNames[(size_t)Action::SpeedDown] = "Num 5";
	{ // nothing on screen unless piloting; then one light per system, the journey, and the way out
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, set);
		t.Update({}, dt);
		SpawnAndWalkUp(t, w);
		t.Update({}, dt);
		assert(!w.panel);
		t.Update(Use(), dt);
		assert(w.panel && w.panel->title == "TARDIS");
		const char *lights[] = {"Flight", "Anti-gravs", "Handbrake", "Cloak", "Physlock", "Shields"};
		for (int i = 0; i < 6; i++) assert(w.panel->rows[i].kind == PanelRow::Kind::System && w.panel->rows[i].label == lights[i]);
		assert(Row(w.panel, "Cloak")->key == "L" && Row(w.panel, "Physlock")->key == "G");
		assert(Row(w.panel, "Flight")->value == "OFF" && !Row(w.panel, "Flight")->on);
		assert(Row(w.panel, "Travel to waypoint")->progress < 0);
		assert(w.panel->rows.back().kind == PanelRow::Kind::Footer && w.panel->rows.back().key == "E");
		assert(!Row(w.panel, "Speed")); // flight only, as are the movement keys, which never show
		for (auto &r : w.panel->rows) assert(r.key != "W" && r.key != "LSHIFT" && r.key != "SPACE");
		t.Update(Press(Action::FlightToggle), dt);
		assert(Row(w.panel, "Flight")->on && Row(w.panel, "Flight")->value == "ACTIVE" && Row(w.panel, "Anti-gravs")->on);
		assert(Row(w.panel, "Speed")->value == "+0 km/h" && Row(w.panel, "Speed")->key == "Num 8/Num 5");
		assert(Row(w.panel, "Spin")->value == "anti-clockwise");
		w.panel.reset();
		t.Update(Use(), dt);
		assert(!w.panel);
	}
	{ // mid-journey the travel row carries its phase and progress
		FakeWorld w; FakeAudio au;
		w.waypoint = V3{50, 60, 0};
		Tardis t(w, au, set);
		SpawnAndWalkUp(t, w);
		t.Update(Use(), dt);
		t.Update(Press(Action::TeleportDemat), dt);
		const PanelRow *r = Row(w.panel, "Travel to waypoint");
		assert(r->value == "Dematerialising" && r->progress == 0);
		for (int i = 0; i < 60 * 8; i++) t.Update({}, dt);
		r = Row(w.panel, "Travel to waypoint");
		assert(r->progress > 0 && r->progress <= 0.4f);
	}
	{ // HUD Panel=0 hides it
		set.panel = false;
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, set);
		SpawnAndWalkUp(t, w);
		t.Update(Use(), dt);
		assert(!w.panel);
	}
}

static FrameInput Hold(Action a) { FrameInput in; in.down[(size_t)a] = true; return in; }

static void CloakTests()
{
	const float dt = 1 / 60.0f;
	auto pilot = [](Tardis &t, FakeWorld &w) { SpawnAndWalkUp(t, w); t.Update(Use(), 1 / 60.0f); };
	{ // L fades it out over 3 s with the phase sound, and back in over 3 s
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update(Press(Action::CloakToggle), dt);
		assert(t.state().cloak && au.played.back() == Sound::CloakOn && w.notes.back() == "Cloak enabled");
		for (int i = 0; i < 90; i++) t.Update({}, dt);
		assert(w.alpha > 100 && w.alpha < 155); // halfway
		for (int i = 0; i < 91; i++) t.Update({}, dt);
		assert(w.alpha == 0);
		t.Update(Press(Action::CloakToggle), dt);
		assert(!t.state().cloak && au.played.back() == Sound::CloakOff);
		for (int i = 0; i < 60 * 3 + 1; i++) t.Update({}, dt);
		assert(w.alpha == 255);
	}
	{ // L mid-fade turns back from where it was
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update(Press(Action::CloakToggle), dt);
		for (int i = 0; i < 60; i++) t.Update({}, dt);
		int mid = w.alpha;
		t.Update(Press(Action::CloakToggle), dt);
		assert(w.alpha > mid && w.alpha < 255);
	}
	{ // cloaked: no flight sound, police blind only while piloting
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update(Press(Action::FlightToggle), dt);
		assert(au.flight && w.policeHidden == 0);
		t.Update(Press(Action::CloakToggle), dt);
		t.Update({}, dt);
		assert(!au.flight && w.policeHidden > 0);
		t.Update(Press(Action::FlightToggle), dt);
		t.Update(Use(), dt); // out on foot, still cloaked
		int before = w.policeHidden;
		for (int i = 0; i < 10; i++) t.Update({}, dt);
		assert(t.state().cloak && w.policeHidden == before);
	}
	{ // a cloaked journey is silent and stays invisible on arrival
		FakeWorld w; FakeAudio au;
		w.waypoint = V3{50, 60, 0};
		w.safeSpot = V3{50, 60, 0};
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update(Press(Action::CloakToggle), dt);
		au.played.clear();
		t.Update(Press(Action::TeleportDemat), dt);
		while (t.teleport().Busy()) t.Update({}, dt);
		assert(au.played.empty() && w.alpha == 0 && Near(w.ext.pos.x, 50, 1e-3f));
	}
	{ // L does nothing on foot, and a new TARDIS starts uncloaked
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update(Press(Action::CloakToggle), dt);
		assert(!t.state().cloak);
		t.Update(Use(), dt);
		t.Update(Press(Action::CloakToggle), dt);
		t.Update(Use(), dt);
		t.Update(Press(Action::Spawn), dt);
		t.Update(Press(Action::Spawn), dt);
		assert(!t.state().cloak);
		for (int i = 0; i < 10; i++) t.Update({}, dt);
		assert(w.alpha == 255);
	}
}

static void PhyslockTests()
{
	const float dt = 1 / 60.0f;
	auto pilot = [](Tardis &t, FakeWorld &w) { SpawnAndWalkUp(t, w); t.Update(Use(), 1 / 60.0f); };
	{ // G freezes it, piloting or not, and flight under it warns and goes nowhere
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update(Press(Action::PhyslockToggle), dt);
		assert(t.state().physlock && w.frozen && w.notes.back() == "Locking-down mechanism engaged");
		t.Update(Press(Action::FlightToggle), dt);
		assert(t.state().flight && w.frozen && w.notes.back() == "~y~WARNING: Physical lock engaged");
		FrameInput fwd = Hold(Action::FlightForward);
		for (int i = 0; i < 60; i++) t.Update(fwd, dt);
		assert(w.vel.Len() == 0);
		t.Update(Use(), dt);
		t.Update({}, dt);
		assert(!t.piloting() && w.frozen);
		t.Update(Use(), dt);
		t.Update(Press(Action::PhyslockToggle), dt);
		assert(!w.frozen && !w.gravity); // flight takes it again
	}
	{ // trains are held only under physlock; a train hitting it crashes and shakes the camera
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		pilot(t, w);
		t.Update({}, dt);
		assert(w.trainsHeld == 0);
		t.Update(Press(Action::PhyslockToggle), dt);
		w.trainHits = true;
		t.Update({}, dt);
		assert(w.trainsHeld == 2 && au.played.back() == Sound::FlightFall && w.shakes == 1);
		t.Update({}, dt);
		assert(w.shakes == 1);
		t.Update(Press(Action::PhyslockToggle), dt);
		int held = w.trainsHeld;
		t.Update({}, dt);
		assert(w.trainsHeld == held);
	}
}

static void SpeedTests()
{
	const float dt = 1 / 60.0f;
	{ // a tap is 1 km/h; held, 1 km/h steps every 0.1 s, then 5 from the 5th repeat
		SpeedDial d;
		assert(d.Step(1, true, dt) == 1);
		int sum = 0;
		for (int i = 0; i < 20; i++) sum += d.Step(1, false, dt); // 0.33 s: still the first delay
		assert(sum == 0);
		std::vector<int> steps;
		for (int i = 0; i < 60; i++) if (int s = d.Step(1, false, dt)) steps.push_back(s);
		assert(steps.size() >= 6 && steps[0] == 1 && steps[3] == 1 && steps[4] == 5 && steps[5] == 5);
		assert(d.Step(0, false, dt) == 0 && d.Step(-1, true, dt) == -1);
	}
	{ // top speed: GMod's ~137 km/h, scaled by the offset, never under 1 km/h
		assert(Near(FlightModel::kTopSpeedKmh, 136.5f, 0.5f));
		assert(FlightModel::kTopSpeedKmh + FlightModel::kMinSpeedOffset >= 1);
		FlightModel m({});
		TardisState s;
		s.ToggleFlight();
		Stick st;
		st.forward = true;
		auto cruise = [&](int offset) {
			s.speedOffset = offset;
			Body b;
			for (int i = 0; i < 60 * 30; i++) b = m.Step(s, b, &st, {}, dt);
			return b.vel.Len() * 3.6f;
		};
		assert(Near(cruise(0), FlightModel::kTopSpeedKmh, 2));
		assert(Near(cruise(100), FlightModel::kTopSpeedKmh + 100, 3));
		assert(Near(cruise(-100), FlightModel::kTopSpeedKmh - 100, 1));
	}
	{ // Num 8 / Num 5 / Num 0 only in flight; the floor holds; removing the TARDIS forgets it
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update(Use(), dt);
		t.Update(Press(Action::SpeedUp), dt);
		assert(t.state().speedOffset == 0);
		t.Update(Press(Action::FlightToggle), dt);
		t.Update(Press(Action::SpeedUp), dt);
		t.Update({}, dt);
		t.Update(Press(Action::SpeedUp), dt);
		assert(t.state().speedOffset == 2);
		t.Update(Press(Action::SpeedReset), dt);
		assert(t.state().speedOffset == 0);
		t.Update(Press(Action::SpeedDown), dt);
		for (int i = 0; i < 60 * 10; i++) t.Update(Hold(Action::SpeedDown), dt);
		assert(t.state().speedOffset == FlightModel::kMinSpeedOffset);
		t.Update(Use(), dt);
		t.Update(Press(Action::Spawn), dt);
		t.Update(Press(Action::Spawn), dt);
		assert(t.state().speedOffset == 0);
	}
}

static void LampTests()
{
	const float dt = 1 / 60.0f;
	{ // GMod's rules: float/flight or travel pulse it, AlwaysOn keeps it steady, cloak puts it out
		assert(!Lamp(false, false, false, false, 0).on);
		LampLight steady = Lamp(false, false, false, true, 0);
		assert(steady.on && Near(steady.glow, 100 / 255.0f, 1e-4f) && steady.size == 1);
		float lo = 1, hi = 0;
		for (float t = 0; t < 2; t += 0.01f) { float g = Lamp(true, false, false, false, t).glow; lo = std::min(lo, g); hi = std::max(hi, g); }
		assert(lo < 0.05f && hi > 0.5f);
		assert(!Lamp(true, true, true, true, 0).on);
	}
	{ // through the use case: dark parked, lit and washing in flight, dark again cloaked, wash only in flight not float
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update(Use(), dt);
		assert(!w.lamp.on && !w.wash);
		t.Update(Press(Action::FloatToggle), dt);
		assert(w.lamp.on && !w.wash);
		t.Update(Press(Action::FlightToggle), dt);
		assert(w.lamp.on && w.wash);
		t.Update(Press(Action::CloakToggle), dt);
		assert(!w.lamp.on && !w.wash);
	}
	{ // travelling lights it and washes until it vanishes
		FakeWorld w; FakeAudio au;
		w.waypoint = V3{50, 60, 0};
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update(Use(), dt);
		t.Update(Press(Action::TeleportDemat), dt);
		assert(w.lamp.on && w.wash);
		while (!t.teleport().Hidden()) t.Update({}, dt);
		assert(!w.lamp.on && !w.wash);
	}
}

static void ShieldTests()
{
	const float dt = 1 / 60.0f;
	{ // K turns it on with a flash and the power sound; it keeps working with the pilot out on foot
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update(Press(Action::ShieldToggle), dt);
		assert(!t.state().shield); // not on foot
		t.Update(Use(), dt);
		t.Update(Press(Action::ShieldToggle), dt);
		assert(t.state().shield && w.shieldFlashes == 1 && au.played.back() == Sound::ShieldOn && w.notes.back() == "Shields enabled");
		assert(w.shieldFrames == 1 && w.shieldVisible);
		t.Update(Use(), dt);
		for (int i = 0; i < 5; i++) t.Update({}, dt);
		assert(!t.piloting() && w.shieldFrames == 7);
	}
	{ // cloaked: still reflecting, no flash; off: nothing; a new TARDIS starts without it
		FakeWorld w; FakeAudio au;
		Tardis t(w, au, En());
		SpawnAndWalkUp(t, w);
		t.Update(Use(), dt);
		t.Update(Press(Action::CloakToggle), dt);
		t.Update(Press(Action::ShieldToggle), dt);
		assert(w.shieldFlashes == 0 && w.shieldFrames == 1 && !w.shieldVisible);
		t.Update(Press(Action::ShieldToggle), dt);
		int frames = w.shieldFrames;
		t.Update({}, dt);
		assert(w.shieldFrames == frames && au.played.back() == Sound::ShieldOff);
		t.Update(Press(Action::ShieldToggle), dt);
		t.Update(Use(), dt);
		t.Update(Press(Action::Spawn), dt);
		t.Update(Press(Action::Spawn), dt);
		assert(!t.state().shield);
	}
}

static void TextTests()
{
	Text en = LoadLang("en-US"), pt = LoadLang("pt-BR");
	// every line the mod shows is in English, the fallback; pt-BR has them all and nothing English lacks
	for (auto &[k, v] : pt.lines) assert(en.lines.count(k));
	for (auto &[k, v] : en.lines) assert(pt.lines.count(k));
	assert(pt("panel.flight") == "Voo" && pt("msg.spin", pt("spin.clockwise")) == "Sentido de giro definido para horário");
	assert(en("no.such.line") == "no.such.line");
	// a language file lays over English line by line
	Text mixed = LoadLang("en-US");
	std::istringstream partial("[Panel]\npanel.flight=Vol\n");
	mixed.Add(partial);
	assert(mixed("panel.flight") == "Vol" && mixed("panel.cloak") == "Cloak");
}

int main()
{
	RulesTests();
	FlightTests();
	TeleportTests();
	CameraTests();
	TardisTests();
	TravelTests();
	SoundTests();
	PanelTests();
	CloakTests();
	PhyslockTests();
	SpeedTests();
	LampTests();
	ShieldTests();
	TextTests();
	std::puts("all tests passed");
}
