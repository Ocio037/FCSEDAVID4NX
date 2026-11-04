#pragma once
#include<Types.h>
#include<DynamicArray.h>
#include<DynamicString.h>
namespace GameUtil
{
	bool FileRead(const String& FilePath, DynamicArray<u8>& Out);
	DynamicArray<u8> FileRead(const String& FilePath);
	String GetExeDirectory();
	String ResolvePath(const String& RelativePath);
}
