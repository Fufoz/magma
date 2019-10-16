#include "logging.h"

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <windows.h>
#endif

namespace magma
{
namespace log
{

	static int logMask = 0;

	enum Color
	{
#ifdef _WIN32
		COLOR_RED = FOREGROUND_RED|FOREGROUND_INTENSITY,
		COLOR_GREEN = FOREGROUND_GREEN|FOREGROUND_INTENSITY,
		COLOR_YELLOW = FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY,
		COLOR_WHITE = FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY
#endif
	};

	void setSeverityMask(LogMask mask)
	{
		logMask |= mask;
	}

	const char* maskToStr(LogMask mask)
	{
		switch(mask)
		{
			case MASK_INFO : return "Info";
			case MASK_DEBUG : return "Debug";
			case MASK_WARN : return "Warning";
			case MASK_ERROR : return "Fatal";
			default : return "Unknown";
		}
	}

	Color logLvlToColor(LogMask mask)
	{
		switch(mask) {
			case MASK_INFO : return COLOR_WHITE;
			case MASK_DEBUG : return COLOR_GREEN;
			case MASK_WARN : return COLOR_YELLOW;
			case MASK_ERROR : return COLOR_RED;
			default : return COLOR_WHITE;
		}
	}

	void dump(const std::string& info, LogMask mask)
	{
		if(logMask & mask)
		{
			FILE* streamHandle = mask & MASK_ERROR ? stderr : stdout;
#ifdef _WIN32
			HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
			CONSOLE_SCREEN_BUFFER_INFO consoleInfo = {};
			GetConsoleScreenBufferInfo(consoleHandle, &consoleInfo);
			SetConsoleTextAttribute(consoleHandle, COLOR_GREEN);
			fprintf(streamHandle, "[Magma]");
			SetConsoleTextAttribute(consoleHandle, logLvlToColor(mask));
			fprintf(streamHandle, " %s\n", info.c_str());
			SetConsoleTextAttribute(consoleHandle, consoleInfo.wAttributes);
#endif
		}

	}

}
}