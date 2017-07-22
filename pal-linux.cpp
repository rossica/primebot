#include "pal.h"
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "commandparsertypes.h"
#include "networkcontroller.h"
#include "prime.h"


extern AllPrimebotSettings ProgramSettings;
extern NetworkController* Controller;
extern Primebot* Bot;

void CtrlCHandler(int type)
{
    static bool ShutdownInProgress = false;
    std::thread(
        [type]
        {
            switch(type)
            {
            case SIGINT:
            case SIGTERM:
                if(ProgramSettings.NetworkSettings.Server && Controller != nullptr)
                {
                    if(!ShutdownInProgress)
                    {
                        ShutdownInProgress = true;
                        Controller->Shutdown();
                    }
                    // Now that cleanup is complete, exit the program.
                    exit(EXIT_SUCCESS);
                }
                else if(Bot != nullptr)
                {
                    Bot->Stop();
                    // No need to exit here, the program will exit on its own.
                }
                break;
            default:
                printf("unrecognized signal: %d", type);
            }
        }
    ).detach();
}

bool RegisterSignalHandler()
{
    struct sigaction SigIntHandler;

    SigIntHandler.sa_handler = CtrlCHandler;
    sigemptyset(&SigIntHandler.sa_mask);
    SigIntHandler.sa_flags = SA_RESTART;

    if(sigaction(SIGINT, &SigIntHandler, nullptr))
    {
        fprintf(
            stderr,
            "Failed to register signal handler: %s",
            strerror(errno));
        return false;
    }

    if(sigaction(SIGTERM, &SigIntHandler, nullptr))
    {
        fprintf(
            stderr,
            "Failed to register signal handler: %s",
            strerror(errno));
        return false;
    }
    return true;
}


bool MakeDirectory(const char* Path)
{
    char CurrentFolder[PATH_MAX] = { 0 };
    const char* End = nullptr;

    End = strchr(Path, '/');

    while(End != nullptr)
    {
        strncpy(CurrentFolder, Path, End - Path + 1);
        if(mkdir(CurrentFolder, S_IRWXU))
        {
            if(errno != EEXIST)
            {
                fprintf(
                    stderr,
                    "Failed to create directory %s with error %s",
                    CurrentFolder,
                    strerror(errno));
                    // Don't delete partially-created hierarchies
                    return false;
            }
        }
        End = strchr(++End, '/');
    }

    // Create last folder
    if(mkdir(Path, S_IRWXU))
    {
        if(errno != EEXIST)
        {
            fprintf(
                stderr,
                "Failed to create directory %s with error %s",
                Path,
                strerror(errno));
                // Don't delete partially-created hierarchies
                return false;
        }
    }
    return true;    
}
