// Audio adapter: plays the TARDIS WAVs in 3D with miniaudio, heard from GTA's rendered camera.
#pragma once
#include <memory>
#include <string>
#include "../app/ports.h"

class MaAudio : public Audio
{
public:
	// soundDir holds the WAVs tools/extract_sounds.py pulls out of TARDIS Rewrite
	MaAudio(std::string soundDir, float volume);
	~MaAudio() override;

	void Play(Sound s, V3 at, bool attached = false) override;
	void MoveAttached(V3 at) override;
	void Loop(LoopSound s, std::optional<LoopState> state) override;

	// Once per script frame: moves the listener to GTA's camera and frees finished one-shots.
	// If frames stop coming (pause menu, alt-tab), a watchdog pauses all sound until they resume.
	void Tick(float dt);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
