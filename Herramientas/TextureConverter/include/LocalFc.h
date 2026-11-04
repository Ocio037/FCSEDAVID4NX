#pragma once
#include<string>
namespace LocalFc
{
	char* ReadFile(const char* Name);
	void CreateFile(const char* Name, const char* Data, int Size);
}