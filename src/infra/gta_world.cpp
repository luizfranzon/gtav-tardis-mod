#include "gta_world.h"
#include <windows.h>
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

	// the exterior can be a prop or a replaced vehicle (the TardisV model replaces the Maverick)
	if (STREAMING::IS_MODEL_A_VEHICLE(h))
	{
		ext_ = VEHICLE::CREATE_VEHICLE(h, at.pos.x, at.pos.y, at.pos.z, 0, FALSE, TRUE);
		VEHICLE::SET_VEHICLE_DOORS_LOCKED(ext_, 2); // you board through the TARDIS, not GTA's enter-vehicle
		// silence the Maverick: a bicycle's audio profile has no engine or rotor sound, and an undriveable
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

void GtaWorld::DriveExterior(Quat r, V3 v)
{
	ENTITY::SET_ENTITY_VELOCITY(ext_, v.x, v.y, v.z);
	ENTITY::SET_ENTITY_QUATERNION(ext_, r.x, r.y, r.z, r.w);
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

// ---------------------------------------------------------------- player

Pose GtaWorld::PlayerPose() { return {From(ENTITY::GET_ENTITY_COORDS(Me(), TRUE)), Quat::Yaw(ENTITY::GET_ENTITY_HEADING(Me()))}; }

bool GtaWorld::PlayerInVehicle() { return PED::IS_PED_IN_ANY_VEHICLE(Me(), FALSE); }

void GtaWorld::Board()
{
	Ped ped = Me();
	ENTITY::SET_ENTITY_VISIBLE(ped, FALSE, FALSE);
	ENTITY::SET_ENTITY_COLLISION(ped, FALSE, FALSE);
	ENTITY::SET_ENTITY_INVINCIBLE(ped, TRUE);
	ENTITY::ATTACH_ENTITY_TO_ENTITY(ped, ext_, 0, 0, 0, 1.0f, 0, 0, 0, FALSE, FALSE, FALSE, FALSE, 2, TRUE);
	Log("player boarded");
}

void GtaWorld::Unboard(V3 exit)
{
	Ped ped = Me();
	ENTITY::DETACH_ENTITY(ped, TRUE, FALSE);
	ENTITY::SET_ENTITY_COORDS_NO_OFFSET(ped, exit.x, exit.y, exit.z, FALSE, FALSE, FALSE);
	ENTITY::SET_ENTITY_VISIBLE(ped, TRUE, FALSE);
	ENTITY::SET_ENTITY_COLLISION(ped, TRUE, TRUE);
	ENTITY::SET_ENTITY_INVINCIBLE(ped, FALSE);
	Log("player out at %.2f %.2f %.2f", exit.x, exit.y, exit.z);
}

// ---------------------------------------------------------------- queries

float GtaWorld::GroundZ(V3 p)
{
	float z;
	return GAMEPLAY::GET_GROUND_Z_FOR_3D_COORD(p.x, p.y, p.z, &z, FALSE) ? z : p.z;
}

V3 GtaWorld::CastToWorld(V3 a, V3 b)
{
	BOOL hit = FALSE;
	Vector3 end, normal;
	Entity ent;
	int ray = WORLDPROBE::_CAST_RAY_POINT_TO_POINT(a.x, a.y, a.z, b.x, b.y, b.z, 1 /* map */, ext_, 7);
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
}

void GtaWorld::LockControls()
{
	CONTROLS::DISABLE_ALL_CONTROL_ACTIONS(0);
	CONTROLS::ENABLE_CONTROL_ACTION(0, PauseMenu, TRUE);
	CONTROLS::ENABLE_CONTROL_ACTION(0, PauseMenuAlt, TRUE);
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

void GtaWorld::DrawLegend(const std::vector<LegendLine> &lines)
{
	// right edge, vertically centred, on a translucent panel; headings in TARDIS blue, keys in amber
	const float lineH = 0.022f, w = 0.20f, x = 0.995f - w, pad = 0.006f;
	float h = lines.size() * lineH + 2 * pad, y = 0.5f - h / 2;
	GRAPHICS::DRAW_RECT(x + w / 2, y + h / 2, w, h, 0, 0, 0, 140);
	auto text = [](const std::string &s, float tx, float ty, int r, int g, int b, float rightEdge) {
		UI::SET_TEXT_FONT(0);
		UI::SET_TEXT_SCALE(0.0f, 0.30f);
		UI::SET_TEXT_COLOUR(r, g, b, 255);
		UI::SET_TEXT_DROPSHADOW(1, 0, 0, 0, 255);
		if (rightEdge > 0) { UI::SET_TEXT_RIGHT_JUSTIFY(TRUE); UI::SET_TEXT_WRAP(0.0f, rightEdge); }
		UI::_SET_TEXT_ENTRY((char *)"STRING");
		UI::_ADD_TEXT_COMPONENT_STRING((char *)s.c_str());
		UI::_DRAW_TEXT(tx, ty);
	};
	float ly = y + pad;
	for (auto &l : lines)
	{
		if (l.key.empty()) text(l.text, x + pad, ly, 110, 170, 255, 0);
		else
		{
			text(l.key, x + pad, ly, 255, 220, 120, 0);
			text(l.text, 0, ly, 255, 255, 255, x + w - pad);
		}
		ly += lineH;
	}
}

void GtaWorld::Notify(const std::string &text)
{
	UI::_SET_NOTIFICATION_TEXT_ENTRY((char *)"STRING");
	UI::_ADD_TEXT_COMPONENT_STRING((char *)text.c_str());
	UI::_DRAW_NOTIFICATION(FALSE, TRUE);
	Log("msg: %s", text.c_str());
}

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
