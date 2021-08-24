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
    char userDirPath[MAX_PATH];
    SHGetSpecialFolderPathA(NULL, userDirPath, CSIDL_PROFILE, true);
    std::string userDir = std::string() + userDirPath + "/";
    for (std::string::iterator it = dir.begin(); it != dir.end(); ++it) {
        if (*it == '\\') *it = '/';
    }
    return userDir;
#endif
}
