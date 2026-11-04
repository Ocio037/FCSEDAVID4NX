#pragma once
#include<MemTypes.h>
#include<DynamicString.h>
namespace Game 
{ 
	class GameSystem;
}
namespace App
{
	class Application final
	{
	public:
		Application();
		~Application();
		bool Init();
		[[nodiscard]] bool IsRunning() const;
		void RunFrame();
		void Finalize();
		[[nodiscard]] const String& GetInitError() const;
	private:
		//System
		Mem::UniquePtr<Game::GameSystem> System;
	};
}
