#define NOMINMAX
#include<DisplayTimer.h>
#include<Timer.h>
#include<algorithm>
IDXGIOutput* gx::FindOutputForMonitor(IDXGIFactory* factory, HMONITOR monitor)
{
	if (!factory || !monitor)
	{
		return nullptr;
	}
	for (UINT adapterIndex = 0;; ++adapterIndex)
	{
		IDXGIAdapter* adapter = nullptr;
		const HRESULT adapterResult = factory->EnumAdapters(adapterIndex, &adapter);
		if (adapterResult == DXGI_ERROR_NOT_FOUND || FAILED(adapterResult))
		{
			break;
		}
		if (!adapter)
		{
			continue;
		}
		for (UINT outputIndex = 0;; ++outputIndex)
		{
			IDXGIOutput* output = nullptr;
			const HRESULT result = adapter->EnumOutputs(outputIndex, &output);
			if (result == DXGI_ERROR_NOT_FOUND || FAILED(result))
			{
				break;
			}
			if (!output)
			{
				continue;
			}

			DXGI_OUTPUT_DESC description{};
			if (SUCCEEDED(output->GetDesc(&description)) && description.Monitor == monitor)
			{
				adapter->Release();
				return output;
			}
			output->Release();
		}
		adapter->Release();
	}
	return nullptr;
}
f32 gx::CalculateCombinationRatio(u64 frameTicks, u64 videoPhase, u64 videoInterval)
{
	if (frameTicks == 0 || videoInterval == 0)
	{
		return 0.0f;
	}
	const u64 doubledFrameTicks = frameTicks * 2;
	u64 alignedVideoTicks = videoInterval;
	while (alignedVideoTicks < doubledFrameTicks)
	{
		alignedVideoTicks += videoInterval;
	}
	s64 remainingTicks = static_cast<s64>(alignedVideoTicks - doubledFrameTicks) - static_cast<s64>(videoPhase);
	if (remainingTicks < 0)
	{
		remainingTicks += static_cast<s64>(videoInterval);
	}
	return static_cast<f32>((std::min)(1.0, static_cast<f64>(remainingTicks) / static_cast<f64>(frameTicks)));
}
gx::DisplayTimer::DisplayTimer()
{

}
gx::DisplayTimer::~DisplayTimer()
{
	Stop();
}
bool gx::DisplayTimer::Start(HWND window)
{
	if (!window)
	{
		return false;
	}
	Window = window;
	RetraceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!RetraceEvent)
	{
		return false;
	}
	Worker = CreateThread(nullptr, 0, &ThreadEntry, this, 0, nullptr);
	if (!Worker)
	{
		CloseHandle(RetraceEvent);
		RetraceEvent = nullptr;
		return false;
	}
	SetThreadPriority(Worker, THREAD_PRIORITY_ABOVE_NORMAL);
	return true;
}
bool gx::DisplayTimer::WaitForNext()
{
	if (!Worker || !RetraceEvent)
	{
		return false;
	}
	for ever
	{
		if (InterlockedCompareExchange(&State, 0, 0) < 0)
		{
			return false;
		}
		const LONG64 currentCount = InterlockedCompareExchange64(&RetraceCount, 0, 0);
		if (currentCount > LastConsumedCount)
		{
			LastConsumedCount = currentCount;
			return true;
		}
		if (WaitForSingleObject(RetraceEvent, INFINITE) != WAIT_OBJECT_0)
		{
			return false;
		}
	}
}
u64 gx::DisplayTimer::GetRetraceCount()
{
	const LONG64 count = InterlockedCompareExchange64(&RetraceCount, 0, 0);
	return count > 0 ? static_cast<u64>(count) : 0;
}
bool gx::DisplayTimer::IsAvailable()
{
	return Worker && InterlockedCompareExchange(&State, 0, 0) > 0;
}
u64 gx::DisplayTimer::GetLastRetraceTick()
{
	const LONG64 tick = InterlockedCompareExchange64(&LastRetraceTick, 0, 0);
	return tick > 0 ? static_cast<u64>(tick) : 0;
}
u64 gx::DisplayTimer::GetRetraceIntervalTicks()
{
	const LONG64 interval = InterlockedCompareExchange64(&RetraceIntervalTicks, 0, 0);
	return interval > 0 ? static_cast<u64>(interval) : 0;
}
DWORD __stdcall gx::DisplayTimer::ThreadEntry(void* context)
{
	return static_cast<DisplayTimer*>(context)->Run();
}
DWORD gx::DisplayTimer::Run()
{
	using CreateFactoryFunction = HRESULT(WINAPI*)(REFIID, void**);
	HMODULE dxgiModule = LoadLibraryW(L"dxgi.dll");
	CreateFactoryFunction createFactory = dxgiModule ? reinterpret_cast<CreateFactoryFunction>(GetProcAddress(dxgiModule, "CreateDXGIFactory")) : nullptr;
	IDXGIFactory* factory = nullptr;
	if (!createFactory || FAILED(createFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory))) || !factory)
	{
		SetUnavailable();
		if (dxgiModule) FreeLibrary(dxgiModule);
		return 0;
	}
	IDXGIOutput* output = nullptr;
	HMONITOR outputMonitor = nullptr;
	while (InterlockedCompareExchange(&StopRequested, 0, 0) == 0)
	{
		const HMONITOR windowMonitor = IsWindow(Window)
			? MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST)
			: nullptr;
		if (!windowMonitor)
		{
			if (output)
			{
				output->Release();
				output = nullptr;
			}
			outputMonitor = nullptr;
			SetUnavailable();
			Sleep(10);
			continue;
		}

		if (!output || outputMonitor != windowMonitor)
		{
			IDXGIOutput* newOutput = FindOutputForMonitor(factory, windowMonitor);
			if (!newOutput)
			{
				if (output)
				{
					output->Release();
					output = nullptr;
				}
				outputMonitor = nullptr;
				SetUnavailable();
				Sleep(10);
				continue;
			}
			if (output)
			{
				output->Release();
			}
			output = newOutput;
			outputMonitor = windowMonitor;
			InterlockedExchange64(&LastRetraceTick, 0);
			InterlockedExchange64(&RetraceIntervalTicks, 0);
			InterlockedExchange(&State, 1);
			SetEvent(RetraceEvent);
		}

		if (FAILED(output->WaitForVBlank()))
		{
			output->Release();
			output = nullptr;
			outputMonitor = nullptr;
			SetUnavailable();
			Sleep(10);
			continue;
		}

		const u64 tick = Timer::getTick();
		const LONG64 previousTick = InterlockedExchange64(&LastRetraceTick, static_cast<LONG64>(tick));
		if (previousTick > 0 && tick > static_cast<u64>(previousTick))
		{
			InterlockedExchange64(&RetraceIntervalTicks, static_cast<LONG64>(tick - static_cast<u64>(previousTick)));
		}
		InterlockedIncrement64(&RetraceCount);
		InterlockedExchange(&State, 1);
		SetEvent(RetraceEvent);
	}
	if (output)
	{
		output->Release();
	}
	factory->Release();
	FreeLibrary(dxgiModule);
	return 0;
}
void gx::DisplayTimer::SetUnavailable()
{
	InterlockedExchange(&State, -1);
	SetEvent(RetraceEvent);
}
void gx::DisplayTimer::Stop()
{
	if (Worker)
	{
		InterlockedExchange(&StopRequested, 1);
		if (RetraceEvent)
		{
			SetEvent(RetraceEvent);
		}
		WaitForSingleObject(Worker, INFINITE);
		CloseHandle(Worker);
		Worker = nullptr;
	}
	if (RetraceEvent)
	{
		CloseHandle(RetraceEvent);
		RetraceEvent = nullptr;
	}
}