#pragma once

#include <cstdlib>

#ifdef WIN32
#define _WIN32_IE 0x0400
#include <shlobj.h>
#include <windef.h>
#endif

std::string getUserDirectory() {
#ifdef __unix__
	return std::string() + getenv("HOME") + "/";
#else
	// Use WinAPI to query the user directory.
	WCHAR path[MAX_PATH + 1];
	if (SHGetSpecialFolderPathW(NULL, path, CSIDL_PROFILE, FALSE)) {
		std::wstring ws(path);
		std::string str(ws.begin(), ws.end());
		return str + '/';
	} else { 
		return NULL;
	}
#endif
}
