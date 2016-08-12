#ifdef _GTEST

#include <iostream>
#include <ctime>
#include <thread>
#include <map>
#include <atomic>

#include <Hyperscan.h>
#include <PatternSearchBenchmark.h>
#include "LinearSearch.h"

#include <gtest/gtest.h>

using namespace std;
using namespace Hyperscan;

namespace Tests {

bool VectorEquivalent(const std::vector<int> & lhs, const std::vector<int> & rhs) {
    std::set<int> ls(lhs.begin(), lhs.end());
    std::set<int> rs(rhs.begin(), rhs.end());

    return ls == rs;
}

void randomTest(bool withMultiThreading = false) {
    const int MAX_LEN_TEXT = 10000,
              MAX_LEN_WORD = 100,

              CNT_TESTS = 100,
              MAX_CNT_TEXTS = 100,
              MAX_CNT_WORDS = 100,

              ALPH_SIZE = 10;

    string word;
    word.reserve(MAX_LEN_WORD);

    string text;
    text.reserve(MAX_LEN_TEXT);

    vector<pair<string, int>> deleted;

    for (int i = 0; i < CNT_TESTS; ++i) {
        LinearSearch<int> ls;
        HyperscanWrapper<int> ps;

        const int cntWords = rand() % MAX_CNT_WORDS + 1;
        const int cntTexts = rand() % MAX_CNT_TEXTS + 1;
//        cerr << "test #" << i << "; words: " << cntWords << "; texts: " << cntTexts;

        deleted.clear();
        for (int j = 0; j < cntWords; ++j) {
            const int lenW = rand() % MAX_LEN_WORD + 1;
            word.clear();

            for (int k = 0; k < lenW; ++k) {
                word.push_back(rand() % ALPH_SIZE + 'a');
            }

            ASSERT_TRUE(ls.Insert(word, j));
            ASSERT_TRUE(ps.Insert(".*" + word + ".*", j));

            if (rand() % 2)  deleted.push_back({word, j});
        }

        if (deleted.size()) deleted.pop_back();

        for (pair<string, int>& p: deleted) {
            ASSERT_TRUE(ls.Delete(p.first, p.second));
            ASSERT_TRUE(ps.Delete(".*" + p.first + ".*", p.second));
        }

        ls.Build();
        ps.Build();

        ASSERT_EQ(ps.Size(), ls.Size());
        ASSERT_EQ(ps.Size(), cntWords - deleted.size());

        std::vector<std::thread> threads;
        for (int j = 0; j < cntTexts; ++j) {
            const int lenT = rand() % MAX_LEN_TEXT + 1;
            text.clear();

            for (int k = 0; k < lenT; ++k) {
                text.push_back(rand() % ALPH_SIZE + 'a');
            }

            if (withMultiThreading) {
                threads.emplace_back([text, &ps, &ls]() {
                    ASSERT_TRUE(VectorEquivalent(ps.Find(text), ls.Find(text)));
                });
            } else {
                ASSERT_TRUE(VectorEquivalent(ps.Find(text), ls.Find(text)));
            }
        }

        for (thread& t: threads) {
            t.join();
        }
    }
}

void checkManualRegexs(HyperscanWrapper<int>& ps) {
    {
        std::vector<int> res;
        ASSERT_TRUE(VectorEquivalent(ps.Find("za"), res));
    }

    {
        std::vector<int> res{6};
        ASSERT_TRUE(VectorEquivalent(ps.Find("z."), res));
    }

    {
        std::vector<int> res;
        ASSERT_TRUE(VectorEquivalent(ps.Find("asd fasd fasd fasdf asdfa sd"), res));
    }

    {
        std::vector<int> res{1};
        ASSERT_TRUE(VectorEquivalent(ps.Find("Putin"), res));
    }

    {
        std::vector<int> res;
        ASSERT_TRUE(VectorEquivalent(ps.Find("\\teeeeeract\\"), res));
    }

    {
        std::vector<int> res{3, 4};
        ASSERT_TRUE(VectorEquivalent(ps.Find("\\teract\\"), res));
    }

    {
        std::vector<int> res{1, 3, 4, 2};
        ASSERT_TRUE(VectorEquivalent(ps.Find("ads as dasd PutAn asd fas d\\teRacI\\ Booooooomba"), res));
    }

    {
        std::vector<int> res{1, 2};
        ASSERT_TRUE(VectorEquivalent(ps.Find("ads as dasd PutAn asd fas Booooooomba \\teraCTTT\\"), res));
    }

    {
        std::vector<int> res{1, 2, 0};
        ASSERT_TRUE(VectorEquivalent(ps.Find("ads as dasd PutAn .*Put.n.* fas Booooooomba \\teraCTTT\\"), res));
    }
}

void WorstCaseTest(bool notMatchTest = false) {
    HyperscanWrapper<int> ps;
    int CNT_W = 1e3;
    int LEN_T = 1e8;

    if (notMatchTest) { // TrieSearch so slow in this case, O(max_len * |text|) ~ (1e3 * 1e6) iterations
        LEN_T = 1e6;
    }

    std::string a;
    a.reserve(CNT_W);
    a.append(".*");
    for (int i = 0; i < CNT_W; ++i) {
        a.push_back('a');
        if (notMatchTest) {
            ps.Insert(a + "b", i);
        } else {
            ps.Insert(a, i);
        }
    }
    a.append(".*");

    std::string text;
    text.reserve(LEN_T);
    for (int i = 0; i < LEN_T; ++i) {
        text.push_back('a');
    }

    ps.Build();

    double start = clock();
    auto res = ps.Find(text);

#ifdef BENCHMARK
    std::cerr << "worst case: " << ps.Size() << " " << text.size() << std::endl;
    std::cerr << "WorstCase find time: " << (clock() - start) / CLOCKS_PER_SEC << std::endl;
#endif

    if (notMatchTest) {
        ASSERT_EQ(res.size(), 0);
    } else {
        ASSERT_EQ(res.size(), CNT_W);
        int i = 0;
        for (int r: res) {
            ASSERT_EQ(r, i++);
        }
    }
}

} // namespace Tests

using namespace Tests;

TEST (HyperscanWrapper, BinaryData) {
    char const * pat1 = "..\xff\x89\x12\xf0.";
    char const * pat2 = ".asd";
    char const * pat3 = "\xff\x89\x12\xf0u";

    char const * text = "asd fasd\xf9  \xff\x89\x12\xf0 \x09 asdf asd f24   23 ";
    HyperscanWrapper<int> ps;

    ps.InsertAndBuild(pat1, strlen(pat1), 999);
    ps.InsertAndBuild(pat2, strlen(pat2), 777);
    ps.InsertAndBuild(pat3, strlen(pat3), 111);

    auto res = ps.Find(text, strlen(text));
    ASSERT_TRUE(VectorEquivalent(res, {999, 777}));
}

TEST (HyperscanWrapper, ManualInsertDelete) {
    HyperscanWrapper<int> ps;

    vector<pair<string, int>> dict {
        {"abcd", 11},
        {"abcd", 1},
        {"abc", 2},
        {"abce", 3},
        {"bcu", 4},
        {"bcdf", 5},
        {"bcde", 6},
        {"Z", 7},
        {"A", 8},
        {"AA", 9},
        {"AAA", 10}};

    for (auto & pp : dict)
        ASSERT_TRUE(ps.Insert(".*" + pp.first + ".*", pp.second));

    ASSERT_FALSE(ps.Insert(".*" + dict.front().first + ".*", dict.front().second));

    ps.Build();

    ASSERT_EQ(ps.Size(), 11);

    {
        std::vector<std::pair<std::string, std::vector<int>>> texts = {
            {"abcu", {2, 4}},
            {"abcd", {1, 11, 2}},
            {"abcd", {1, 11, 2}},
            {"bcdf", {5}},
            {"abceabcdfe", {2, 3, 1, 11, 5}},
            {"abceabcdfebcobcpabpbcdebcobcupp", {1, 11, 2, 3, 4, 5, 6}},
            {"ZZZZZZZZZZZZZZZZZZZZZZZZ", {7}},
            {"AAAAAAAAAAAAAAAAAAAAAAA", {8, 9, 10}},
            {"AA", {8, 9}},
            {"A", {8}}
        };

        for (auto pp: texts) {
            ASSERT_TRUE(VectorEquivalent(ps.Find(pp.first), pp.second));
        }
    }

    ASSERT_TRUE(ps.Delete(".*bcu.*", 4));
    ASSERT_FALSE(ps.Delete(".*bcu.*", 4));

    ASSERT_TRUE(ps.Delete(".*abcd.*", 1));
    ASSERT_TRUE(ps.Delete(".*A.*", 8));
    ps.Build();

    ASSERT_EQ(ps.Size(), 8);

    {
        std::vector<std::pair<std::string, std::vector<int>>> texts = {
            {"abcu", {2}},
            {"abcd", {2, 11}},
            {"bcdf", {5}},
            {"abceabcdfe", {2, 3, 5, 11}},
            {"abceabcdfebcobcpabpbcdebcobcupp", {11, 2, 3, 5, 6}},
            {"ZZZZZZZZZZZZZZZZZZZZZZZZ", {7}},
            {"AAAAAAAAAAAAAAAAAAAAAAA", {9, 10}},
            {"AA", {9}},
            {"A", {}}
        };

        for (auto pp: texts) {
            ASSERT_TRUE(VectorEquivalent(ps.Find(pp.first), pp.second));
        }
    }
}

TEST(HyperscanWrapper, DeleteAllPatterns) {
    {
        HyperscanWrapper<int> ps2;
        {
            ASSERT_TRUE(ps2.Insert(".*a.*", 1));
            ps2.Insert(".*b.*", 2);
            ps2.Build();
            std::vector<int> res {1};
            ASSERT_TRUE(VectorEquivalent(ps2.Find(";lkj;lkja"), res));
        }

        {
            ASSERT_TRUE(ps2.Delete(".*a.*", 1));
            ps2.Delete(".*b.*", 2);
            ps2.Build();
            std::vector<int> res;
            ASSERT_TRUE(VectorEquivalent(ps2.Find(";lkj;lkja"), res));
            ASSERT_EQ(ps2.Size(), 0);
        }
    }

    {
        HyperscanWrapper<int> ps2;
        ps2.Insert(".*abc.*", 1);
        ps2.Insert(".*abcde.*", 2);

        ps2.Delete(".*abcde.*", 2);
        ps2.Delete(".*abc.*", 1);
        ps2.Build();

        std::vector<int> res;
        ASSERT_TRUE(VectorEquivalent(ps2.Find("abc"), res));
        ASSERT_EQ(ps2.Size(), 0);
    }
}

TEST (HyperscanWrapper, ManualRegex) {
    HyperscanWrapper<int> ps;

    ps.Insert("\\.\\*Put\\.n\\.\\*", 0);
    ps.Insert(".*Put.n.*", 1);
    ps.Insert(".*Boo.*mba.*", 2);
    ps.Insert(".*\\\\te.ac.\\\\.*", 3);
    ps.Insert(".*\\\\te.ac.\\\\.*", 4);
    ps.Insert(".*z\\..*", 6);
    ps.Build();

    checkManualRegexs(ps);
}


TEST (HyperscanWrapper, Random) {
    randomTest();
}

TEST (HyperscanWrapper, SimpleMultiThreading) {
    randomTest(true);
}

// swmr - Single Writer Multiple Reader test
TEST (HyperscanWrapper, manualSwmrThreading) {
    HyperscanWrapper<int> ps;

    vector<pair<string, int>> v {
        {"abcd", 1},
        {"abc", 2},
        {"abce", 3},
        {"bcdf", 5},
        {"bcde", 6},
        {"A", 8},
        {"AAA", 10}};

    for (auto & pp : v)
        ASSERT_TRUE(ps.Insert(pp.first, pp.second));
    ps.Build();

    std::vector<int> res{1, 2, 8, 10, 6};

    char const * text = "bcu abcd AA Z AAA bcdef";
    ASSERT_TRUE(VectorEquivalent(ps.Find(text), res));

    std::atomic<int> cnt_readed(0);
    std::atomic<int> version(0);

    const int CNT_READERS = 10;

    std::thread writer([&ps, &cnt_readed]() {
        ps.Insert("bcu", 4);
        ps.Build();
        while (cnt_readed.load(std::memory_order_acquire) != CNT_READERS);
        cnt_readed.store(0);

        ps.Insert("abcd", 11);
        ps.Insert("Z", 7);
        ps.Insert("AA", 9);
        ps.Build();
        while (cnt_readed.load(std::memory_order_acquire) != CNT_READERS);
        cnt_readed.store(0);

        ps.Delete("abcd", 1);
        ps.Delete("abc", 2);
        ps.Build();
    });

    std::vector<std::thread> readers;
    for (int i = 0; i < CNT_READERS; ++i) {
        readers.emplace_back([&ps, res, text, &cnt_readed]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            std::vector<int> r(res);
            r.push_back(4);
            ASSERT_TRUE(VectorEquivalent(ps.Find(text), r));

            cnt_readed.fetch_add(1, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::seconds(1));

            r.push_back(11);
            r.push_back(7);
            r.push_back(9);
            ASSERT_TRUE(VectorEquivalent(ps.Find(text), r));

            cnt_readed.fetch_add(1, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            r.erase(std::find(r.begin(), r.end(), 1));
            r.erase(std::find(r.begin(), r.end(), 2));

            ASSERT_TRUE(VectorEquivalent(ps.Find(text), r));
        });
    }

    for (int i = 0; i < CNT_READERS; ++i) {
        readers[i].join();
    }

    writer.join();
}

TEST(HyperscanWrapper, StressMultithreading) {

}

TEST (HyperscanWrapper, WorstCase) {
    WorstCaseTest();
}

TEST (HyperscanWrapper, WorstCaseNotMatch) {
    WorstCaseTest(true);
}

// created only for example of usage PatternSearchBenchmark class
TEST (HyperscanWrapper, FromFile) {
    PatternSearchBenchmark<HyperscanWrapper> psb;

    psb
        .ReadFile("resources/war_peace")
        .SetPatterns({
            ".*CHAPTER.*",
            ".*reward.*",
            ".*Pierre.*",
            ".*asdfasdf.*",
            ".*HelloAAA.*",
            ".*AHello.*",
            ".*Helloo.*",
            ".*123213.*",
            ".*rewardu.*",
            ".*qewrqwer.*",
            ".*iuzxycv.*",
            ".*CHAPTERX.*",
            ".*8762183476218934.*",
            ".*Pierre1*",
            ".*ri..on",
            ".*.ap.leon"})
        .BenchmarkFind({0, 1, 2, 13, 14, 15});
}

TEST (HyperscanWrapper, Or) {
    HyperscanWrapper<int> hw;

    hw.Insert("Putin|teract", 0);
    hw.Insert("Putin|teract", 1);
    hw.Insert("IOI_239", 2);
    hw.Insert("^Putil", 3);
    hw.Build();

    ASSERT_TRUE(VectorEquivalent(hw.Find("Putin teract"), {0, 1}));
    ASSERT_TRUE(VectorEquivalent(hw.Find("teract"), {0, 1}));
    ASSERT_TRUE(VectorEquivalent(hw.Find("Putin"), {0, 1}));
    ASSERT_TRUE(VectorEquivalent(hw.Find("tdsfasdf asdf asdfteractfdIOI_239 asd fasd fasd"), {0, 1, 2}));
    ASSERT_TRUE(VectorEquivalent(hw.Find("adsfa sf asdfPutin teract asdf asdf asdf "), {0, 1}));
    ASSERT_TRUE(VectorEquivalent(hw.Find("zxcpvuiqPutilwe r;asdlfk a.cv),m as;lei"), {}));
    ASSERT_TRUE(VectorEquivalent(hw.Find("Putilasdfasd IOI_239 fxczv xcv teruct"), {2, 3}));
    ASSERT_TRUE(VectorEquivalent(hw.Find("terractPuutin123 123 123"), {}));
}

#endif
