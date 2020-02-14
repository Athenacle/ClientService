#include "pch.h"

#include <string>

#if defined WINDOWS || defined _WINDOWS_
#pragma comment(lib, "WindowsProtobufLib.lib")
#pragma comment(lib, "WS2_32.lib")
#endif

using namespace protobuf;

using CH = char;

unsigned int random_value()
{
    static std::random_device rd;
    return rd();
}

const CH *random_string(int len = 20)
{
    size_t actual_size;
    if (len <= 0) {
        actual_size = 10 + random_value() % 10;
    } else {
        actual_size = static_cast<size_t>(len);
    }
    CH *buffer = new CH[actual_size + 1];
    for (size_t t = 0; t < actual_size; t++) {
        buffer[t] = random_value() % ('z' - 'a') + 'a';
    }
    buffer[actual_size] = '\0';
    return buffer;
}

TEST(protobufLib, encode1)
{
    LogPackage l;
    char *buf;
    size_t s;
    l.toBytes(&buf, &s);

    ProtobufPacketDecoder decoder;
    decoder.read(buf, s);
    EXPECT_TRUE(decoder.GetSize() == 1);

    decoder.Reset();
    delete[] buf;
}

CoreMessage *buildLargePackage()
{
    auto evts = random_value() % 50 + 50;
    LogPackage *lp = new LogPackage;
    const CH *p;
    lp->Description(std::string(p = random_string()));
    for (uint32_t i = 0; i < evts; i++) {
        auto xml = random_string();
        auto msg = random_string(50);
        auto times = random_string(10);
        auto f = random_string(20);
        Event e(std::string(xml),
                std::string(msg),
                std::string(times),
                std::string(f),
                random_value() % 5,
                random_value());

        lp->AddLogEvent(std::move(e));
        delete[] xml;
        delete[] msg;
        delete[] times;
        delete[] f;
    }
    delete[] p;
    return lp;
}

TEST(protobufLib, encode2)
{
    LogPackage l;
    char *buf;
    size_t s;
    l.toBytes(&buf, &s);

    ProtobufPacketDecoder decoder;

    for (size_t i = 0; i < s; i++) {
        EXPECT_TRUE(decoder.GetSize() == 0);
        decoder.read(buf + i, 1);
    }
    EXPECT_TRUE(decoder.GetSize() == 1);

    decoder.Reset();
    delete[] buf;
}


TEST(protobufLib, encode3)
{
    CoreMessage *p = buildLargePackage();
    CoreMessage *r = buildLargePackage();

    char *buf1, *buf2, *buf3;
    size_t s1, s2;


    p->toBytes(&buf1, &s1);
    r->toBytes(&buf2, &s2);

    size_t s3 = (int32_t)s1 + s2;

    buf3 = new char[s3];
    memcpy(buf3, buf1, s1);
    memcpy(buf3 + s1, buf2, s2);

    ProtobufPacketDecoder decoder;

    decoder.read(buf3, s3);
    EXPECT_EQ(decoder.GetSize(), 2);
    decoder.Reset();

    std::random_device rd;

    int offset = rd() % (s3);

    decoder.read(buf3, offset);
    decoder.read(buf3 + offset, s3 - offset);

    EXPECT_TRUE(decoder.GetSize() == 2);

    decoder.Reset();
    for (size_t i = 0; i < s3; i++) {
        decoder.read(buf3 + i, 1);
    }

    EXPECT_TRUE(decoder.GetSize() == 2);

    delete[] buf1;
    delete[] buf2;
    delete[] buf3;

    delete p;
    delete r;

    decoder.Reset();
}

#if !defined _WIN32 || !defined _WIN64
int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    initProtobufLibrary();
    int ret = RUN_ALL_TESTS();
    shutdownProtobufLibrary();
    return ret;
}
#endif