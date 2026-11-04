#pragma once
#include<FocaUtils/FocaCon.h>
#if defined(_DEBUG)
#include <FocaUtils/FocaDebugProcessBars.h>
#endif

namespace FSys
{
	namespace Utils
	{
		enum class Result : __int8
		{
			CORRECT,
			WARNING,
			ERROR,
		};
	}
}
