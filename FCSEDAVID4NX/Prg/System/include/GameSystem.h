#pragma once
#include<Types.h>
#include<Macros.h>
#include<Display.h>
#include<DisplayTimer.h>
#include<CommandBuffer.h>
#include<Filter.h>
#include<MemoryDebugConsole.h>
#ifdef _DEBUG
#include<HeapView.h>
#endif
#include<Shader.h>
#include<FileLoader.h>
#include<Font.h>
#include<MemAlcExp.h>
#include<MemTypes.h>
#include<GameCamera.h>
#include<GameWorld.h>
#include<DynamicString.h>
#ifdef _DEBUG
#include<FocaUtils/FocaUtilBar.h>
#include<FocaUtils/FocaCon.h>
#endif
namespace Game
{
	class GameSystem final : NonCopy<>
	{
	public:
		GameSystem();
		~GameSystem();
		bool Init();
		[[nodiscard]] bool IsRunning() const;
		void RunFrame();
		void Finalize();
		[[nodiscard]] const String& GetInitError() const;
		[[nodiscard]] gx::Display& GetDisplay();
		[[nodiscard]] gx::CommandBuffer& GetCommandBuffer();
		[[nodiscard]] gx::Shader& GetShader();
		[[nodiscard]] f32 GetCombinationRatio() const;
	private:
		bool InitializeHeaps();
		bool InitWindow();
		bool LoadShaders(String& Error);
		bool InitializeFilter(String& Error);
		bool LoadDebugFont(String& Error);
		void ProcessMessages();
		Mem::MemAlcExp SystemHeap;
		Mem::MemAlcExp GameHeap;
		Mem::MemAlcExp WorldHeap;
		Mem::MemAlcExp ArchiveHeap;
		Mem::MemAlcExp J2DHeap;
		Mem::MemAlcExp CommandHeap;
		gx::Display Display;
		gx::CommandBuffer CmdBuffer;
		Mem::UniquePtr<Filter> Antialiasing;
		gx::Shader Shader;
		GameUtil::FileLoader Resources;
		Mem::UniquePtr<Font::font> DebugFont;
		MemoryDebugConsole MemoryConsole;
		HeapView MemoryHeapView;
		FSys::Utils::FocaCon ScriptConsole;
		gx::Shader DebugOverlayShader;
		FSys::Utils::FocaUtilBar* ProcessBars;
		bool ProcessBarsVisible;
		GameWorld World;
		GameCamera Camera;
		f32 CombinationRatio;
		bool Running;
		gx::DisplayTimer DisplayRetraceClock;
		bool DisplayRetraceClockStarted = false;
		u64 PreviousTime = 0;
		f64 Accumulator = 0.0;
		bool ProcessBarFrameStarted = false;
		u64 PreviousRefreshCount = 0;
		bool PreviousRefreshCountValid = false;
		u64 PreviousFrameTick = 0;
		String InitError;
#ifdef _WIN32
		HWND Hwnd = nullptr;
		HINSTANCE HInstance = nullptr;
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
	};
}
