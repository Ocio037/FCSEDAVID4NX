#include<SysData.h>
#include<runtime.h>
#include<array>
#include<cstdio>
#include<cstring>
namespace
{
	#ifndef FCSEDAVID4NX_BUILD_AUTHOR
	#define FCSEDAVID4NX_BUILD_AUTHOR unknown
	#endif
	#define FCSEDAVID4NX_STRINGIFY_INNER(Value) #Value
	#define FCSEDAVID4NX_STRINGIFY(Value) FCSEDAVID4NX_STRINGIFY_INNER(Value)
	u32 ReadTwoDigits(const char* Digits)
	{
		if (Digits[0] < '0' || Digits[0] > '9' || Digits[1] < '0' || Digits[1] > '9')
		{
			return 0;
		}
		return static_cast<u32>(Digits[0] - '0') * 10u + static_cast<u32>(Digits[1] - '0');
	}
}
Game::BuildInfo Game::SysData::GetBuildInfo()
{
	BuildInfo Result;
	const char* Date = __DATE__;
	const u32 Day = Date[4] == ' ' ? static_cast<u32>(Date[5] - '0') : ReadTwoDigits(Date + 4);
	const u32 Year = static_cast<u32>(Date[7] - '0') * 1000u + static_cast<u32>(Date[8] - '0') * 100u + static_cast<u32>(Date[9] - '0') * 10u + static_cast<u32>(Date[10] - '0');
	if (Day > 0 && Day <= 31 && Year >= 1000)
	{
		static constexpr std::array<const char*, 12> Months =
		{
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
		};
		for (u32 Index = 0; Index < Months.size(); ++Index)
		{
			if (std::strncmp(Date, Months[Index], 3) == 0)
			{
				Result.Date = Year * 10000u + (Index + 1u) * 100u + Day;
				break;
			}
		}
	}
	const char* Time = __TIME__;
	const u32 Hour = ReadTwoDigits(Time);
	const u32 Minute = ReadTwoDigits(Time + 3);
	const u32 Second = ReadTwoDigits(Time + 6);
	if (Hour < 24 && Minute < 60 && Second < 60)
	{
		Result.Time = Hour * 10000u + Minute * 100u + Second;
	}
	return Result;
}
const char* Game::SysData::GetBuildAuthor()
{
	static constexpr char Author[] = FCSEDAVID4NX_STRINGIFY(FCSEDAVID4NX_BUILD_AUTHOR);
	return Author[0] == '\0' ? "unknown" : Author;
}
u32 Game::SysData::GetBuildModeNumber()
{
#ifdef _DEBUG
	return 0;
#else
	return 1;
#endif
}
const char* Game::SysData::GetBuildModeName()
{
#ifdef _DEBUG
	return "DEBUG";
#else
	return "RELEASE";
#endif
}
std::string Game::SysData::GetBuildInfoText()
{
	const BuildInfo Info = GetBuildInfo();
	const unsigned int Year = Info.Date / 10000u;
	const unsigned int Month = (Info.Date / 100u) % 100u;
	const unsigned int Day = Info.Date % 100u;
	const unsigned int Hour = Info.Time / 10000u;
	const unsigned int Minute = (Info.Time / 100u) % 100u;
	char Text[160]{};
	std::snprintf(Text, sizeof(Text), "%s %04u/%02u/%02u %02u:%02u %s", GetBuildModeName(), Year, Month, Day, Hour, Minute, GetBuildAuthor());
	return Text;
}
void Game::SysData::RegisterScriptNatives(ohana::runtime& Runtime)
{
	Runtime.register_natives(
	{
		{ "compilacion_fecha", [](const std::vector<ohana::runtime::Value>& Arguments) -> ohana::runtime::Value
		{
			return Arguments.empty() ? ohana::runtime::Value(static_cast<std::int64_t>(Game::SysData::GetBuildInfo().Date)) : ohana::runtime::Value(std::int64_t(0));
		} },
		{ "compilacion_hora", [](const std::vector<ohana::runtime::Value>& Arguments) -> ohana::runtime::Value
		{
			return Arguments.empty() ? ohana::runtime::Value(static_cast<std::int64_t>(Game::SysData::GetBuildInfo().Time)) :
				ohana::runtime::Value(std::int64_t(0));
		} },
		{ "compilacion_autor", [](const std::vector<ohana::runtime::Value>& Arguments) -> ohana::runtime::Value
		{
			return Arguments.empty() ? ohana::runtime::Value(std::string(Game::SysData::GetBuildAuthor())) :
				ohana::runtime::Value(std::string{});
		} },
		{ "compilacion_modo", [](const std::vector<ohana::runtime::Value>& Arguments) -> ohana::runtime::Value
		{
			return Arguments.empty() ? ohana::runtime::Value(static_cast<std::int64_t>(Game::SysData::GetBuildModeNumber())) :
				ohana::runtime::Value(std::int64_t(-1));
		} },
		{ "compilacion_info", [](const std::vector<ohana::runtime::Value>& Arguments) -> ohana::runtime::Value
		{
			return Arguments.empty() ? ohana::runtime::Value(Game::SysData::GetBuildInfoText()) : ohana::runtime::Value(std::string{});
		} }
	});
}