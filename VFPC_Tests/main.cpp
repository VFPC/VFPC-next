// main.cpp
// Single entry point for the VFPC_Tests harness.
// All test files register their suites via Google Test's global registry;
// this file just initialises GTest and runs them.

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
