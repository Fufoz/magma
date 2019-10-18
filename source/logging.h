#ifndef LOGGING_H
#define LOGGING_H

#include <fmt/format.h>

namespace magma
{
namespace log
{

	enum LogMask
	{
		MASK_INFO  = 1 << 0,
		MASK_DEBUG = 1 << 1,
		MASK_WARN  = 1 << 2,
		MASK_ERROR = 1 << 3,
		MASK_ALL   = MASK_INFO|MASK_DEBUG|MASK_WARN|MASK_ERROR
	};

	void setSeverityMask(LogMask mask);
	
	void dump(const std::string& info, LogMask mask);
	
	template<typename... Args> void info(Args... args)
	{
		auto string = fmt::format(args...);
		dump(string, MASK_INFO);
	}

	template<typename... Args> void error(Args... args)
	{
		auto string = fmt::format(args...);
		dump(string, MASK_ERROR);
	}

	template<typename... Args> void warn(Args... args)
	{
		auto string = fmt::format(args...);
		dump(string, MASK_WARN);
	}

	template<typename... Args> void debug(Args... args)
	{
		auto string = fmt::format(args...);
		dump(string, MASK_DEBUG);
	}

}
}

#endif