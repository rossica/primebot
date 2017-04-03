#pragma once

// Cross-platform development, here we come...
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2ipdef.h>
#include <WS2tcpip.h>
#define s_addr S_un.S_addr
#define socklen_t int
typedef SOCKET NETSOCK;
inline bool IsSocketValid(NETSOCK sock) { return sock != INVALID_SOCKET; }
#elif defined __linux__
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
#include <cstring>
#include <sys/types.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <netdb.h>
typedef int NETSOCK;
#define INVALID_SOCKET (-1)
#define SD_BOTH SHUT_RDWR
#define SD_SEND SHUT_WR
#define closesocket close
inline bool IsSocketValid(NETSOCK sock) { return sock >= 0; }
#define memcpy_s(dest, destsz, src, srcsz) memcpy(dest, src, destsz)
#endif
