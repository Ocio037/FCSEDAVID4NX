#pragma once
#include<Windows.h>
namespace Timer
{
	inline u64 getTick()
	{
		LARGE_INTEGER longInteger;
		return (QueryPerformanceCounter(&longInteger) != FALSE) ? (u64)longInteger.QuadPart : 0ULL;
	}
	inline u64 getFrequency()
	{
		static const u64 frequency = []()
		{
			LARGE_INTEGER longInteger{};
			return (::QueryPerformanceFrequency(&longInteger) != FALSE) ? static_cast<u64>(longInteger.QuadPart) : 0ULL;
		}();
		return frequency;
	}
	inline u64 GetMilliSecond()
	{
		const u64 frequency = getFrequency();
		if (frequency == 0)
		{
			return 0;
		}
		const u64 tick = getTick();
		return (tick / frequency) * 1000ULL + ((tick % frequency) * 1000ULL) / frequency;
	}
	inline u64 GetMicroSecond()
	{
		const u64 frequency = getFrequency();
		if (frequency == 0)
		{
			return 0;
		}
		const u64 tick = getTick();
		return (tick / frequency) * 1000000ULL + ((tick % frequency) * 1000000ULL) / frequency;
	}
}
