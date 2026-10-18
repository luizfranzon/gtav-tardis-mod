// Composition root: wires the GTA adapters into the TARDIS and runs it as a ScriptHookV script.
#include <windows.h>
#include <main.h>
#include <natives.h>
#include "app/tardis.h"
#include "infra/gta_world.h"
#include "infra/ini.h"
#include "infra/log.h"
#include "infra/ma_audio.h"

static std::string g_dir; // folder of TARDIS.asi (the game folder)

static void ScriptMain()
{
	OpenLog(g_dir + "TARDIS.log");
	Log("TARDIS starting, game version %d", (int)getGameVersion());
	ModConfig cfg = LoadConfig(g_dir + "TARDIS.ini");

	GtaWorld world(cfg.exteriorModel);
	IniStore store(g_dir + "TARDIS_state.ini");
	GtaInput input(cfg.keys);
	MaAudio audio(g_dir + "TARDIS\\sounds\\", cfg.soundVolume);
	Tardis tardis(world, store, audio, cfg.tardis);

	while (!PLAYER::IS_PLAYER_PLAYING(PLAYER::PLAYER_ID())) WAIT(100);
	tardis.Start();
	for (;;)
	{
		invoke<Void>(0xF4F2C0D4EE209E20); // INVALIDATE_IDLE_CAM: the idle cinematic cam fights the pilot cam
		float dt = GAMEPLAY::GET_FRAME_TIME();
		tardis.Update(input.Poll(), dt);
		world.Maintain();
		audio.Tick(dt);
		WAIT(0);
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		char p[MAX_PATH];
		GetModuleFileNameA(module, p, MAX_PATH);
		g_dir = p;
		g_dir = g_dir.substr(0, g_dir.find_last_of("\\/") + 1);
		scriptRegister(module, ScriptMain);
	}
	else if (reason == DLL_PROCESS_DETACH)
		scriptUnregister(module);
	return TRUE;
}
