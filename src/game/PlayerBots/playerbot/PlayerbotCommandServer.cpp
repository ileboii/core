
#include "playerbot/playerbot.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/PlayerbotFactory.h"
#include "PlayerbotCommandServer.h"
#include <cstdlib>
#include <iostream>
#include <thread>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

INSTANTIATE_SINGLETON_1(PlayerbotCommandServer);

static bool ReadLine(SOCKET sock, std::string* buffer, std::string* line)
{
    std::string::iterator pos;
    while ((pos = find(buffer->begin(), buffer->end(), '\n')) == buffer->end())
    {
        char buf[1025];
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
            return false;

        buf[n] = 0;
        *buffer += buf;
    }

    *line = std::string(buffer->begin(), pos);
    *buffer = std::string(pos + 1, buffer->end());
    return true;
}

static void session(SOCKET sock)
{
    try
    {
        std::string buffer, request;
        while (ReadLine(sock, &buffer, &request)) {
            std::string response = sRandomPlayerbotMgr.HandleRemoteCommand(request) + "\n";
            send(sock, response.c_str(), (int)response.size(), 0);
            request = "";
        }
    }
    catch (std::exception& e)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "%s",e.what());
    }
    closesocket(sock);
}

static void serverLoop(short port)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return;
#endif

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
        return;

    int optval = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(listenSock);
        return;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(listenSock);
        return;
    }

    for (;;)
    {
        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET)
            continue;

        std::thread t(session, clientSock);
        t.detach();
    }
}

static void Run()
{
    if (!sPlayerbotAIConfig.commandServerPort) {
        return;
    }

    std::ostringstream s; s << "Starting Playerbot Command Server on port " << sPlayerbotAIConfig.commandServerPort;
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "%s",s.str().c_str());

    try
    {
        serverLoop(sPlayerbotAIConfig.commandServerPort);
    }
    catch (std::exception& e)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "%s",e.what());
    }
}

void PlayerbotCommandServer::Start()
{
    std::thread serverThread(Run);
    serverThread.detach();
}
