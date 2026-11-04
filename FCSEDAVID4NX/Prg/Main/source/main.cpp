#include"Application.h"
#include<MemGlobal.h>
#include<Types.h>
#include<Windows.h>
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR CmdLine, int CmdShow)
{
	if (!Mem::InitializeGlobalHeap())
	{
		return 1;
	}
	App::Application app;
	if (!app.Init())
	{
		return 2;
	}
	for ever
	{
		if (!app.IsRunning())
		{
			break;
		}
		app.RunFrame();
	}
	app.Finalize();
	return 0;
}