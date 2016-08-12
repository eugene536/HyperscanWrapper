#include <iostream>
#include <fstream>
#include <algorithm>
#include <Hyperscan.h>
#include <iomanip>
#include <PatternSearchBenchmark.h>
#include <LinearSearch.h>
#include <BoostScan.h>

namespace {

using namespace std;

struct PatternHandler {
    const int ALPH_SIZE = 26;

    PatternHandler(int cnt_pat = 1000, int max_len = 100) {
        for (int i = 0; i < cnt_pat; ++i) {
            int len = rand() % max_len + 1;
            std::string p;
            p.reserve(len);

            for (int j = 0; j < len; ++j) {
                p.push_back(char('a' + rand() % ALPH_SIZE));
            }

            patterns.push_back(p);

            if (rand() % 3 == 0)
                deleted.push_back(make_pair(p, i));
        }
    }

    vector<string> patterns;
    vector<pair<string, int>> deleted;
};

PatternHandler patternHandler;


using namespace Hyperscan;
using namespace std;

template<class PatternSearchT>
void BM_INSERT() {
    PatternSearchT ps;

    double start = clock();

    for (size_t i = 0; i < patternHandler.patterns.size(); ++i) {
        ps.Insert(patternHandler.patterns[i], i);
    }

    cerr << "  BM_INSERT: " << (clock() - start) / CLOCKS_PER_SEC << endl;
}


template<class PatternSearchT>
void BM_DELETE() {
    PatternSearchT ps;

    for (size_t i = 0; i < patternHandler.patterns.size(); ++i) {
        ps.Insert(patternHandler.patterns[i], i);
    }

    double start = clock();

    for (auto & pp: patternHandler.deleted) {
        ps.Delete(pp.first, pp.second);
    }

    cerr << "  BM_DELETE: " << (clock() - start) / CLOCKS_PER_SEC << endl;
}

template<class PatternSearchT>
void BM_BUILD() {
    PatternSearchT ps;

    for (size_t i = 0; i < patternHandler.patterns.size(); ++i) {
        ps.Insert(patternHandler.patterns[i], i);
    }

    double start = clock();

    ps.Build();

    cerr << "  BM_BUILD: " << (clock() - start) / CLOCKS_PER_SEC << endl;
}

struct Generated {
    string text;
    vector<string> words;
    vector<pair<string, int>> deleted;
};

Generated generator(
    const int CNT_W = 100,
    const int MAX_LEN_WORD = 100,
    const int LEN_T = 1e7,
    const int ALPH_SIZE = 10,
    const double PERC_DELETED = 0.5)
{
    Generated res;

    string word;
    word.reserve(MAX_LEN_WORD);
    res.deleted.clear();

    for (int j = 0; j < CNT_W; ++j) {
        const int lenW = rand() % MAX_LEN_WORD + 1;
        word.clear();

        word += ".*";
        for (int k = 0; k < lenW; ++k) {
            word.push_back(rand() % ALPH_SIZE + 'a');
        }
        word += ".*";

        res.words.push_back(word);

        if (rand() % 100 < (PERC_DELETED * 100))  res.deleted.push_back({word, j});
    }

    if (res.deleted.size()) res.deleted.pop_back();

    res.text.reserve(LEN_T);
    for (int k = 0; k < LEN_T; ++k)
        res.text.push_back(rand() % ALPH_SIZE + 'a');

    return res;
}

Generated g_for_rf = generator();

template<class PatternSearchT>
void BM_RANDOM_FIND() {
    PatternSearchT ps;

    for (size_t i = 0; i < g_for_rf.words.size(); ++i) {
        ps.Insert(g_for_rf.words[i], i);
    }

    for (auto &pp : g_for_rf.deleted) {
        ps.Delete(pp.first, pp.second);
    }

    ps.Build();

    double start = clock();
    cerr << "  cnt: " << ps.Find(g_for_rf.text).size() << endl;
    cerr << "  BM_RANDOM_FIND: " << (clock() - start) / CLOCKS_PER_SEC << endl;
}

Generated g_for_1_5k;
std::vector<std::string> texts_1_5k;

template<class PatternSearchT>
void BM_PACKETS_1_5k(
    const int CNT_PACKETS = 1e4,
    const int CNT_W = 1,
    const int MAX_LEN_WORD = 30,
    const int LEN_T = 1500,
    const int ALPH_SIZE = 5,
    const double PERC_DELETED = 0) {

    if (g_for_1_5k.text.empty()) {
        g_for_1_5k = generator(CNT_W, MAX_LEN_WORD, LEN_T, ALPH_SIZE, PERC_DELETED);
    }

    PatternSearchT ps;


    for (size_t i = 0; i < g_for_1_5k.words.size(); ++i) {
        ps.Insert(g_for_1_5k.words[i], i);
    }

    for (auto &pp : g_for_1_5k.deleted) {
        ps.Delete(pp.first, pp.second);
    }

    ps.Build();

    if (texts_1_5k.empty()) {
        for (int i = 0; i < CNT_PACKETS; ++i) {
            std::string t;
            t.reserve(LEN_T);
            for (int k = 0; k < LEN_T; ++k)
                t.push_back(rand() % ALPH_SIZE + 'a');
            texts_1_5k.push_back(std::move(t));
        }
    }

    double cnt_s = 0;
    int x = 0;
    for (int i = 0; i < CNT_PACKETS; ++i) {
        double start = clock();
        x += ps.Find(texts_1_5k[i]).size();
        cnt_s += (clock() - start) / CLOCKS_PER_SEC;
    }
    std::cerr << "  BM_PACKETS_1_5k: " << x << "; time in sec: " << cnt_s << std::endl;
}

template<template <typename> class PatternSearchT>
void BMAll() {
    BM_INSERT<PatternSearchT<int>>();
    BM_DELETE<PatternSearchT<int>>();
    BM_BUILD<PatternSearchT<int>>();
    BM_RANDOM_FIND<PatternSearchT<int>>();
    BM_PACKETS_1_5k<PatternSearchT<int>>();

    PatternSearchBenchmark<PatternSearchT> psb;

    psb
        .ReadFile("resources/wp")
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
            ".*Pierre.*",
            ".*ri..on.*",
            ".*.ap.leon.*"})
        .BenchmarkFind({0, 1, 2, 13, 14, 15});
}
} // anonymous namespace

void startBM() {
#ifdef BENCHMARK
    cerr << "Hyperscan" << endl;
    BMAll<HyperscanWrapper>();
    cerr << "BoostScan" << endl;
    BMAll<BoostScan>();
#endif
}
