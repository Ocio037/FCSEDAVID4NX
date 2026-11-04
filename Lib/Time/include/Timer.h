#pragma once
#ifndef FOCA_TIME_FOCA_TIMER_H_
#define FOCA_TIME_FOCA_TIMER_H_
#include<Types.h>
namespace Timer
{
	inline u64 getTick();
	inline u64 getFrequency();
	inline u64 GetMilliSecond();
	inline u64 GetMicroSecond();
}
#include"Timer.inl"
#endif // !FOCA_TIME_FOCA_TIMER_H_
