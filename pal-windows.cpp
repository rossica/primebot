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

// Borrowed from the solution available here:
// http://stackoverflow.com/questions/1530760/how-do-i-recursively-create-a-folder-in-win32
bool MakeDirectory(const char* Path)
{
    char CurrentFolder[MAX_PATH] = { 0 };
    const char* End = nullptr;

    End = strchr(Path, '/');

    while (End != nullptr)
    {
        strncpy_s(CurrentFolder, Path, End - Path + 1);
        if (!CreateDirectory(CurrentFolder, nullptr))
        {
            DWORD Error = GetLastError();
            if (Error != ERROR_ALREADY_EXISTS)
            {
                printf(
                    "Failed to create directory %s with error %u",
                    CurrentFolder,
                    Error);
                // Don't even try to delete partially-created directories.
                return false;
            }
        }
        End = strchr(++End, '/');
    }

    // Create last folder
    if (!CreateDirectory(Path, nullptr))
    {
        if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
            printf(
                "Failed to create directory %s with error %u",
                CurrentFolder,
                GetLastError());
            return false;
        }
    }
    return true;
}
