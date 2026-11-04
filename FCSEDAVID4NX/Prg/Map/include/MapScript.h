#pragma once
#include<Types.h>
#include<Macros.h>
#include<MemTypes.h>
#include<DynamicArray.h>
#include<DynamicString.h>
#include<runtime.h>
#include<string_view>
#ifdef _DEBUG
#include<FocaUtils/FocaCon.h>
#endif

namespace Game
{
	class MapSystem;
	class MapScript final : NonCopy<>
	{
	public:
		MapScript();
		~MapScript();
		bool Initialize(const DynamicArray<u8>& Bytecode, MapSystem& Map, String* Error = nullptr, FSys::Utils::FocaCon* ScriptConsole = nullptr, u32 InitialTrackId = 0, bool* PlayerEnabled = nullptr);
		void Update();
		void Finalize();
		static void ShowLoadError(FSys::Utils::FocaCon* ScriptConsole, std::string_view Error);
	private:
		void RegisterNatives();
		MapSystem* MapOwner;
		bool* PlayerEnabled;
		DynamicArray<u8> ScriptBytecode;
		Mem::UniquePtr<ohana::runtime> Runtime;
		FSys::Utils::FocaCon* ScriptConsole;
	};
}
