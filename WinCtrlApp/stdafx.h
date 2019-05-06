// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

/////////////////////////////////////
// Additional headers and stuff
/////////////////////////////////////
#include <windowsx.h>
#include <commctrl.h>
#include <shobjidl_core.h>
#include <memory>

// Direct2D
#include <D2d1.h>
#include <D2d1helper.h>
#include <Dwrite.h>

// Misc
#include <propvarutil.h>



template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}