#include"Application.h"
#include"GameSystem.h"
App::Application::Application()
{

}
App::Application::~Application()
{

}
bool App::Application::Init()
{
	System = Mem::MakeUnique<Game::GameSystem>();
	if (!System->Init())
	{
		return false;
	}
	return true;
}
bool App::Application::IsRunning() const
{
	return System && System->IsRunning();
}
void App::Application::RunFrame()
{
	if (System && System->IsRunning())
	{
		System->RunFrame();
	}
}
void App::Application::Finalize()
{
	if (System)
	{
		System.reset();
	}
}
const String& App::Application::GetInitError() const
{
	static const String EmptyError;
	return System ? System->GetInitError() : EmptyError;
}