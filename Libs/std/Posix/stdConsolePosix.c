// Native builds: the console is the process's standard output (no Win32 console window).
#include <std/Win95/stdConsole.h>
#include <stdio.h>

static WORD stdConsole_attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

int J3DAPI stdConsole_Startup(const char* pTitleText, int attributes, int bShowMinimized)
{
    J3D_UNUSED(pTitleText);
    J3D_UNUSED(bShowMinimized);
    stdConsole_attributes = (WORD)attributes;
    return 1;
}

void stdConsole_Shutdown(void)
{
    fflush(stdout);
}

int J3DAPI stdConsole_SetAttributes(WORD wAttributes)
{
    stdConsole_attributes = wAttributes;
    return 1;
}

int J3DAPI stdConsole_SetConsoleTextAttribute(WORD wAttributes)
{
    stdConsole_attributes = wAttributes;
    return 1;
}

int stdConsole_InitOutputConsole(void)
{
    return 1;
}

int J3DAPI stdConsole_WriteConsole(const char* pText, uint32_t textAttribute)
{
    J3D_UNUSED(textAttribute);
    fputs(pText, stdout);
    return 1;
}

void stdConsole_InstallHooks(void) {}
void stdConsole_ResetGlobals(void) {}
