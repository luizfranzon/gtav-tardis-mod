#include "pilot_camera.h"
#include <algorithm>

CameraShot PilotCamera::Update(Pose ext, float lookX, float lookY, int wheel)
{
	view_.yaw -= lookX * sens_;
	view_.pitch = std::clamp(view_.pitch - lookY * sens_, -89.0f, 89.0f);
	if (wheel)
		distU_ = std::clamp(distU_ - wheel * 0.03f * (1.1f + distU_) * 3, 90.0f, 500.0f); // ~3 GMod wheel units a notch

	V3 pivot = ext.pos + ext.rot.Up() * (60 * kUnit);
	V3 fwd = view_.Forward();
	V3 want = pivot - fwd * (distU_ * kUnit);
	V3 hit = cast_(pivot, want);
	bool blocked = (hit - want).Len() > 1e-3f;
	return {blocked ? hit + fwd * (10 * kUnit) : want, view_};
}
