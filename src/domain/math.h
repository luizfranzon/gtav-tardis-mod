// Vectors and rotations in GTA's frame: +X right/east, +Y forward/north, +Z up, right-handed.
#pragma once
#include <cmath>

constexpr float kPi = 3.14159265f;
constexpr float kUnit = 0.0254f; // one GMod unit in metres (72 u player = 1.83 m)

inline float Rad(float deg) { return deg * kPi / 180.0f; }

struct V3
{
	float x = 0, y = 0, z = 0;
	V3 operator+(V3 o) const { return {x + o.x, y + o.y, z + o.z}; }
	V3 operator-(V3 o) const { return {x - o.x, y - o.y, z - o.z}; }
	V3 operator*(float s) const { return {x * s, y * s, z * s}; }
	float Dot(V3 o) const { return x * o.x + y * o.y + z * o.z; }
	V3 Cross(V3 o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
	float Len() const { return std::sqrt(Dot(*this)); }
	V3 Norm() const { float l = Len(); return l > 1e-6f ? *this * (1.0f / l) : V3{}; }
};

struct Quat
{
	float x = 0, y = 0, z = 0, w = 1;

	static Quat Yaw(float deg) { float h = Rad(deg) / 2; return {0, 0, std::sin(h), std::cos(h)}; }

	// this rotation, then `o` in this rotation's local frame
	Quat operator*(Quat o) const
	{
		return {w * o.x + x * o.w + y * o.z - z * o.y, w * o.y - x * o.z + y * o.w + z * o.x,
		        w * o.z + x * o.y - y * o.x + z * o.w, w * o.w - x * o.x - y * o.y - z * o.z};
	}

	V3 Rotate(V3 v) const
	{
		V3 q{x, y, z};
		V3 t = q.Cross(v) * 2.0f;
		return v + t * w + q.Cross(t);
	}
	V3 Up() const { return Rotate({0, 0, 1}); }
	V3 Forward() const { return Rotate({0, 1, 0}); }
	V3 Right() const { return Rotate({1, 0, 0}); }

	Quat Norm() const
	{
		float l = std::sqrt(x * x + y * y + z * z + w * w);
		return {x / l, y / l, z / l, w / l};
	}
	// advance by a world-space angular velocity (rad/s) for dt seconds
	Quat Integrate(V3 o, float dt) const
	{
		Quat d{o.x * w + o.y * z - o.z * y, o.y * w + o.z * x - o.x * z, o.z * w + o.x * y - o.y * x,
		       -o.x * x - o.y * y - o.z * z};
		float h = 0.5f * dt;
		return Quat{x + h * d.x, y + h * d.y, z + h * d.z, w + h * d.w}.Norm();
	}
};

struct Pose
{
	V3 pos;
	Quat rot;
};

// A camera looking along pitch/yaw (degrees, GTA rotation order 2)
struct View
{
	float pitch = 0, yaw = 0;
	V3 Forward() const
	{
		return {-std::sin(Rad(yaw)) * std::cos(Rad(pitch)), std::cos(Rad(yaw)) * std::cos(Rad(pitch)), std::sin(Rad(pitch))};
	}
	V3 Right() const { return {std::cos(Rad(yaw)), std::sin(Rad(yaw)), 0}; }
	V3 Up() const { return Right().Cross(Forward()); }
};
