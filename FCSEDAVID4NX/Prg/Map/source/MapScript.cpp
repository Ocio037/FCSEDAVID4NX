#include"MapScript.h"
#include"GameMap.h"
#include<SysData.h>
#include<algorithm>
#include<charconv>
#include<cstddef>
#include<cstdint>
#include<iterator>
#include<limits>
#include<array>
#include<string_view>
#include<utility>

namespace
{
#ifdef _DEBUG
	bool ConvertUtf8ToChar16(std::string_view Source, char16* Destination, std::size_t Capacity)
	{
		if (Destination == nullptr || Capacity == 0)
		{
			return false;
		}
		std::size_t SourceOffset = 0;
		std::size_t DestinationOffset = 0;
		while (SourceOffset < Source.size() && DestinationOffset + 1 < Capacity)
		{
			const auto FirstByte = static_cast<u8>(Source[SourceOffset]);
			std::uint32_t CodePoint = 0xfffd;
			std::size_t SequenceLength = 1;
			if (FirstByte < 0x80)
			{
				CodePoint = FirstByte;
			}
			else if ((FirstByte & 0xe0) == 0xc0 && SourceOffset + 1 < Source.size())
			{
				const auto SecondByte = static_cast<u8>(Source[SourceOffset + 1]);
				if ((SecondByte & 0xc0) == 0x80)
				{
					CodePoint = ((FirstByte & 0x1f) << 6) | (SecondByte & 0x3f);
					SequenceLength = 2;
					if (CodePoint < 0x80) CodePoint = 0xfffd;
				}
			}
			else if ((FirstByte & 0xf0) == 0xe0 && SourceOffset + 2 < Source.size())
			{
				const auto SecondByte = static_cast<u8>(Source[SourceOffset + 1]);
				const auto ThirdByte = static_cast<u8>(Source[SourceOffset + 2]);
				if ((SecondByte & 0xc0) == 0x80 && (ThirdByte & 0xc0) == 0x80)
				{
					CodePoint = ((FirstByte & 0x0f) << 12) | ((SecondByte & 0x3f) << 6) | (ThirdByte & 0x3f);
					SequenceLength = 3;
					if (CodePoint < 0x800 || (CodePoint >= 0xd800 && CodePoint <= 0xdfff)) CodePoint = 0xfffd;
				}
			}
			else if ((FirstByte & 0xf8) == 0xf0 && SourceOffset + 3 < Source.size())
			{
				const auto SecondByte = static_cast<u8>(Source[SourceOffset + 1]);
				const auto ThirdByte = static_cast<u8>(Source[SourceOffset + 2]);
				const auto FourthByte = static_cast<u8>(Source[SourceOffset + 3]);
				if ((SecondByte & 0xc0) == 0x80 && (ThirdByte & 0xc0) == 0x80 && (FourthByte & 0xc0) == 0x80)
				{
					CodePoint = ((FirstByte & 0x07) << 18) | ((SecondByte & 0x3f) << 12) |
						((ThirdByte & 0x3f) << 6) | (FourthByte & 0x3f);
					SequenceLength = 4;
					if (CodePoint < 0x10000 || CodePoint > 0x10ffff) CodePoint = 0xfffd;
				}
			}
			SourceOffset += SequenceLength;
			if (CodePoint <= 0xffff)
			{
				Destination[DestinationOffset++] = static_cast<char16>(CodePoint);
			}
			else
			{
				if (DestinationOffset + 2 >= Capacity)
				{
					break;
				}
				CodePoint -= 0x10000;
				Destination[DestinationOffset++] = static_cast<char16>(0xd800 + (CodePoint >> 10));
				Destination[DestinationOffset++] = static_cast<char16>(0xdc00 + (CodePoint & 0x3ff));
			}
		}
		Destination[DestinationOffset] = 0;
		return true;
	}
#endif
}
Game::MapScript::MapScript() : 
	MapOwner(nullptr),
	PlayerEnabled(nullptr),
	ScriptConsole(nullptr)
{

}
Game::MapScript::~MapScript()
{
	Finalize();
}
bool Game::MapScript::Initialize(const DynamicArray<u8>& Bytecode, MapSystem& Map, String* Error, FSys::Utils::FocaCon* ScriptConsoleIn, u32 InitialTrackId, bool* PlayerEnabledIn)
{
	Finalize();
	if (Error)
	{
		Error->clear();
	}
	MapOwner = &Map;
	PlayerEnabled = PlayerEnabledIn;
	ScriptConsole = ScriptConsoleIn;
	ScriptBytecode.reserve(Bytecode.size());
	for (u8 Byte : Bytecode)
	{
		ScriptBytecode.push_back(Byte);
	}
	try
	{
		Runtime = Mem::MakeUnique<ohana::runtime>(ScriptBytecode.data(), ScriptBytecode.size());
		if (!Runtime)
		{
			if (Error) *Error = "No se pudo crear el runtime OhanaScript del mapa.";
			Finalize();
			return false;
		}
		Runtime->set_output([this](std::string_view Text)
		{
			if (ScriptConsole)
			{
				char16 ConvertedText[FSys::Utils::FocaCon::MaxLineCharacters]{};
				if (ConvertUtf8ToChar16(Text, ConvertedText, std::size(ConvertedText)))
				{
					ScriptConsole->PrintLine(ConvertedText);
					ScriptConsole->SetVisible(true);
				}
			}
			(void)Text;
		});
		RegisterNatives();
		Runtime->execute();
		if (InitialTrackId != 0)
		{
			constexpr std::string_view TrackFunctionPrefix = "__event_editor_track_";
			std::array<char, TrackFunctionPrefix.size() + 10> TrackFunctionName{};
			std::copy(TrackFunctionPrefix.begin(), TrackFunctionPrefix.end(), TrackFunctionName.begin());
			const auto TrackIdResult = std::to_chars(TrackFunctionName.data() + TrackFunctionPrefix.size(),
				TrackFunctionName.data() + TrackFunctionName.size(), InitialTrackId);
			if (TrackIdResult.ec != std::errc{})
			{
				if (Error) *Error = "El ID de pista inicial no es valido.";
				Finalize();
				return false;
			}
			const std::string_view TrackFunction(TrackFunctionName.data(), static_cast<std::size_t>(TrackIdResult.ptr - TrackFunctionName.data()));
			bool TrackFound = false;
			for (std::uint32_t FunctionIndex = 0; FunctionIndex < Runtime->function_count(); ++FunctionIndex)
			{
				if (Runtime->function_name(FunctionIndex) == TrackFunction)
				{
					Runtime->call_preserving_state(FunctionIndex);
					TrackFound = true;
					break;
				}
			}
			if (!TrackFound)
			{
				if (Error) *Error = "StartEvent solicita una pista que no existe en este FEST.";
				Finalize();
				return false;
			}
		}
	}
	catch (const std::exception& Exception)
	{
		if (Error)
		{
			*Error = "Error al ejecutar el script del mapa: ";
			Error->append(Exception.what());
		}
		Finalize();
		return false;
	}
	return true;
}
void Game::MapScript::Update()
{
	if (!Runtime || !Runtime->is_suspended())
	{
		return;
	}
	try
	{
		Runtime->resume();
	}
	catch (const std::exception&)
	{
		Runtime.reset();
		if (ScriptConsole)
		{
			ScriptConsole->EndMenu();
			ScriptConsole->ClearLines();
			ScriptConsole->Print(u"OhanaScript script failed; execution stopped");
			ScriptConsole->SetVisible(true);
		}
	}
}
void Game::MapScript::Finalize()
{
	Runtime.reset();
	ScriptBytecode.clear();
	MapOwner = nullptr;
	PlayerEnabled = nullptr;
	if (ScriptConsole)
	{
		ScriptConsole->EndMenu();
		ScriptConsole->SetVisible(false);
		ScriptConsole = nullptr;
	}

}
void Game::MapScript::ShowLoadError(FSys::Utils::FocaCon* Console, std::string_view Error)
{
	if (!Console)
	{
		return;
	}
	Console->Clear();
	Console->PrintLine(u"No se pudo cargar el mapa solicitado.");
	if (!Error.empty())
	{
		char16 ConvertedError[FSys::Utils::FocaCon::MaxLineCharacters]{};
		if (ConvertUtf8ToChar16(Error, ConvertedError, std::size(ConvertedError)))
		{
			Console->PrintLine(ConvertedError);
		}
	}
	Console->SetVisible(true);
}
void Game::MapScript::RegisterNatives()
{
	if (!Runtime)
	{
		return;
	}
	Game::SysData::RegisterScriptNatives(*Runtime);
	auto createMenu = [this](const std::vector<ohana::runtime::Value>& Arguments) -> ohana::runtime::Value
	{
		std::string_view Title;
		char16 ConvertedTitle[FSys::Utils::FocaCon::MaxLineCharacters]{};
		if (!ScriptConsole || !ohana::get_string_argument(Arguments, 0, Title) ||
			!ConvertUtf8ToChar16(Title, ConvertedTitle, std::size(ConvertedTitle)))
		{
			return false;
		}
		ScriptConsole->Clear();
		ScriptConsole->BeginMenu(ConvertedTitle);
		return true;
		(void)Arguments;
		return false;
	};
	auto addMenuOption = [this](const std::vector<ohana::runtime::Value>& Arguments) -> ohana::runtime::Value
	{
		std::string_view Label;
		char16 ConvertedLabel[FSys::Utils::FocaCon::MaxLineCharacters]{};
		if (!ScriptConsole || !ohana::get_string_argument(Arguments, 0, Label) ||
			!ConvertUtf8ToChar16(Label, ConvertedLabel, std::size(ConvertedLabel)))
		{
			return false;
		}
		return ScriptConsole->AddMenuOption(ConvertedLabel);
		(void)Arguments;
		return false;
	};
	auto setMenuOptionEnabled = [this](const std::vector<ohana::runtime::Value>& Arguments) -> ohana::runtime::Value
	{
		std::int64_t OptionIndex = 0;
		bool Enabled = false;
		if (!ScriptConsole || !ohana::get_integer_argument(Arguments, 0, OptionIndex) || OptionIndex < 0 || static_cast<std::uint64_t>(OptionIndex) > std::numeric_limits<u32>::max() || !ohana::get_boolean_argument(Arguments, 1, Enabled))
		{
			return false;
		}
		return ScriptConsole->SetMenuOptionEnabled(static_cast<u32>(OptionIndex), Enabled);
		(void)Arguments;
		return false;
	};
	auto showMenu = [this](const std::vector<ohana::runtime::Value>&) -> ohana::runtime::Value
	{
#ifdef _DEBUG
		if (!ScriptConsole || !ScriptConsole->IsMenuActive()) return false;
		ScriptConsole->SetVisible(true);
		return true;
#else
		return false;
#endif
	};
	auto closeMenu = [this](const std::vector<ohana::runtime::Value>&) -> ohana::runtime::Value
	{
#ifdef _DEBUG
		if (!ScriptConsole) return false;
		ScriptConsole->EndMenu();
		ScriptConsole->SetVisible(false);
		return true;
#else
		return false;
#endif
	};
	auto getMenuSelection = [this](const std::vector<ohana::runtime::Value>&) -> ohana::runtime::Value
	{
#ifdef _DEBUG
		if (ScriptConsole && ScriptConsole->IsMenuActive())
		{
			const u32 Selection = ScriptConsole->GetMenuSelection();
			if (Selection != FSys::Utils::FocaCon::NoMenuOption) return static_cast<std::int64_t>(Selection);
		}
#endif
		return std::int64_t(-1);
	};
	auto waitForMenuSelection = [this](const std::vector<ohana::runtime::Value>&) -> ohana::runtime::Value
	{
#ifdef _DEBUG
		if (!ScriptConsole || !ScriptConsole->IsMenuActive())
		{
			return std::int64_t(-1);
		}
		if (!ScriptConsole->IsVisible())
		{
			ScriptConsole->SetVisible(true);
		}
		if (ScriptConsole->GetMenuSelection() == FSys::Utils::FocaCon::NoMenuOption)
		{
			return std::int64_t(-1);
		}
		u32 Selection = FSys::Utils::FocaCon::NoMenuOption;
		if (ScriptConsole->ConsumeMenuSelection(Selection))
		{
			return static_cast<std::int64_t>(Selection);
		}
		return ohana::runtime::Yield{};
#else
		return std::int64_t(-1);
#endif
	};
	auto loadMap = [this](const std::vector<ohana::runtime::Value>& Arguments) -> ohana::runtime::Value
	{
		std::int64_t MapId = 0;
		if (!MapOwner || Arguments.size() != 1 || !ohana::get_integer_argument(Arguments, 0, MapId) || MapId < 0 ||
			static_cast<std::uint64_t>(MapId) > std::numeric_limits<u32>::max())
		{
			return false;
		}
		MapOwner->RequestMapLoad(static_cast<u32>(MapId));
		return true;
	};
	auto setPlayerEnabled = [this](const std::vector<ohana::runtime::Value>& Arguments) -> ohana::runtime::Value
	{
		bool Enabled = false;
		if (!MapOwner || !ohana::get_boolean_argument(Arguments, 0, Enabled))
		{
			return false;
		}
		return MapOwner->SetPlayerEnabled(Enabled);
	};
	Runtime->register_natives({
		{ "menu_crear", std::move(createMenu) },
		{ "menu_opcion", std::move(addMenuOption) },
		{ "menu_habilitar", std::move(setMenuOptionEnabled) },
		{ "menu_mostrar", std::move(showMenu) },
		{ "menu_cerrar", std::move(closeMenu) },
		{ "menu_seleccion", std::move(getMenuSelection) },
		{ "menu_esperar", std::move(waitForMenuSelection) },
		{ "mapa_cargar", std::move(loadMap) },
		{ "jugador_habilitar", std::move(setPlayerEnabled) }
	});
}
