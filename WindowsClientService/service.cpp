#include <uv.h>
#include <thread>

#include "service.h"

DWORD WINAPI ServiceThreadStartPROC(LPVOID param)
{
    auto p = reinterpret_cast<service::ServiceHandler*>(param);
    return p->StartService();
}

namespace service
{
    HandlerDispatcher* HandlerDispatcher::_dispatcher = nullptr;

    std::mutex HandlerDispatcher::_mutex;

    void HandlerDispatcher::StartDefaultLoop()
    {
        uv_timer_start(
            _timer, [](uv_timer_t*) {}, 10 * 60 * 1000, -1);
        uv_run(_loop, UV_RUN_DEFAULT);
    }

    void HandlerDispatcher::NetworkStartFinished()
    {
        for (auto& s : _services) {
            std::get<0>(s)->NetworkStartFinished();
        }
    }

    HandlerDispatcher& HandlerDispatcher::GetHandlerDispatcher()
    {
        return *_dispatcher;
    }

    HandlerDispatcher& HandlerDispatcher::GetHandlerDispatcher(LPCSTR addr, uint16_t port)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_dispatcher == nullptr) {
            _dispatcher = new HandlerDispatcher(addr, port);
        }
        return GetHandlerDispatcher();
    }

    void ServiceHandler::StartThread(HANDLE& hThread, HANDLE& hStop)
    {
        hThread = this->hThread =
            CreateThread(NULL, 0, ServiceThreadStartPROC, this, GetThreadStartFlag(), NULL);
        hStop = GetStopHandle();
    }

}  // namespace service
