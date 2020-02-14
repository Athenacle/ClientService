//
// pch.h
// Header for standard system include files.
//

#pragma once

#include "gtest/gtest.h"

#include <random>
#include "protobufLib.h"

class FooEnvironment : public testing::Environment
{
public:
    virtual void SetUp()
    {
        initProtobufLibrary();
    }
    virtual void TearDown()
    {
        shutdownProtobufLibrary();
    }
};
