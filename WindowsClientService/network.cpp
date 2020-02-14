

#include "service.h"

#include "protobufLib.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Dnsapi.lib")

namespace
{
    void uvAllocateBufferCB(uv_handle_t* handle, size_t size, uv_buf_t* buf)
    {
        *buf = uv_buf_init(new char[size], size);
    }


    HINSTANCE hInst;
    void LoadErrorLibrary()
    {
        hInst = LoadLibrary(L"ntdsbmsg.dll");
        if (hInst == NULL) {
        } else {
        }
    }
}  // namespace

namespace service
{
    std::atomic_bool HandlerDispatcher::wsInitlized(false);

    using namespace protobuf;

    void uvNetworkWriteCB(uv_write_t* w, int status)
    {
        if (status == 0) {
            OutputDebugStringEx("Sending success.\n");
        } else {
            OutputDebugStringEx("Sending failed. %s\n", uv_strerror(status));
        }

        auto bufs = reinterpret_cast<uv_buf_t*>(w->data);
        delete[](bufs[0].base);
        delete[] bufs;
        delete w;
    }

    void uvNetworkConnectCB(uv_connect_t* conn, int status)
    {
        auto nh = reinterpret_cast<HandlerDispatcher*>(conn->data);
        delete conn;
        if (status == 0) {
            OutputDebugStringEx(L"Connect successfully.\n");

            uv_read_start(*nh, uvAllocateBufferCB, uvNetworkReadCB);

            protobuf::ConnectPackage conn;

            char* buf;
            size_t size;
            conn.toBytes(&buf, &size);

            nh->Send(buf, size);

        } else {
            OutputDebugStringEx("Connect failed: %s\n", uv_strerror(status));
        }
    }

    void uvNetworkReadCB(uv_stream_t* stream, ssize_t size, const uv_buf_t* bufs)
    {
        HandlerDispatcher* handler = reinterpret_cast<HandlerDispatcher*>(stream->data);

        if (size > 0) {
            handler->decoder.read(bufs->base, size);
            CoreMessage* msg = handler->decoder.GetProtobufMessage();
            if (msg != nullptr) {
                if (msg->Op() == Operation::CONNECT) {
                    handler->NetworkStartFinished();
                } else {
                    HandlerDispatcher::GetHandlerDispatcher().SubmitProtobufPackage(msg);
                }
                delete msg;
            }
        }
        delete[] bufs->base;
    }

    void HandlerDispatcher::WSAErrorHandler(DWORD dwErr)
    {
        const size_t BuffSize = 512;
        WCHAR Buff[BuffSize];

        DWORD ReturnSize;

        const DWORD moduleFlag = FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS;

        LPCWSTR msg = ParseSystemErrorCode(dwErr);

        if (msg != nullptr) {
            OutputDebugStringEx(L"SystemError: %s.\n", msg);
            return;
        }

        ReturnSize = FormatMessage(moduleFlag, hInst, dwErr, 0, Buff, BuffSize, NULL);

        if (ReturnSize != 0) {
            OutputDebugStringEx(L"WSAError: %s.\n", Buff);
            return;
        }

        OutputDebugStringEx("WSAError: Message String Not Found. Code: %d(0x%x)", dwErr, dwErr);
    }

    void HandlerDispatcher::InitTCP()
    {
        if (!wsInitlized) {
            initNetworkHandler();
        }

        int addrSize = sizeof(struct sockaddr_in);

        ZeroMemory(&sock, addrSize);

        std::wstring address;
        Convert(address, remoteAddress);

        int status = WSAStringToAddressW(
            (LPWSTR)address.c_str(), AF_INET, NULL, (LPSOCKADDR)&sock, &addrSize);

        if (status != 0 && WSAGetLastError() == WSAEINVAL) {
            OutputDebugStringEx(L"Cannot translate '%s' into sockaddr, try it via DNS\n",
                                address.c_str());

            DNS_RECORD* record;
            status =
                DnsQuery_W(address.c_str(), DNS_TYPE_A, DNS_QUERY_STANDARD, NULL, &record, NULL);

            OutputDebugStringEx("DnsQuery_a return with %d\n", status);

            if (record != nullptr && 0 == address.compare(record->pName)) {
                IP4_ADDRESS ip = record->Data.A.IpAddress;
                memcpy(&sock.sin_addr, &ip, sizeof DWORD);
            } else {
                OutputDebugStringEx("DnsQuery %s failed.\n", address.c_str());
                return;
            }

            DnsRecordListFree(record, DnsFreeRecordList);
        }

        sock.sin_family = AF_INET;
        sock.sin_port = htons(remotePort);
    }

    void HandlerDispatcher::initNetworkHandler()
    {
        auto versionWord = MAKEWORD(2, 2);
        WSADATA data;
        int status = WSAStartup(versionWord, &data);
        if (status == 0) {
            OutputDebugStringEx(L"WSAStartup successfully\n");
            wsInitlized = true;
        } else {
            WSAErrorHandler(WSAGetLastError());
        }

        atexit([]() {
            if (wsInitlized) {
                WSACleanup();
                wsInitlized = false;
            }
        });
    }

    HandlerDispatcher::HandlerDispatcher(LPCSTR address, uint16_t port)
        : remoteAddress(address), remotePort(port)
    {
        _loop = uv_default_loop();
        _timer = new uv_timer_t;
        uv_timer_init(_loop, _timer);

        ZeroMemory(&sock, sizeof(sock));
        tcp = new uv_tcp_t;
        uv_tcp_init(_loop, tcp);
        tcp->data = this;
    }

    bool HandlerDispatcher::Connect()
    {
        InitTCP();
        uv_connect_t* conn = new uv_connect_t;
        conn->data = this;

        uv_tcp_connect(conn, tcp, (const sockaddr*)(&sock), uvNetworkConnectCB);

        return true;
    }

    bool HandlerDispatcher::Send(char* buf, size_t s)
    {
        uv_write_t* w = new uv_write_t;
        uv_buf_t* bufs = new uv_buf_t[1];
        bufs[0].base = buf;
        bufs[0].len = s;
        w->data = bufs;
        return uv_write(w, reinterpret_cast<uv_stream_t*>(tcp), bufs, 1, uvNetworkWriteCB);
    }

}  // namespace service