#pragma once
#ifndef GX_DETAIL_DISPLAY_INFO_H_
#define GX_DETAIL_DISPLAY_INFO_H_
#ifdef _WIN32
#include<Windows.h>
#endif // _WIN32
namespace gx 
{
	namespace detail 
	{
		struct DisplayResolution
		{
			u32 Width;
			u32 Height;
		};
		struct DisplayInfo
		{
			DisplayResolution Resolution;
			bool UseOffscreenScene = false;
#ifdef _WIN32
			HINSTANCE hInstance;
			HWND Hwnd;
#endif // _WIN32
		};
	}
}
#endif // !GX_DETAIL_DISPLAY_INFO_H_
