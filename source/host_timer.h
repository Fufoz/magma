#include <chrono>

#include "logging.h"

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

struct HostTimer
{
	TimePoint startPoint;

	void start()
	{
		startPoint = std::chrono::high_resolution_clock::now();
	}

	float stop()
	{
		return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - startPoint).count() / 1000000.f;
	}
};