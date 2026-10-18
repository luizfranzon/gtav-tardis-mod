// The pilot's third-person orbit around the exterior (TARDIS Rewrite sh_thirdperson.lua):
// pivot 60 u above the origin, 210 u back, wheel zoom 90-500 u, pulled in front of walls.
#pragma once
#include <functional>
#include "math.h"

struct CameraShot
{
	V3 pos;
	View view;
};

class PilotCamera
{
public:
	// Where a segment from->to first hits the world (or `to` if it doesn't)
	using CastFn = std::function<V3(V3 from, V3 to)>;

	PilotCamera(float sensitivity, CastFn cast) : sens_(sensitivity), cast_(std::move(cast)) {}

	void Reset(View start) { view_ = start; }
	// look: mouse delta this frame (-1..1 per axis); wheel: +1 per notch toward the TARDIS
	CameraShot Update(Pose exterior, float lookX, float lookY, int wheel);
	View view() const { return view_; }

private:
	float sens_;
	CastFn cast_;
	View view_{-10, 0};
	float distU_ = 210;
};
