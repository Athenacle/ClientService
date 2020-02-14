/*
 *
 * WangXiao (zjjhwxc@gmail.com)
 */

#include "clientServer.h"
#include "protobufLib.h"

#include <spdlog/spdlog.h>

using namespace protobuf;
using namespace spdlog;
using namespace database;

namespace
{
    struct _work {
        Client *c;
        LogPackage *p;
    };

    void InsertWindowsEvents(Client *c, LogPackage *l)
    {
        uv_work_t *w = new uv_work_t;
        auto wo = new _work;
        wo->c = c;
        wo->p = l;
        w->data = wo;

        uv_queue_work(
            UvHandler::GetUVHandler()->GetLoop(),
            w,
            [](uv_work_t *w) {
                _work *wo = reinterpret_cast<_work *>(w->data);
                try {
                    Database::GetDatabase()->InsertWindowsEvents(*wo->c->_client,
                                                                 wo->p->GetEvents());
                    if (wo->p->NeedAccept()) {
                        AcceptLastEventPackage c(wo->c->lid + 1);
                        wo->c->writeSomething(c);
                    }
                } catch (pqxx::sql_error &) {
                    RefusePackage r(wo->p->Id(), "Refuse: database exception");
                    wo->c->writeSomething(r);
                }
            },
            [](uv_work_t *w, int) {
                _work *wo = reinterpret_cast<_work *>(w->data);
                delete wo->p;
                delete wo;
            });
    }

}  // namespace

void Client::writeSomething(CoreMessage &msg)
{
    UvHandler::GetUVHandler()->WriteToNetwork(msg, reinterpret_cast<uv_stream_t *>(clientSocket));
}

void Client::readFromNetwork(char *buf, int size)
{
    std::lock_guard<std::mutex> lock(_mutex);
    decoder.read(buf, size);
    CoreMessage *ret = decoder.GetProtobufMessage();

    if (ret == nullptr) {
        return;
    }

    if (UvHandler::GetUVHandler()->GetDatabaseConnected()) {
        RefusePackage re(ret->Id(), "Refuse Package: database disconnected.");
        writeSomething(re);
        delete ret;
        return;
    }

    try {
        switch (ret->Op()) {
            case Operation::UPDATE_LOG: {
                LogPackage *l = dynamic_cast<LogPackage *>(ret);
                spdlog::info("Event Forwarder: Get {} events (Client {})", l->GetEvents().size());
                lid = l->GetEvents().cend()->rid;

                InsertWindowsEvents(this, l);

                // this LogPackage was sent for insert events into database. DOTNOT destroy it.
                // refer to client.cpp:30
                ret = nullptr;

            } break;
            case Operation::CONNECT: {
                this->_client = database::Database ::GetDatabase()->GetClient(*ret);
                if (this->_client) {
                    ConnectPackage cp;
                    writeSomething(cp);
                }
            } break;
            case Operation::QUERY_LAST_EVENT: {
                lid = database::Database::GetDatabase()->GetLastEventRecordID(*_client);
                ReturnLastEventPackage lastPackage(lid + 1);
                writeSomething(lastPackage);
            } break;
            default:
                break;
        }
    } catch (std::bad_cast &) {
        RefusePackage r(ret->Id(), "Internal exception. bad_cast");
        writeSomething(r);
    }
    delete ret;
}