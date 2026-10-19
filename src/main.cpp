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

	while (!PLAYER::IS_PLAYER_PLAYING(PLAYER::PLAYER_ID())) WAIT(100);
	// GET_CURRENT_LANGUAGE, by its id; a language with no file falls back to English line by line
	static const char *kLanguages[] = {"en-US", "fr-FR", "de-DE", "it-IT", "es-ES", "pt-BR", "pl-PL", "ru-RU", "ko-KR",
	                                   "zh-TW", "ja-JP", "es-MX", "zh-CN"};
	int lang = invoke<int>(0x2BDD44CC428A7EAE);
	cfg.tardis.text = LoadText(g_dir + "TARDIS\\translations\\", lang >= 0 && lang < 13 ? kLanguages[lang] : "en-US");

	GtaWorld world(cfg.exteriorModel, cfg.lamp);
	GtaInput input(cfg.keys);
	MaAudio audio(g_dir + "TARDIS\\sounds\\", cfg.soundVolume);
	Tardis tardis(world, audio, cfg.tardis);
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
