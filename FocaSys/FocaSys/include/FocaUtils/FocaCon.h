#pragma once
#if defined(_DEBUG)
#include<Buffer.h>
#include<CommandBuffer.h>
#include<Display.h>
#include<Macros.h>
#include<Shader.h>
#include<Texture.h>
#include<Types.h>
#include<array>
#include<cstddef>
namespace Font
{
	class font;
}
namespace FSys::Utils
{
	class FocaCon final : private NonCopy<>
	{
	public:
		static constexpr u32 MaxLineCount = 32;
		static constexpr u32 MaxLineCharacters = 160;
		static constexpr u32 MaxMenuOptionCount = 32;
		static constexpr u32 VisibleMenuOptionCount = 14;
		static constexpr u32 NoMenuOption = 0xffffffffu;
		FocaCon();
		~FocaCon();
		[[nodiscard]] bool Initialize(gx::Display& Display, gx::Shader& DrawShader);
		void Finalize();
		void Clear();
		void ClearLines();
		void PrintLine(const char16* Text);
		void Print(const char16* Format, ...);
		void SetVisible(bool Visible);
		void ToggleVisible();
		[[nodiscard]] bool IsVisible() const;
		void Move(s32 HorizontalDirection, s32 VerticalDirection);
		void SetPosition(f32 X, f32 Y);
		void SetTextScale(f32 Scale);
		void SetScrollRange(std::size_t MaximumOffset);
		void ScrollPage(s32 Direction);
		[[nodiscard]] std::size_t GetScrollOffset() const;
		void BeginMenu(const char16* Title);
		void EndMenu();
		void ClearMenuOptions();
		[[nodiscard]] bool AddMenuOption(const char16* Label, bool Enabled = true);
		[[nodiscard]] bool SetMenuOptionEnabled(u32 OptionIndex, bool Enabled);
		[[nodiscard]] bool SetMenuSelection(u32 OptionIndex);
		void MoveMenuSelection(s32 Direction);
		[[nodiscard]] u32 GetMenuOptionCount() const;
		[[nodiscard]] u32 GetMenuSelection() const;
		[[nodiscard]] bool ActivateMenuSelection();
		[[nodiscard]] bool ConsumeMenuSelection(u32& OptionIndex);
		[[nodiscard]] bool IsMenuActive() const;
		void Draw(gx::CommandBuffer& CommandBuffer, Font::font& DebugFont);
	private:
		struct Line
		{
			char16 Text[MaxLineCharacters]{};
		};
		struct MenuOption
		{
			char16 Label[MaxLineCharacters]{};
			bool Enabled = true;
		};
		void AppendLine(const wchar_t* Text);
		void AppendLine(const char16* Text);
		void CopyMenuText(char16* Destination, const char16* Source);
		void KeepMenuSelectionVisible();
		void DrawBackground(gx::CommandBuffer& CommandBuffer, f32 X, f32 Y, f32 Width, f32 Height, f32 ScreenWidth, f32 ScreenHeight);
		gx::Display* Display = nullptr;
		gx::Shader* DrawShader = nullptr;
		gx::Buffer BackgroundBuffer;
		gx::Texture WhiteTexture;
		std::array<Line, MaxLineCount> Lines{};
		std::array<MenuOption, MaxMenuOptionCount> MenuOptions{};
		char16 MenuTitle[MaxLineCharacters]{};
		u32 LineCount = 0;
		u32 MenuOptionCount = 0;
		u32 MenuSelection = NoMenuOption;
		u32 MenuFirstVisibleOption = 0;
		u32 ActivatedMenuOption = NoMenuOption;
		std::size_t ScrollOffset = 0;
		std::size_t MaximumScrollOffset = 0;
		f32 PositionX = 0.0f;
		f32 PositionY = 0.0f;
		f32 TextScale = 1.0f;
		bool Visible = false;
		bool MenuActive = false;
	};
}
#endif
