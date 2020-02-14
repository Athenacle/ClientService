/*
 *
 * WangXiao (zjjhwxc@gmail.com)
 */

#include "clientServer.h"

#include <pqxx/version.hxx>
#include <spdlog/spdlog.h>

#include <cassert>

using namespace pqxx;
using namespace database;
using namespace protobuf;

namespace
{
    protobuf::OsType dispatch(const std::string &os)
    {
        if (os == "Linux") {
            return protobuf::OsType::os_linux;
        } else if (os == "Windows") {
            return protobuf::OsType::os_windows;
        } else {
            return protobuf::OsType::os_other;
        }
    }


}  // namespace


//////////////////////////////////////////////////////////////////////////

Database *Database::_db = nullptr;

#define DEBUG_PRINT_SQL                                       \
    do {                                                      \
        spdlog::debug("{}:{} - {}", __FILE__, __LINE__, sql); \
    } while (false)

DbClient *Database::GetClient(const CoreMessage &msg)
{
    auto *dc = new DbClient;

    dc->_clientName = msg.GetClientName();
    dc->_clientOs = msg.GetOsType();
    dc->_clientOsVersion = msg.GetOSVersion();
    dc->_clientUniqueID = msg.MachineID();

    auto p = _clients.find(dc->_clientUniqueID);
    if (p != _clients.end()) {
        auto saved = p->second;
        if (dc->_clientOsVersion != saved->_clientOsVersion) {
            spdlog::info("Client #{}@{} OsVersion changed from {} to {}",
                         saved->_clientID,
                         saved->_clientUniqueID,
                         saved->_clientOsVersion,
                         dc->_clientOsVersion);

            pqxx::work w(*conn);
            std::string sql =
                "UPDATE \"Client\" SET \"ClientLastConnect\" = NOW(), \"ClientOSVersion\" = '";

            sql += w.quote(dc->_clientOsVersion);
            sql += "' WHERE \"CliendID\" = ";
            sql += std::to_string(dc->_clientID);

            DEBUG_PRINT_SQL;
            auto r = w.exec(sql);
            w.commit();
            r.clear();
        } else {
            std::string sql =
                "UPDATE \"Client\" SET \"ClientLastConnect\" = NOW() WHERE \"ClientID\" = ";
            sql += std::to_string(dc->_clientID);

            DEBUG_PRINT_SQL;
            pqxx::work w(*conn);
            auto r = w.exec(sql);
            w.commit();
            r.clear();
        }
        return p->second;
    } else {
        pqxx::work w(*conn);

        auto r = w.exec_prepared("insertClient",
                                 dc->_clientName,                                          //1
                                 dc->_clientOs == OsType::os_linux ? "Linux" : "Windows",  //2
                                 dc->_clientOsVersion,                                     //3
                                 dc->_clientUniqueID);                                     //4

        w.commit();
        if (r.size() == 1) {
            dc->_clientID = r[0][0].as<long>();
            spdlog::info("insert new client: #{}@{}", dc->_clientID, dc->_clientUniqueID);
        } else {
            assert(false);
        }
        r.clear();
        return dc;
    }
}

const std::string &dispatchEventSeverity(int s)
{
    static std::string severity[] = {"others", "fatal", "error", "warning", "info", "verbose"};
    if (s > 5 || s <= 0) {
        s = 0;
    }
    return severity[s];
}

int database::Database::InsertWindowsEvents(const DbClient &c,
                                            const std::vector<protobuf::Event> &evts)
{
    /*
        INSERT INTO 
        public."WindowsEvents"
            ("ClientID", "EventSeverity", "EventTimestamp", "EventScope", "EventMessage", "EventRecordID")
        VALUES ($1, $2, $3, $4, $5, $6) RETURNING "EventID";
    */
    pqxx::work w(*conn);
    auto cid = c._clientID;
    int inserted = 0;
    try {
        for (const auto &evt : evts) {
            auto insE = w.exec_prepared("insertEvent",
                                        cid,
                                        dispatchEventSeverity(evt.level),
                                        evt.timeStamp,
                                        evt.provider,
                                        evt.format,
                                        evt.rid);
            insE.clear();
            if (insE.size() == 1) {
                // INSERT INTO public."WindowsEventXML"("EventID", "EventXML") VALUES ($1, $2)";
                int eid = insE[0][0].as<int>();
                auto r = w.exec_prepared("insertEventXML", std::to_string(eid), evt.xml);
                r.clear();
                inserted++;
            }
        }
        w.commit();
    } catch (const pqxx::sql_error &se) {
        spdlog::error("database exception: {}({}) ", se.what(), se.sqlstate());
        w.abort();
    }

    return inserted;
}

Database *Database::InitDatabase(std::string &&cstr)
{
    return _db = new Database(std::move(cstr));
}

Database *Database::GetDatabase()
{
    return _db;
}

void Database::GetAllClients()
{
    pqxx::work w(*conn);

    pqxx::result clients = w.exec("SELECT * from \"Client\"");

    w.commit();

    spdlog::info("Startup: Get total {} clients from database", clients.size());

    for (auto c : clients) {
        DbClient dbC;

        dbC._clientID = c[0].as<long>();
        dbC._clientName = c[1].c_str();
        dbC._clientOs = dispatch(c[2].as<std::string>());
        dbC._clientOsVersion = c[3].c_str();
        dbC._clientUniqueID = c[4].c_str();
        dbC._clientRegisterTime = c[5].c_str();

        spdlog::info("Startup: Client #{}@{}, name {}, OS: {} {}, added at {}",
                     dbC._clientID,
                     dbC._clientUniqueID,
                     dbC._clientName,
                     c[2].c_str(),
                     dbC._clientOsVersion,
                     dbC._clientRegisterTime);

        _clients[std::move(std::string(c[4].c_str()))] = new DbClient(std::move(dbC));
    }
    assert(_clients.size() == clients.size());
    clients.clear();
}

bool Database::Connect()
{
    char insertClient[] =
        "INSERT INTO public.\"Client\"("
        "\"ClientName\", \"ClientOS\", \"ClientOSVersion\", \"ClientUniqueID\","
        "\"ClientRegisterTime\", \"ClientLastConnect\") VALUES ($1, $2, $3, $4,  NOW(), NOW()) "
        "RETURNING *";

    char insertEvent[] =
        "INSERT INTO public.\"WindowsEvents\"("
        "\"ClientID\", \"EventSeverity\", \"EventTimestamp\", "
        "\"EventScope\", \"EventMessage\", \"EventRecordID\")"
        "VALUES ($1, $2, $3, $4, $5, $6) RETURNING \"EventID\"";

    char insertEventXML[] =
        "INSERT INTO public.\"WindowsEventXML\"(\"EventID\", \"EventXML\") VALUES ($1, $2)";
    spdlog::info("DB: libpqxx version {}", PQXX_VERSION);

    pqxx::broken_connection bc;

    for (int time = 0; time < 5; time++) {
        try {
            conn = new pqxx::connection(connectionString);
            if (conn->is_open()) {
                break;
            }
        } catch (pqxx::broken_connection &con) {
            delete conn;
            utils::Sleep(std::pow(2, time));
            bc = con;
        }
    }
    if (!conn->is_open()) {
        spdlog::error("connect to database server failed: {}", bc.what());
        return false;
    }
    assert(conn->is_open());
    auto sv = conn->get_variable("server_version");
    spdlog::info("Connecting to database server successfully. Server version: {}", sv);
    GetAllClients();

    conn->prepare("insertEvent", insertEvent);
    conn->prepare("insertEventXML", insertEventXML);
    conn->prepare("insertClient", insertClient);
    return true;
}

int database::Database::GetLastEventRecordID(const DbClient &dbc)
{
    std::string sql = "select \"EventRecordID\" from \"WindowsEvents\" WHERE \"ClientID\" = ";
    sql += std::to_string(dbc._clientID);
    DEBUG_PRINT_SQL;
    pqxx::work w(*conn);
    auto r = w.exec(sql);
    int ret = 0;

    if (r.size() == 0) {
        return 0;
    } else {
        for (const auto &c : r) {
            int t = c[0].as<int>();
            if (t > ret) {
                ret = t;
            }
        }
    }

    return ret;
}

bool Database::CheckConnectStatus()
{
    return conn->is_open();
}