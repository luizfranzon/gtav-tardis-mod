// The light on the roof, ported from TARDIS Rewrite (modules/sh_exterior_light.lua, sh_float.lua, teleport/sh_tp_hooks.lua,
// sh_cloak.lua). Pure: no game calls.
#pragma once
#include <algorithm>
#include <cmath>

struct LampLight
{
	bool on = false;
	float glow = 0; // the sprite's alpha in GMod, 0..1: 100/255 steady, pulsing between ~0 and ~0.5
	float size = 1; // the dynamic light's radius multiplier: 1, or 1 +- 0.1 pulsing
};

// On and pulsing while it floats (flight brings float) or travels; steady when the player keeps it always on
// (GMod extlight-alwayson); off while cloaked. t is seconds, for the 3.7 rad/s pulse.
inline LampLight Lamp(bool floating, bool travelling, bool cloak, bool alwaysOn, float t)
{
	bool pulse = floating || travelling;
	if (cloak || !(pulse || alwaysOn)) return {};
	if (!pulse) return {true, 100 / 255.0f, 1};
	float s = std::sin(t * 3.7f);
	return {true, std::max(0.0f, (s + 0.2f) * (255 / 4.0f) + 255 / 2.0f - 70) / 255.0f, 1 + 0.1f * s};
}
