/*
 *
 * WangXiao (zjjhwxc@gmail.com)
 */
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined WINDOWS || defined _WINDOWS_ || defined _MSC_VER
#pragma warning(disable : 4251 4275)
#endif

#include <atomic>
#include <string>
#include <queue>
#include <unordered_map>

#include <pqxx/pqxx>
#include <uv.h>

#include "protobufLib.h"

int GetCoreMessage(const protobuf::CoreMessage &);

struct Config {
    std::string host;
    std::string password;
    std::string username;
    std::string connectionString;
    unsigned int port;
};

struct Config *ReadConfig(const char *);

#ifndef DEFAULT_LISTEN_ADDRESS
#define DEFAULT_LISTEN_ADDRESS "0.0.0.0"
#endif

#ifndef DEFAULT_LISTEN_PORT
#define DEFAULT_LISTEN_PORT (53222)
#endif

#ifndef DEFAULT_DBNAME
#define DEFAULT_DBNAME "ClientMonitorService"
#endif

#ifndef DEFUALT_CONFIG_FILE
#define DEFAULT_CONFIG_FILE "ServerConfig.json"
#endif

constexpr char DEFAULT_DB_NAME[] = DEFAULT_DBNAME;

namespace utils
{
    void Sleep(int seconds);
}

namespace database
{
    // enum class OperationSystem { os_windows, os_linux, os_others };

    struct DbClient {
        int _clientID;
        std::string _clientName;
        protobuf::OsType _clientOs;
        std::string _clientOsVersion;
        std::string _clientUniqueID;
        std::string _clientRegisterTime;
    };


    class Database
    {
        pqxx::connection *conn;

        std::string connectionString;

        std::map<std::string, DbClient *> _clients;

        void GetAllClients();

        static Database *_db;

        ~Database()
        {
            delete conn;
            for (auto &c : _clients) {
                delete c.second;
            }
            _clients.clear();
        }

        Database(std::string &&str) : connectionString(str)
        {
            conn = nullptr;
        }

    public:
        int InsertWindowsEvents(const DbClient &, const std::vector<protobuf::Event> &);

        int GetLastEventRecordID(const DbClient &);

        DbClient *GetClient(const protobuf::CoreMessage &);

        static void DestroyDatabase()
        {
            delete _db;
            _db = nullptr;
        }

        static Database *InitDatabase(std::string &&);

        static Database *GetDatabase();

        bool Connect();

        bool CheckConnectStatus();
    };
}  // namespace database


struct Client {
    uv_tcp_t *clientSocket;
    database::DbClient *_client;

    std::atomic_uint32_t lid;

    std::mutex _mutex;

    protobuf::ProtobufPacketDecoder decoder;

    void clientDisConnected() {}

    void writeSomething(protobuf ::CoreMessage &msg);

    void readFromNetwork(char *buf, int size);

    Client(uv_loop_t *loop)
    {
        lid = 1;
        clientSocket = new uv_tcp_t;
        uv_tcp_init(loop, clientSocket);
        clientSocket->data = this;
    }

    ~Client()
    {
        decoder.Reset();
        delete clientSocket;
    }

    void printRemote()
    {
        sockaddr_in addr;
        int nameLen;
        int ret = uv_tcp_getpeername(clientSocket, (sockaddr *)&addr, &nameLen);
        if (ret) {
            char buffer[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &(addr.sin_addr), buffer, INET_ADDRSTRLEN);
        } else {
        }
    }

    void ReadStop()
    {
        uv_read_stop((uv_stream_t *)clientSocket);
    }

    void close()
    {
        uv_close(*this, nullptr);
    }

    operator uv_handle_t *()
    {
        return reinterpret_cast<uv_handle_t *>(clientSocket);
    }

    operator uv_stream_t *()
    {
        return reinterpret_cast<uv_stream_t *>(clientSocket);
    }
};


class UvHandler
{
    struct SendObject {
        char *buf;
        size_t length;
        uv_stream_t *sock;
    };

    uv_loop_t *loop;
    uv_tcp_t *tcp;
    uv_async_t *writeAsync;
    uv_async_t *stopAsync;

    std::mutex _mutex;

    std::atomic_bool dbConnected;

    std::unordered_map<void *, Client *> _clientMap;
    std::queue<SendObject> _sending_queue;

    static UvHandler *server;

    UvHandler();

    ~UvHandler()
    {
        _mutex.lock();
        while (_sending_queue.size() > 0) {
            auto &p = _sending_queue.front();
            _sending_queue.pop();
            delete[] p.buf;
        }
        _mutex.unlock();
        uv_loop_close(loop);
        delete tcp;
        delete writeAsync;
        delete stopAsync;
    }


public:
    static void DestroyUvHandler();

    static UvHandler *GetUVHandler();

    Client *RemoveClient(uv_stream_t *t)
    {
        _mutex.lock();
        Client *ret = nullptr;
        const auto &p = _clientMap.find(t);
        if (p != _clientMap.cend()) {
            _clientMap.erase(p);
            ret = p->second;
        }
        _mutex.unlock();
        return ret;
    }

    void AddClient(Client *c)
    {
        uv_stream_t *p = *c;
        _mutex.lock();
        assert(_clientMap.find(p) != _clientMap.end());
        _clientMap.emplace(p, c);
        _mutex.unlock();
    }

    bool GetDatabaseConnected() const
    {
        return dbConnected;
    }

    void SetDatabaseConnect(bool c)
    {
        dbConnected = c;
    }

    void _WriteToNetwork();

    void WriteToNetwork(protobuf::CoreMessage &, uv_stream_t *);

    void ClientDisconnect(Client *);

    void ReadFromNetwork(Client *, char *, ssize_t);

    void SetupNetwork(const char * = DEFAULT_LISTEN_ADDRESS);

    void UvLoopRun()
    {
        uv_run(loop, UV_RUN_DEFAULT);
    }

    uv_loop_t *GetLoop()
    {
        return loop;
    }

    void StopLoop();
};
