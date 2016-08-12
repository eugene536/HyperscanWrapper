/*
 *
TODO:
      + fix benchmark and cmake with custom file, maybe trasport this functions to TEST
      + test for |,
        &, !
      + thread tests with std::atomic_bool release/acquire
      CRU in Hyperscan
      + doxygen
      wiki about hyperscan
 *
 * */

#include <iostream>

#ifdef BENCHMARK
#  include <benchmarks.h>
#endif

#ifdef _GTEST
#  include <gtest/gtest.h>
#endif

using namespace std;

int main(int argc, char **argv) {
#ifdef BENCHMARK
    startBM();
#endif

#ifdef _GTEST
    std::cout << "Run tests..." << std::endl;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
#endif

    return 0;
}
