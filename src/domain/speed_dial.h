// The pilot's flight speed keys (not in GMod): a tap moves the speed 1 km/h; held, it repeats every 0.1 s, and from
// the 5th repeat on in 5 km/h steps. Pure: no game calls.
#pragma once

struct SpeedDial
{
	static constexpr float kFirstDelay = 0.35f; // seconds before a held key repeats, so a tap is one step
	static constexpr float kRepeat = 0.1f;
	static constexpr int kFastAfter = 5, kFastStep = 5;

	float next = 0;
	int repeats = 0;

	// dir: +1 while faster is held, -1 slower, 0 neither; pressed: that key went down this frame.
	// Returns the km/h to add this frame.
	int Step(int dir, bool pressed, float dt)
	{
		if (!dir) { repeats = 0; return 0; }
		if (pressed) { repeats = 0; next = kFirstDelay; return dir; }
		if ((next -= dt) > 0) return 0;
		next += kRepeat;
		return dir * (++repeats >= kFastAfter ? kFastStep : 1);
	}
};
