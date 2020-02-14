#pragma once

#ifndef PROTOBUFLIB_H
#define PROTOBUFLIB_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined _MSC_VER
#pragma warning(disable : 4251)
#else
#if defined __GNUC__ && !defined __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
#endif
#include <clientService.pb.h>

#if defined __GNUC__ && !defined __clang__
#pragma GCC diagnostic pop
#endif


#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <algorithm>
#include <mutex>
#include <deque>

void getTimeStamp(std::string&);

void getOsVersion(std::string&);

void initProtobufLibrary(const std::string& = "");

void shutdownProtobufLibrary();

class coreMessage;

namespace protobuf
{
    enum class Operation {
        NONE_OPERATION,
        CHECK_VERSION,
        UPDATE_LOG,
        QUERY_LAST_EVENT,
        RETURN_LAST_EVENT,
        ACCEPT_LAST_EVENT,
        CONNECT,
        REFUSE
    };

    enum class LogLevel { fatal, error, warning, info, verbose };

    enum class OsType { os_windows, os_linux, os_other };

    class CoreMessage;
    class LogPackage;

    std::ostream& operator<<(std::ostream& ios, const protobuf::CoreMessage&);
    std::ostream& operator<<(std::ostream& ios, const protobuf::LogPackage&);

    class CoreMessage
    {
        friend void ::initProtobufLibrary(const std::string&);
        friend std::ostream& operator<<(std::ostream& ios, const CoreMessage&);

        int32_t _id = -1;

        std::string _clientName;

        std::string _osVersion;

        OsType _osType;

        static std::string _myClientName;

        static std::string _myOsVersion;

        static OsType _myOsType;

        static std::string _myMachineID;

        Operation _op;

        std::string _timeStamp;

        std::string _description;

        std::string _message;

        std::string _machineID;

    protected:
        CoreMessage(Operation op) : _op(op) {}

        CoreMessage(LogLevel, const std::string& description, const std::string& timeStamp);
        CoreMessage(LogLevel, const std::string& description);

        CoreMessage(const std::string& desc, Operation op);

        virtual void buildPBObj(coreMessage&) = 0;

        CoreMessage(LogLevel);

    public:
        virtual ~CoreMessage() {}

        CoreMessage() = delete;

        CoreMessage(coreMessage& core);

        CoreMessage(int id, const std::string& desc);

        bool toBytes(char** ptr, size_t* size) ;

        static CoreMessage* BuildObj(const coreMessage&);
        static CoreMessage* parseFromIStream(std::istream*);

        static CoreMessage* parseFromArray(char*, size_t);

        const std::string& GetClientName() const
        {
            return _clientName;
        }

        static const std::string& getClientName()
        {
            return _myClientName;
        }

        const std::string& GetOSVersion() const
        {
            return _osVersion;
        }

        OsType GetOsType() const
        {
            return _osType;
        }

        const std::string& MachineID() const
        {
            return _machineID;
        }

        int32_t Id() const
        {
            return _id;
        }
        void Id(int32_t val)
        {
            _id = val;
        }
        protobuf::Operation Op() const
        {
            return _op;
        }
        void Op(protobuf::Operation val)
        {
            _op = val;
        }
        const std::string& TimeStamp() const
        {
            return _timeStamp;
        }
        void TimeStamp(const std::string& val)
        {
            _timeStamp = val;
        }
        const std::string& Description() const
        {
            return _description;
        }

        void Description(std::string&& val)
        {
            _description.swap(val);
        }

        void Description(const std::string& val)
        {
            _description = val;
        }
        const std::string& Message() const
        {
            return _message;
        }
        void Message(const std::string& val)
        {
            _message = val;
        }
    };

    class ConnectPackage : public CoreMessage
    {
    public:
        ConnectPackage();
        virtual void buildPBObj(coreMessage&) {}
    };

    class AcceptLastEventPackage : public CoreMessage
    {
        uint32_t _lEID;

    public:
        AcceptLastEventPackage(uint32_t lastEID)
            : CoreMessage("AcceptLastEventPackage", Operation::ACCEPT_LAST_EVENT), _lEID(lastEID)
        {
        }

        uint32_t GetLastEventID() const
        {
            return _lEID;
        }

        virtual void buildPBObj(coreMessage& msg) ;
    };

    class ReturnLastEventPackage : public CoreMessage
    {
        uint32_t _lEID;

    public:
        ReturnLastEventPackage(uint32_t lastEID)
            : CoreMessage("ReturnLastEventPackage", Operation::RETURN_LAST_EVENT), _lEID(lastEID)
        {
        }

        uint32_t GetLastEventID() const
        {
            return _lEID;
        }

        virtual void buildPBObj(coreMessage& msg);
    };

    class RefusePackage : public CoreMessage
    {
        int refusedID;

    public:
        virtual void buildPBObj(coreMessage& msg) 
        {
            msg.set_refuseid(refusedID);
        }

        RefusePackage(int r, const std::string& desc) : CoreMessage(Operation::REFUSE), refusedID(r)
        {
            Message("Refuse package");
            Description(desc);
        }

        RefusePackage(int r) : CoreMessage(Operation::REFUSE), refusedID(r)
        {
            Message("Refuse package");
        }
    };

    class QueryLastEventPackage : public CoreMessage
    {
    public:
        QueryLastEventPackage() : CoreMessage("QueryLastEventPackage", Operation::QUERY_LAST_EVENT)
        {
        }

        void buildPBObj(coreMessage&) {}
    };

    class UpdatePackage : public CoreMessage
    {
        std::string _versionString;
        std::string _checkSum;
        std::string _package;


    public:
        const std::string& VersionString() const
        {
            return _versionString;
        }
        void VersionString(const std::string& val)
        {
            _versionString = val;
        }
        const std::string& CheckSum() const
        {
            return _checkSum;
        }
        void CheckSum(const std::string& val)
        {
            _checkSum = val;
        }
        const std::string& Package() const
        {
            return _package;
        }
        void Package(const std::string& val)
        {
            _package = val;
        }
    };

    struct Event {
        std::string xml;
        std::string format;
        std::string timeStamp;
        std::string provider;

        LogLevel llevel;
        uint32_t level;
        uint32_t rid;

        static uint32_t dispatch(LogLevel l)
        {
            switch (l) {
                case protobuf::LogLevel::fatal:
                    return 1;
                case protobuf::LogLevel::error:
                    return 2;
                case protobuf::LogLevel::warning:
                    return 3;
                case protobuf::LogLevel::info:
                    return 4;
                case protobuf::LogLevel::verbose:
                    return 5;
                default:
                    break;
            }
        }

        static LogLevel dispatch(uint32_t l)
        {
            switch (l) {
                case 1:
                    return LogLevel::fatal;
                case 2:
                    return LogLevel::error;
                case 3:
                    return LogLevel::warning;
                case 4:
                    return LogLevel::info;
                case 5:
                default:
                    return LogLevel::verbose;
            }
        }

        Event() {}

        Event(const std::string& x,
              const std::string& f,
              const std::string& p,
              const std::string& t,
              LogLevel l,
              uint32_t r)
            : xml(x), format(f), timeStamp(t), provider(p), llevel(l), rid(r)
        {
            level = dispatch(l);
        }

        Event(const std::string& x,
              const std::string& f,
              const std::string& p,
              const std::string& t,
              uint32_t l,
              uint32_t r)
            : xml(x), format(f), timeStamp(t), provider(p), level(l), rid(r)
        {
            llevel = dispatch(l);
        }
    };

    class LogPackage : public CoreMessage
    {
        friend std::ostream& operator<<(std::ostream& ios, const LogPackage&);

    protected:
        std::vector<Event> evts;
        bool logNeedAccept;

    public:
        const std::vector<Event>& GetEvents() const
        {
            return evts;
        }

        bool NeedAccept() const
        {
            return logNeedAccept;
        }

        void forEachEvent(std::function<void(const Event&)> func)
        {
            std::for_each(evts.cbegin(), evts.cend(), func);
        }

        void reserve(size_t i)
        {
            evts.reserve(i);
        }

        void AddLogEvent(std::vector<Event>&& evts)
        {
            this->evts.swap(evts);
        }

        void AddLogEvent(const std::vector<Event>& evts)
        {
            this->evts = evts;
        }

        void AddLogEvent(Event&& evt)
        {
            evts.emplace_back(std::move(evt));
        }

        void AddLogEvent(const Event& evt)
        {
            evts.emplace_back(evt);
        }

        virtual void buildPBObj(coreMessage&) ;

        LogPackage(bool ac = false) : CoreMessage(LogLevel::info)
        {
            logNeedAccept = ac;
        }
    };

    static constexpr int PACKAGE_HEADER_SIZE = 4 + 4 + 32;

    class ProtobufPacketDecoder
    {
        uint32_t compressSize;
        uint32_t remainSize;
        uint32_t serialSize;
        uint32_t headerSize;

        std::deque<CoreMessage*> _vec;

        char header_buffer[PACKAGE_HEADER_SIZE];

        std::mutex _mutex;
        std::stringstream _buf;

        void parseHeader(char*, size_t);
        CoreMessage* _read(void*, size_t);

    public:
        static void ProtobufPacketEncoder(char**, size_t& size, std::vector<const CoreMessage&>);
        static void ProtobufPacketEncoder(char**, size_t& size, CoreMessage&);

        auto GetSize()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _vec.size();
        }

        CoreMessage* GetProtobufMessage()
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (!_vec.empty()) {
                auto ret = _vec.front();
                _vec.pop_front();
                return ret;
            } else {
                return nullptr;
            }
        }

        void Reset()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            for (auto& p : _vec) {
                delete p;
            }
            _vec.clear();
            _buf.clear();
            compressSize = remainSize = headerSize = 0;
        }

        ~ProtobufPacketDecoder()
        {
            Reset();
        }

        ProtobufPacketDecoder() : compressSize(0), remainSize(0), serialSize(0), headerSize(0)
        {
            memset(header_buffer, 0, sizeof header_buffer);
        }

        CoreMessage* read(void*, size_t);
    };

}  // namespace protobuf

protobuf::OsType getOsType();

const char* SHA256(void*, size_t);

size_t CompressSizeBound(size_t);
bool Compress(char**, size_t*, const char*, size_t);
bool DeCompress(char*, size_t*, const char*, size_t);


#endif
