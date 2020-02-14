
#include "clientServer.h"

#include <spdlog/spdlog.h>

using namespace protobuf;

namespace
{
    std::mutex _uvMutex;

    struct Workdata {
        Client *c;
        char *buf;
        ssize_t size;
    };

}  // namespace


////// libuv callback functions

void uvAllocCB(uv_handle_t *, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void uvCloseCB(uv_handle_t *handle)
{
    Client *c = reinterpret_cast<Client *>(handle->data);
    c->clientDisConnected();
    UvHandler::GetUVHandler()->ClientDisconnect(c);
    delete c;
}

void uvReadCB(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
{
    Client *c = reinterpret_cast<Client *>(client->data);
    auto cname = c->_client->_clientName;
    if (nread < 0) {
        switch (nread) {
            case UV_EOF:
                spdlog::info("Network closed. (Client: {})", cname);
                uv_close(reinterpret_cast<uv_handle_t *>(client), uvCloseCB);
                break;
            case UV_ECONNRESET:
                spdlog::info("Network reset.  (Client: {})", cname);
                uv_close(reinterpret_cast<uv_handle_t *>(client), uvCloseCB);
                break;
            default:
                spdlog::warn("Network read error {}. (Client: {})", uv_strerror(errno), cname);
        }
        if (buf->base != nullptr) {
            delete[] buf;
        }
    } else if (nread > 0) {
        UvHandler::GetUVHandler()->ReadFromNetwork(c, buf->base, nread);
    }
}

void uvConnectCB(uv_stream_t *server, int status)
{
    if (status < 0) {
        fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        return;
    }

    Client *client = new Client(UvHandler::GetUVHandler()->GetLoop());
    UvHandler::GetUVHandler()->AddClient(client);

    if (uv_accept(server, *client) == 0) {
        uv_read_start(*client, uvAllocCB, uvReadCB);
        client->printRemote();
    } else {
        client->close();
    }
}

///// libuv callback functions end


UvHandler *UvHandler::server = nullptr;

UvHandler::UvHandler()
{
    loop = uv_default_loop();
    tcp = new uv_tcp_t;
    writeAsync = new uv_async_t;
    stopAsync = new uv_async_t;

    tcp->data = loop->data = stopAsync->data = writeAsync->data = this;

    uv_async_init(loop, writeAsync, [](uv_async_t *t) {
        auto h = reinterpret_cast<UvHandler *>(t->data);
        h->_WriteToNetwork();
    });

    uv_async_init(loop, stopAsync, [](uv_async_t *async) {
        UvHandler *h = UvHandler::GetUVHandler();
        uv_walk(
            h->GetLoop(),
            [](uv_handle_t *h, void *uvh) {
                void *p = h->data;
                UvHandler *uvHandler = (UvHandler *)uvh;
                if (p != nullptr) {
                    Client *c = uvHandler->RemoveClient((uv_stream_t *)h);
                    if (c != nullptr) {
                        c->ReadStop();
                        c->close();
                        delete c;
                    }
                }
                uv_close((uv_handle_t *)h, nullptr);
            },
            h);
    });

    uv_tcp_init(loop, tcp);
}

void UvHandler::DestroyUvHandler()
{
    std::lock_guard<std::mutex> lock(_uvMutex);
    delete server;
    server = nullptr;
}

UvHandler *UvHandler::GetUVHandler()
{
    std::lock_guard<std::mutex> lock(_uvMutex);
    if (server == nullptr) {
        server = new UvHandler;
    }

    return server;
}

void UvHandler::SetupNetwork(const char *addr)
{
    struct sockaddr_in sockAddr;
    int status;
    status = uv_ip4_addr(addr, DEFAULT_LISTEN_PORT, &sockAddr);
    if (status < 0) {
        spdlog::error("Startup: Translate address {} failed.", addr);
    } else {
        spdlog::debug("Startup: Translate address {} successfully.", addr);
    }

    status = uv_tcp_bind(tcp, (const struct sockaddr *)&sockAddr, 0);

    if (status < 0) {
        spdlog::error("Startup: bind address {}:{} failed: {}.",
                      addr,
                      DEFAULT_LISTEN_PORT,
                      uv_strerror(status));
    } else {
        spdlog::debug("Startup: bind address {}:{} successfully.", addr, DEFAULT_LISTEN_PORT);
    }

    status = uv_listen((uv_stream_t *)tcp, 100, uvConnectCB);

    if (status) {
        spdlog::error("Startup: listen error: {}", uv_strerror(status));
        exit(1);
    } else {
        spdlog::info("Startup: listen successfully.");
    }
}


void UvHandler::ReadFromNetwork(Client *c, char *buf, ssize_t size)
{
    c->readFromNetwork(buf, size);
    delete[] buf;
}

void UvHandler::WriteToNetwork(CoreMessage &msg, uv_stream_t *sock)
{
    char *buf;
    size_t size;
    msg.toBytes(&buf, &size);

    UvHandler::SendObject obj;
    obj.buf = buf;
    obj.length = size;
    obj.sock = sock;

    _mutex.lock();
    _sending_queue.emplace(std::move(obj));
    _mutex.unlock();

    uv_async_send(writeAsync);
}

void UvHandler::_WriteToNetwork()
{
    _mutex.lock();

    while (_sending_queue.size() > 0) {
        auto front = _sending_queue.front();

        uv_write_t *req = new uv_write_t;
        uv_buf_t *bufs = new uv_buf_t[1];

        bufs[0].base = front.buf;
        bufs[0].len = front.length;

        req->data = bufs;

        uv_write(req, front.sock, bufs, 1, [](uv_write_t *req, int status) {
            uv_buf_t *bufs = reinterpret_cast<uv_buf_t *>(req->data);
            if (status == 0) {
                spdlog::debug("sending package success. length: {}", bufs->len);
            }

            delete[] bufs->base;
            delete[] bufs;
            delete req;
        });
        _sending_queue.pop();
    }

    _mutex.unlock();
}

void UvHandler::ClientDisconnect(Client *c)
{
    _mutex.lock();

    uv_stream_t *s = *c;
    const auto &p = _clientMap.find(s);
    assert(p != _clientMap.cend());
    _clientMap.erase(p);

    _mutex.unlock();
}

void UvHandler::StopLoop()
{
    uv_async_send(stopAsync);
}
