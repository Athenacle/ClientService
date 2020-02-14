
#include "service.h"
#include "WindowsClientService.h"

#pragma comment(lib, "WS2_32.lib")

_declspec(dllexport) void start()
{
    initProtobufLibrary();

    auto &dispatcher = service::HandlerDispatcher::GetHandlerDispatcher(
         "10.70.20.60", 53222);

    auto *event = new service::EventForwardHandler;


    dispatcher.AddServiceHandler(event);

    dispatcher.Connect();

    dispatcher.StartDefaultLoop();
}