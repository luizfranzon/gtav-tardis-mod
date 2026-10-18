// Domain and application tests, no GTA needed. Build and run: tests\run_tests.bat
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include "../src/app/tardis.h"

static bool Near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

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
	bool cameraOn = false, locked = false, phoneBlocked = false;
	std::vector<std::string> notes;
	std::string hint;
	std::vector<LegendLine> legend;

	bool SpawnExterior(Pose at) override { if (!modelOk) return false; has = true; ext = at; return true; }
	void DespawnExterior() override { has = false; }
	bool HasExterior() override { return has; }
	Pose ExteriorPose() override { return ext; }
	V3 ExteriorVelocity() override { return vel; }
	void DriveExterior(Quat r, V3 v) override { ext.rot = r; vel = v; }
	void SetExteriorPhysics(bool g, bool f) override { gravity = g; frozen = f; }
	void SetExteriorAngularVelocity(V3 w) override { spin = w; }
	Pose PlayerPose() override { return player; }
	bool PlayerInVehicle() override { return inVehicle; }
	void Board() override { boarded = true; }
	void Unboard(V3 at) override { boarded = false; unboardedAt = at; }
	float GroundZ(V3) override { return 0; }
	V3 CastToWorld(V3, V3 to) override { return to; }
	View GameplayView() override { return {}; }
	void ShowCamera(std::optional<CameraShot> s) override { cameraOn = s.has_value(); }
	void LockControls() override { locked = true; }
	void BlockPhone() override { phoneBlocked = true; }
	void Hint(const std::string &t) override { hint = t; }
	void DrawLegend(const std::vector<LegendLine> &l) override { legend = l; }
	void Notify(const std::string &t) override { notes.push_back(t); }
};

struct FakeAudio : Audio
{
	std::vector<Sound> played;
	std::optional<LoopState> flight;
	void Play(Sound s, V3) override { played.push_back(s); }
	void Loop(LoopSound, std::optional<LoopState> st) override { flight = st; }
};

struct FakeStore : Store
{
	std::optional<SavedTardis> saved;
	int saves = 0;
	std::optional<SavedTardis> Load() override { return saved; }
	void Save(const std::optional<SavedTardis> &t) override { saved = t; saves++; }
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
	{ // F5 spawns 4 m ahead with the door facing the player, F5 again removes it; both are saved
		FakeWorld w; FakeStore st; FakeAudio au;
		Tardis t(w, st, au, {});
		t.Update(Press(Action::Spawn), dt);
		assert(w.has && Near(w.ext.pos.y, 4, 1e-4f));
		assert(Near(w.ext.rot.Forward().y, -1, 1e-4f));
		assert(st.saved.has_value());
		t.Update(Press(Action::Spawn), dt);
		assert(!w.has && !st.saved.has_value());
	}
	{ // missing model: no exterior, the player is told
		FakeWorld w; FakeStore st; FakeAudio au;
		w.modelOk = false;
		Tardis t(w, st, au, {});
		t.Update(Press(Action::Spawn), dt);
		assert(!w.has && w.notes.back().find("not found") != std::string::npos);
	}
	{ // E nearby boards and pilots; E again steps out through the door
		FakeWorld w; FakeStore st; FakeAudio au;
		Tardis t(w, st, au, {});
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
		FakeWorld w; FakeStore st; FakeAudio au;
		Tardis t(w, st, au, {});
		SpawnAndWalkUp(t, w);
		w.inVehicle = true;
		t.Update(Use(), dt);
		assert(!t.piloting());
	}
	{ // R flies (gravity off), J mid-flight drops to anti-gravs, R is then refused
		FakeWorld w; FakeStore st; FakeAudio au;
		Tardis t(w, st, au, {});
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
		FakeWorld w; FakeStore st; FakeAudio au;
		Tardis t(w, st, au, {});
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
		FakeWorld w; FakeStore st; FakeAudio au;
		Tardis t(w, st, au, {});
		t.Update(Press(Action::Spawn), dt);
		t.Update(Press(Action::FlightToggle), dt);
		assert(!t.state().flight);
	}
	{ // parked far away it freezes, and it saves every 5 s
		FakeWorld w; FakeStore st; FakeAudio au;
		Tardis t(w, st, au, {});
		t.Update(Press(Action::Spawn), dt);
		w.player.pos = {0, 500, 0};
		int before = st.saves;
		for (int i = 0; i < 60 * 6; i++) t.Update({}, dt);
		assert(w.frozen && st.saves > before);
	}
	{ // a saved TARDIS comes back where it was left, handbrake included
		FakeWorld w; FakeStore st; FakeAudio au;
		st.saved = SavedTardis{{{10, 20, 30}, {}}, true};
		Tardis t(w, st, au, {});
		t.Start();
		assert(w.has && w.ext.pos.x == 10 && t.state().handbrake && w.frozen);
	}
}

static void SoundTests()
{
	const float dt = 1 / 60.0f;
	{ // F5 plays the spawn and delete sounds
		FakeWorld w; FakeStore st; FakeAudio au;
		Tardis t(w, st, au, {});
		t.Update(Press(Action::Spawn), dt);
		t.Update(Press(Action::Spawn), dt);
		assert(au.played.size() == 2 && au.played[0] == Sound::Spawn && au.played[1] == Sound::Delete);
	}
	{ // flight loop only while flying, pitch 0.95 at rest rising with speed up to 1.10
		FakeWorld w; FakeStore st; FakeAudio au;
		Tardis t(w, st, au, {});
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
	{ // falling onto the ground: thud when slow, crash when fast, not twice within 2 s
		FakeWorld w; FakeStore st; FakeAudio au;
		Tardis t(w, st, au, {});
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

static bool Shows(const std::vector<LegendLine> &l, const std::string &key, const std::string &text)
{
	for (auto &x : l) if (x.key == key && x.text == text) return true;
	return false;
}

static void LegendTests()
{
	const float dt = 1 / 60.0f;
	TardisSettings set;
	for (size_t i = 0; i < set.keyNames.size(); i++) set.keyNames[i] = kActions[i].key;
	set.keyNames[(size_t)Action::FlightBoost] = "Shift";
	FakeWorld w; FakeStore st; FakeAudio au;
	Tardis t(w, st, au, set);
	t.Update({}, dt);
	assert(Shows(w.legend, "F5", "Spawn TARDIS"));
	SpawnAndWalkUp(t, w);
	t.Update({}, dt);
	assert(Shows(w.legend, "F5", "Remove TARDIS"));
	t.Update(Use(), dt);
	assert(Shows(w.legend, "R", "Flight") && Shows(w.legend, "J", "Handbrake") && !Shows(w.legend, "Shift", "Boost"));
	t.Update(Press(Action::FlightToggle), dt);
	t.Update({}, dt);
	assert(Shows(w.legend, "Shift", "Boost") && Shows(w.legend, "WASD", "Move"));
	assert(Shows(w.legend, "", "Flight ON   Anti-gravs ON   Handbrake OFF"));
	set.legend = false;
	FakeWorld w2;
	Tardis off(w2, st, au, set);
	off.Update({}, dt);
	assert(w2.legend.empty());
}

int main()
{
	RulesTests();
	FlightTests();
	CameraTests();
	TardisTests();
	SoundTests();
	LegendTests();
	std::puts("all tests passed");
}
