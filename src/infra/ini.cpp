#include "ini.h"
#include <windows.h>
#include <cctype>
#include <cstdio>
#include <map>
#include "log.h"

namespace
{
std::string Str(const std::string &file, const char *sec, const char *key, const char *def)
{
	char buf[256];
	GetPrivateProfileStringA(sec, key, def, buf, sizeof buf, file.c_str());
	return buf;
}

float Num(const std::string &file, const char *sec, const char *key, float def)
{
	std::string v = Str(file, sec, key, "");
	return v.empty() ? def : (float)atof(v.c_str());
}
}

int KeyCode(std::string name)
{
	for (auto &c : name) c = (char)toupper((unsigned char)c);
	if (name.rfind("KEY_", 0) == 0) name = name.substr(4);
	static const std::map<std::string, int> named = {
		{"SPACE", VK_SPACE}, {"LSHIFT", VK_LSHIFT}, {"RSHIFT", VK_RSHIFT}, {"LCONTROL", VK_LCONTROL},
		{"RCONTROL", VK_RCONTROL}, {"LALT", VK_LMENU}, {"RALT", VK_RMENU}, {"TAB", VK_TAB}, {"ENTER", VK_RETURN},
		{"BACKSPACE", VK_BACK}, {"LBRACKET", VK_OEM_4}, {"RBRACKET", VK_OEM_6}, {"MOUSE_LEFT", VK_LBUTTON},
		{"MOUSE_RIGHT", VK_RBUTTON}, {"MOUSE_MIDDLE", VK_MBUTTON}, {"UP", VK_UP}, {"DOWN", VK_DOWN},
		{"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT}, {"INSERT", VK_INSERT}, {"DELETE", VK_DELETE}, {"HOME", VK_HOME},
		{"END", VK_END}, {"PAGEUP", VK_PRIOR}, {"PAGEDOWN", VK_NEXT}};
	if (auto it = named.find(name); it != named.end()) return it->second;
	if (name.size() == 1 && isalnum((unsigned char)name[0])) return name[0];
	if (name.size() >= 2 && name[0] == 'F')
	{
		int n = atoi(name.c_str() + 1);
		if (n >= 1 && n <= 24) return VK_F1 + n - 1;
	}
	return 0;
}

std::string KeyLabel(std::string name)
{
	for (auto &c : name) c = (char)toupper((unsigned char)c);
	if (name.rfind("KEY_", 0) == 0) name = name.substr(4);
	static const std::map<std::string, std::string> pretty = {
		{"SPACE", "Space"}, {"LSHIFT", "Shift"}, {"RSHIFT", "R.Shift"}, {"LCONTROL", "Ctrl"}, {"RCONTROL", "R.Ctrl"},
		{"LALT", "Alt"}, {"RALT", "R.Alt"}, {"MOUSE_LEFT", "Left mouse"}, {"MOUSE_RIGHT", "Right mouse"},
		{"MOUSE_MIDDLE", "Middle mouse"}, {"LBRACKET", "["}, {"RBRACKET", "]"}, {"ENTER", "Enter"}, {"TAB", "Tab"}};
	auto it = pretty.find(name);
	return it != pretty.end() ? it->second : name;
}

ModConfig LoadConfig(const std::string &f)
{
	ModConfig c;
	c.tardis.flight.boostSpeed = Num(f, "Flight", "BoostSpeed", c.tardis.flight.boostSpeed);
	c.tardis.flight.stabilizeK = Num(f, "Physics", "StabilizeK", c.tardis.flight.stabilizeK);
	c.tardis.mouseSensitivity = Num(f, "Camera", "MouseSensitivity", c.tardis.mouseSensitivity);
	c.tardis.exitOffset = Num(f, "Exterior", "ExitOffset", c.tardis.exitOffset);
	c.soundVolume = Num(f, "Sound", "Volume", c.soundVolume);
	c.tardis.legend = Num(f, "HUD", "Legend", 1) != 0;
	c.exteriorModel = Str(f, "Exterior", "Model", c.exteriorModel.c_str());
	for (size_t i = 0; i < c.keys.size(); i++)
	{
		std::string v = Str(f, "Keys", kActions[i].id, kActions[i].key);
		c.keys[i] = KeyCode(v);
		if (!c.keys[i])
		{
			Log("bad key '%s' for %s, using %s", v.c_str(), kActions[i].id, kActions[i].key);
			v = kActions[i].key;
			c.keys[i] = KeyCode(v);
		}
		c.tardis.keyNames[i] = KeyLabel(v);
	}
	return c;
}

std::optional<SavedTardis> IniStore::Load()
{
	if (Str(path_, "Exterior", "Exists", "0") != "1") return std::nullopt;
	SavedTardis t;
	Pose &p = t.pose;
	if (sscanf_s(Str(path_, "Exterior", "Pose", "").c_str(), "%f %f %f %f %f %f %f", &p.pos.x, &p.pos.y, &p.pos.z,
	             &p.rot.x, &p.rot.y, &p.rot.z, &p.rot.w) != 7)
		return std::nullopt;
	p.rot = p.rot.Norm();
	t.handbrake = Str(path_, "Exterior", "Handbrake", "0") == "1";
	return t;
}

void IniStore::Save(const std::optional<SavedTardis> &t)
{
	const char *f = path_.c_str();
	WritePrivateProfileStringA("Exterior", "Exists", t ? "1" : "0", f);
	if (!t) return;
	char buf[256];
	const Pose &p = t->pose;
	snprintf(buf, sizeof buf, "%f %f %f %f %f %f %f", p.pos.x, p.pos.y, p.pos.z, p.rot.x, p.rot.y, p.rot.z, p.rot.w);
	WritePrivateProfileStringA("Exterior", "Pose", buf, f);
	WritePrivateProfileStringA("Exterior", "Handbrake", t->handbrake ? "1" : "0", f);
}
