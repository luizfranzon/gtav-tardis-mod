// TARDIS.log next to the .asi. Log() is a no-op until OpenLog().
#pragma once
#include <string>

void OpenLog(const std::string &path);
void Log(const char *fmt, ...);
