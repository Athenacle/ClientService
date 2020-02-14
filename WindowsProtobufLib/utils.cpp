
#include "protobufLib.h"

#include <zlib.h>

#include <cassert>

using namespace protobuf;


#if defined _WINDOWS_ || defined _WIN32
//#include <ntstatus.h>
#include <windows.h>
#include <rpcdce.h>
#include <bcrypt.h>
#define STATUS_SUCCESS 0
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Bcrypt.lib")
#elif defined UNIX && defined UNIX_HAVE_UNAME
#include <sys/utsname.h>
#include <arpa/inet.h>

#include <openssl/sha.h>

#endif

#if defined _WINDOWS_ || defined WINDOWS
const char* SHA256(void* data, size_t size)
{
    // https://docs.microsoft.com/en-us/windows/win32/seccng/creating-a-hash-with-cng
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;

    NTSTATUS status = -1;
    DWORD cbData = 0, cbHash = 0, cbHashObject = 0;
    PBYTE pbHashObject = NULL;
    PBYTE pbHash = NULL;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);

    assert(status == STATUS_SUCCESS);

    status = BCryptGetProperty(
        hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);

    assert(status == STATUS_SUCCESS);

    pbHashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHashObject);
    assert(pbHashObject != nullptr);

    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);

    assert(status == STATUS_SUCCESS);

    pbHash = new BYTE[cbHash];
    assert(pbHash != nullptr);

    status = BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, NULL, 0, 0);

    assert(status == STATUS_SUCCESS);

    status = BCryptHashData(hHash, (PBYTE)data, size, 0);

    assert(status == STATUS_SUCCESS);

    status = BCryptFinishHash(hHash, pbHash, cbHash, 0);

clean:
    if (hAlg) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    if (hHash) {
        BCryptDestroyHash(hHash);
    }

    if (pbHashObject) {
        HeapFree(GetProcessHeap(), 0, pbHashObject);
    }

    return (char*)pbHash;
}


#else
const char* SHA256(void* data, size_t size)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data, size);
    SHA256_Final(hash, &sha256);

    char* ret = new char[SHA256_DIGEST_LENGTH];

    memcpy(ret, hash, SHA256_DIGEST_LENGTH);

    return ret;
}

#endif

namespace
{
    std::string timeBias = "+0";

    const size_t UUID_LENGTH = 36;

    char* dmi_system_uuid(const unsigned char* p, short ver)
    {
        int only0xFF = 1, only0x00 = 1;
        int i;

        for (i = 0; i < 16 && (only0x00 || only0xFF); i++) {
            if (p[i] != 0x00)
                only0x00 = 0;
            if (p[i] != 0xFF)
                only0xFF = 0;
        }

        if (only0xFF || only0x00) {
            return nullptr;
        }


        char* ret = new char[UUID_LENGTH + 4];

        memset(ret, 0, (4 + UUID_LENGTH) * sizeof(char));

        if (ver >= 0x0206)
            snprintf(ret,
                     UUID_LENGTH + 3,
                     "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                     p[3],
                     p[2],
                     p[1],
                     p[0],
                     p[5],
                     p[4],
                     p[7],
                     p[6],
                     p[8],
                     p[9],
                     p[10],
                     p[11],
                     p[12],
                     p[13],
                     p[14],
                     p[15]);
        else
            snprintf(ret,
                     UUID_LENGTH + 3,
                     "-%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                     p[0],
                     p[1],
                     p[2],
                     p[3],
                     p[4],
                     p[5],
                     p[6],
                     p[7],
                     p[8],
                     p[9],
                     p[10],
                     p[11],
                     p[12],
                     p[13],
                     p[14],
                     p[15]);
        return ret;
    }

#if defined _WINDOWS_ || defined WINDOWS
    struct dmi_header {
        BYTE type;
        BYTE length;
        WORD handle;
    };

    struct RawSMBIOSData {
        BYTE ___;
        BYTE majorVersion;
        BYTE minorVersion;
        BYTE dmiRevision;
        DWORD Length;
        BYTE biosData[];
    };

    char* GetDMIUUID()
    {
        DWORD bufsize = 0;
        int ret = 0;
        RawSMBIOSData* sbios;
        dmi_header* h = NULL;
        char* uuid = nullptr;
        int flag = 1;

        ret = GetSystemFirmwareTable('RSMB', 0, 0, 0);
        if (!ret) {
            return nullptr;
        }

        bufsize = ret;

        BYTE* buf = new BYTE[bufsize + 1];

        ret = GetSystemFirmwareTable('RSMB', 0, buf, bufsize);
        if (!ret) {
            delete[] buf;
            return nullptr;
        }

        sbios = (RawSMBIOSData*)buf;

        BYTE* p = sbios->biosData;

        if (sbios->Length != bufsize - 8) {
            delete[] buf;
            return nullptr;
        }

        for (DWORD i = 0; i < sbios->Length; i++) {
            h = (dmi_header*)p;

            if (h->type == 1) {
                auto ver = (sbios->majorVersion << 8)  // * 0x100
                           + sbios->minorVersion;
                uuid = dmi_system_uuid(p + 0x8, ver);
                break;
            }

            p += h->length;

            while ((*(WORD*)p) != 0)
                p++;
            p += 2;
        }

        delete[] buf;
        return uuid;
    }

    char* GetProductID()
    {
        const char ProductID_PATH[] = "SOFTWARE\\Microsoft\\Cryptography";
        HKEY key;
        LSTATUS status = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                                       ProductID_PATH,
                                       REG_OPTION_OPEN_LINK | KEY_QUERY_VALUE,
                                       KEY_READ,
                                       &key);
        assert(status == ERROR_SUCCESS);

        char* data = new char[UUID_LENGTH + 4];
        memset(data, 0, (UUID_LENGTH + 4) * sizeof(char));

        DWORD dwSize = UUID_LENGTH + 4;
        DWORD type;

        DWORD rstatus = RegQueryValueExA(key, "MachineGuid", NULL, &type, (BYTE*)data, &dwSize);
        assert(rstatus == ERROR_SUCCESS);
        RegCloseKey(key);
        return data;
    }

    char* MachineID()
    {
        char* biosUUID = GetDMIUUID();
        char* windowsMachineID = GetProductID();
        UUID bios, product;
        auto status = UuidFromStringA((RPC_CSTR)biosUUID, &bios);
        assert(status == RPC_S_OK);
        status = UuidFromStringA((RPC_CSTR)windowsMachineID, &product);
        assert(status == RPC_S_OK);

        RPC_CSTR b, p;

        status = UuidToStringA(&bios, &b);
        assert(status == RPC_S_OK);
        status = UuidToStringA(&product, &p);
        assert(status == RPC_S_OK);

        char result[UUID_LENGTH * 2 + 2] = {0};

        size_t l = snprintf(result, UUID_LENGTH * 2 + 2, "%s+%s", b, p);

        auto sha = SHA256(result, l);

        RPC_CSTR resultUUID;

        status = UuidToStringA((UUID*)sha, &resultUUID);

        char* ret = new char[UUID_LENGTH + 2];

        snprintf(ret, UUID_LENGTH + 1, "%s", resultUUID);

        delete[] biosUUID;
        delete[] windowsMachineID;
        delete[] sha;

        RpcStringFreeA(&b);
        RpcStringFreeA(&p);
        RpcStringFreeA(&resultUUID);

        return ret;
    }
#else
    char* MachineID()
    {
        char* ret = new char[10];
        ret[0] = '.', ret[1] = '.', ret[2] = '.', ret[3] = 0;
        return ret;
    }
#endif
}  // namespace


void getComputerName(std::string& name)
{
#if defined _WINDOWS_ || defined WINDOWS
    char buffer[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    auto status = GetComputerNameA(buffer, &size);
    assert(status);
    name = buffer;
#elif defined UNIX && defined UNIX_HAVE_UNAME
    struct utsname unameObject;
    uname(&unameObject);
    name = unameObject.nodename;
#endif
}

void initGetTimestamp()
{
#if defined _WINDOWS_ || defined WINDOWS
    TIME_ZONE_INFORMATION tzInfo;
    DWORD status = GetTimeZoneInformation(&tzInfo);
    if (status >= 0) {
        auto tb = tzInfo.Bias / -60;
        if (tb > 0) {
            timeBias = "+";
            timeBias += std::to_string(tb);
        } else if (tb < 0) {
            timeBias = std::to_string(tb);
        }
    }
#endif
}

void shutdownProtobufLibrary()
{
    google::protobuf::ShutdownProtobufLibrary();
}

void getTimeStamp(std::string& ts)
{
#if defined _WINDOWS_ || defined WINDOWS
    SYSTEMTIME t;
    GetLocalTime(&t);
    const int bufferSize = 1024;
    char buffer[bufferSize];
    snprintf(buffer,
             bufferSize,
             "%4d-%02d-%02d %02d:%02d:%02d.%d%s",
             t.wYear,
             t.wMonth,
             t.wDay,
             t.wHour,
             t.wMinute,
             t.wSecond,
             t.wMilliseconds,
             timeBias.c_str());
    ts = buffer;
#else
#endif
}

protobuf::OsType getOsType()
{
#if defined _WINDOWS_ || defined WINDOWS
    return OsType::os_windows;
#elif defined UNIX
    return OsType::os_linux;
#else
#error "Bad Operation System"
#endif
}

void getOsVersion(std::string& version)
{
    std::string ret("");

#if defined _WINDOWS_ || defined _WIN32
    HKEY key;
    LSTATUS status = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                                   "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                                   REG_OPTION_OPEN_LINK | KEY_QUERY_VALUE,
                                   KEY_READ,
                                   &key);

    assert(status == ERROR_SUCCESS);


    CHAR data[100] = {0};
    DWORD dwSize = 100;
    DWORD type;
    DWORD dword;

    DWORD nRet =
        RegQueryValueExA(key, "CurrentMajorVersionNumber", NULL, &type, (BYTE*)&dword, &dwSize);
    if (nRet == ERROR_SUCCESS) {
        ret += std::to_string(dword);
        ret += ".";
    }

    nRet = RegQueryValueExA(key, "CurrentMinorVersionNumber", NULL, &type, (BYTE*)&dword, &dwSize);
    if (nRet == ERROR_SUCCESS) {
        ret += std::to_string(dword);
        ret += ".";
    }

    dwSize = 100;
    nRet = RegQueryValueExA(key, "ReleaseId", NULL, &type, (BYTE*)data, &dwSize);
    if (nRet == ERROR_SUCCESS) {
        ret += data;
        ret += ".";
    }

    dwSize = 100;
    nRet = RegQueryValueExA(key, "CurrentBuild", NULL, &type, (BYTE*)data, &dwSize);
    if (nRet == ERROR_SUCCESS) {
        ret += data;
    }

    RegCloseKey(key);
#elif defined UNIX && defined UNIX_HAVE_UNAME

    struct utsname unameObject;
    uname(&unameObject);
    ret += unameObject.release;
    ret += "(";
    ret += unameObject.machine;
    ret += ")";
    ret += unameObject.version;
#else
#error "Bad Operation System"
#endif
    version = ret;
}

void initProtobufLibrary(const std::string&)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    getComputerName(CoreMessage::_myClientName);
    getOsVersion(CoreMessage::_myOsVersion);

    auto mid = MachineID();
    CoreMessage::_myMachineID = mid;
    delete[] mid;

    initGetTimestamp();
}

size_t CompressSizeBound(size_t s)
{
    return compressBound(s);
}

bool Compress(char** out, size_t* outSize, const char* in, size_t inSize)
{
    *outSize = 0;
    auto bs = compressBound(inSize);
    uLongf os = bs;
    *out = new char[bs];
    auto ret = (Z_OK == ::compress2((Bytef*)*out, &os, (Bytef*)in, inSize, Z_BEST_COMPRESSION));
    *outSize = os;
    return ret;
}

bool DeCompress(char* out, size_t* outSize, const char* in, size_t inSize)
{
    return Z_OK == uncompress((Bytef*)out, (uLongf*)outSize, (const Bytef*)in, inSize);
}
