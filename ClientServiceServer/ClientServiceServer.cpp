/*
 *
 * WangXiao (zjjhwxc@gmail.com)
 */

#include "clientServer.h"

#include <iostream>
#include <map>
#include <sstream>
#include <mutex>
#include <atomic>

#include <spdlog/spdlog.h>

#if defined _WINDOWS_ || defined WINDOWS
#pragma comment(lib, "WindowsProtobufLib.lib")
#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Userenv.lib")
#pragma comment(lib, "Psapi.lib")
void SetupSignals() {}

void utils::Sleep(int s)
{
    ::Sleep(s * 1000);
}

#elif defined UNIX
#include <signal.h>
#include <unistd.h>
#include <errno.h>
// if the connection to postgres broken, a SIGPIPE will be received.
// refers to http://pqxx.org/development/libpqxx/wiki/FaqTroubleshooting

namespace
{
    void SIGPIPEHandler(int)
    {
        spdlog::warn("SIGPIPE received.");
        auto uv = UvHandler::GetUVHandler();
        auto status = database::Database::GetDatabase()->CheckConnectStatus();
        uv->SetDatabaseConnect(status);
        if (status) {
            return;
        } else {
            status = database::Database::GetDatabase()->Connect();
            if (!status) {
                spdlog::error("reconnecting to database failed. exiting...");
                uv->StopLoop();
            }
        }
    }

    void SIGTERMHandler(int)
    {
        spdlog::info("SIGTERM received. exiting...");
        UvHandler::GetUVHandler()->StopLoop();
    }

    using SIGNAL_FUNC = void (*)(int);

    void _SetupSignals(int sig, SIGNAL_FUNC func)
    {
        struct sigaction act, oact;

        act.sa_handler = func;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
#ifdef SA_INTERRUPT
        act.sa_flags |= SA_INTERRUPT;
#endif
        if (sigaction(sig, &act, &oact) < 0) {
            spdlog::error("Setup sig {} handler error. {}", sig, strerror(errno));
        } else {
            spdlog::debug("Setup sig {} handler success.", sig);
        }
    }
}  // namespace

void SetupSignals()
{
    _SetupSignals(SIGPIPE, SIGPIPEHandler);
    _SetupSignals(SIGTERM, SIGTERMHandler);
}

void utils::Sleep(int s)
{
    ::sleep(s);
}


#endif

using namespace protobuf;
using namespace spdlog;


int main(int, const char *argv[])
{
    spdlog::set_level(spdlog::level::debug);

    info("{} starting...", argv[0]);

    auto conf = ReadConfig(DEFAULT_CONFIG_FILE);

    if (database::Database::InitDatabase(std::move(conf->connectionString))->Connect()) {
        initProtobufLibrary();
        atexit(shutdownProtobufLibrary);
        SetupSignals();
        UvHandler::GetUVHandler()->SetupNetwork();
        UvHandler::GetUVHandler()->UvLoopRun();

        UvHandler::DestroyUvHandler();
        database::Database::DestroyDatabase();
    }

    delete conf;
    return 0;
}