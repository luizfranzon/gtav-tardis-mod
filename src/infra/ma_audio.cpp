#include "ma_audio.h"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <thread>
#include <main.h>
#include <natives.h>
#include <miniaudio.h>
#include "log.h"

namespace
{
const char *kFiles[] = {"spawn.wav", "delete.wav", "flight_land.wav", "flight_fall.wav", "demat.wav", "mat.wav", "demat_fail.wav", "cloak_on.wav", "cloak_off.wav", "shield_on.wav", "shield_off.wav", "interrupt.wav"}; // by Sound
const char *kLoops[] = {"flight_loop.wav"};                                               // by LoopSound

// miniaudio is OpenGL-style (+Y up, -Z forward); GTA is +Z up, +Y north
void To(V3 v, float &x, float &y, float &z) { x = v.x; y = v.z; z = -v.y; }

// Source "soundlevel 90" carries far; this keeps it loud nearby and audible across a block
void Spatialize(ma_sound *s)
{
	ma_sound_set_attenuation_model(s, ma_attenuation_model_inverse);
	ma_sound_set_min_distance(s, 4.0f);
	ma_sound_set_max_distance(s, 250.0f);
}

V3 CamCoord() { Vector3 v = invoke<Vector3>(0xA200EB1EE790F448); return {v.x, v.y, v.z}; }   // GET_FINAL_RENDERED_CAM_COORD
V3 CamRot() { Vector3 v = invoke<Vector3>(0x5B4E4C817FCC2DFB, 2); return {v.x, v.y, v.z}; } // GET_FINAL_RENDERED_CAM_ROT
}

struct MaAudio::Impl
{
	std::string dir;
	ma_engine engine{};
	bool ok = false;
	struct OneShot { ma_sound snd; bool attached; };
	std::list<OneShot> oneShots;
	ma_sound loops[1]{};
	bool loopReady[1]{}, loopOn[1]{};
	V3 lastCam;

	std::mutex m;
	std::atomic<long long> lastTick{0};
	std::atomic<bool> stop{false}, paused{false};
	std::thread watchdog;

	static long long Now()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
};

MaAudio::MaAudio(std::string dir, float volume) : impl_(std::make_unique<Impl>())
{
	Impl &i = *impl_;
	i.dir = std::move(dir);
	if (ma_engine_init(nullptr, &i.engine) != MA_SUCCESS) { Log("audio: no output device"); return; }
	i.ok = true;
	ma_engine_set_volume(&i.engine, volume);
	for (int l = 0; l < 1; l++)
	{
		std::string f = i.dir + kLoops[l];
		i.loopReady[l] = ma_sound_init_from_file(&i.engine, f.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &i.loops[l]) == MA_SUCCESS;
		if (!i.loopReady[l]) { Log("audio: can't load %s", f.c_str()); continue; }
		ma_sound_set_looping(&i.loops[l], MA_TRUE);
		Spatialize(&i.loops[l]);
	}
	i.lastTick = Impl::Now();
	i.watchdog = std::thread([&i] {
		while (!i.stop)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			std::lock_guard<std::mutex> g(i.m);
			if (!i.paused && Impl::Now() - i.lastTick > 250)
			{
				ma_engine_stop(&i.engine);
				i.paused = true;
			}
		}
	});
	Log("audio: ready (%s)", i.dir.c_str());
}

MaAudio::~MaAudio()
{
	Impl &i = *impl_;
	i.stop = true;
	if (i.watchdog.joinable()) i.watchdog.join();
	if (!i.ok) return;
	for (auto &s : i.oneShots) ma_sound_uninit(&s.snd);
	for (int l = 0; l < 1; l++) if (i.loopReady[l]) ma_sound_uninit(&i.loops[l]);
	ma_engine_uninit(&i.engine);
}

void MaAudio::Play(Sound s, V3 at, bool attached)
{
	Impl &i = *impl_;
	if (!i.ok) return;
	std::lock_guard<std::mutex> g(i.m);
	std::string f = i.dir + kFiles[(int)s];
	auto &shot = i.oneShots.emplace_back();
	shot.attached = attached;
	ma_sound &snd = shot.snd;
	if (ma_sound_init_from_file(&i.engine, f.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &snd) != MA_SUCCESS)
	{
		Log("audio: can't load %s", f.c_str());
		i.oneShots.pop_back();
		return;
	}
	Spatialize(&snd);
	float x, y, z;
	To(at, x, y, z);
	ma_sound_set_position(&snd, x, y, z);
	ma_sound_start(&snd);
}

void MaAudio::MoveAttached(V3 at)
{
	Impl &i = *impl_;
	if (!i.ok) return;
	std::lock_guard<std::mutex> g(i.m);
	float x, y, z;
	To(at, x, y, z);
	for (auto &s : i.oneShots) if (s.attached) ma_sound_set_position(&s.snd, x, y, z);
}

void MaAudio::StopAttached()
{
	Impl &i = *impl_;
	if (!i.ok) return;
	std::lock_guard<std::mutex> g(i.m);
	for (auto it = i.oneShots.begin(); it != i.oneShots.end();)
		if (it->attached) { ma_sound_uninit(&it->snd); it = i.oneShots.erase(it); }
		else ++it;
}

void MaAudio::Loop(LoopSound s, std::optional<LoopState> st)
{
	Impl &i = *impl_;
	int l = (int)s;
	if (!i.ok || !i.loopReady[l]) return;
	std::lock_guard<std::mutex> g(i.m);
	ma_sound *snd = &i.loops[l];
	if (!st)
	{
		if (i.loopOn[l]) { ma_sound_stop(snd); i.loopOn[l] = false; }
		return;
	}
	float x, y, z;
	To(st->at, x, y, z);
	ma_sound_set_position(snd, x, y, z);
	To(st->vel, x, y, z);
	ma_sound_set_velocity(snd, x, y, z);
	ma_sound_set_pitch(snd, st->pitch);
	ma_sound_set_volume(snd, st->volume);
	if (!i.loopOn[l]) { ma_sound_start(snd); i.loopOn[l] = true; }
}

void MaAudio::Tick(float dt)
{
	Impl &i = *impl_;
	if (!i.ok) return;
	std::lock_guard<std::mutex> g(i.m);
	i.lastTick = Impl::Now();
	if (i.paused) { ma_engine_start(&i.engine); i.paused = false; }

	V3 cam = CamCoord(), rot = CamRot();
	View v{rot.x, rot.z};
	V3 vel = dt > 0 ? (cam - i.lastCam) * (1.0f / dt) : V3{};
	if (vel.Len() > 200) vel = {}; // a camera cut, not movement
	i.lastCam = cam;
	float x, y, z;
	To(cam, x, y, z);
	ma_engine_listener_set_position(&i.engine, 0, x, y, z);
	To(v.Forward(), x, y, z);
	ma_engine_listener_set_direction(&i.engine, 0, x, y, z);
	ma_engine_listener_set_world_up(&i.engine, 0, 0, 1, 0);
	To(vel, x, y, z);
	ma_engine_listener_set_velocity(&i.engine, 0, x, y, z);

	for (auto it = i.oneShots.begin(); it != i.oneShots.end();)
	{
		if (ma_sound_at_end(&it->snd)) { ma_sound_uninit(&it->snd); it = i.oneShots.erase(it); }
		else ++it;
	}
}
