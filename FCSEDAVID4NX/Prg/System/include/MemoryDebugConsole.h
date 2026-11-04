#pragma once
#ifdef _DEBUG
#include<Macros.h>
#include<Types.h>
#include<CommandBuffer.h>
#include<Display.h>
#include<Font.h>
#include<MemAlcExp.h>
#include<Shader.h>
#include<FocaUtils/FocaCon.h>
#include<array>
namespace Game
{
	class MemoryDebugConsole final : NonCopy<>
	{
	public:
		[[nodiscard]] bool Initialize(gx::Display& Display, gx::Shader& DrawShader);
		void Finalize();
		void Toggle() noexcept;
		void Move(s32 HorizontalDirection, s32 VerticalDirection) noexcept;
		void ScrollPage(s32 Direction) noexcept;
		[[nodiscard]] bool IsVisible() const noexcept;
		void Draw(gx::CommandBuffer& CommandBuffer, Font::font& DebugFont, const std::array<Mem::MemAlcExp*, 6>& Heaps);
	private:
		FSys::Utils::FocaCon Console;
	};
}
#endif
