/*
 *
 * WangXiao (zjjhwxc@gmail.com)
 */

#include "clientServer.h"

#include <cerrno>
#include <cstring>
#include <atomic>

#include <spdlog/spdlog.h>

#include <rapidjson/document.h>

#ifdef UNIX
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#else
#include <fstream>
#endif

#define BUILD_JSON_STRING_STATEMENT(path, dest, defaultValue) \
    do {                                                      \
        const auto& v = document[path];                       \
        if (v.IsString())                                     \
            ret->dest = v.GetString();                        \
        else                                                  \
            ret->dest = defaultValue;                         \
    } while (false);

struct Config* ReadConfig(const char* path)
{
    char* buffer = nullptr;
#ifdef UNIX
    struct stat st;
    int fp = stat(path, &st);
    if (fp == -1) {
        spdlog::error("Open file {} failed: {}. exiting...", path, strerror(errno));
        exit(1);
    }

    fp = open(path, O_RDONLY);

    auto s = st.st_size;
    buffer = new char[s + 1];

    int status = read(fp, buffer, s);
    if (status != s) {
        spdlog::error("Read file {} failed, need {} bytes, but read {} bytes.", path, s, status);
        exit(1);
    }
    buffer[s] = 0;
    close(fp);
#else
    std::fstream fp(path, std::ios::in | std::ios::binary);
    auto b = fp.tellg();
    fp.seekg(0, std::ios::end);
    auto e = fp.tellg();

    auto length = e - b;

    fp.seekg(0, std::ios::beg);

    buffer = new char[length + 1];

    fp.read(buffer, length);

    fp.close();

    buffer[length] = 0;
#endif

    rapidjson::Document document;
    document.Parse(buffer);

    if (document.HasParseError()) {
        spdlog::error("Parse json file {} failed. exiting...", path);
        exit(1);
    }

    auto ret = new Config;

    BUILD_JSON_STRING_STATEMENT("host", host, "localhost")
    BUILD_JSON_STRING_STATEMENT("username", username, "postgres")
    BUILD_JSON_STRING_STATEMENT("password", password, "");

    const auto& v = document["port"];
    if (v.IsInt()) {
        ret->port = v.GetInt();
        if (ret->port > 65535 || ret->port <= 0) {
            ret->port = 5432;
            spdlog::warn("Invalid postgres port {}, set to default 5432", ret->port);
        }
    } else {
        ret->port = 5432;
    }

    // postgresql://[user[:password]@][netloc][:port][,...][/dbname][?param1=value1&...]
    std::string con = fmt::format(
        "postgresql://{username}:{password}@{host}:{port}/"
        "{dbname}?application_name=ClientServiceServer",
        fmt::arg("username", ret->username),
        fmt::arg("password", ret->password),
        fmt::arg("host", ret->host),
        fmt::arg("port", ret->port),
        fmt::arg("dbname", DEFAULT_DB_NAME));

    auto l = std::char_traits<char>::length(con.c_str());

    auto cs = new char[l + 1];

    std::char_traits<char>::copy(cs, con.c_str(), l);

    cs[l] = 0;

    ret->connectionString = cs;

    spdlog::info("Connect to postgres: {}", con);

    delete[] buffer;

    delete[] cs;

    return ret;
}