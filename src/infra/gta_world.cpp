#include "gta_world.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <main.h>
#include <natives.h>
#include "log.h"

namespace
{
V3 From(const Vector3 &v) { return {v.x, v.y, v.z}; }
Ped Me() { return PLAYER::PLAYER_PED_ID(); }

enum Control
{
	LookLR = 1, LookUD = 2, Context = 51, PauseMenu = 199, PauseMenuAlt = 200, ScrollUp = 241, ScrollDown = 242
};
}

// ---------------------------------------------------------------- exterior

bool GtaWorld::SpawnExterior(Pose at)
{
	Hash h = GAMEPLAY::GET_HASH_KEY((char *)model_.c_str());
	if (!STREAMING::IS_MODEL_VALID(h)) { Log("model '%s' is not valid", model_.c_str()); return false; }
	STREAMING::REQUEST_MODEL(h);
	for (int i = 0; i < 200 && !STREAMING::HAS_MODEL_LOADED(h); i++) WAIT(0);
	if (!STREAMING::HAS_MODEL_LOADED(h)) { Log("model '%s' did not load", model_.c_str()); return false; }

	// the exterior can be a prop or a vehicle (the TardisV model is the add-on vehicle "tardis")
	if (STREAMING::IS_MODEL_A_VEHICLE(h))
	{
		ext_ = VEHICLE::CREATE_VEHICLE(h, at.pos.x, at.pos.y, at.pos.z, 0, FALSE, TRUE);
		VEHICLE::SET_VEHICLE_DOORS_LOCKED(ext_, 2); // you board through the TARDIS, not GTA's enter-vehicle
		// silence it (it is a Maverick underneath): a bicycle's audio profile has no engine or rotor sound, and an undriveable
		// vehicle never starts its engine
		invoke<Void>(0x4F0C413926060B38, ext_, "bmx"); // FORCE_USE_AUDIO_GAME_OBJECT
		VEHICLE::SET_VEHICLE_UNDRIVEABLE(ext_, TRUE);
		// indestructible (damage is off by default): no dents, nothing breaks off, nothing hurts it
		VEHICLE::SET_VEHICLE_CAN_BE_VISIBLY_DAMAGED(ext_, FALSE);
		VEHICLE::SET_VEHICLE_CAN_BREAK(ext_, FALSE);
		VEHICLE::SET_VEHICLE_STRONG(ext_, TRUE);
		VEHICLE::SET_VEHICLE_TYRES_CAN_BURST(ext_, FALSE);
		VEHICLE::SET_VEHICLE_EXPLODES_ON_HIGH_EXPLOSION_DAMAGE(ext_, FALSE);
		VEHICLE::SET_DISABLE_VEHICLE_PETROL_TANK_DAMAGE(ext_, TRUE);
		VEHICLE::SET_VEHICLE_ENGINE_CAN_DEGRADE(ext_, FALSE);
		Maintain();
	}
	else
		ext_ = OBJECT::CREATE_OBJECT(h, at.pos.x, at.pos.y, at.pos.z, FALSE, TRUE, TRUE);
	Vector3 lo, hi;
	GAMEPLAY::GET_MODEL_DIMENSIONS(h, &lo, &hi);
	baseZ_ = -lo.z;
	boxLo_ = {lo.x, lo.y, lo.z};
	boxHi_ = {hi.x, hi.y, hi.z};
	topZ_ = hi.z;
	STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(h);
	ENTITY::SET_ENTITY_AS_MISSION_ENTITY(ext_, TRUE, TRUE);
	ENTITY::SET_ENTITY_QUATERNION(ext_, at.rot.x, at.rot.y, at.rot.z, at.rot.w);
	ENTITY::SET_ENTITY_DYNAMIC(ext_, TRUE);
	ENTITY::SET_ENTITY_LOAD_COLLISION_FLAG(ext_, TRUE);
	ENTITY::SET_ENTITY_INVINCIBLE(ext_, TRUE); // damage is off by default; stage 4 adds health
	ENTITY::SET_ENTITY_PROOFS(ext_, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);
	ENTITY::SET_ENTITY_CAN_BE_DAMAGED(ext_, FALSE);
	ROPE::ACTIVATE_PHYSICS(ext_);
	Log("exterior %d spawned at %.2f %.2f %.2f (%s)", ext_, at.pos.x, at.pos.y, at.pos.z, model_.c_str());
	return true;
}

void GtaWorld::DespawnExterior()
{
	if (HasExterior()) ENTITY::DELETE_ENTITY(&ext_);
	ext_ = 0;
	Log("exterior removed");
}

bool GtaWorld::HasExterior() { return ext_ && ENTITY::DOES_ENTITY_EXIST(ext_); }

Pose GtaWorld::ExteriorPose()
{
	Pose p{From(ENTITY::GET_ENTITY_COORDS(ext_, TRUE)), {}};
	ENTITY::GET_ENTITY_QUATERNION(ext_, &p.rot.x, &p.rot.y, &p.rot.z, &p.rot.w);
	return p;
}

V3 GtaWorld::ExteriorVelocity() { return From(ENTITY::GET_ENTITY_VELOCITY(ext_)); }

void GtaWorld::Maintain()
{
	if (policeBlind_ && !policeBlindNow_)
	{
		PLAYER::SET_MAX_WANTED_LEVEL(5);
		PLAYER::SET_POLICE_IGNORE_PLAYER(PLAYER::PLAYER_ID(), FALSE);
		Log("police: can see the player again");
	}
	policeBlind_ = policeBlindNow_;
	policeBlindNow_ = false;
	if (!trainsHeldNow_ && !heldTrains_.empty())
	{
		for (auto &[engine, speed] : heldTrains_)
			if (ENTITY::DOES_ENTITY_EXIST(engine)) VEHICLE::SET_TRAIN_CRUISE_SPEED(engine, speed);
		heldTrains_.clear();
		Log("physlock: trains go on");
	}
	trainsHeldNow_ = false;
	DrawShield();

	if (!HasExterior() || !ENTITY::IS_ENTITY_A_VEHICLE(ext_)) return;
	VEHICLE::SET_VEHICLE_ENGINE_ON(ext_, FALSE, TRUE, TRUE); // airborne helicopters try to spin up
	invoke<Void>(0xFD280B4D7F3ABC4D, ext_, 0.0f);          // SET_HELI_BLADES_SPEED
	if (VEHICLE::GET_VEHICLE_BODY_HEALTH(ext_) < 1000.0f)    // anything that still got through
	{
		VEHICLE::SET_VEHICLE_FIXED(ext_);
		VEHICLE::SET_VEHICLE_DEFORMATION_FIXED(ext_);
		VEHICLE::SET_VEHICLE_ENGINE_ON(ext_, FALSE, TRUE, TRUE);
	}
}

void GtaWorld::DriveExterior(V3 w, V3 v)
{
	ENTITY::SET_ENTITY_VELOCITY(ext_, v.x, v.y, v.z);
	invoke<Void>(0x8339643499D1222E, ext_, w.x, w.y, w.z); // SET_ENTITY_ANGULAR_VELOCITY
}

void GtaWorld::SetExteriorPhysics(bool gravity, bool frozen)
{
	ENTITY::SET_ENTITY_HAS_GRAVITY(ext_, gravity);
	if (ENTITY::IS_ENTITY_A_VEHICLE(ext_)) VEHICLE::SET_VEHICLE_GRAVITY(ext_, gravity); // vehicles ignore the entity flag
	ENTITY::FREEZE_ENTITY_POSITION(ext_, frozen);
	if (!frozen) ROPE::ACTIVATE_PHYSICS(ext_);
	Log("exterior physics: gravity %d frozen %d", gravity, frozen);
}

void GtaWorld::SetExteriorAngularVelocity(V3 w)
{
	invoke<Void>(0x8339643499D1222E, ext_, w.x, w.y, w.z); // SET_ENTITY_ANGULAR_VELOCITY
	Log("exterior angular velocity %.2f %.2f %.2f", w.x, w.y, w.z);
}

void GtaWorld::PlaceExterior(Pose at)
{
	ENTITY::SET_ENTITY_COORDS_NO_OFFSET(ext_, at.pos.x, at.pos.y, at.pos.z + baseZ_, FALSE, FALSE, FALSE);
	ENTITY::SET_ENTITY_QUATERNION(ext_, at.rot.x, at.rot.y, at.rot.z, at.rot.w);
	ENTITY::SET_ENTITY_VELOCITY(ext_, 0, 0, 0);
	Log("exterior placed with its base at %.2f %.2f %.2f (origin +%.2f)", at.pos.x, at.pos.y, at.pos.z, baseZ_);
}

void GtaWorld::SetExteriorAlpha(int a)
{
	if (a >= 255) ENTITY::RESET_ENTITY_ALPHA(ext_);
	else ENTITY::SET_ENTITY_ALPHA(ext_, a, FALSE);
}

void GtaWorld::ShowLamp(LampLight l)
{
	if (!l.on || !HasExterior()) return;
	Vector3 p = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(ext_, 0, 0, topZ_ - lamp_.belowTop);
	// GMod's dynamic light (5 m, brightness 2.5, radius pulsing), plus a tight bright one on the lens for its sprite
	GRAPHICS::DRAW_LIGHT_WITH_RANGE(p.x, p.y, p.z, lamp_.r, lamp_.g, lamp_.b, 5.0f * l.size, 2.5f);
	GRAPHICS::DRAW_LIGHT_WITH_RANGE(p.x, p.y, p.z + 0.1f, lamp_.r, lamp_.g, lamp_.b, 0.8f, 20.0f * l.glow);
}

// ---------------------------------------------------------------- player

Pose GtaWorld::PlayerPose() { return {From(ENTITY::GET_ENTITY_COORDS(Me(), TRUE)), Quat::Yaw(ENTITY::GET_ENTITY_HEADING(Me()))}; }

bool GtaWorld::PlayerInVehicle() { return PED::IS_PED_IN_ANY_VEHICLE(Me(), FALSE); }

void GtaWorld::Board()
{
	Ped ped = Me();
	ENTITY::SET_ENTITY_VISIBLE(ped, FALSE, FALSE);
	ENTITY::SET_ENTITY_COLLISION(ped, FALSE, FALSE);
	ENTITY::SET_ENTITY_INVINCIBLE(ped, TRUE);
	if (ENTITY::IS_ENTITY_A_VEHICLE(ext_)) PED::SET_PED_INTO_VEHICLE(ped, ext_, -1);
	if (PED::IS_PED_IN_VEHICLE(ped, ext_, FALSE))
	{
		// in the pilot's seat, so the AI sees the player in their vehicle and shoots it (tanks included). Attached
		// instead, the player was an invisible ped on foot that nobody could find. The doors stay locked: no jacking.
		PED::SET_PED_CAN_BE_DRAGGED_OUT(ped, FALSE);
		PED::SET_PED_CAN_BE_KNOCKED_OFF_VEHICLE(ped, 1);
		Log("player boarded (pilot's seat)");
	}
	else // a prop exterior, or a model with no seat
	{
		ENTITY::ATTACH_ENTITY_TO_ENTITY(ped, ext_, 0, 0, 0, 1.0f, 0, 0, 0, FALSE, FALSE, FALSE, FALSE, 2, TRUE);
		Log("player boarded (attached: no seat)");
	}
}

void GtaWorld::Unboard(V3 exit, bool parachute)
{
	Ped ped = Me();
	if (PED::IS_PED_IN_ANY_VEHICLE(ped, FALSE)) AI::CLEAR_PED_TASKS_IMMEDIATELY(ped); // out of the seat at once
	else ENTITY::DETACH_ENTITY(ped, TRUE, FALSE);
	PED::SET_PED_CAN_BE_DRAGGED_OUT(ped, TRUE);
	PED::SET_PED_CAN_BE_KNOCKED_OFF_VEHICLE(ped, 0);
	ENTITY::SET_ENTITY_COORDS_NO_OFFSET(ped, exit.x, exit.y, exit.z, FALSE, FALSE, FALSE);
	ENTITY::SET_ENTITY_VISIBLE(ped, TRUE, FALSE);
	ENTITY::SET_ENTITY_COLLISION(ped, TRUE, TRUE);
	ENTITY::SET_ENTITY_INVINCIBLE(ped, FALSE);
	// GADGET_PARACHUTE: the game lets them open it as they fall (F / attack)
	if (parachute) WEAPON::GIVE_WEAPON_TO_PED(ped, GAMEPLAY::GET_HASH_KEY((char *)"GADGET_PARACHUTE"), 1, FALSE, TRUE);
	Log("player out at %.2f %.2f %.2f%s", exit.x, exit.y, exit.z, parachute ? " (in the air, parachute)" : "");
}

// ---------------------------------------------------------------- queries

float GtaWorld::GroundZ(V3 p)
{
	float z;
	STREAMING::REQUEST_COLLISION_AT_COORD(p.x, p.y, p.z);
	return GAMEPLAY::GET_GROUND_Z_FOR_3D_COORD(p.x, p.y, p.z, &z, FALSE) ? z : p.z;
}

std::optional<V3> GtaWorld::SafeSpot(V3 p)
{
	// GSC flags: 2 not isolated, 4 not interior, 8 not water. Strictest first; the log says which one worked.
	// ponytail: 4 alone may still pick a rooftop or a cliff ledge; tighten once TARDIS.log shows what each flag set returns
	for (int flags : {2 | 4 | 8, 4 | 8, 4})
	{
		Vector3 out;
		if (PATHFIND::GET_SAFE_COORD_FOR_PED(p.x, p.y, p.z, TRUE, &out, flags))
		{
			Log("safe-coord: flags %d", flags);
			return From(out);
		}
	}
	return std::nullopt;
}

void GtaWorld::Preload(std::optional<V3> at)
{
	PATHFIND::REMOVE_NAVMESH_REQUIRED_REGIONS();
	if (at) PATHFIND::ADD_NAVMESH_REQUIRED_REGION(at->x, at->y, 50.0f);
}

bool GtaWorld::FocusLoaded() { return STREAMING::IS_NEW_LOAD_SCENE_LOADED(); }

void GtaWorld::FadeScreen(bool out)
{
	if (out) CAM::DO_SCREEN_FADE_OUT(500);
	else CAM::DO_SCREEN_FADE_IN(500);
}

V3 GtaWorld::RoadNode(V3 p)
{
	Vector3 out;
	return PATHFIND::GET_CLOSEST_VEHICLE_NODE(p.x, p.y, p.z, &out, 1, 3.0f, 0) ? From(out) : p;
}

void GtaWorld::ClearArea(V3 at, float r) { GAMEPLAY::CLEAR_AREA(at.x, at.y, at.z, r, TRUE, FALSE, FALSE, FALSE); }

void GtaWorld::FocusOn(std::optional<V3> at)
{
	if (at)
	{
		STREAMING::_SET_FOCUS_AREA(at->x, at->y, at->z, 0, 0, 0);       // SET_FOCUS_POS_AND_VEL
		invoke<BOOL>(0xACCFB4ACF53551B0, at->x, at->y, at->z, 150.0f, 0); // NEW_LOAD_SCENE_START_SPHERE
	}
	else
	{
		STREAMING::NEW_LOAD_SCENE_STOP();
		STREAMING::CLEAR_FOCUS();
	}
}

std::optional<V3> GtaWorld::Waypoint()
{
	if (!UI::IS_WAYPOINT_ACTIVE()) return std::nullopt;
	return From(UI::GET_BLIP_INFO_ID_COORD(UI::GET_FIRST_BLIP_INFO_ID(8))); // 8: the waypoint's sprite
}

V3 GtaWorld::CastToWorld(V3 a, V3 b)
{
	BOOL hit = FALSE;
	Vector3 end, normal;
	Entity ent;
	// START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE: _CAST_RAY_POINT_TO_POINT is the async probe, still pending
	// when read in the same frame, so it reported no hit almost every time
	int ray = WORLDPROBE::_0x7EE9F5D83DD4F90E(a.x, a.y, a.z, b.x, b.y, b.z, 1 /* map */, ext_, 7);
	WORLDPROBE::_GET_RAYCAST_RESULT(ray, &hit, &end, &normal, &ent);
	return hit ? From(end) : b;
}

View GtaWorld::GameplayView()
{
	Vector3 r = CAM::GET_GAMEPLAY_CAM_ROT(2);
	return {r.x, r.z};
}

// ---------------------------------------------------------------- presentation

void GtaWorld::ShowCamera(std::optional<CameraShot> shot)
{
	if (!shot)
	{
		if (!cam_) return;
		CAM::RENDER_SCRIPT_CAMS(FALSE, FALSE, 0, TRUE, FALSE);
		CAM::DESTROY_CAM(cam_, FALSE);
		cam_ = 0;
		UI::UNLOCK_MINIMAP_ANGLE();
		return;
	}
	if (!cam_)
	{
		cam_ = CAM::CREATE_CAM((char *)"DEFAULT_SCRIPTED_CAMERA", TRUE);
		CAM::SET_CAM_FOV(cam_, CAM::GET_GAMEPLAY_CAM_FOV());
		CAM::RENDER_SCRIPT_CAMS(TRUE, FALSE, 0, TRUE, FALSE);
	}
	CAM::SET_CAM_COORD(cam_, shot->pos.x, shot->pos.y, shot->pos.z);
	CAM::SET_CAM_ROT(cam_, shot->view.pitch, 0, shot->view.yaw, 2);
	// seated, the minimap turns with the vehicle, so a spinning TARDIS spun the map; it follows the camera instead
	UI::LOCK_MINIMAP_ANGLE(((int)std::lround(shot->view.yaw) % 360 + 360) % 360);
}

void GtaWorld::LockControls()
{
	CONTROLS::DISABLE_ALL_CONTROL_ACTIONS(0);
	CONTROLS::ENABLE_CONTROL_ACTION(0, PauseMenu, TRUE);
	CONTROLS::ENABLE_CONTROL_ACTION(0, PauseMenuAlt, TRUE);
	UI::HIDE_HUD_COMPONENT_THIS_FRAME(6); // the vehicle name the seat would pop up
}

void GtaWorld::HidePlayer()
{
	Ped ped = Me();
	ENTITY::SET_ENTITY_VISIBLE(ped, FALSE, FALSE); // water and cutscene logic turn the ped visible again
	WEAPON::SET_CURRENT_PED_WEAPON(ped, GAMEPLAY::GET_HASH_KEY((char *)"WEAPON_UNARMED"), TRUE);
	PLAYER::DISABLE_PLAYER_FIRING(PLAYER::PLAYER_ID(), TRUE); // left click beeped through the disabled controls
}

void GtaWorld::Explode(float magnitude)
{
	if (!HasExterior()) return;
	V3 at = ExteriorPose().pos;
	// EXP_TAG_GRENADE (0), audible, visible, shaking. Not owned: no crime. The TARDIS and its pilot are proofed.
	// ponytail: magnitude -> damage scale is a guess (GMod's 60 = 0.6); tune once it's seen in game
	FIRE::ADD_EXPLOSION(at.x, at.y, at.z, 0, std::min(magnitude / 100.0f, 1.0f), TRUE, FALSE, 1.0f);
	Log("explosion at %.2f %.2f %.2f (magnitude %.0f)", at.x, at.y, at.z, magnitude);
}

void GtaWorld::ClearWanted()
{
	Player me = PLAYER::PLAYER_ID();
	if (PLAYER::GET_PLAYER_WANTED_LEVEL(me) == 0) return;
	PLAYER::CLEAR_PLAYER_WANTED_LEVEL(me);
	Log("police: wanted level cleared (demat)");
}

void GtaWorld::HideFromPolice()
{
	Player me = PLAYER::PLAYER_ID();
	if (PLAYER::GET_PLAYER_WANTED_LEVEL(me) > 0)
	{
		PLAYER::CLEAR_PLAYER_WANTED_LEVEL(me);
		Log("police: wanted level cleared");
	}
	PLAYER::SET_MAX_WANTED_LEVEL(0); // restricted areas (Fort Zancudo, the airport) can't raise one either
	PLAYER::SET_POLICE_IGNORE_PLAYER(me, TRUE);
	policeBlindNow_ = true;
}

bool GtaWorld::HoldTrains()
{
	trainsHeldNow_ = true;
	if (!HasExterior()) return false;
	V3 at = ExteriorPose().pos;
	static int vehicles[1024];
	int n = worldGetAllVehicles(vehicles, 1024);
	// carriage -> the engine that drives it (only an engine takes SET_TRAIN_SPEED)
	std::map<int, int> engineOf;
	for (int i = 0; i < n; i++)
	{
		int v = vehicles[i];
		if (!VEHICLE::IS_THIS_MODEL_A_TRAIN(ENTITY::GET_ENTITY_MODEL(v))) continue;
		if (!engineOf.count(v)) engineOf[v] = v;
		for (int c = 1; c < 32; c++)
		{
			int car = VEHICLE::GET_TRAIN_CARRIAGE(v, c);
			if (!car) break;
			engineOf[car] = v;
		}
	}
	// held: every train with a carriage against the TARDIS; one that isn't any more (it travelled off) goes on
	bool hit = false;
	std::map<int, float> held;
	for (auto &[car, engine] : engineOf)
	{
		if (held.count(engine)) continue;
		// the TARDIS in the carriage's own frame, inside its box grown by about the TARDIS's half width
		Vector3 lo, hi, p = ENTITY::GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS(car, at.x, at.y, at.z);
		GAMEPLAY::GET_MODEL_DIMENSIONS(ENTITY::GET_ENTITY_MODEL(car), &lo, &hi);
		const float m = 1.0f;
		if (p.x < lo.x - m || p.x > hi.x + m || p.y < lo.y - m || p.y > hi.y + m || p.z < lo.z - 2 || p.z > hi.z + 2) continue;
		if (auto it = heldTrains_.find(engine); it != heldTrains_.end()) { held[engine] = it->second; continue; }
		held[engine] = std::max(ENTITY::GET_ENTITY_SPEED(engine), 5.0f);
		Log("physlock: train %d (engine %d) stopped at %.1f m/s", car, engine, held[engine]);
		hit = true;
	}
	for (auto &[engine, speed] : heldTrains_)
		if (!held.count(engine) && ENTITY::DOES_ENTITY_EXIST(engine)) VEHICLE::SET_TRAIN_CRUISE_SPEED(engine, speed);
	heldTrains_ = held;
	for (auto &[engine, speed] : heldTrains_)
	{
		VEHICLE::SET_TRAIN_SPEED(engine, 0);
		VEHICLE::SET_TRAIN_CRUISE_SPEED(engine, 0);
	}
	return hit;
}

void GtaWorld::ShakeCamera()
{
	if (cam_) CAM::SHAKE_CAM(cam_, (char *)"SMALL_EXPLOSION_SHAKE", 0.6f);
	else CAM::SHAKE_GAMEPLAY_CAM((char *)"SMALL_EXPLOSION_SHAKE", 0.6f);
}

void GtaWorld::BlockPhone()
{
	// story calls ring through the player's ringtone and pop the phone up; kill both every frame so a
	// call can't even start ringing, and hang up anything that still connects
	if (AUDIO::IS_PED_RINGTONE_PLAYING(Me())) Log("phone: ringtone blocked");
	AUDIO::STOP_PED_RINGTONE(Me());
	MOBILE::DESTROY_MOBILE_PHONE();
	if (AUDIO::IS_MOBILE_PHONE_CALL_ONGOING())
	{
		AUDIO::STOP_SCRIPTED_CONVERSATION(FALSE);
		Log("phone: call hung up");
	}
}

void GtaWorld::Hint(const std::string &text)
{
	UI::_SET_TEXT_COMPONENT_FORMAT((char *)"STRING");
	UI::_ADD_TEXT_COMPONENT_STRING((char *)text.c_str());
	UI::_DISPLAY_HELP_TEXT_FROM_STRING_LABEL(0, FALSE, FALSE, -1);
}

namespace
{
struct Rgb { int r, g, b; };
constexpr Rgb kCyan{0, 190, 255}, kDimCyan{0, 70, 110}, kAmber{255, 190, 70}, kWhite{235, 245, 255}, kGrey{110, 125, 140};

void Rect(float x, float y, float w, float h, Rgb c, int a) { GRAPHICS::DRAW_RECT(x + w / 2, y + h / 2, w, h, c.r, c.g, c.b, a); }

// left-aligned at x, or right-aligned to x when right; scale is GTA's text scale
void Label(const std::string &s, float x, float y, Rgb c, float scale, bool right = false, int font = 4)
{
	UI::SET_TEXT_FONT(font);
	UI::SET_TEXT_SCALE(0.0f, scale);
	UI::SET_TEXT_COLOUR(c.r, c.g, c.b, 255);
	UI::SET_TEXT_DROPSHADOW(1, 0, 0, 0, 200);
	if (right) { UI::SET_TEXT_RIGHT_JUSTIFY(TRUE); UI::SET_TEXT_WRAP(0.0f, x); }
	UI::_SET_TEXT_ENTRY((char *)"STRING");
	UI::_ADD_TEXT_COMPONENT_STRING((char *)s.c_str());
	UI::_DRAW_TEXT(right ? 0.0f : x, y);
}
}

void GtaWorld::DrawPanel(const Panel &panel)
{
	// a console on the right edge, vertically centred: a cyan frame and title bar, one light per system,
	// keys in amber caps, and the journey's progress bar under its row
	const float aspect = GRAPHICS::_GET_SCREEN_ASPECT_RATIO(FALSE);
	const float w = 0.205f, rowH = 0.027f, titleH = 0.034f, pad = 0.008f, edge = 0.0015f, barH = 0.006f;
	float h = titleH + pad;
	for (auto &r : panel.rows) h += rowH + (r.kind == PanelRow::Kind::Footer ? pad : 0) + (r.progress >= 0 ? rowH : 0);
	h += pad;
	float x = 0.99f - w, y = 0.5f - h / 2, right = x + w - pad;

	Rect(x, y, w, h, {4, 12, 28}, 205);                    // body
	Rect(x, y, w, edge * aspect, kCyan, 230);              // frame
	Rect(x, y + h - edge * aspect, w, edge * aspect, kCyan, 230);
	Rect(x, y, edge, h, kCyan, 230);
	Rect(x + w - edge, y, edge, h, kCyan, 230);
	Rect(x, y, w, titleH, kCyan, 45);                      // title bar
	Label(panel.title, x + pad, y + 0.004f, kCyan, 0.50f);
	Rect(right - 0.012f, y + titleH / 2 - 0.004f * aspect, 0.008f, 0.008f * aspect, kCyan, 200); // a lamp in the corner

	auto keyCap = [&](const std::string &k, float rx, float ty) { // amber key, right-aligned to rx; returns its left edge
		// sized to the text as drawn (font 4 is narrow, so a per-letter guess made long keys far too wide)
		UI::SET_TEXT_FONT(4);
		UI::SET_TEXT_SCALE(0.0f, 0.36f);
		UI::_SET_TEXT_ENTRY_FOR_WIDTH((char *)"STRING");
		UI::_ADD_TEXT_COMPONENT_STRING((char *)k.c_str());
		float kw = 0.006f + UI::_GET_TEXT_SCREEN_WIDTH(TRUE);
		Rect(rx - kw, ty + 0.003f, kw, rowH - 0.006f, kAmber, 40);
		Label(k, rx - 0.003f, ty + 0.002f, kAmber, 0.36f, true);
		return rx - kw;
	};
	const float valueX = right, keyX = x + w * 0.62f, labelX = x + pad + 0.012f;
	float ty = y + titleH + pad / 2;
	for (auto &r : panel.rows)
	{
		using K = PanelRow::Kind;
		switch (r.kind)
		{
		case K::System:
		{
			float led = 0.0065f;
			if (r.on) Rect(x + pad - led * 0.4f, ty + rowH / 2 - led * aspect * 0.9f, led * 1.8f, led * aspect * 1.8f, kCyan, 50); // glow
			Rect(x + pad, ty + rowH / 2 - led * aspect / 2, led, led * aspect, r.on ? kCyan : kDimCyan, 255);
			Label(r.label, labelX, ty + 0.002f, r.on ? kWhite : kGrey, 0.40f);
			keyCap(r.key, keyX, ty);
			Label(r.value, valueX, ty + 0.002f, r.on ? kCyan : kGrey, 0.40f, true);
			break;
		}
		case K::Action:
			Rect(x + pad, ty + 0.003f, edge * 2, rowH - 0.006f, kCyan, 200);
			Label(r.label, labelX, ty + 0.002f, kWhite, 0.40f);
			keyCap(r.key, valueX, ty);
			if (r.progress >= 0)
			{
				ty += rowH;
				Label(r.value, labelX, ty + 0.002f, kCyan, 0.36f);
				float bx = x + w * 0.55f, bw = right - bx, by = ty + rowH / 2 - barH / 2;
				Rect(bx, by, bw, barH, kDimCyan, 160);
				Rect(bx, by, bw * std::clamp(r.progress, 0.0f, 1.0f), barH, kCyan, 255);
			}
			break;
		case K::Reading:
			Label(r.label, labelX, ty + 0.002f, kGrey, 0.38f);
			keyCap(r.key, keyX, ty);
			Label(r.value, valueX, ty + 0.002f, kWhite, 0.38f, true);
			break;
		case K::Footer:
			ty += pad;
			Rect(x + pad, ty - pad / 2, w - 2 * pad, edge, kCyan, 120);
			Label(r.label, labelX, ty + 0.002f, kGrey, 0.38f);
			keyCap(r.key, valueX, ty);
			break;
		}
		ty += rowH;
	}
}

void GtaWorld::Notify(const std::string &text)
{
	UI::_SET_NOTIFICATION_TEXT_ENTRY((char *)"STRING");
	UI::_ADD_TEXT_COMPONENT_STRING((char *)text.c_str());
	UI::_DRAW_NOTIFICATION(FALSE, TRUE);
	Log("msg: %s", text.c_str());
}

void GtaWorld::Trace(const std::string &text) { Log("%s", text.c_str()); }

// ---------------------------------------------------------------- input

FrameInput GtaInput::Poll()
{
	// we run inside the game, so "focused" = the foreground window is ours (Legacy calls it grcWindow,
	// Enhanced sgaWindow)
	DWORD pid = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &pid);
	bool focused = pid == GetCurrentProcessId();

	FrameInput in;
	for (size_t i = 0; i < keys_.size(); i++)
	{
		bool d = focused && (GetAsyncKeyState(keys_[i]) & 0x8000);
		in.down[i] = d;
		in.pressed[i] = d && !last_.down[i];
	}
	// the "disabled" queries also read controls that aren't disabled, so they work piloting or not
	in.use = CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, Context);
	in.lookX = CONTROLS::GET_DISABLED_CONTROL_NORMAL(0, LookLR);
	in.lookY = CONTROLS::GET_DISABLED_CONTROL_NORMAL(0, LookUD);
	in.wheel = (CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, ScrollUp) ? 1 : 0) -
	           (CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, ScrollDown) ? 1 : 0);
	last_ = in;
	return in;
}
