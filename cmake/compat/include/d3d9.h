// Indycompiled: MinGW-w64's d3d9.h lacks the PDIRECT3DDEVICE9 alias the DirectX SDK provides.
#pragma once
#include_next <d3d9.h>
typedef struct IDirect3DDevice9* PDIRECT3DDEVICE9;
