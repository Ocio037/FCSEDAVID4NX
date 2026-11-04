#pragma once
#ifndef GX_DISPLAYTIMER
#define GX_DISPLAYTIMER
#include<Types.h>
#include<dxgi.h>
namespace gx {
	IDXGIOutput* FindOutputForMonitor(IDXGIFactory* factory, HMONITOR monitor);
	f32 CalculateCombinationRatio(u64 frameTicks, u64 videoPhase, u64 videoInterval);
	class DisplayTimer final
	{
	public:
		DisplayTimer();
		~DisplayTimer();
		bool Start(HWND window);
		bool WaitForNext();
		u64 GetRetraceCount();
		bool IsAvailable();
		u64 GetLastRetraceTick();
		u64 GetRetraceIntervalTicks();
	private:
		static DWORD WINAPI ThreadEntry(void* context);
		DWORD Run();
		void SetUnavailable();
		void Stop();
		HWND Window = nullptr;
		HANDLE RetraceEvent = nullptr;
		HANDLE Worker = nullptr;
		volatile LONG StopRequested = 0;
		volatile LONG State = 0;
		alignas(8) volatile LONG64 RetraceCount = 0;
		alignas(8) volatile LONG64 LastRetraceTick = 0;
		alignas(8) volatile LONG64 RetraceIntervalTicks = 0;
		LONG64 LastConsumedCount = 0;
	};
}
#endif // !GX_DISPLAYTIMER