
#include "protobufLib.h"


#include <cassert>
#include <sstream>

#if defined _WINDOWS_ || defined _WIN32
#include <windows.h>
#elif defined UNIX && defined UNIX_HAVE_UNAME
#include <sys/utsname.h>
#include <arpa/inet.h>
#endif


using std::string;

using namespace protobuf;

namespace
{
    Operation dispatch(coreMessage_Operation op)
    {
        switch (op) {
            case coreMessage_Operation_CONNECT:
                return Operation::CONNECT;
            case coreMessage_Operation_CHECK_VERSION:
                return Operation::CHECK_VERSION;
            case coreMessage_Operation_UPDATE_LOG:
                return Operation::UPDATE_LOG;
            case coreMessage_Operation_QUERY_LAST_EVENT:
                return Operation::QUERY_LAST_EVENT;
            case coreMessage_Operation_RETURN_LAST_EVENT:
                return Operation::RETURN_LAST_EVENT;
            case coreMessage_Operation_ACCEPT_LAST_EVENT:
                return Operation::ACCEPT_LAST_EVENT;
            case coreMessage_Operation_coreMessage_Operation_INT_MIN_SENTINEL_DO_NOT_USE_:
            case coreMessage_Operation_coreMessage_Operation_INT_MAX_SENTINEL_DO_NOT_USE_:
            default:
                break;
        }
        return Operation::NONE_OPERATION;
    }
#define buildLogLevelDispatch(l) \
    case LogLevel::l:            \
        return coreMessage_logLevel_##l

    coreMessage_logLevel dispatch(protobuf::LogLevel level)
    {
        switch (level) {
            buildLogLevelDispatch(error);
            buildLogLevelDispatch(fatal);
            buildLogLevelDispatch(warning);
            buildLogLevelDispatch(info);
            buildLogLevelDispatch(verbose);
            default:
                break;
        }
        return coreMessage_logLevel_verbose;
    }
#undef buildLogLevelDispatch

#define buildLevelDispatch(l)      \
    case coreMessage_logLevel_##l: \
        return LogLevel::l

    protobuf::LogLevel dispatch(coreMessage_logLevel l)
    {
        switch (l) {
            buildLevelDispatch(fatal);
            buildLevelDispatch(error);
            buildLevelDispatch(warning);
            buildLevelDispatch(info);
            buildLevelDispatch(verbose);
            default:
                break;
        }
        return LogLevel::verbose;
    }
#undef buildLevelDispatch

    coreMessage_Operation dispatch(Operation op)
    {
        switch (op) {
            case protobuf::Operation::CONNECT:
                return coreMessage_Operation_CONNECT;
            case protobuf::Operation::CHECK_VERSION:
                return coreMessage_Operation_CHECK_VERSION;
            case protobuf::Operation::UPDATE_LOG:
                return coreMessage_Operation_UPDATE_LOG;
            case protobuf::Operation::QUERY_LAST_EVENT:
                return coreMessage_Operation_QUERY_LAST_EVENT;
            case protobuf::Operation::RETURN_LAST_EVENT:
                return coreMessage_Operation_RETURN_LAST_EVENT;
            case protobuf::Operation::ACCEPT_LAST_EVENT:
                return coreMessage_Operation_ACCEPT_LAST_EVENT;
            default:
                break;
        }
        return coreMessage_Operation_coreMessage_Operation_INT_MIN_SENTINEL_DO_NOT_USE_;
    }

    coreMessage_osType dispatch(OsType ot)
    {
        if (ot == OsType::os_linux) {
            return coreMessage_osType_linux_os;
        } else {  // if (ot == OsType::os_windows) {
            return coreMessage_osType_windows_os;
        }
    }

    int nextID()
    {
        static int id = 0;
        return id++;
    }
}  // namespace


std::string CoreMessage::_myClientName;
std::string CoreMessage::_myOsVersion;
std::string CoreMessage::_myMachineID;
OsType CoreMessage::_myOsType = getOsType();

CoreMessage::CoreMessage(int id, const std::string& desc) : _id(id), _description(desc) {}

CoreMessage::CoreMessage(LogLevel, const string& description, const string& timeStamp)
    : _id(nextID()), _osType(getOsType()), _timeStamp(timeStamp), _description(description)
{
    _op = Operation::UPDATE_LOG;
}

CoreMessage::CoreMessage(LogLevel, const string& description)
    : _id(nextID()), _osType(getOsType()), _op(Operation::UPDATE_LOG), _description(description)
{
}

CoreMessage::CoreMessage(const std::string& desc, Operation op)
    : _id(nextID()), _op(op), _description(desc)
{
}

CoreMessage::CoreMessage(LogLevel) : _id(nextID()), _op(Operation::UPDATE_LOG) {}


bool CoreMessage::toBytes(char** ptr, size_t* size)
{
    coreMessage core;

    core.set_id(Id());
    core.set_op(dispatch(Op()));
    auto ts = TimeStamp();
    if (ts.empty()) {
        getTimeStamp(ts);
        core.set_timestamp(ts);
    } else {
        core.set_timestamp(TimeStamp());
    }
    core.set_description(Description());
    core.set_clientname(CoreMessage::_myClientName);
    core.set_osversion(CoreMessage::_myOsVersion);
    core.set_machineid(CoreMessage::_myMachineID);

    buildPBObj(core);

    char *serialBuffer, *compressBuffer, *pkgBuffer;
    size_t serialSize, compressSize, pkgSize;
    bool serialStatus, compressStatus;

    serialStatus = compressStatus = false;
    serialSize = compressSize = pkgSize = 0;
    serialBuffer = compressBuffer = pkgBuffer = nullptr;

    serialSize = core.ByteSizeLong();
    serialBuffer = new char[serialSize];
    serialStatus = core.SerializeToArray(serialBuffer, serialSize);

    const char* dgstBuffer = SHA256(serialBuffer, serialSize);
    compressStatus = Compress(&compressBuffer, &compressSize, serialBuffer, serialSize);
    assert(compressStatus);

    if (compressSize < serialSize) {
        pkgSize = PACKAGE_HEADER_SIZE + compressSize;
        pkgBuffer = new char[pkgSize];

        *((uint32_t*)pkgBuffer + 0) = htonl(serialSize);
        *((uint32_t*)pkgBuffer + 1) = htonl(compressSize);

        memmove(pkgBuffer + 8, dgstBuffer, 32);
        memmove(pkgBuffer + PACKAGE_HEADER_SIZE, compressBuffer, compressSize);
    } else {
        pkgSize = PACKAGE_HEADER_SIZE + serialSize;
        pkgBuffer = new char[pkgSize];

        *((uint32_t*)pkgBuffer + 0) = htonl(serialSize);
        *((uint32_t*)pkgBuffer + 1) = htonl(serialSize);

        memmove(pkgBuffer + 8, dgstBuffer, 32);
        memmove(pkgBuffer + PACKAGE_HEADER_SIZE, serialBuffer, serialSize);
    }
    delete[] compressBuffer;
    delete[] serialBuffer;
    delete[] dgstBuffer;

    *ptr = pkgBuffer;
    *size = pkgSize;

    return serialStatus;
}

CoreMessage* CoreMessage::BuildObj(const coreMessage& core)
{
    CoreMessage* msg = nullptr;

    switch (core.op()) {
        case coreMessage_Operation_UPDATE_LOG: {
            LogPackage* _msg = new LogPackage(core.logneedaccept());
            int size = core.log_size();
            _msg->reserve(size);
            for (int i = 0; i < size; i++) {
                const auto& levt = core.log(i);
                Event e(levt.xmleventmessage(),
                        levt.raweventmessage(),
                        levt.scope(),
                        levt.timestamp(),
                        levt.level(),
                        levt.recordid());

                _msg->AddLogEvent(std::move(e));
            }
            msg = _msg;
        } break;
        case coreMessage_Operation_QUERY_LAST_EVENT: {
            msg = new QueryLastEventPackage;
        } break;
        case coreMessage_Operation_RETURN_LAST_EVENT: {
            msg = new ReturnLastEventPackage(core.lastevent());
        } break;
        case coreMessage_Operation_ACCEPT_LAST_EVENT: {
            msg = new AcceptLastEventPackage(core.lastevent());
        } break;
        case coreMessage_Operation_CONNECT: {
            msg = new ConnectPackage;
            break;
        }
        default:
            assert(false);
            break;
    }

    if (msg != nullptr) {
        msg->_id = core.id() == -1 ? nextID() : core.id();
        msg->_description = core.description();
        msg->_clientName = core.clientname();
        msg->_osVersion = core.osversion();
        msg->_timeStamp = core.timestamp();
        msg->_machineID = core.machineid();
        msg->_osType = core.os() == coreMessage_osType_windows_os ? protobuf::OsType::os_windows
                                                                  : protobuf::OsType::os_linux;
    }
    return msg;
}

CoreMessage* CoreMessage::parseFromIStream(std::istream* is)
{
    coreMessage core;
    bool ret = core.ParseFromIstream(is);
    if (ret) {
        return BuildObj(core);
    } else {
        return nullptr;
    }
}

CoreMessage* CoreMessage::parseFromArray(char* data, size_t size)
{
    coreMessage core;
    bool ret = core.ParseFromArray(data, size);
    if (ret) {
        return BuildObj(core);
    } else {
        return nullptr;
    }
}

std::ostream& protobuf::operator<<(std::ostream& ios, const CoreMessage& core)
{
    ios << "CoreMessage: id: " << core.Id() << "\t"
        << "Operation: " << coreMessage_Operation_Name(dispatch(core.Op())) << "\t"
        << "Operation System: " << (core._osType == OsType::os_windows ? "Windows" : "Linux") << "("
        << core._osVersion << ")"
        << "ClientName: " << core._clientName << std::endl
        << "description: " << core._description << std::endl
        << "TimeStamp: " << core._timeStamp << std::endl;

    switch (core.Op()) {
        case Operation::UPDATE_LOG: {
            try {
                auto& l = dynamic_cast<const LogPackage&>(core);
                ios << l;
            } catch (std::bad_cast&) {
            }
        } break;
        default:
            break;
    }
    return ios;
}

std::ostream& protobuf::operator<<(std::ostream& ios, const protobuf::LogPackage& log)
{
    //  ios << "Log: [" << coreMessage_logLevel_Name(dispatch(log.Level())) << "]"
    //      << "-" << log.Scope() << ": " << log.LogMessage() << std::endl;
    return ios;
}

void LogPackage::buildPBObj(coreMessage& obj)
{
    obj.mutable_log()->Reserve(evts.size());

    const size_t size = 1024;
    char buffer[size];

    snprintf(buffer,
             size,
             "LogPackage. Windows Event RecordID from %d to %d.",
             (evts.cbegin()->rid),
             (evts.cend()->rid));

    Description(std::move(std::string(buffer)));

    for (const auto& evt : this->evts) {
        auto l = obj.add_log();
        l->set_scope(evt.provider);
        l->set_raweventmessage(evt.format);
        l->set_timestamp(evt.timeStamp);
        l->set_xmleventmessage(evt.xml);
        l->set_level(dispatch(evt.llevel));
        l->set_recordid(evt.rid);
    }
    obj.set_logneedaccept(this->logNeedAccept);
}

void ReturnLastEventPackage::buildPBObj(coreMessage& msg)
{
    msg.set_lastevent(_lEID);
}

void AcceptLastEventPackage::buildPBObj(coreMessage& msg)
{
    msg.set_lastevent(_lEID);
}

void ProtobufPacketDecoder::parseHeader(char* data, size_t size)
{
    auto _size = size;
    auto _data = data;
    if (headerSize < PACKAGE_HEADER_SIZE) {
        int c = 0;
        for (; 0 < (PACKAGE_HEADER_SIZE - headerSize) && _size > 0;
             c++, _size--, headerSize++, _data++) {
            header_buffer[headerSize] = data[c];
        }
        if (_size <= 0) {
            return;
        }
    }
    if (headerSize == PACKAGE_HEADER_SIZE) {
        if (compressSize == 0) {
            compressSize = remainSize = htonl(*((uint32_t*)header_buffer + 1));
            serialSize = htonl(*((uint32_t*)header_buffer + 0));

            _read(_data, _size);
        } else {
            assert(false);
        }
    }
}

void ProtobufPacketDecoder::ProtobufPacketEncoder(char** buf, size_t& size, CoreMessage& msg)
{
    msg.toBytes(buf, &size);
}

CoreMessage* ProtobufPacketDecoder::read(void* data, size_t size)
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return _read(data, size);
}

CoreMessage* ProtobufPacketDecoder::_read(void* data, size_t size)
{
    CoreMessage* ret = nullptr;

    char* buf = nullptr;

    if (compressSize != 0) {
        if (remainSize <= size) {
            buf = new char[compressSize];

            _buf.read(buf, compressSize - remainSize);
            assert(_buf.gcount() == compressSize - remainSize);

            memcpy(buf + compressSize - remainSize, data, remainSize);

            // here, `buf' contains all data received from remote.

            const char* dgst;

            if (serialSize == compressSize) {
                //donot de-compress the `buf'

                dgst = SHA256(buf, serialSize);
                assert(memcmp((char*)header_buffer + 8, dgst, 32) == 0);

                ret = CoreMessage::parseFromArray(buf, serialSize);
            } else {
                auto uncompressBuffer = new char[(uint64_t)serialSize + 1];
                size_t uncompressSize = serialSize;
                DeCompress(uncompressBuffer, &uncompressSize, buf, compressSize);

                dgst = SHA256(uncompressBuffer, uncompressSize);
                assert(memcmp((char*)header_buffer + 8, dgst, 32) == 0);
                ret = CoreMessage::parseFromArray(uncompressBuffer, uncompressSize);
                delete[] uncompressBuffer;
            }

            _vec.push_back(ret);
            assert(ret != nullptr);
            compressSize = headerSize = 0;

            delete[] dgst;
            delete[] buf;
            if (size >= remainSize) {
                size -= remainSize;
                char* p = (char*)data + remainSize;
                headerSize = remainSize = compressSize = 0;
                parseHeader(p, size - remainSize);
            } else {
                assert(false);
            }
        } else {
            _buf.write((char*)data, size);
            remainSize -= size;
            assert(remainSize <= compressSize);
        }
    } else {
        parseHeader((char*)data, size);
    }

    assert(_buf);
    return ret;
}

protobuf::ConnectPackage::ConnectPackage() : CoreMessage(nextID(), "Connect")
{
    Op(Operation::CONNECT);
}