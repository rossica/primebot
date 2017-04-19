#include <cstdio>
#include <stdlib.h>
#include <signal.h>
#include "commandparsertypes.h"
#include "networkcontroller.h"
#include "prime.h"
#include "pal.h"

// Defines for static GMP lib on Windows
// Solution taken from here: http://stackoverflow.com/questions/30412951/unresolved-external-symbol-imp-fprintf-and-imp-iob-func-sdl2
#pragma comment(lib, "legacy_stdio_definitions.lib")
extern "C"
{
    FILE * __iob_func()
    {
        static FILE* StdFiles[] =
        {
            // Filler nullptrs discovered via debugger. The addresses of stdin, stdout, and stderr are
            // 88 bytes apart. On 64-bit OS, that's 11 pointers.
            stdin,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            stdout,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            stderr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
        };

        return (FILE*) StdFiles;
    };
}

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
            Controller->Shutdown();
            // Now that cleanup has completed, let the OS kill the program
            // by returning FALSE.
            return FALSE;
        }
        else if (Bot != nullptr)
        {
            Bot->Stop();
            // Don't let the OS kill this, because it'll exit on its own.
            return TRUE;
        }
        break;
    default:
        printf("unrecognized signal: %u", type);
    }

    // let the OS kill the program by returning FALSE.
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
                fprintf(
                    stderr,
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
            fprintf(
                stderr,
                "Failed to create directory %s with error %u",
                CurrentFolder,
                GetLastError());
            return false;
        }
    }
    return true;
}
