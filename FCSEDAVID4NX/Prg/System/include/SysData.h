#pragma once
#include<Types.h>
#include<string>
namespace ohana
{
	class runtime;
}
namespace Game
{
	struct BuildInfo
	{
		u32 Date = 0;
		u32 Time = 0;
	};
	class SysData final
	{
	public:
		[[nodiscard]] static BuildInfo GetBuildInfo();
		[[nodiscard]] static const char* GetBuildAuthor();
		[[nodiscard]] static u32 GetBuildModeNumber();
		[[nodiscard]] static const char* GetBuildModeName();
		[[nodiscard]] static std::string GetBuildInfoText();
		static void RegisterScriptNatives(ohana::runtime& Runtime);
	};
}