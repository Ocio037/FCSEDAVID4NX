#include"GameSystem.h"
#include<FileReader.h>
#include<DisplayTimer.h>
#include<Timer.h>
#include<algorithm>
#include<limits>
#include<DynamicArray.h>
#include<DynamicString.h>
#include<array>
#include<Font.h>
#include<MemGlobal.h>
#include<new>
#if defined(_WIN32)
#include<dxgi.h>
#endif
#define ErrorDebug "ERRORDBG"
static constexpr u32 WindowWidth = 1280;
static constexpr u32 WindowHeight = 720;
static constexpr u32 SceneBufferWidth = 640;
static constexpr u32 SceneBufferHeight = 480;
static constexpr const wchar_t* WindowClassName = L"FCSEDAVID4NX";
static constexpr const wchar_t* WindowTitle = L"FCSEDAVID4NX";
static constexpr u32 ShaderVertexId = 0;
static constexpr u32 ShaderFragmentId = 1;
static constexpr u32 AntialiasVertexShaderId = 4;
static constexpr u32 AntialiasFragmentShaderId = 5;
static constexpr u32 AntialiasPresentFragmentShaderId = 6;
static constexpr u32 DebugFontAtlasId = 7;
static constexpr u32 DebugFontCharsetId = 8;
static constexpr u32 DebugFontVertexShaderId = 9;
static constexpr u32 DebugFontFragmentShaderId = 10;
static constexpr u32 DebugFontGridCols = 16;
static constexpr u32 DebugFontGridRows = 8;
static constexpr f64 FixedDeltaSeconds = 1.0 / 60.0;
static constexpr f64 MaxFrameDeltaSeconds = 0.25;
static constexpr u64 NominalRetraceMicroseconds = 16666;
static constexpr std::size_t MiB = 1024 * 1024;
static constexpr std::size_t SystemHeapSize = 3 * MiB;
static constexpr std::size_t GameHeapSize = 1 * MiB;
static constexpr std::size_t WorldHeapSize = 7 * MiB;
static constexpr std::size_t ArchiveHeapSize = 8 * MiB;
static constexpr std::size_t J2DHeapSize = 2 * MiB;
static constexpr std::size_t CommandHeapSize = 2 * MiB;
static constexpr std::size_t RootHeapReserveSize = 2 * MiB;
static_assert(GameHeapSize < SystemHeapSize);
static_assert(SystemHeapSize + WorldHeapSize + ArchiveHeapSize + J2DHeapSize + CommandHeapSize + RootHeapReserveSize == Mem::DefaultGlobalHeapSize);
static const String ContentRoot = "../../";
namespace
{
	struct DecodedTga
	{
		u32 Width = 0;
		u32 Height = 0;
		DynamicArray<u8> Pixels;
	};
	bool DecodeTgaRgba(const DynamicArray<u8>& data, DecodedTga& image)
	{
		if (data.size() < 18 || data[1] != 0 || (data[2] != 2 && data[2] != 10))
		{
			return false;
		}
		image.Width = static_cast<u32>(data[12]) | (static_cast<u32>(data[13]) << 8);
		image.Height = static_cast<u32>(data[14]) | (static_cast<u32>(data[15]) << 8);
		const u32 bitsPerPixel = data[16];
		const u32 bytesPerPixel = bitsPerPixel / 8;
		const u64 pixelCount64 = static_cast<u64>(image.Width) * image.Height;
		if (image.Width == 0 || image.Height == 0 || (bitsPerPixel != 24 && bitsPerPixel != 32) ||
			pixelCount64 > static_cast<u64>((std::numeric_limits<size_t>::max)() / 4))
		{
			return false;
		}
		const size_t pixelCount = static_cast<size_t>(pixelCount64);
		image.Pixels.assign(pixelCount * 4, 0);
		size_t inputOffset = 18 + data[0];
		size_t outputPixel = 0;
		const bool topOrigin = (data[17] & 0x20) != 0;
		const bool rightOrigin = (data[17] & 0x10) != 0;
		auto emitPixel = [&](const u8* pixel)
		{
			const u32 fileY = static_cast<u32>(outputPixel / image.Width);
			const u32 fileX = static_cast<u32>(outputPixel % image.Width);
			const u32 x = rightOrigin ? image.Width - 1 - fileX : fileX;
			const u32 y = topOrigin ? image.Height - 1 - fileY : fileY;
			const size_t destination = (static_cast<size_t>(y) * image.Width + x) * 4;
			image.Pixels[destination + 0] = pixel[2];
			image.Pixels[destination + 1] = pixel[1];
			image.Pixels[destination + 2] = pixel[0];
			image.Pixels[destination + 3] = bytesPerPixel == 4 ? pixel[3] : 255;
			++outputPixel;
		};
		while (outputPixel < pixelCount)
		{
			if (data[2] == 2)
			{
				if (inputOffset + bytesPerPixel > data.size()) return false;
				emitPixel(data.data() + inputOffset);
				inputOffset += bytesPerPixel;
				continue;
			}
			if (inputOffset >= data.size()) return false;
			const u8 packet = data[inputOffset++];
			const size_t count = static_cast<size_t>((packet & 0x7f) + 1);
			if (outputPixel + count > pixelCount) return false;
			if (packet & 0x80)
			{
				if (inputOffset + bytesPerPixel > data.size()) return false;
				const u8* pixel = data.data() + inputOffset;
				inputOffset += bytesPerPixel;
				for (size_t i = 0; i < count; ++i) emitPixel(pixel);
			}
			else
			{
				if (inputOffset + count * bytesPerPixel > data.size()) return false;
				for (size_t i = 0; i < count; ++i)
				{
					emitPixel(data.data() + inputOffset);
					inputOffset += bytesPerPixel;
				}
			}
		}
		return true;
	}
}
Game::GameSystem::GameSystem() : 
	ProcessBars(nullptr),
	ProcessBarsVisible(true),
	CombinationRatio(0.0f),
	Running(false)
{

}
Game::GameSystem::~GameSystem()
{
	Finalize();
}
bool Game::GameSystem::Init()
{
	if (SystemHeap.IsInitialized())
	{
		Finalize();
	}
	InitError.clear();
	if (!InitializeHeaps())
	{
		InitError = "No se pudieron inicializar los heaps del juego dentro del heap global de 24 MiB.";
		return false;
	}
	Mem::CurrentHeapScope gameHeapScope(GameHeap);
	try
	{
		Resources.SetAllocationHeap(&ArchiveHeap);
	}
	catch (const std::bad_alloc&)
	{
		InitError = "No hay memoria suficiente en ArchiveHeap para preparar el cargador de recursos.";
		return false;
	}
#ifdef _DEBUG
	{
		Mem::CurrentHeapScope heapScope(J2DHeap);
		MemoryConsole.Finalize();
		MemoryHeapView.Finalize();
	}
#endif
	{
		Mem::CurrentHeapScope heapScope(J2DHeap);
		if (DebugFont)
		{
			DebugFont->Destroy();
			DebugFont.reset();
		}
	}
	{
		Mem::CurrentHeapScope heapScope(WorldHeap);
		World.Finalize();
	}
	{
		Mem::CurrentHeapScope heapScope(ArchiveHeap);
		Resources.Clear();
	}
	if (!InitWindow())
	{
		InitError = "No se pudo crear la ventana del juego.";
		return false;
	}
	gx::detail::DisplayInfo info{};
	info.Resolution.Width = WindowWidth;
	info.Resolution.Height = WindowHeight;
	info.UseOffscreenScene = true;
#ifdef _WIN32
	info.hInstance = HInstance;
	info.Hwnd = Hwnd;
#endif
	{
		Mem::CurrentHeapScope heapScope(SystemHeap);
		Display.Initialize(info);
		Shader.Initialize(&Display);
	}
	{
		Mem::CurrentHeapScope heapScope(CommandHeap);
		CmdBuffer.Initialize(Display);
	}
	{
		Mem::CurrentHeapScope heapScope(SystemHeap);
		if (!LoadShaders(InitError))
		{
			return false;
		}
	}
	{
		Mem::CurrentHeapScope heapScope(CommandHeap);
		if (!InitializeFilter(InitError))
		{
			return false;
		}
	}
	{
		Mem::CurrentHeapScope heapScope(J2DHeap);
		if (!LoadDebugFont(InitError))
		{
			return false;
		}
	}
	const GameUtil::ArcEntry* overlayVertexShader = Resources.FindResourceById(GameUtil::DataArchiveName, ShaderVertexId);
	const GameUtil::ArcEntry* overlayFragmentShader = Resources.FindResourceById(GameUtil::DataArchiveName, ShaderFragmentId);
	{
		Mem::CurrentHeapScope heapScope(SystemHeap);
		DebugOverlayShader.Initialize(&Display);
		if (!overlayVertexShader || !overlayFragmentShader || !DebugOverlayShader.Load(const_cast<u8*>(overlayVertexShader->DecompressedData.data()), static_cast<u32>(overlayVertexShader->DecompressedData.size()), const_cast<u8*>(overlayFragmentShader->DecompressedData.data()), static_cast<u32>(overlayFragmentShader->DecompressedData.size())))
		{
			InitError = ErrorDebug;
			return false;
		}
	}
#ifdef _DEBUG
	{
		Mem::CurrentHeapScope heapScope(J2DHeap);
		if (!MemoryConsole.Initialize(Display, DebugOverlayShader) ||
			!MemoryHeapView.Initialize(Display, DebugOverlayShader) ||
			!ScriptConsole.Initialize(Display, DebugOverlayShader))
		{
			InitError = "No se pudieron inicializar las vistas de memoria.";
			return false;
		}
		ScriptConsole.SetPosition(24.0f, 18.0f);
	}
#endif
	{
		Mem::CurrentHeapScope heapScope(J2DHeap);
		ProcessBars = FSys::Utils::FocaUtilBar::Create(Display, DebugOverlayShader);
		if (!ProcessBars)
		{
			InitError = ErrorDebug;
			return false;
		}
		ProcessBars->SetVisible(ProcessBarsVisible);
	}
	{
		Mem::CurrentHeapScope heapScope(WorldHeap);
		if (!World.Init(Resources, Display, Shader, GameUtil::ResolvePath(ContentRoot + "Build/map/map.dat"), GameUtil::ResolvePath(ContentRoot + "Build/map/mapinfo.dat"), &InitError
#ifdef _DEBUG
			, &ScriptConsole
#endif
			))
		{
			return false;
		}
	}
	Running = true;
	DisplayRetraceClockStarted = DisplayRetraceClock.Start(Hwnd);
	PreviousTime = Timer::GetMicroSecond();
	Accumulator = 0.0;
	ProcessBarFrameStarted = false;
	PreviousRefreshCount = DisplayRetraceClock.GetRetraceCount();
	PreviousRefreshCountValid = DisplayRetraceClockStarted;
	PreviousFrameTick = Timer::getTick();
	{
		Mem::CurrentHeapScope heapScope(CommandHeap);
		CmdBuffer.SetColor(0.0f, 0.0f, 0.0f, 1.0f);
	}
	return true;
}
bool Game::GameSystem::InitializeHeaps()
{
	if (SystemHeap.IsInitialized())
	{
		return GameHeap.IsInitialized() && WorldHeap.IsInitialized() && ArchiveHeap.IsInitialized() && J2DHeap.IsInitialized() && CommandHeap.IsInitialized();
	}
	Mem::MemBase* rootHeap = Mem::GetGlobalHeap();
	if (rootHeap == nullptr || !rootHeap->IsInitialized() || !SystemHeap.Initialize(*rootHeap, SystemHeapSize))
	{
		return false;
	}
	if (GameHeap.Initialize(SystemHeap, GameHeapSize) &&
		WorldHeap.Initialize(*rootHeap, WorldHeapSize) &&
		ArchiveHeap.Initialize(*rootHeap, ArchiveHeapSize) &&
		J2DHeap.Initialize(*rootHeap, J2DHeapSize) &&
		CommandHeap.Initialize(*rootHeap, CommandHeapSize))
	{
		return true;
	}
	CommandHeap.Finalize();
	J2DHeap.Finalize();
	ArchiveHeap.Finalize();
	WorldHeap.Finalize();
	GameHeap.Finalize();
	SystemHeap.Finalize();
	return false;
}
bool Game::GameSystem::IsRunning() const
{
	return Running;
}

void Game::GameSystem::RunFrame()
{
	if (!Running)
	{
		return;
	}
	Mem::CurrentHeapScope gameHeapScope(GameHeap);
	u64 frameTicks = 0;
	u64 videoPhase = 0;
	u64 videoInterval = 0;
	if (ProcessBars && ProcessBarFrameStarted)
		{
			ProcessBars->WholeLoopEnd();
		}
		if (ProcessBars)
		{
			ProcessBars->WholeLoopStart();
			ProcessBarFrameStarted = true;
			ProcessBars->IdleStart();
		}
		const bool waitedForVBlank = DisplayRetraceClockStarted && DisplayRetraceClock.WaitForNext();
		{
			Mem::CurrentHeapScope heapScope(CommandHeap);
			if (!waitedForVBlank)
			{
				CmdBuffer.WaitForPresent();
			}
			CmdBuffer.WaitForPresent();
		}
		const u64 frameTick = Timer::getTick();
		frameTicks = frameTick >= PreviousFrameTick ? frameTick - PreviousFrameTick : 0;
		PreviousFrameTick = frameTick;
		const u64 lastRetraceTick = waitedForVBlank ? DisplayRetraceClock.GetLastRetraceTick() : 0;
		videoInterval = waitedForVBlank ? DisplayRetraceClock.GetRetraceIntervalTicks() : 0;
		videoPhase = videoInterval > 0 && lastRetraceTick > 0 && frameTick >= lastRetraceTick ? (frameTick - lastRetraceTick) % videoInterval : 0;
		const u64 currentTime = Timer::GetMicroSecond();
		const u64 elapsedMicroseconds = currentTime >= PreviousTime ? currentTime - PreviousTime : 0ULL;
		const f64 elapsedSeconds = static_cast<f64>(elapsedMicroseconds) / 1000000.0;
		PreviousTime = currentTime;
		Accumulator += std::min(elapsedSeconds, MaxFrameDeltaSeconds);
		if (ProcessBars)
		{
			ProcessBars->IdleEnd();
		}
		if (ProcessBars)
		{
			ProcessBars->GpStart();
		}
		ProcessMessages();
		if (!Running)
		{
			if (ProcessBars)
			{
				ProcessBars->GpEnd();
			}
			if (ProcessBars && ProcessBarFrameStarted)
			{
				ProcessBars->WholeLoopEnd();
			}
			return;
		}
		{
			Mem::CurrentHeapScope heapScope(CommandHeap);
			CmdBuffer.Render();
		}
		f32 yawDirection = 0.0f;
		f32 pitchDirection = 0.0f;
		bool cameraInputEnabled = true;
#ifdef _DEBUG
		cameraInputEnabled = !MemoryConsole.IsVisible() && !MemoryHeapView.IsVisible() && !ScriptConsole.IsVisible();
#endif
		if (cameraInputEnabled)
		{
			if (GetAsyncKeyState(VK_LEFT) & 0x8000) yawDirection -= 1.0f;
			if (GetAsyncKeyState(VK_RIGHT) & 0x8000) yawDirection += 1.0f;
			if (GetAsyncKeyState(VK_UP) & 0x8000) pitchDirection -= 1.0f;
			if (GetAsyncKeyState(VK_DOWN) & 0x8000) pitchDirection += 1.0f;
		}
		CmdBuffer.SetShader(&Shader);
		Camera.Apply(Shader, World.GetPlayerPosition() + glm::vec3(0.0f, 0.25f, 0.0f), WindowWidth, WindowHeight);
		{
			Mem::CurrentHeapScope heapScope(WorldHeap);
			World.Draw(CmdBuffer, Shader);
		}
		{
			Mem::CurrentHeapScope heapScope(J2DHeap);
			if (DebugFont)
			{
				DebugFont->Update();
#ifdef _DEBUG
				if (MemoryHeapView.IsVisible() || MemoryConsole.IsVisible())
				{
					const std::array<Mem::MemAlcExp*, 6> heaps =
					{
						&SystemHeap, &GameHeap, &WorldHeap, &ArchiveHeap, &J2DHeap, &CommandHeap
					};
					if (MemoryConsole.IsVisible())
					{
						MemoryConsole.Draw(CmdBuffer, *DebugFont, heaps);
					}
					else
					{
						MemoryHeapView.Draw(CmdBuffer, *DebugFont, heaps);
					}
				}
				else if (ScriptConsole.IsVisible())
				{
					ScriptConsole.Draw(CmdBuffer, *DebugFont);
				}
				else
#endif
				{
					DebugFont->DrawTextU(u"MapNoM: ActionTest (No. 0)", 0.0f, 0.0f, 10.0f, 14.0f);
				}
			}
			if (ProcessBars)
			{
				ProcessBars->DrawProcessBar(CmdBuffer);
			}
		}
		{
			Mem::CurrentHeapScope heapScope(CommandHeap);
			CmdBuffer.EndScenePass();
			if (Antialiasing)
			{
				Antialiasing->Record(CmdBuffer.GetNativeFrameContext());
			}
			CmdBuffer.Submit();
			CmdBuffer.Present();
		}
		if (ProcessBars)
		{
			ProcessBars->CpuStart();
		}
		CombinationRatio = gx::CalculateCombinationRatio(frameTicks, videoPhase, videoInterval);
		while (Accumulator >= FixedDeltaSeconds)
		{
			Camera.Update(FixedDeltaSeconds, yawDirection, pitchDirection);
			{
				Mem::CurrentHeapScope heapScope(WorldHeap);
				World.Update(FixedDeltaSeconds);
			}
			Accumulator -= FixedDeltaSeconds;
		}
		if (ProcessBars)
		{
			ProcessBars->CpuEnd();
		}
		if (ProcessBars)
		{
			ProcessBars->GpWaitStart();
		}
		{
			Mem::CurrentHeapScope heapScope(CommandHeap);
			CmdBuffer.WaitForGpu();
		}
		if (ProcessBars)
		{
			ProcessBars->GpWaitEnd();
		}
		if (ProcessBars)
		{
			ProcessBars->GpEnd();
		}
		if (ProcessBars)
		{
			u64 retraceCount = elapsedMicroseconds / NominalRetraceMicroseconds;
			if (elapsedMicroseconds % NominalRetraceMicroseconds >= NominalRetraceMicroseconds / 2)
			{
				++retraceCount;
			}
			if (DisplayRetraceClock.IsAvailable())
			{
				const u64 currentRefreshCount = DisplayRetraceClock.GetRetraceCount();
				retraceCount = PreviousRefreshCountValid && currentRefreshCount >= PreviousRefreshCount ? currentRefreshCount - PreviousRefreshCount : 0;
				PreviousRefreshCount = currentRefreshCount;
				PreviousRefreshCountValid = true;
			}
			else
			{
				PreviousRefreshCountValid = false;
			}
			const u32 boundedRetraceCount = static_cast<u32>((std::min<u64>(retraceCount, 0xFFFFFFFFULL)));
			ProcessBars->SetCostFrame(boundedRetraceCount);
		}
}
void Game::GameSystem::Finalize()
{
	Running = false;
	if (WorldHeap.IsInitialized())
	{
		Mem::CurrentHeapScope heapScope(WorldHeap);
		World.Finalize();
	}
	else
	{
		World.Finalize();
	}
#ifdef _DEBUG
	if (J2DHeap.IsInitialized())
	{
		Mem::CurrentHeapScope heapScope(J2DHeap);
		MemoryConsole.Finalize();
		MemoryHeapView.Finalize();
		ScriptConsole.Finalize();
	}
	else
	{
		MemoryConsole.Finalize();
		MemoryHeapView.Finalize();
		ScriptConsole.Finalize();
	}
#endif
	if (J2DHeap.IsInitialized())
	{
		Mem::CurrentHeapScope heapScope(J2DHeap);
		if (DebugFont)
		{
			DebugFont->Destroy();
			DebugFont.reset();
		}
	}
	else if (DebugFont)
	{
		DebugFont->Destroy();
		DebugFont.reset();
	}
	if (ProcessBars)
	{
		if (J2DHeap.IsInitialized())
		{
			Mem::CurrentHeapScope heapScope(J2DHeap);
			FSys::Utils::FocaUtilBar::Destroy();
		}
		else
		{
			FSys::Utils::FocaUtilBar::Destroy();
		}
		ProcessBars = nullptr;
	}
	if (CommandHeap.IsInitialized())
	{
		Mem::CurrentHeapScope heapScope(CommandHeap);
		if (Antialiasing)
		{
			Antialiasing->Finalize();
			Antialiasing.reset();
		}
	}
	else if (Antialiasing)
	{
		Antialiasing->Finalize();
		Antialiasing.reset();
	}
	if (SystemHeap.IsInitialized())
	{
		Mem::CurrentHeapScope heapScope(SystemHeap);
		DebugOverlayShader.Finalize();
		Shader.Finalize();
	}
	else
	{
		DebugOverlayShader.Finalize();
		Shader.Finalize();
	}
	if (CommandHeap.IsInitialized())
	{
		Mem::CurrentHeapScope heapScope(CommandHeap);
		CmdBuffer.Finalize();
	}
	else
	{
		CmdBuffer.Finalize();
	}
	if (SystemHeap.IsInitialized())
	{
		Mem::CurrentHeapScope heapScope(SystemHeap);
		Display.Finalize();
	}
	else
	{
		Display.Finalize();
	}
	if (ArchiveHeap.IsInitialized())
	{
		Mem::CurrentHeapScope heapScope(ArchiveHeap);
		Resources.Clear();
	}
	else
	{
		Resources.Clear();
	}
	Resources.SetAllocationHeap(nullptr);
	if (Hwnd)
	{
		DestroyWindow(Hwnd);
		Hwnd = nullptr;
	}
	if (HInstance)
	{
		UnregisterClassW(WindowClassName, HInstance);
		HInstance = nullptr;
	}
	CommandHeap.Finalize();
	J2DHeap.Finalize();
	ArchiveHeap.Finalize();
	WorldHeap.Finalize();
	GameHeap.Finalize();
	SystemHeap.Finalize();
}
const String& Game::GameSystem::GetInitError() const
{
	return InitError;
}
gx::Display& Game::GameSystem::GetDisplay()
{
	return Display;
}
gx::CommandBuffer& Game::GameSystem::GetCommandBuffer()
{
	return CmdBuffer;
}
gx::Shader& Game::GameSystem::GetShader()
{
	return Shader;
}
f32 Game::GameSystem::GetCombinationRatio() const
{
	return CombinationRatio;
}
bool Game::GameSystem::LoadShaders(String& Error)
{
	const String dataPath = GameUtil::ResolvePath(ContentRoot + "Build/data/data.dat");
	if (!Resources.RequestMountArchive(GameUtil::DataArchiveName, dataPath, &Error))
	{
		return false;
	}
	if (Resources.ReceiveArchive(GameUtil::DataArchiveName, &Error) == nullptr)
	{
		return false;
	}
	const GameUtil::ArcEntry* vertEntry = Resources.FindResourceById(GameUtil::DataArchiveName, ShaderVertexId);
	const GameUtil::ArcEntry* fragEntry = Resources.FindResourceById(GameUtil::DataArchiveName, ShaderFragmentId);
	if (!vertEntry || !fragEntry)
	{
		Error = "data.dat no contiene los shaders con ID 0 y 1.";
		return false;
	}
	if (vertEntry->DecompressedData.empty() || fragEntry->DecompressedData.empty())
	{
		Error = "Uno de los shaders en data.dat quedo vacio al descomprimir.";
		return false;
	}
	if (!Shader.Load(const_cast<u8*>(vertEntry->DecompressedData.data()), static_cast<u32>(vertEntry->DecompressedData.size()), const_cast<u8*>(fragEntry->DecompressedData.data()), static_cast<u32>(fragEntry->DecompressedData.size())))
	{
		Error = "No se pudieron crear los shaders Vulkan. Revisa que los IDs 0 y 1 contengan SPIR-V valido.";
		return false;
	}
	return true;
}
bool Game::GameSystem::InitializeFilter(String& Error)
{
	const GameUtil::ArcEntry* antialiasVertex = Resources.FindResourceById(GameUtil::DataArchiveName, AntialiasVertexShaderId);
	const GameUtil::ArcEntry* antialiasFragment = Resources.FindResourceById(GameUtil::DataArchiveName, AntialiasFragmentShaderId);
	const GameUtil::ArcEntry* antialiasPresentFragment = Resources.FindResourceById(GameUtil::DataArchiveName, AntialiasPresentFragmentShaderId);
	if (!antialiasVertex || !antialiasFragment || !antialiasPresentFragment ||
		antialiasVertex->DecompressedData.empty() || antialiasFragment->DecompressedData.empty() ||
		antialiasPresentFragment->DecompressedData.empty())
	{
		Error = "data.dat no contiene los shaders del filtro con los IDs 4, 5 y 6.";
		return false;
	}
	try
	{
		Antialiasing = Mem::MakeUnique<Filter>(static_cast<Mem::MemBase&>(CommandHeap));
		if (Antialiasing && Antialiasing->Initialize(CmdBuffer,
			antialiasVertex->DecompressedData.data(), static_cast<u32>(antialiasVertex->DecompressedData.size()),
			antialiasFragment->DecompressedData.data(), static_cast<u32>(antialiasFragment->DecompressedData.size()),
			antialiasPresentFragment->DecompressedData.data(), static_cast<u32>(antialiasPresentFragment->DecompressedData.size()),
			SceneBufferWidth, SceneBufferHeight))
		{
			return true;
		}
	}
	catch (const std::bad_alloc&)
	{
		Antialiasing.reset();
		Error = "No hay memoria suficiente en CommandHeap para inicializar el filtro.";
		return false;
	}
	Antialiasing.reset();
	Error = "No se pudo inicializar el filtro de imagen.";
	return false;
}
bool Game::GameSystem::LoadDebugFont(String& Error)
{
	const GameUtil::ArcEntry* atlasEntry = Resources.FindResourceById(GameUtil::DataArchiveName, DebugFontAtlasId);
	const GameUtil::ArcEntry* charsetEntry = Resources.FindResourceById(GameUtil::DataArchiveName, DebugFontCharsetId);
	const GameUtil::ArcEntry* vertexEntry = Resources.FindResourceById(GameUtil::DataArchiveName, DebugFontVertexShaderId);
	const GameUtil::ArcEntry* fragmentEntry = Resources.FindResourceById(GameUtil::DataArchiveName, DebugFontFragmentShaderId);
	if (!atlasEntry || !charsetEntry || !vertexEntry || !fragmentEntry || atlasEntry->DecompressedData.empty() ||
		charsetEntry->DecompressedData.empty() || vertexEntry->DecompressedData.empty() || fragmentEntry->DecompressedData.empty())
	{
		Error = "data.dat requiere el atlas ID 7, el charset ID 8 y los shaders de fuente IDs 9 y 10.";
		return false;
	}
	if ((charsetEntry->DecompressedData.size() % 2) != 0)
	{
		Error = "El charset ID 8 debe estar codificado como UTF-16LE.";
		return false;
	}
	DecodedTga atlas;
	if (!DecodeTgaRgba(atlasEntry->DecompressedData, atlas) || atlas.Width % DebugFontGridCols != 0 || atlas.Height % DebugFontGridRows != 0)
	{
		Error = "No se pudo decodificar el atlas TGA del ID 7 o sus dimensiones no coinciden con la cuadricula 16x8.";
		return false;
	}
	DynamicArray<char16> charset;
	charset.reserve(charsetEntry->DecompressedData.size() / 2);
	for (size_t i = 0; i < charsetEntry->DecompressedData.size(); i += 2)
	{
		charset.push_back(static_cast<char16>(static_cast<u16>(charsetEntry->DecompressedData[i]) | (static_cast<u16>(charsetEntry->DecompressedData[i + 1]) << 8)));
	}
	if (charset.size() >= 3 && charset[0] == 0xFEFF)
	{
		charset.erase(charset.begin());
	}
	if (charset.empty())
	{
		Error = "El charset ID 8 no contiene caracteres.";
		return false;
	}
	try
	{
		DebugFont = Mem::MakeUnique<Font::font>(static_cast<Mem::MemBase&>(J2DHeap));
	}
	catch (const std::bad_alloc&)
	{
		Error = "No hay memoria suficiente en J2DHeap para crear la fuente.";
		return false;
	}
	if (!DebugFont)
	{
		Error = "No hay memoria suficiente en J2DHeap para crear la fuente.";
		return false;
	}
	DebugFont->Create(Display, CmdBuffer);
	Font::AtlasInfo atlasInfo{};
	atlasInfo.Width = static_cast<u16>(atlas.Width);
	atlasInfo.Height = static_cast<u16>(atlas.Height);
	atlasInfo.GridCols = static_cast<u16>(DebugFontGridCols);
	atlasInfo.GridRows = static_cast<u16>(DebugFontGridRows);
	DebugFont->SetAtlasInfo(atlasInfo);
	DebugFont->LoadCharsetMap(charset.data(), charset.size());
	DebugFont->LoadShader(reinterpret_cast<const char*>(vertexEntry->DecompressedData.data()), static_cast<u32>(vertexEntry->DecompressedData.size()), reinterpret_cast<const char*>(fragmentEntry->DecompressedData.data()), static_cast<u32>(fragmentEntry->DecompressedData.size()));
	DebugFont->SetTextColor(1.0f, 1.0f, 1.0f, 1.0f);
	DebugFont->LoadAtlas(atlas.Pixels.data(), atlas.Pixels.size(), 0);
	DebugFont->FinAtlasLoad();
	if (!DebugFont->IsValid())
	{
		DebugFont->Destroy();
		DebugFont.reset();
		Error = "No se pudo inicializar la fuente con los recursos del data.dat.";
		return false;
	}
	return true;
}
bool Game::GameSystem::InitWindow()
{
#ifdef _WIN32
	HInstance = GetModuleHandleW(nullptr);
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = HInstance;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wc.lpszClassName = WindowClassName;
	if (!RegisterClassExW(&wc))
	{
		return false;
	}
	RECT rect = { 0, 0, static_cast<LONG>(WindowWidth), static_cast<LONG>(WindowHeight) };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	Hwnd = CreateWindowExW(0, WindowClassName, WindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, HInstance, this );
	if (!Hwnd)
	{
		return false;
	}
	ShowWindow(Hwnd, SW_SHOW);
	UpdateWindow(Hwnd);
	return true;
#else
	return false;
#endif
}
void Game::GameSystem::ProcessMessages()
{
#ifdef _WIN32
	MSG msg{};
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			Running = false;
			return;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
#endif
}
#ifdef _WIN32
LRESULT CALLBACK Game::GameSystem::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_NCCREATE)
	{
		const auto* createInfo = reinterpret_cast<const CREATESTRUCTW*>(lParam);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createInfo->lpCreateParams));
		return TRUE;
	}
	auto* game = reinterpret_cast<GameSystem*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
#ifdef _DEBUG
		if (game && game->ScriptConsole.IsVisible() && game->ScriptConsole.IsMenuActive())
		{
			if (wParam == VK_UP)
			{
				game->ScriptConsole.MoveMenuSelection(-1);
				return 0;
			}
			if (wParam == VK_DOWN)
			{
				game->ScriptConsole.MoveMenuSelection(1);
				return 0;
			}
			if (wParam == VK_RETURN)
			{
				(void)game->ScriptConsole.ActivateMenuSelection();
				return 0;
			}
			if (wParam == VK_ESCAPE)
			{
				game->ScriptConsole.EndMenu();
				game->ScriptConsole.SetVisible(false);
				return 0;
			}
		}
		if (game && (game->MemoryConsole.IsVisible() || game->MemoryHeapView.IsVisible()))
		{
			const bool showingMemoryConsole = game->MemoryConsole.IsVisible();
			auto moveConsole = [game, showingMemoryConsole](s32 HorizontalDirection, s32 VerticalDirection)
			{
				if (showingMemoryConsole)
				{
					game->MemoryConsole.Move(HorizontalDirection, VerticalDirection);
				}
				else
				{
					game->MemoryHeapView.Move(HorizontalDirection, VerticalDirection);
				}
			};
			if (wParam == VK_UP)
			{
				moveConsole(0, -1);
				return 0;
			}
			if (wParam == VK_DOWN)
			{
				moveConsole(0, 1);
				return 0;
			}
			if (wParam == VK_LEFT)
			{
				moveConsole(-1, 0);
				return 0;
			}
			if (wParam == VK_RIGHT)
			{
				moveConsole(1, 0);
				return 0;
			}
			if (wParam == VK_PRIOR)
			{
				if (showingMemoryConsole) game->MemoryConsole.ScrollPage(-1);
				return 0;
			}
			if (wParam == VK_NEXT)
			{
				if (showingMemoryConsole) game->MemoryConsole.ScrollPage(1);
				return 0;
			}
		}
#endif
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
			return 0;
		}
		break;
	case WM_KEYUP:
		if (wParam == 'P' && game)
		{
			game->Camera.Toggle3DView();
			return 0;
		}
		if (wParam == VK_F1 && game)
		{
			game->World.RequestMapLoad(1);
			return 0;
		}
		if (wParam == VK_F2 && game)
		{
			game->World.ToggleCollisionVisibility();
			return 0;
		}
#ifdef _DEBUG
		if (wParam == VK_F4 && game)
		{
			if (game->ScriptConsole.IsVisible())
			{
				game->ScriptConsole.EndMenu();
				game->ScriptConsole.SetVisible(false);
			}
			game->MemoryConsole.Toggle();
			return 0;
		}
		if (wParam == VK_F5 && game)
		{
			if (game->ScriptConsole.IsVisible())
			{
				game->ScriptConsole.EndMenu();
				game->ScriptConsole.SetVisible(false);
			}
			game->MemoryHeapView.Toggle();
			return 0;
		}
		if (wParam == VK_F3 && game && game->ProcessBars)
		{
			game->ProcessBarsVisible = !game->ProcessBarsVisible;
			game->ProcessBars->SetVisible(game->ProcessBarsVisible);
			return 0;
		}
#endif
		break;
	case WM_MOUSEWHEEL:
		if (game)
		{
#ifdef _DEBUG
			if (game->MemoryConsole.IsVisible() || game->MemoryHeapView.IsVisible())
			{
				return 0;
			}
#endif
			const f32 wheelSteps = static_cast<f32>(GET_WHEEL_DELTA_WPARAM(wParam)) /
				static_cast<f32>(WHEEL_DELTA);
			game->Camera.AddZoom(wheelSteps);
		}
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}
#endif
