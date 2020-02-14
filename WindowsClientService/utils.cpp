
#include <strsafe.h>

#include <cassert>
#include <codecvt>
#include <locale>
#include <string>

#include "service.h"

#ifdef _DEBUG
#include <cstdio>

void OutputDebugStringEx(LPCWSTR fmt, ...)
{
    va_list ap = NULL;
    va_start(ap, fmt);
    size_t nLen = _vscwprintf(fmt, ap) + 1;
    wchar_t* buf = new wchar_t[nLen];
    _vsnwprintf_s(buf, nLen, nLen, fmt, ap);
    va_end(ap);
    OutputDebugStringW(buf);
    delete[] buf;
}

void OutputDebugStringEx(LPCSTR fmt, ...)
{
    va_list ap = NULL;
    va_start(ap, fmt);
    size_t nLen = _vscprintf(fmt, ap) + 1;
    char* buf = new char[nLen];
    _vsnprintf_s(buf, nLen, nLen, fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
    delete[] buf;
}
#endif

LPSTR Convert(LPCWSTR in)
{
    DWORD dBufSize = WideCharToMultiByte(CP_UTF8, 0, in, -1, NULL, 0, NULL, FALSE);

    char* dBuf = new char[dBufSize];
    memset(dBuf, 0, dBufSize);

    int nRet = WideCharToMultiByte(CP_UTF8, 0, in, -1, dBuf, dBufSize, NULL, FALSE);

    return dBuf;
}

LPWSTR Convert(LPCSTR in)
{
    auto l = std::char_traits<char>::length(in);
    DWORD dBufSize = MultiByteToWideChar(CP_UTF8, 0, in, -1, NULL, 0);

    wchar_t* dBuf = new wchar_t[dBufSize];
    wmemset(dBuf, 0, dBufSize);

    int nRet = MultiByteToWideChar(CP_UTF8, 0, in, -1, dBuf, dBufSize);
    return dBuf;
}

LPCWSTR ParseSystemErrorCode(DWORD ec)
{
    const size_t BuffSize = 512;

    WCHAR Buff[BuffSize];

    DWORD ReturnSize;

    const DWORD systemFlag = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

    ReturnSize = FormatMessage(systemFlag, NULL, ec, 0, Buff, BuffSize, NULL);

    if (ReturnSize == 0) {
        return nullptr;
    } else {
        LPWSTR out = new WCHAR[ReturnSize + 2];
        auto ret = StringCchCopy(out, ReturnSize + 1, Buff);
        assert(S_OK == ret);
        return out;
    }
}

void Convert(std::wstring& out, const std::string& in)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    //std::string narrow = converter.to_bytes(wide_utf16_source_string);
    out = converter.from_bytes(in);
}
