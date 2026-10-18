// Audio adapter: plays the TARDIS WAVs in 3D with miniaudio, heard from GTA's rendered camera.
#pragma once
#include <memory>
#include <string>
#include "../app/ports.h"

class MaAudio : public Audio
{
public:
	// soundDir holds spawn.wav, delete.wav, flight_loop.wav, flight_land.wav, flight_fall.wav
	MaAudio(std::string soundDir, float volume);
	~MaAudio() override;

	void Play(Sound s, V3 at) override;
	void Loop(LoopSound s, std::optional<LoopState> state) override;

	// Once per script frame: moves the listener to GTA's camera and frees finished one-shots.
	// If frames stop coming (pause menu, alt-tab), a watchdog pauses all sound until they resume.
	void Tick(float dt);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
