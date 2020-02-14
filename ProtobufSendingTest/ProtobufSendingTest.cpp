#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#include "test.h"

#pragma comment(lib, "ws2_32.lib")
#include "protobufLib.h"
#pragma comment(lib, "WindowsProtobufLib.lib")

using namespace protobuf;

void PrintCSBackupAPIErrorMessage(DWORD dwErr, std::wstring& out)
{
    WCHAR wszMsgBuff[512];  // Buffer for text.

    DWORD dwChars;  // Number of chars returned.

    // Try to get the message from the system errors.
    dwChars = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL,
                            dwErr,
                            0,
                            wszMsgBuff,
                            512,
                            NULL);

    if (0 == dwChars) {
        // The error code did not exist in the system errors.
        // Try Ntdsbmsg.dll for the error code.

        HINSTANCE hInst;

        // Load the library.
        hInst = LoadLibrary(L"Ntdsbmsg.dll");
        if (NULL == hInst) {
            printf("cannot load Ntdsbmsg.dll\n");
            exit(1);  // Could 'return' instead of 'exit'.
        }

        // Try getting message text from ntdsbmsg.
        dwChars = FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
                                hInst,
                                dwErr,
                                0,
                                wszMsgBuff,
                                512,
                                NULL);

        // Free the library.
        FreeLibrary(hInst);
    }

    // Display the error message, or generic text if not found.
    out = dwChars ? wszMsgBuff : L"Error message not found";
}

CoreMessage* generateMessage()
{
    LogPackage* ret = new LogPackage();//"test_scope", "testing message", LogLevel::info);
    return ret;
}

using namespace std;

unsigned int sock;

int main()
{
    initProtobufLibrary();
    WSADATA wsaData;
    int status = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (status == 0) {
        cout << "WSAStartup success." << endl;
    } else {
        wstring err;
        PrintCSBackupAPIErrorMessage(WSAGetLastError(), err);
        wcout << L"WSAStartup failed: " << err << endl;
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock > 0) {
        cout << "create socket success." << endl;
    } else {
        wstring err;
        PrintCSBackupAPIErrorMessage(WSAGetLastError(), err);
        wcout << L"create socket failed: " << err << endl;
        exit(2);
    }

    sockaddr_in sockAddr;
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = PF_INET;

    sockAddr.sin_addr.s_addr = inet_addr("10.70.20.60");

    sockAddr.sin_port = htons(53222);
    int ret = connect(sock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));

    if (ret == -1) {
        wstring err;
        PrintCSBackupAPIErrorMessage(WSAGetLastError(), err);
        wcout << L"connect to remote failed: " << err << endl;
        exit(3);
    } else {
        cout << "connect to remote with success." << endl;

        init();

        begin();

        protobuf::CoreMessage* pc = generateMessage();

        std::cerr << *pc;

        char* buf;
        size_t size;
        bool ret = pc->toBytes(&buf, &size);

        size_t s = ::send(sock, buf, size, 0);
        if (s == SOCKET_ERROR) {
            int e = WSAGetLastError();
            // PrintCSBackupAPIErrorMessage(e);
        }

        closesocket(sock);

        WSACleanup();
    }
    Sleep(1000 * 2);
    shutdownProtobufLibrary();
    return 0;
}
