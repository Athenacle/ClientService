#include <cassert>

#include "protobufLib.h"
#include "service.h"

#include <tinyxml2.h>
#include <winevt.h>
#include <iostream>
#include <unordered_map>

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "WindowsProtobufLib.lib")

using namespace tinyxml2;

namespace
{
    using protobuf::Event;
    using EventList = std::vector<protobuf::Event>;

    using MetadataMap = std::unordered_map<std::wstring, EVT_HANDLE>;

    MetadataMap& GetMetadataMap()
    {
        static MetadataMap _map;
        return _map;
    }

    EVT_HANDLE GetProviderMetadata(const std::wstring& provider)
    {
        MetadataMap& _map = GetMetadataMap();

        if (provider == L"e1dexpress") {
            OutputDebugStringEx(L"Load metadata for provider %ws failed\n", provider.c_str());
        }

        auto p = _map.find(provider);
        if (p != _map.end()) {
            return p->second;
        } else {
            auto m = EvtOpenPublisherMetadata(NULL, provider.c_str(), NULL, 0, 0);
            if (m == NULL) {
                OutputDebugStringEx(
                    L"OpenMetadata for %s failed: %s\n", p, ParseSystemErrorCode(GetLastError()));
                return NULL;
            } else {
                _map.emplace(provider, m);
                return m;
            }
        }
    }

    LPWSTR GetMessageString(EVT_HANDLE hMetadata, EVT_HANDLE hEvent)
    {
        LPWSTR pBuffer = NULL;
        DWORD dwBufferSize = 0;
        DWORD dwBufferUsed = 0;
        DWORD status = 0;

        const EVT_FORMAT_MESSAGE_FLAGS FormatId = EvtFormatMessageEvent;

        if (!EvtFormatMessage(
                hMetadata, hEvent, 0, 0, NULL, FormatId, dwBufferSize, pBuffer, &dwBufferUsed)) {
            status = GetLastError();
            if (ERROR_INSUFFICIENT_BUFFER == status) {
                dwBufferSize = dwBufferUsed;

                pBuffer = new WCHAR[dwBufferSize * sizeof WCHAR];

                if (pBuffer) {
                    status = EvtFormatMessage(hMetadata,
                                              hEvent,
                                              0,
                                              0,
                                              NULL,
                                              FormatId,
                                              dwBufferSize,
                                              pBuffer,
                                              &dwBufferUsed);
                    if (status) {
                        return pBuffer;
                    } else {
                        return nullptr;
                    }
                }
            } else if (ERROR_EVT_MESSAGE_NOT_FOUND == status
                       || ERROR_EVT_MESSAGE_ID_NOT_FOUND == status) {
                if (pBuffer != nullptr) {
                    delete[] pBuffer;
                }
                return nullptr;
            }
        }

        return pBuffer;
    }

    void FormatEvent(Event& evt, EVT_HANDLE hEvt)
    {
        auto p = Convert(evt.provider.c_str());
        auto hProviderMetadata = GetProviderMetadata(p);

        if (hProviderMetadata == NULL) {
            evt.format = "";
        } else {
            auto msg = GetMessageString(hProviderMetadata, hEvt);
            if (msg != NULL) {
                auto cmsg = Convert(msg);
                evt.format.swap(std::string(cmsg));
                delete[] msg;
                delete[] cmsg;
            } else {
                evt.format = evt.xml;
            }
        }

        delete[] p;
    }

    void ParseEventXml(Event& evt, LPWSTR in)
    {
        tinyxml2::XMLDocument doc;
        char* xml = Convert(in);

        doc.Parse(xml);


        auto root = doc.RootElement();
        auto system = root->FirstChildElement("System");

        if (system != nullptr) {
            auto level = system->FirstChildElement("Level");
            if (level != nullptr) {
                evt.level = atoi(level->GetText());
                evt.llevel = protobuf::Event::dispatch(evt.level);
            }
            auto rid = system->FirstChildElement("EventRecordID");
            if (rid != nullptr) {
                evt.rid = std::atoi(rid->GetText());
            }
            auto provider = system->FirstChildElement("Provider");
            if (provider != nullptr) {
                evt.provider = std::string(provider->Attribute("Name"));
            }
            auto timeStamp = system->FirstChildElement("TimeCreated");
            if (timeStamp != nullptr) {
                evt.timeStamp = std::string(timeStamp->Attribute("SystemTime"));
            }
        }

        evt.xml = std::move(std::string(xml));
        delete[] xml;
    }

    LPWSTR PrintEvent(EVT_HANDLE hEvent)
    {
        DWORD status = ERROR_SUCCESS;
        DWORD dwBufferSize = 0;
        DWORD dwBufferUsed = 0;
        DWORD dwPropertyCount = 0;
        LPWSTR pRenderedContent = NULL;

        if (!EvtRender(NULL,
                       hEvent,
                       EvtRenderEventXml,
                       dwBufferSize,
                       pRenderedContent,
                       &dwBufferUsed,
                       &dwPropertyCount)) {
            if (ERROR_INSUFFICIENT_BUFFER == (status = GetLastError())) {
                dwBufferSize = dwBufferUsed;
                pRenderedContent = new WCHAR[dwBufferSize];

                if (pRenderedContent) {
                    EvtRender(NULL,
                              hEvent,
                              EvtRenderEventXml,
                              dwBufferSize,
                              pRenderedContent,
                              &dwBufferUsed,
                              &dwPropertyCount);
                } else {
                    OutputDebugStringEx(L"%s malloc failed\n", __FUNCTION__);
                    status = ERROR_OUTOFMEMORY;
                    goto cleanup;
                }
            }

            if (ERROR_SUCCESS != (status = GetLastError())) {
                OutputDebugStringEx(L"%s EvtRender failed with %d\n", __FUNCTION__, GetLastError());
                goto cleanup;
            }
        }
        return pRenderedContent;

    cleanup:

        if (pRenderedContent)
            delete[] pRenderedContent;

        return nullptr;
    }


    int EnumerateResults(EVT_HANDLE hResults, uint32_t ignore, std::vector<Event>& sent)
    {
        static constexpr int ARRAY_SIZE = service::EventForwardHandler::eventsPerSend;

        DWORD status = ERROR_SUCCESS;
        EVT_HANDLE hEvents[ARRAY_SIZE];
        DWORD dwReturned = 0;

        int evtCount = 0;

        protobuf::Event e;

        if (!EvtNext(hResults, ARRAY_SIZE, hEvents, INFINITE, 0, &dwReturned)) {
            if (ERROR_NO_MORE_ITEMS != (status = GetLastError())) {
                wprintf(L"EvtNext failed with %lu\n", status);
                evtCount = -1;
            }

            goto cleanup;
        }
        evtCount = dwReturned;

        for (DWORD i = 0; i < dwReturned; i++) {
            LPWSTR evtXml;
            if ((evtXml = PrintEvent(hEvents[i])) != nullptr) {
                ParseEventXml(e, evtXml);
                delete[] evtXml;

                if (e.rid >= ignore) {
                    FormatEvent(e, hEvents[i]);
                    sent.emplace_back(std::move(e));
                }
                EvtClose(hEvents[i]);
                hEvents[i] = 0;
            } else {
                goto cleanup;
            }
        }
        if (sent.size() == 0) {
            return EnumerateResults(hResults, ignore, sent);
        } else {
            return dwReturned;
        }
    cleanup:

        for (DWORD i = 0; i < dwReturned; i++) {
            if (NULL != hEvents[i])
                EvtClose(hEvents[i]);
        }

        return status;
    }

}  // namespace

namespace service
{
    using namespace protobuf;

    DWORD EventForwardHandler::Start()
    {
        auto _map = GetMetadataMap();

        OutputDebugStringEx("%s start.\n", __FUNCTION__);

        hSubscription = EvtSubscribe(NULL,
                                     aWaitHandles[1],
                                     pwsPath,
                                     pwsQuery,
                                     NULL,
                                     NULL,
                                     NULL,
                                     EvtSubscribeStartAtOldestRecord);
        if (hSubscription == 0) {
            OutputDebugStringEx("EvtSubscribe Failed %s\n", ParseSystemErrorCode(GetLastError()));
            return 1;
        } else {
            do {
                DWORD status, dwWait;
                dwWait = WaitForMultipleObjects(
                    sizeof(aWaitHandles) / sizeof(aWaitHandles[0]), aWaitHandles, FALSE, INFINITE);

                auto event = dwWait - WAIT_OBJECT_0;

                if (THREAD_STOP_EVENT == event) {
                    OutputDebugStringEx(L"ThreadStopEvent occur. Thread exiting...\n");
                    break;
                } else if (EVENT_SUBCRIBE_EVENT == event || EVENT_UPLOAD_LAST_ID_EVENT == event) {
                    eventsToSend.clear();
                    int count = EnumerateResults(hSubscription, lastEventIDUploaded, eventsToSend);

                    if (count < 0) {
                        break;
                    } else {
                        if (eventsToSend.size() > 0) {
                            LogPackage* lp =
                                new LogPackage(count == EventForwardHandler::eventsPerSend);
                            lp->AddLogEvent(eventsToSend);
                            char* buffer;
                            size_t size;
                            lp->toBytes(&buffer, &size);
                            HandlerDispatcher::GetHandlerDispatcher().Send(buffer, size);
                            delete lp;
                        }
                    }

                    if (event == EVENT_SUBCRIBE_EVENT) {
                        ResetEvent(aWaitHandles[EVENT_SUBCRIBE_EVENT]);
                    } else {
                        ResetEvent(aWaitHandles[EVENT_UPLOAD_LAST_ID_EVENT]);
                    }
                } else {
                    if (WAIT_FAILED == dwWait) {
                        OutputDebugStringEx(L"WaitForSingleObject failed with %s\n",
                                            ParseSystemErrorCode(GetLastError()));
                    }
                    break;
                }
            } while (true);
        }


        for (auto& p : _map) {
            EVT_HANDLE e = p.second;
            EvtClose(e);
        }

        return 0;
    }

    EventForwardHandler::~EventForwardHandler()
    {
        if (hSubscription)
            EvtClose(hSubscription);

        for (auto& h : aWaitHandles) {
            CloseHandle(h);
        }
    }

    EventForwardHandler::EventForwardHandler() : lastEventIDUploaded(0), hSubscription(nullptr)
    {
        ZeroMemory(aWaitHandles, sizeof aWaitHandles);

        eventsToSend.reserve(eventsPerSend);

        aWaitHandles[THREAD_STOP_EVENT] = CreateEvent(NULL, TRUE, FALSE, NULL);
        aWaitHandles[EVENT_SUBCRIBE_EVENT] = CreateEvent(NULL, TRUE, TRUE, NULL);
        aWaitHandles[EVENT_UPLOAD_LAST_ID_EVENT] = CreateEvent(NULL, TRUE, FALSE, NULL);
    }

    void EventForwardHandler::CheckLastEventID()
    {
        QueryLastEventPackage q;
        char* buf;
        size_t size;
        assert(q.toBytes(&buf, &size));
    }

    void EventForwardHandler::NetworkStartFinished()
    {
        QueryLastEventPackage q;
        char* buf;
        size_t size;
        q.toBytes(&buf, &size);

        HandlerDispatcher::GetHandlerDispatcher().Send(buf, size);
    }

    void EventForwardHandler::GetProtobufPackage(protobuf::CoreMessage* msg)
    {
        if (msg->Op() == Operation::RETURN_LAST_EVENT && lastEventIDUploaded == 0) {
            try {
                ReturnLastEventPackage* rep = dynamic_cast<ReturnLastEventPackage*>(msg);
                auto lid = rep->GetLastEventID();
                this->lastEventIDUploaded = lid;
                OutputDebugStringEx("Get Last Event ID %d from remote.\n", lid);
                ResumeThread(hThread);
            } catch (std::bad_cast&) {
            }
        } else if (msg->Op() == Operation::ACCEPT_LAST_EVENT) {
            try {
                AcceptLastEventPackage* rep = dynamic_cast<AcceptLastEventPackage*>(msg);
                auto lid = rep->GetLastEventID();
                this->lastEventIDUploaded = lid;
                OutputDebugStringEx("Accept Last Event ID %d from remote.\n", lid);
                SetEvent(aWaitHandles[EVENT_UPLOAD_LAST_ID_EVENT]);
            } catch (std::bad_cast&) {
            }
        }
    }

}  // namespace service