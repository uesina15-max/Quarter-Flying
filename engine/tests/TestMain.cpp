#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>
#include <iostream>

int main(int argc, char* argv[])
{
    std::cout << "Running Engine Tests (Unit + Property-Based)..." << std::endl;
    
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Run all tests
    return RUN_ALL_TESTS();
}
