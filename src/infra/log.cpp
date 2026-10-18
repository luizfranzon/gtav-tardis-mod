#include "log.h"
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <share.h>

static FILE *g_log;

void OpenLog(const std::string &path) { g_log = _fsopen(path.c_str(), "w", _SH_DENYWR); }

void Log(const char *fmt, ...)
{
	if (!g_log) return;
	SYSTEMTIME t;
	GetLocalTime(&t);
	fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
	va_list a;
	va_start(a, fmt);
	vfprintf(g_log, fmt, a);
	va_end(a);
	fputc('\n', g_log);
	fflush(g_log);
}
