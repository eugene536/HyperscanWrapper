#ifdef _GTEST

#include <iostream>
#include <ctime>
#include <thread>
#include <map>
#include <atomic>

#include <HyperscanWithEscapedCharacter.h>
#include <PatternSearchBenchmark.h>

#include <gtest/gtest.h>

using namespace std;
using namespace Hyperscan;

namespace Tests {
    void checkManualRegexs(HyperscanWrapper<int>& ps);
    bool VectorEquivalent(const std::vector<int> & lhs, const std::vector<int> & rhs);
}

using namespace Tests;

TEST (HyperscanWithEscapedCharacter, ManualRegex) {
    HyperscanWithEscapedCharacter<int> ps;

    ps.Insert(".\\*Put.n.\\*", 0);
    ps.Insert("*Put?n*", 1);
    ps.Insert("*Boo*mba*", 2);
    ps.Insert("*\\\\te?ac?\\\\*", 3);
    ps.Insert("*\\\\te?ac?\\\\*", 4);
    ps.Insert("*z.*", 6);

    ps.Build();
    checkManualRegexs(ps);

    {
        HyperscanWithEscapedCharacter<int> hps;
        ASSERT_TRUE(hps.InsertAndBuild("*@aksoran*", 1));
        ASSERT_TRUE(hps.InsertAndBuild("*84224B*", 2));

        ASSERT_TRUE(VectorEquivalent(hps.Find("001E7384224B@aksoran.kz"), {1, 2}));

        ASSERT_TRUE(hps.DeleteAndBuild("*84224B*", 2));

        ASSERT_TRUE(VectorEquivalent(hps.Find("001E7384224B@aksoran.kz"), {1}));
    }
}

#endif
