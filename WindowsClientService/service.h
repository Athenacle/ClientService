/*
 *
 * WangXiao (zjjhwxc@gmail.com)
 */
#ifndef SERVICE_H
#define SERVICE_H

#include <WinSock2.h>
#include <windows.h>
#include <WinDNS.h>
#include <winevt.h>

#include <mutex>
#include <uv.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <tuple>

#include "protobufLib.h"

#ifdef _DEBUG

void OutputDebugStringEx(LPCWSTR fmt, ...);

void OutputDebugStringEx(LPCSTR fmt, ...);

#else
#define OutputDebugStringEx(fmt, ...) ((void*)0)
#endif

LPCWSTR ParseSystemErrorCode(DWORD);

LPSTR Convert(LPCWSTR in);
LPWSTR Convert(LPCSTR in);

void Convert(std::wstring& out, const std::string& in);

DWORD WINAPI ServiceThreadStartPROC(LPVOID param);

namespace service
{
    const char defaultRemoteAddress[] = "asus-laptop-wireless.athenacle.xyz";
    const uint16_t defaultRemotePort = 53222;

    void uvNetworkReadCB(uv_stream_t*, ssize_t, const uv_buf_t*);
    void uvNetworkWriteCB(uv_write_t*, int);
    void uvNetworkConnectCB(uv_connect_t*, int);

    class ServiceHandler
    {
        friend DWORD WINAPI ::ServiceThreadStartPROC(LPVOID param);

    protected:
        HANDLE hThread;

    private:
        DWORD StartService()
        {
            return this->Start();
        }

        virtual HANDLE GetStopHandle() = 0;
        virtual DWORD Start() = 0;

        virtual DWORD GetThreadStartFlag() const
        {
            return 0;
        }

    public:
        void StartThread(HANDLE&, HANDLE&);

        virtual ~ServiceHandler() {}
        virtual void ServiceCheckStatus() = 0;
        virtual void NetworkStartFinished() = 0;
        virtual void GetProtobufPackage(protobuf::CoreMessage*) = 0;
    };


    class EventForwardHandler : public ServiceHandler
    {
        std::atomic_uint32_t lastEventIDUploaded;

        EVT_HANDLE hSubscription;

        HANDLE aWaitHandles[3];

        std::vector<protobuf::Event> eventsToSend;

        virtual DWORD GetThreadStartFlag() const
        {
            return CREATE_SUSPENDED;
        }

        static const int THREAD_STOP_EVENT = 0;
        static const int EVENT_SUBCRIBE_EVENT = 1;
        static const int EVENT_UPLOAD_LAST_ID_EVENT = 2;

        static constexpr LPCWSTR pwsPath = L"System";
        static constexpr LPCWSTR pwsQuery = L"*";

        virtual HANDLE GetStopHandle()
        {
            return aWaitHandles[THREAD_STOP_EVENT];
        }

        virtual DWORD Start();

    public:
        static constexpr DWORD eventsPerSend = 30;

        virtual ~EventForwardHandler();
        EventForwardHandler();

        void CheckLastEventID();

        virtual void ServiceCheckStatus() {}
        virtual void NetworkStartFinished();
        virtual void GetProtobufPackage(protobuf::CoreMessage*);
    };

    class HandlerDispatcher
    {
        friend void uvNetworkReadCB(uv_stream_t*, ssize_t, const uv_buf_t*);
        friend void uvNetworkWriteCB(uv_write_t*, int);
        friend void uvNetworkConnectCB(uv_connect_t*, int);

        std::string remoteAddress;
        uint16_t remotePort;

        struct sockaddr_in sock;

        static void WSAErrorHandler(DWORD);

        static std::atomic_bool wsInitlized;

        uv_tcp_t* tcp;

        // std::stringstream _inBuffer;

        protobuf::ProtobufPacketDecoder decoder;

        void InitTCP();

    public:
        operator uv_tcp_t*()
        {
            return tcp;
        }

        operator uv_stream_t*()
        {
            return reinterpret_cast<uv_stream_t*>(tcp);
        }

        bool Connect();

        bool Send(char*, size_t);

        static void initNetworkHandler();

        uv_loop_t* _loop;

        uv_timer_t* _timer;

        static HandlerDispatcher* _dispatcher;

        static std::mutex _mutex;

        HandlerDispatcher(LPCSTR, uint16_t);

        std::vector<std::tuple<ServiceHandler*, HANDLE, HANDLE>> _services;

    public:
        void StartDefaultLoop();

        void SubmitProtobufPackage(protobuf::CoreMessage* msg)
        {
            for (auto& p : _services) {
                std::get<0>(p)->GetProtobufPackage(msg);
            }
        }

        void AddServiceHandler(ServiceHandler* h)
        {
            HANDLE hThread;
            HANDLE hSTOP;
            h->StartThread(hThread, hSTOP);
            _services.push_back(std::make_tuple(h, hThread, hSTOP));
        }

        void NetworkStartFinished();

        uv_loop_t* GetLoop() const
        {
            return _loop;
        }

        static HandlerDispatcher& GetHandlerDispatcher();
        static HandlerDispatcher& GetHandlerDispatcher(LPCSTR, uint16_t);
    };
}  // namespace service

#endif
