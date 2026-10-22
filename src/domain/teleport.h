// Dematerialise / materialise, ported from TARDIS Rewrite (modules/teleport/sh_tp_main.lua; interiors/base.lua
// Exterior.Teleport; the Toyota's DematSequenceDelays). This port has no vortex: once faded out, the exterior
// waits invisible at the destination until the ground there is found (Arrive), sits out the premat delay, and
// fades back in. Pure: no game calls.
#pragma once
#include <algorithm>

struct Teleport
{
	enum class Phase { Idle, Demat, Seek, Premat, Mat };
	enum class Event { None, Vanished, Landing, Done };

	static constexpr float kDematSeq[] = {200, 100, 150, 50, 100, 0};   // alpha steps (DematSequence)
	static constexpr float kMatSeq[] = {50, 150, 100, 200, 150, 255};   // (MatSequence)
	static constexpr int kSteps = 6;
	static constexpr float kSpeed = 0.77f * 66; // alpha per second (SequenceSpeed is per 66 Hz tick)
	static constexpr float kDematDelay = 2.5f;  // before the first fade
	static constexpr float kPrematDelay = 8.5f; // from the mat sound to the fade-in

	Phase phase = Phase::Idle;
	float alpha = 255;
	int step = 0;
	float wait = 0;

	bool Busy() const { return phase != Phase::Idle; }
	bool Hidden() const { return phase == Phase::Seek || phase == Phase::Premat; }

	// How far along the journey is, 0..1, for the pilot's panel: the fade-out is the first 40%, the wait for the
	// landing spot holds there, the premat delay runs to 60%, and the fade-in fills the rest
	float Progress() const
	{
		switch (phase)
		{
		case Phase::Demat: return 0.4f * step / kSteps;
		case Phase::Seek: return 0.4f;
		case Phase::Premat: return 0.4f + 0.2f * std::clamp(1 - wait / kPrematDelay, 0.0f, 1.0f);
		case Phase::Mat: return 0.6f + 0.4f * step / kSteps;
		default: return 0;
		}
	}

	void Start() { phase = Phase::Demat; alpha = 255; step = 0; wait = kDematDelay; }
	void Arrive() { if (phase == Phase::Seek) { phase = Phase::Premat; wait = kPrematDelay; } }

	// The handbrake mid-journey: over at once, fully visible where it is. True if it was still dematerialising.
	bool Interrupt()
	{
		bool demat = phase == Phase::Demat;
		phase = Phase::Idle;
		alpha = 255;
		return demat;
	}

	// Vanished: fully faded out, move it to the destination. Landing: put it down there, it fades in now.
	Event Step(float dt)
	{
		if (phase == Phase::Idle || phase == Phase::Seek) return Event::None;
		if ((wait -= dt) > 0) return Event::None;
		if (phase == Phase::Premat) { phase = Phase::Mat; step = 0; return Event::Landing; }

		float target = (phase == Phase::Demat ? kDematSeq : kMatSeq)[step];
		alpha = alpha < target ? std::min(target, alpha + kSpeed * dt) : std::max(target, alpha - kSpeed * dt);
		if (alpha != target || ++step < kSteps) return Event::None;
		if (phase == Phase::Demat) { phase = Phase::Seek; return Event::Vanished; }
		phase = Phase::Idle;
		return Event::Done;
	}
};
