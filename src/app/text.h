// The words the player sees, by key, from translations/<language>.ini laid over en-US.ini. Pure: no game calls.
#pragma once
#include <istream>
#include <map>
#include <string>

struct Text
{
	std::map<std::string, std::string> lines;

	// key=text lines (UTF-8); [sections] and ; comments are skipped. A later file overrides an earlier one.
	void Add(std::istream &ini)
	{
		std::string l;
		while (std::getline(ini, l))
		{
			if (!l.empty() && l.back() == '\r') l.pop_back();
			if (l.rfind("\xEF\xBB\xBF", 0) == 0) l = l.substr(3); // BOM
			size_t eq = l.find('=');
			if (l.empty() || l[0] == ';' || l[0] == '[' || eq == std::string::npos) continue;
			lines[l.substr(0, eq)] = l.substr(eq + 1);
		}
	}

	// the key itself when no file has it, so a missing line shows up instead of vanishing
	std::string operator()(const std::string &key) const
	{
		auto it = lines.find(key);
		return it != lines.end() ? it->second : key;
	}

	// with {} replaced by arg
	std::string operator()(const std::string &key, const std::string &arg) const
	{
		std::string s = (*this)(key);
		if (size_t at = s.find("{}"); at != std::string::npos) s.replace(at, 2, arg);
		return s;
	}
};
