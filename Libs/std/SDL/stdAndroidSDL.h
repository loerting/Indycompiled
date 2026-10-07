// Android builds: the game data lives in the APK's assets (read in place) and in the app's internal storage
// (writable files). See stdAndroidSDL.c.
#ifndef STD_ANDROIDSDL_H
#define STD_ANDROIDSDL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __ANDROID__

// Prepares the data dir in internal storage (Resource/, SaveGames/, the asset folders the game lists or writes)
// and makes Resource/ the working dir, as the game's install layout expects. Returns 0 on success.
int stdAndroid_PrepareDataDir(void);

// Opens a read-only file from the APK's assets when it isn't in internal storage; pPath is resolved against the
// working dir (e.g. "./CD1.GOB" -> asset "Resource/CD1.GOB") and matched case-insensitively. NULL if not found.
FILE* stdAndroid_OpenAsset(const char* pPath, const char* mode);

// Whether pPath (resolved as for stdAndroid_OpenAsset) is an asset
bool stdAndroid_AssetExists(const char* pPath);

// The file names of the assets in folder pDir that aren't in internal storage, one per call (NULL at the end);
// *pNext keeps the position and starts at 0
const char* stdAndroid_NextAsset(const char* pDir, size_t* pNext);

#endif // __ANDROID__
#endif // STD_ANDROIDSDL_H
