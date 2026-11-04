#include"FileReader.h"
#include<fstream>
#ifdef _WIN32
#include<Windows.h>
#endif

bool GameUtil::FileRead(const String& FilePath, DynamicArray<u8>& Out)
{
	std::ifstream file(FilePath.c_str(), std::ios::binary | std::ios::ate);
	if (!file.is_open())
		return false;

	std::streamsize fileSize = file.tellg();
	if (fileSize <= 0)
	{
		file.close();
		return false;
	}

	file.seekg(0, std::ios::beg);

	Out.resize(static_cast<u64>(fileSize));
	if (!file.read(reinterpret_cast<char*>(Out.data()), fileSize))
	{
		file.close();
		return false;
	}

	file.close();
	return true;
}

DynamicArray<u8> GameUtil::FileRead(const String& FilePath)
{
	DynamicArray<u8> buffer;
	FileRead(FilePath, buffer);
	return buffer;
}

String GameUtil::GetExeDirectory()
{
#ifdef _WIN32
	char path[MAX_PATH];
	GetModuleFileNameA(nullptr, path, MAX_PATH);
	String exePath(path);
	size_t pos = exePath.find_last_of("\\/");
	if (pos != String::npos)
		return exePath.substr(0, pos + 1);
	return "";
#else
	return "";
#endif
}

String GameUtil::ResolvePath(const String& RelativePath)
{
	return GetExeDirectory() + RelativePath;
}
