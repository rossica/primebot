#define WIN32_LEAN_AND_MEAN   
#include <windows.h>
#include <cstdio>
#include <stdlib.h>
#include <signal.h>
#include "commandparsertypes.h"
#include "networkcontroller.h"
#include "prime.h"
#include "pal.h"

extern AllPrimebotSettings ProgramSettings;
extern NetworkController* Controller;
extern Primebot* Bot;

BOOL WINAPI CtrlCHandler(DWORD type)
{
    switch (type)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        if (ProgramSettings.NetworkSettings.Server && Controller != nullptr)
        {
            Controller->ShutdownClients();
        }
        else if (Bot != nullptr)
        {
            Bot->Stop();
        }
        break;
    default:
        printf("unrecognized signal: %u", type);
    }

    // Now that cleanup has completed, let the OS kill the program
    // by returning FALSE.
    return FALSE;
}

bool RegisterSignalHandler()
{
    if (!SetConsoleCtrlHandler(CtrlCHandler, TRUE))
    {
        fprintf(stderr, "failed to set signal handler");
        return false;
    }
    return true;
}
