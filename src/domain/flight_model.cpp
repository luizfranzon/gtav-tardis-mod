#include "flight_model.h"
#include <algorithm>

namespace
{
constexpr float kTick = 66.0f;       // Source physics rate the GMod numbers are per
constexpr float kLinBrake = 0.6633f; // vel * -0.01 per tick      -> -66*ln(0.99)
constexpr float kAngBrake = 0.9975f; // angvel * -0.015 per tick  -> -66*ln(0.985)
constexpr float kFloatBrake = 3.385f; // angvel * -0.05 per tick  -> -66*ln(0.95)
}

Body FlightModel::Step(const TardisState &s, Body b, const Stick *pilot, View view, float dt) const
{
	if (!Owns(s)) return b;
	if (s.flight) Flight(s, b, pilot, view, dt);
	else Float(b, pilot, view, dt);
	b.rot = b.rot.Integrate(b.angVel, dt);
	return b;
}

void FlightModel::Flight(const TardisState &s, Body &b, const Stick *p, View view, float dt) const
{
	V3 up = b.rot.Up();
	V3 fwd = view.Forward(), ri = view.Right();
	float force = 15, vforce = 5, rforce = 2, tilt = 0;
	const float tforce = 200;
	int spindir = s.SpinDir();
	bool spin = spindir != 0;

	if (p)
	{
		if (p->boost)
		{
			float m = spin ? t_.boostSpeed : std::max(1.0f, t_.boostSpeed * 0.6f);
			force *= m; vforce *= m; rforce *= m;
		}
		V3 acc;
		float yaw = Rad(rforce * kTick * dt);
		if (p->forward) { acc = acc + fwd * force; tilt += 5; }
		if (p->backward) { acc = acc - fwd * force; tilt += 5; }
		if (p->right) { if (p->rotate) b.angVel = b.angVel - up * yaw; else { acc = acc + ri * force; tilt += 5; } }
		if (p->left) { if (p->rotate) b.angVel = b.angVel + up * yaw; else { acc = acc - ri * force; tilt += 5; } }
		if (p->down) acc = acc - up * vforce;
		else if (p->up) acc = acc + up * vforce;
		b.vel = b.vel + acc * (kTick * kUnit * dt); // GMod adds u/s per tick
	}
	if (!spin) tilt = 0;

	float speedU = b.vel.Len() / kUnit;
	if (spin) b.angVel = b.angVel + up * Rad(-spindir * std::sqrt(speedU / tforce) * kTick * dt);

	// stabilize(): lean 0.005 deg per u/s along the velocity, roll `tilt` degrees, then brake.
	// ponytail: a spring toward a target "up" stands in for Source's force-offset pairs; tune stabilizeK
	V3 lean = b.vel * (0.005f / kUnit);
	V3 target = (V3{0, 0, 1} + V3{lean.x, lean.y, 0} * Rad(1)).Norm();
	target = (target + b.rot.Right() * std::tan(Rad(tilt))).Norm();
	b.angVel = b.angVel + up.Cross(target) * (t_.stabilizeK * dt);
	b.angVel = b.angVel * std::exp(-kAngBrake * dt);
	b.vel = b.vel * std::exp(-kLinBrake * dt);
}

void FlightModel::Float(Body &b, const Stick *p, View view, float dt) const
{
	if (!p) return;
	V3 fwd = view.Forward(), left = view.Right() * -1.0f, camUp = view.Up();
	float force = 1, rforce = 2;
	if (p->boost) { force *= 2.5f; rforce *= 2.5f; }
	float a = Rad(force * kTick * dt), r = Rad(rforce * kTick * dt);
	// GMod spins the box around the view's axes: W/S pitch, A/D roll, Alt+A/D yaw
	if (p->forward) b.angVel = b.angVel + left * a;
	if (p->backward) b.angVel = b.angVel - left * a;
	if (p->right) b.angVel = p->rotate ? b.angVel - camUp * r : b.angVel + fwd * a;
	if (p->left) b.angVel = p->rotate ? b.angVel + camUp * r : b.angVel - fwd * a;
	if (p->brake) b.angVel = b.angVel * std::exp(-kFloatBrake * dt);
}
