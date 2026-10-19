// GtaWorld's shield (not in GMod): attacks on the TARDIS go back to whoever made them, lethally and as the player's
// crime, and anything but the player that enters the bubble is thrown out of it.
#include "gta_world.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <main.h>
#include <natives.h>
#include "log.h"

namespace
{
V3 From(const Vector3 &v) { return {v.x, v.y, v.z}; }
Hash H(const char *s) { return GAMEPLAY::GET_HASH_KEY((char *)s); }
float Now() { return GAMEPLAY::GET_GAME_TIMER() / 1000.0f; }

constexpr float kMargin = 3.0f;      // metres of bubble around the box (user's choice): ~5 m radius for TardisV
constexpr float kFlashTime = 0.5f;   // seconds the bubble shows after a hit
// one throw, sized to clear the bubble: up at kUpSpeed, and out fast enough to come down kLandPast beyond it
constexpr float kUpSpeed = 6.0f;     // m/s: ~1.2 s in the air
constexpr float kLandPast = 6.0f;    // metres beyond the bubble
constexpr float kMinOut = 15.0f;     // m/s outward at least
constexpr float kDrag = 1.3f;        // headroom for tyres and ground friction eating the throw
constexpr float kPedImpulse = 3.0f;  // a ragdoll takes an impulse, not a velocity
// ponytail: kDrag and kPedImpulse are guesses; tune them on a bus and a pedestrian in game
constexpr float kPushGap = 2.0f;     // seconds before the same thing is thrown again (it is flying out meanwhile)
constexpr float kArcTime = 0.35f;    // seconds the shock arcs crackle
constexpr float kShooterMemory = 15.0f; // seconds a ped counts as having fired, to own a projectile
constexpr float kProjectileSpeed = 40.0f; // m/s, to time the follow-up strike on a returned projectile's target
constexpr int kSkelHead = 31086;

// the projectiles worth sending back, by model, and what they go back as
const char *ReturnWeapon(Hash model)
{
	static const struct { const char *model, *weapon; } kinds[] = {
		{"w_lr_rpg_rocket", "WEAPON_RPG"}, {"w_lr_homing_rocket", "WEAPON_HOMINGLAUNCHER"},
		{"w_lr_firework_rocket", "WEAPON_FIREWORK"}, {"w_lr_40mm", "WEAPON_GRENADELAUNCHER"},
		{"w_ex_vehiclemissile_1", "WEAPON_RPG"}, {"w_ex_vehiclemissile_2", "WEAPON_RPG"},
		{"w_ex_vehiclemissile_3", "WEAPON_RPG"}, {"w_ex_vehiclemissile_4", "WEAPON_RPG"},
		{"w_smug_airmissile_01b", "WEAPON_RPG"}, {"w_battle_airmissile_01", "WEAPON_RPG"},
		{"w_ex_grenadefrag", "WEAPON_GRENADELAUNCHER"}, {"w_ex_pe", "WEAPON_GRENADELAUNCHER"},
		{"w_ex_pipebomb", "WEAPON_GRENADELAUNCHER"}, {"w_ex_grenadesmoke", "WEAPON_GRENADELAUNCHER_SMOKE"},
		{"w_ex_molotov", "WEAPON_MOLOTOV"},
	};
	// ponytail: a fixed model list; a projectile not on it is just pushed out like any object
	for (auto &k : kinds) if (model == H(k.model)) return k.weapon;
	return nullptr;
}
}

V3 GtaWorld::ShieldCentre()
{
	V3 mid = (boxLo_ + boxHi_) * 0.5f;
	return From(ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(ext_, mid.x, mid.y, mid.z));
}

float GtaWorld::ShieldRadius() { return (boxHi_ - boxLo_).Len() / 2 + kMargin; }

bool GtaWorld::OnBox(V3 p)
{
	const float m = 0.3f;
	V3 l = From(ENTITY::GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS(ext_, p.x, p.y, p.z));
	return l.x > boxLo_.x - m && l.x < boxHi_.x + m && l.y > boxLo_.y - m && l.y < boxHi_.y + m && l.z > boxLo_.z - m && l.z < boxHi_.z + m;
}

void GtaWorld::FlashShield()
{
	if (HasExterior()) flashes_.push_back({ShieldCentre(), 0, true});
}

void GtaWorld::Shield(bool visible)
{
	if (!HasExterior()) return;
	float now = Now();
	V3 c = ShieldCentre();
	float r = ShieldRadius();
	Ped me = PLAYER::PLAYER_PED_ID();
	Vehicle mine = PED::GET_VEHICLE_PED_IS_IN(me, FALSE);
	auto hitAt = [&](V3 towards) { // where the line from the centre to `towards` crosses the bubble
		V3 d = towards - c;
		return d.Len() < 1e-3f ? c : c + d.Norm() * r;
	};
	auto flash = [&](V3 at) {
		if (visible) flashes_.push_back({at, 0, false});
	};
	auto push = [&](Entity e, V3 at, bool ped) { // thrown clear of the bubble in one go, with a shock where it touched
		if (now - pushedAt_[e] < kPushGap) return;
		pushedAt_[e] = now;
		V3 d = V3{at.x - c.x, at.y - c.y, 0};
		float dist = d.Len();
		d = dist < 1e-3f ? V3{1, 0, 0} : d.Norm();
		float air = 2 * kUpSpeed / 9.8f;
		float out = std::max(kMinOut, (r + kLandPast - dist) / air * kDrag);
		V3 v = d * out + V3{0, 0, kUpSpeed};
		V3 touch = hitAt(at);
		if (visible)
		{
			// the stun gun's own bolt and crackle, owned by nobody so it's no crime, plus arcs and a blue flash
			GAMEPLAY::SHOOT_SINGLE_BULLET_BETWEEN_COORDS(touch.x, touch.y, touch.z + 0.3f, at.x, at.y, at.z, 0, TRUE, H("WEAPON_STUNGUN"), 0, TRUE, FALSE, -1.0f);
			arcs_.push_back({touch, e, 0});
		}
		if (ped)
		{
			PED::SET_PED_TO_RAGDOLL(e, 2000, 4000, 0, FALSE, FALSE, FALSE);
			ENTITY::APPLY_FORCE_TO_ENTITY(e, 1, v.x * kPedImpulse, v.y * kPedImpulse, v.z * kPedImpulse, 0, 0, 0, 0, FALSE, TRUE, TRUE, FALSE, TRUE);
		}
		else ENTITY::SET_ENTITY_VELOCITY(e, v.x, v.y, v.z);
		flash(touch);
	};
	// lethal: a target the returned shot didn't kill is finished off; a vehicle blows up, as the player's doing
	auto strike = [&](Ped shooter, float delay) {
		Entity target = PED::IS_PED_IN_ANY_VEHICLE(shooter, FALSE) ? (Entity)PED::GET_VEHICLE_PED_IS_IN(shooter, FALSE) : shooter;
		strikes_.push_back({target, now + delay});
	};

	// shots: each ped's last impact, the moment it lands on the box
	static int peds[1024];
	int np = worldGetAllPeds(peds, 1024);
	for (int i = 0; i < np; i++)
	{
		Ped p = peds[i];
		if (p == me || ENTITY::IS_ENTITY_DEAD(p)) continue;
		V3 pp = From(ENTITY::GET_ENTITY_COORDS(p, TRUE));
		if (PED::IS_PED_SHOOTING(p)) lastShot_[p] = now;
		Vector3 hit;
		if (now - lastShot_[p] < 0.5f && WEAPON::GET_PED_LAST_WEAPON_IMPACT_COORD(p, &hit))
		{
			V3 h = From(hit);
			auto it = lastImpact_.find(p);
			bool fresh = it == lastImpact_.end() || (it->second - h).Len() > 0.01f;
			lastImpact_[p] = h;
			if (fresh && OnBox(h))
			{
				Hash w;
				if (!(PED::IS_PED_IN_ANY_VEHICLE(p, FALSE) && WEAPON::GET_CURRENT_PED_VEHICLE_WEAPON(p, &w))) w = WEAPON::GET_SELECTED_PED_WEAPON(p);
				V3 head = From(PED::GET_PED_BONE_COORDS(p, kSkelHead, 0, 0, 0));
				V3 from = h + (head - h).Norm() * 0.6f; // clear of the box, or it hits the TARDIS again
				GAMEPLAY::SHOOT_SINGLE_BULLET_BETWEEN_COORDS(from.x, from.y, from.z, head.x, head.y, head.z, 1000, TRUE, w, me, TRUE, FALSE, -1.0f);
				strike(p, 0.25f);
				flash(hitAt(pp));
				Log("shield: shot from ped %d returned", p);
			}
		}
		if (!PED::IS_PED_IN_ANY_VEHICLE(p, FALSE) && !ENTITY::IS_ENTITY_ATTACHED(p) && (pp - c).Len() < r) push(p, pp, true);
	}

	// projectiles inside the bubble go back to whoever fired them; anything else in it is pushed out
	static int objs[2048];
	int no = worldGetAllObjects(objs, 2048);
	for (int i = 0; i < no; i++)
	{
		Object o = objs[i];
		if (o == ext_) continue;
		V3 op = From(ENTITY::GET_ENTITY_COORDS(o, TRUE));
		if ((op - c).Len() > r) continue;
		const char *weapon = ReturnWeapon(ENTITY::GET_ENTITY_MODEL(o));
		if (!weapon)
		{
			if (!ENTITY::IS_ENTITY_ATTACHED(o)) push(o, op, false);
			continue;
		}
		// its owner: of the peds that fired lately, the one nearest the line it came in along
		V3 v = From(ENTITY::GET_ENTITY_VELOCITY(o)), back = v.Len() > 1 ? (v * -1.0f).Norm() : V3{};
		Ped owner = 0;
		float best = 1e9f;
		for (auto &[p, t] : lastShot_)
		{
			if (now - t > kShooterMemory || !ENTITY::DOES_ENTITY_EXIST(p) || ENTITY::IS_ENTITY_DEAD(p)) continue;
			V3 d = From(ENTITY::GET_ENTITY_COORDS(p, TRUE)) - op;
			float along = back.Len() > 0 ? d.Dot(back) : 0;
			if (back.Len() > 0 && along < 0) continue; // ahead of it, so not where it came from
			float off = back.Len() > 0 ? (d - back * along).Len() : d.Len();
			if (off < best) best = off, owner = p;
		}
		ENTITY::SET_ENTITY_AS_MISSION_ENTITY(o, TRUE, TRUE);
		OBJECT::DELETE_OBJECT(&o);
		flash(hitAt(op));
		if (!owner) { Log("shield: projectile stopped, no owner found"); continue; }
		V3 to = From(ENTITY::GET_ENTITY_COORDS(owner, TRUE)), from = hitAt(to);
		GAMEPLAY::SHOOT_SINGLE_BULLET_BETWEEN_COORDS(from.x, from.y, from.z, to.x, to.y, to.z, 1000, TRUE, H(weapon), me, TRUE, FALSE, -1.0f);
		strike(owner, (to - from).Len() / kProjectileSpeed + 0.5f);
		Log("shield: projectile returned to ped %d as %s", owner, weapon);
	}

	static int vehicles[1024];
	int nv = worldGetAllVehicles(vehicles, 1024);
	for (int i = 0; i < nv; i++)
	{
		Vehicle v = vehicles[i];
		if (v == ext_ || v == mine || VEHICLE::IS_THIS_MODEL_A_TRAIN(ENTITY::GET_ENTITY_MODEL(v))) continue;
		V3 vp = From(ENTITY::GET_ENTITY_COORDS(v, TRUE));
		if ((vp - c).Len() < r) push(v, vp, false);
	}

	for (auto it = strikes_.begin(); it != strikes_.end();)
	{
		if (now < it->at) { ++it; continue; }
		Entity t = it->target;
		if (ENTITY::DOES_ENTITY_EXIST(t) && !ENTITY::IS_ENTITY_DEAD(t))
		{
			if (ENTITY::IS_ENTITY_A_VEHICLE(t))
			{
				V3 at = From(ENTITY::GET_ENTITY_COORDS(t, TRUE));
				FIRE::ADD_OWNED_EXPLOSION(me, at.x, at.y, at.z, 7 /* car */, 1.0f, TRUE, FALSE, 1.0f);
			}
			// ponytail: a ped finished off this way isn't on the player's record; only the returned shot itself is
			else ENTITY::SET_ENTITY_HEALTH(t, 0);
		}
		it = strikes_.erase(it);
	}
	if (lastShot_.size() > 256) lastShot_.clear(), lastImpact_.clear(), pushedAt_.clear();
}

void GtaWorld::DrawShield()
{
	// shock arcs: a few jagged bolts from where it touched the bubble to the thing being thrown, redrawn every
	// frame so they crackle, with a blue light flickering at the contact
	for (auto &a : arcs_)
	{
		if (!ENTITY::DOES_ENTITY_EXIST(a.target)) { a.age = kArcTime; continue; }
		V3 to = From(ENTITY::GET_ENTITY_COORDS(a.target, TRUE)), span = to - a.from;
		V3 side = span.Cross(V3{0, 0, 1}).Norm(), up = side.Cross(span).Norm();
		auto jitter = [] { return (rand() % 1000) / 1000.0f - 0.5f; };
		for (int bolt = 0; bolt < 3; bolt++)
		{
			V3 p = a.from;
			for (int k = 1; k <= 6; k++)
			{
				V3 q = k == 6 ? to : a.from + span * (k / 6.0f) + side * (jitter() * 0.8f) + up * (jitter() * 0.8f);
				GRAPHICS::DRAW_LINE(p.x, p.y, p.z, q.x, q.y, q.z, 170, 230, 255, 255);
				p = q;
			}
		}
		if (rand() % 2) GRAPHICS::DRAW_LIGHT_WITH_RANGE(a.from.x, a.from.y, a.from.z, 120, 200, 255, 6.0f, 8.0f);
		a.age += GAMEPLAY::GET_FRAME_TIME();
	}
	arcs_.erase(std::remove_if(arcs_.begin(), arcs_.end(), [](const ShockArc &a) { return a.age >= kArcTime; }), arcs_.end());

	// the bubble shows for half a second after a hit (or the toggle): the whole sphere fades out, with sparks and a
	// bright spot where it was hit
	float dt = GAMEPLAY::GET_FRAME_TIME();
	if (flashes_.empty() || !HasExterior()) { flashes_.clear(); return; }
	V3 c = ShieldCentre();
	float r = ShieldRadius(), whole = 0;
	for (auto &f : flashes_)
	{
		if (f.age == 0 && !f.whole)
		{
			STREAMING::REQUEST_NAMED_PTFX_ASSET((char *)"core");
			GRAPHICS::_SET_PTFX_ASSET_NEXT_CALL((char *)"core");
			GRAPHICS::START_PARTICLE_FX_NON_LOOPED_AT_COORD((char *)"ent_brk_sparking_wires", f.at.x, f.at.y, f.at.z, 0, 0, 0, 1.5f, FALSE, FALSE, FALSE);
		}
		float k = 1 - f.age / kFlashTime;
		whole = std::max(whole, k);
		if (!f.whole)
			GRAPHICS::DRAW_MARKER(28, f.at.x, f.at.y, f.at.z, 0, 0, 0, 0, 0, 0, 0.6f, 0.6f, 0.6f, 150, 240, 255, (int)(220 * k),
			                      FALSE, FALSE, 2, FALSE, nullptr, nullptr, FALSE);
		f.age += dt;
	}
	// ponytail: marker 28's scale is taken as its radius; check the bubble's size against the 5 m in game
	GRAPHICS::DRAW_MARKER(28, c.x, c.y, c.z, 0, 0, 0, 0, 0, 0, r, r, r, 0, 190, 255, (int)(90 * whole), FALSE, FALSE, 2, FALSE,
	                      nullptr, nullptr, FALSE);
	flashes_.erase(std::remove_if(flashes_.begin(), flashes_.end(), [](const ShieldFlash &f) { return f.age >= kFlashTime; }), flashes_.end());
}
