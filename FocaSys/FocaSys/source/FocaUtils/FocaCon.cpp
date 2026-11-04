#include<FocaUtils/FocaCon.h>
#if defined(_DEBUG)
#include<Font.h>
#include<algorithm>
#include<cstdarg>
#include<cwchar>
namespace
{
	struct ConsoleVertex
	{
		f32 Position[3]{};
		f32 Normal[3]{};
		f32 Color[4]{};
		f32 UV[2]{};
	};
	static_assert(sizeof(ConsoleVertex) == sizeof(f32) * 12);
}
FSys::Utils::FocaCon::FocaCon()
{

}
FSys::Utils::FocaCon::~FocaCon()
{
	Finalize();
}
bool FSys::Utils::FocaCon::Initialize(gx::Display& DisplayIn, gx::Shader& DrawShaderIn)
{
	Finalize();
	Display = &DisplayIn;
	DrawShader = &DrawShaderIn;
	BackgroundBuffer.Initialize(DisplayIn);
	constexpr std::array<u8, 4> whitePixel = { 255, 255, 255, 255 };
	if (!WhiteTexture.Initialize(DisplayIn, DrawShaderIn, 1, 1, whitePixel.data()))
	{
		Finalize();
		return false;
	}
	const std::array<ConsoleVertex, 6> emptyRectangle{};
	if (!BackgroundBuffer.CreateVertexBuffer(emptyRectangle.data(), sizeof(emptyRectangle)))
	{
		Finalize();
		return false;
	}
	return true;
}
void FSys::Utils::FocaCon::Finalize()
{
	WhiteTexture.Finalize();
	BackgroundBuffer.Finalize();
	Display = nullptr;
	DrawShader = nullptr;
	Clear();
	PositionX = 0.0f;
	PositionY = 0.0f;
	TextScale = 1.0f;
	Visible = false;
	EndMenu();
}
void FSys::Utils::FocaCon::Clear()
{
	ClearLines();
	ScrollOffset = 0;
	MaximumScrollOffset = 0;
}
void FSys::Utils::FocaCon::ClearLines()
{
	LineCount = 0;
}
void FSys::Utils::FocaCon::PrintLine(const char16* Text)
{
	if (Text)
	{
		AppendLine(Text);
	}
}
void FSys::Utils::FocaCon::Print(const char16* Format, ...)
{
	if (!Format)
	{
		return;
	}
	wchar_t wideFormat[MaxLineCharacters]{};
	for (u32 i = 0; i + 1 < MaxLineCharacters && Format[i] != 0; ++i)
	{
		wideFormat[i] = static_cast<wchar_t>(Format[i]);
	}
	wchar_t wideText[MaxLineCharacters]{};
	va_list arguments;
	va_start(arguments, Format);
	std::vswprintf(wideText, MaxLineCharacters, wideFormat, arguments);
	va_end(arguments);
	wideText[MaxLineCharacters - 1] = 0;
	const wchar_t* lineStart = wideText;
	for (const wchar_t* character = wideText; ; ++character)
	{
		if (*character == L'\n' || *character == 0)
		{
			AppendLine(lineStart);
			if (*character == 0)
			{
				break;
			}
			lineStart = character + 1;
		}
	}
}
void FSys::Utils::FocaCon::AppendLine(const wchar_t* Text)
{
	if (LineCount >= MaxLineCount)
	{
		for (u32 i = 1; i < MaxLineCount; ++i)
		{
			Lines[i - 1] = Lines[i];
		}
		LineCount = MaxLineCount - 1;
	}
	Line& line = Lines[LineCount++];
	u32 index = 0;
	while (index + 1 < MaxLineCharacters && Text[index] != 0)
	{
		line.Text[index] = static_cast<char16>(Text[index]);
		++index;
	}
	line.Text[index] = 0;
	for (++index; index < MaxLineCharacters; ++index)
	{
		line.Text[index] = 0;
	}
}
void FSys::Utils::FocaCon::AppendLine(const char16* Text)
{
	if (LineCount >= MaxLineCount)
	{
		for (u32 i = 1; i < MaxLineCount; ++i)
		{
			Lines[i - 1] = Lines[i];
		}
		LineCount = MaxLineCount - 1;
	}
	Line& line = Lines[LineCount++];
	u32 index = 0;
	while (index + 1 < MaxLineCharacters && Text[index] != 0)
	{
		line.Text[index] = Text[index];
		++index;
	}
	line.Text[index] = 0;
	for (++index; index < MaxLineCharacters; ++index)
	{
		line.Text[index] = 0;
	}
}
void FSys::Utils::FocaCon::SetVisible(bool VisibleIn)
{
	Visible = VisibleIn;
}
void FSys::Utils::FocaCon::ToggleVisible()
{
	Visible = !Visible;
}
bool FSys::Utils::FocaCon::IsVisible() const
{
	return Visible;
}
void FSys::Utils::FocaCon::Move(s32 HorizontalDirection, s32 VerticalDirection)
{
	constexpr f32 movementStep = 8.0f;
	PositionX += static_cast<f32>(HorizontalDirection) * movementStep;
	PositionY += static_cast<f32>(VerticalDirection) * movementStep;
}
void FSys::Utils::FocaCon::SetPosition(f32 X, f32 Y)
{
	PositionX = X;
	PositionY = Y;
}
void FSys::Utils::FocaCon::SetTextScale(f32 Scale)
{
	TextScale = (std::max)(0.25f, Scale);
}
void FSys::Utils::FocaCon::SetScrollRange(std::size_t MaximumOffset)
{
	MaximumScrollOffset = MaximumOffset;
	ScrollOffset = (std::min)(ScrollOffset, MaximumScrollOffset);
}
void FSys::Utils::FocaCon::ScrollPage(s32 Direction)
{
	constexpr std::size_t pageRows = 14;
	if (Direction < 0)
	{
		ScrollOffset = ScrollOffset > pageRows ? ScrollOffset - pageRows : 0;
	}
	else if (Direction > 0)
	{
		const std::size_t remaining = MaximumScrollOffset - ScrollOffset;
		ScrollOffset = remaining < pageRows ? MaximumScrollOffset : ScrollOffset + pageRows;
	}
}
std::size_t FSys::Utils::FocaCon::GetScrollOffset() const
{
	return ScrollOffset;
}
void FSys::Utils::FocaCon::BeginMenu(const char16* Title)
{
	ClearMenuOptions();
	CopyMenuText(MenuTitle, Title);
	MenuActive = true;
	ActivatedMenuOption = NoMenuOption;
}
void FSys::Utils::FocaCon::EndMenu()
{
	MenuActive = false;
	MenuSelection = NoMenuOption;
	MenuFirstVisibleOption = 0;
	ActivatedMenuOption = NoMenuOption;
}
void FSys::Utils::FocaCon::ClearMenuOptions()
{
	MenuOptionCount = 0;
	MenuSelection = NoMenuOption;
	MenuFirstVisibleOption = 0;
	ActivatedMenuOption = NoMenuOption;
}
bool FSys::Utils::FocaCon::AddMenuOption(const char16* Label, bool Enabled)
{
	if (!MenuActive || !Label || MenuOptionCount >= MaxMenuOptionCount)
	{
		return false;
	}
	MenuOption& option = MenuOptions[MenuOptionCount];
	CopyMenuText(option.Label, Label);
	option.Enabled = Enabled;
	if (Enabled && MenuSelection == NoMenuOption)
	{
		MenuSelection = MenuOptionCount;
	}
	++MenuOptionCount;
	KeepMenuSelectionVisible();
	return true;
}
bool FSys::Utils::FocaCon::SetMenuOptionEnabled(u32 OptionIndex, bool Enabled)
{
	if (!MenuActive || OptionIndex >= MenuOptionCount)
	{
		return false;
	}
	MenuOptions[OptionIndex].Enabled = Enabled;
	if (!Enabled && MenuSelection == OptionIndex)
	{
		ActivatedMenuOption = NoMenuOption;
		MoveMenuSelection(1);
	}
	else if (Enabled && MenuSelection == NoMenuOption)
	{
		MenuSelection = OptionIndex;
		KeepMenuSelectionVisible();
	}
	KeepMenuSelectionVisible();
	return true;
}
bool FSys::Utils::FocaCon::SetMenuSelection(u32 OptionIndex)
{
	if (!MenuActive || OptionIndex >= MenuOptionCount || !MenuOptions[OptionIndex].Enabled)
	{
		return false;
	}
	MenuSelection = OptionIndex;
	ActivatedMenuOption = NoMenuOption;
	KeepMenuSelectionVisible();
	return true;
}
void FSys::Utils::FocaCon::MoveMenuSelection(s32 Direction)
{
	if (!MenuActive || MenuOptionCount == 0 || Direction == 0)
	{
		return;
	}
	ActivatedMenuOption = NoMenuOption;
	u32 candidate = MenuSelection;
	if (candidate == NoMenuOption || candidate >= MenuOptionCount)
	{
		candidate = Direction > 0 ? MenuOptionCount - 1 : 0;
	}
	for (u32 attempt = 0; attempt < MenuOptionCount; ++attempt)
	{
		if (Direction > 0)
		{
			candidate = (candidate + 1) % MenuOptionCount;
		}
		else
		{
			candidate = candidate == 0 ? MenuOptionCount - 1 : candidate - 1;
		}
		if (MenuOptions[candidate].Enabled)
		{
			MenuSelection = candidate;
			KeepMenuSelectionVisible();
			return;
		}
	}
	MenuSelection = NoMenuOption;
}
u32 FSys::Utils::FocaCon::GetMenuOptionCount() const
{
	return MenuOptionCount;
}
u32 FSys::Utils::FocaCon::GetMenuSelection() const
{
	return MenuSelection;
}
bool FSys::Utils::FocaCon::ActivateMenuSelection()
{
	if (!MenuActive || MenuSelection == NoMenuOption || MenuSelection >= MenuOptionCount || !MenuOptions[MenuSelection].Enabled)
	{
		return false;
	}
	ActivatedMenuOption = MenuSelection;
	return true;
}
bool FSys::Utils::FocaCon::ConsumeMenuSelection(u32& OptionIndex)
{
	if (ActivatedMenuOption == NoMenuOption || ActivatedMenuOption >= MenuOptionCount)
	{
		return false;
	}
	OptionIndex = ActivatedMenuOption;
	ActivatedMenuOption = NoMenuOption;
	return true;
}
bool FSys::Utils::FocaCon::IsMenuActive() const
{
	return MenuActive;
}
void FSys::Utils::FocaCon::CopyMenuText(char16* Destination, const char16* Source)
{
	if (!Destination)
	{
		return;
	}
	u32 index = 0;
	if (Source)
	{
		while (index + 1 < MaxLineCharacters && Source[index] != 0)
		{
			Destination[index] = Source[index];
			++index;
		}
	}
	Destination[index] = 0;
	for (++index; index < MaxLineCharacters; ++index)
	{
		Destination[index] = 0;
	}
}
void FSys::Utils::FocaCon::KeepMenuSelectionVisible()
{
	if (MenuSelection == NoMenuOption || MenuSelection >= MenuOptionCount)
	{
		return;
	}
	if (MenuSelection < MenuFirstVisibleOption)
	{
		MenuFirstVisibleOption = MenuSelection;
	}
	else if (MenuSelection >= MenuFirstVisibleOption + VisibleMenuOptionCount)
	{
		MenuFirstVisibleOption = MenuSelection - VisibleMenuOptionCount + 1;
	}
}
void FSys::Utils::FocaCon::Draw(gx::CommandBuffer& CommandBuffer, Font::font& DebugFont)
{
	if (!Visible || !Display || (LineCount == 0 && !MenuActive))
	{
		return;
	}
	const gx::NativeFrameContext frameContext = CommandBuffer.GetNativeFrameContext();
	const gx::detail::DisplayResolution displayResolution = Display->GetResolution();
	const f32 viewportWidth = static_cast<f32>(frameContext.SceneExtent.width != 0 ? frameContext.SceneExtent.width : displayResolution.Width);
	const f32 viewportHeight = static_cast<f32>(frameContext.SceneExtent.height != 0 ? frameContext.SceneExtent.height : displayResolution.Height);
	if (viewportWidth <= 0.0f || viewportHeight <= 0.0f)
	{
		return;
	}
	const f32 scaleX = viewportWidth / 640.0f;
	const f32 scaleY = viewportHeight / 242.0f;
	const f32 charWidth = (std::max)(1.0f, std::round(10.0f * TextScale * scaleX));
	const f32 charHeight = (std::max)(1.0f, std::round(10.0f * TextScale * scaleY));
	const f32 lineHeight = (std::max)(charHeight, std::round(10.0f * TextScale * scaleY));
	const f32 textOffsetX = std::round(18.0f * TextScale * scaleX);
	const f32 textOffsetY = std::round(7.0f * TextScale * scaleY);
	const u32 visibleOptionCount = MenuActive ? (std::min)(MenuOptionCount, VisibleMenuOptionCount) : 0;
	const u32 titleRowCount = MenuActive && MenuTitle[0] != 0 ? 1 : 0;
	const u32 menuRows = MenuActive ? visibleOptionCount + titleRowCount : 0;
	const f32 panelHeight = lineHeight * static_cast<f32>(LineCount + menuRows + 1);
	PositionX = std::round(PositionX);
	PositionY = std::round(PositionY);
	DrawBackground(CommandBuffer, PositionX, PositionY, viewportWidth - PositionX, panelHeight, viewportWidth, viewportHeight);
	DebugFont.SetTextColor(1.0f, 1.0f, 1.0f, 1.0f);
	for (u32 i = 0; i < LineCount; ++i)
	{
		DebugFont.DrawTextU(Lines[i].Text, PositionX + textOffsetX,
			PositionY + textOffsetY + lineHeight * static_cast<f32>(i), charWidth, charHeight);
	}
	if (MenuActive)
	{
		const f32 menuY = PositionY + textOffsetY + lineHeight * static_cast<f32>(LineCount);
		if (titleRowCount != 0)
		{
			DebugFont.SetTextColor(1.0f, 1.0f, 1.0f, 1.0f);
			DebugFont.DrawTextU(MenuTitle, PositionX + textOffsetX, menuY, charWidth, charHeight);
		}
		for (u32 visibleIndex = 0; visibleIndex < visibleOptionCount; ++visibleIndex)
		{
			const u32 optionIndex = MenuFirstVisibleOption + visibleIndex;
			if (optionIndex >= MenuOptionCount)
			{
				break;
			}
			const MenuOption& option = MenuOptions[optionIndex];
			char16 optionText[MaxLineCharacters]{};
			const bool selected = optionIndex == MenuSelection;
			optionText[0] = selected ? u'>' : (option.Enabled ? u' ' : u'-');
			optionText[1] = u' ';
			u32 characterIndex = 0;
			while (characterIndex + 3 < MaxLineCharacters && option.Label[characterIndex] != 0)
			{
				optionText[characterIndex + 2] = option.Label[characterIndex];
				++characterIndex;
			}
			if (selected)
			{
				DebugFont.SetTextColor(0.35f, 1.0f, 0.35f, 1.0f);
			}
			else if (!option.Enabled)
			{
				DebugFont.SetTextColor(0.55f, 0.55f, 0.55f, 1.0f);
			}
			else
			{
				DebugFont.SetTextColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
			DebugFont.DrawTextU(optionText, PositionX + textOffsetX, menuY + lineHeight * static_cast<f32>(visibleIndex + titleRowCount), charWidth, charHeight);
		}
		DebugFont.SetTextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}
void FSys::Utils::FocaCon::DrawBackground(gx::CommandBuffer& CommandBuffer, f32 X, f32 Y, f32 Width, f32 Height, f32 ScreenWidth, f32 ScreenHeight)
{
	if (!DrawShader || !BackgroundBuffer.IsValid() || !WhiteTexture.IsValid() || Width <= 0.0f || Height <= 0.0f || ScreenWidth <= 0.0f || ScreenHeight <= 0.0f)
	{
		return;
	}
	const f32 left = X * 2.0f / ScreenWidth - 1.0f;
	const f32 top = Y * 2.0f / ScreenHeight - 1.0f;
	const f32 right = (X + Width) * 2.0f / ScreenWidth - 1.0f;
	const f32 bottom = (Y + Height) * 2.0f / ScreenHeight - 1.0f;
	auto makeVertex = [](f32 x, f32 y)
	{
		ConsoleVertex result{};
		result.Position[0] = x;
		result.Position[1] = y;
		result.Color[3] = 224.0f / 255.0f;
		result.UV[0] = 0.5f;
		result.UV[1] = 0.5f;
		return result;
	};
	const std::array<ConsoleVertex, 6> vertices = {
		makeVertex(left, top), makeVertex(right, top), makeVertex(right, bottom),
		makeVertex(left, top), makeVertex(right, bottom), makeVertex(left, bottom)
	};
	if (!BackgroundBuffer.UpdateVertexBuffer(vertices.data(), sizeof(vertices)))
	{
		return;
	}
	const glm::mat4 identity(1.0f);
	DrawShader->SetMatrices(identity, identity, identity);
	CommandBuffer.SetOverlayPass(true);
	CommandBuffer.SetShader(DrawShader);
	CommandBuffer.BindVertexBuffer(&BackgroundBuffer);
	CommandBuffer.BindTexture(&WhiteTexture);
	CommandBuffer.Draw(static_cast<u32>(vertices.size()), 1, 0, 0);
	CommandBuffer.SetOverlayPass(false);
}
#endif
