#ifndef PATTERN_SEARCH_BENCHMARK_H
#define PATTERN_SEARCH_BENCHMARK_H
#include <iostream>
#include <vector>
#include <set>
#include <fstream>
#include <ctime>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

template<typename T>
std::ostream& operator<<(std::ostream& out, const std::set<T>& s) {
    out << "{";
    for (const T& t: s)
        out << t << " ";
    return out << "}";
}

// This class intended for simplifying testing of Hyperscan with files
template<template <typename> class PatternSearchT>
class PatternSearchBenchmark {
public:
    PatternSearchBenchmark & ReadFile(std::string path = "resources/war_peace") {
        std::ifstream file(path);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            std::cerr << "CAN'T FIND FILE: " << path << std::endl;
            exit(1);
        }

        struct stat st;
        fstat(fd, &st);
        char * text = (char*) malloc(st.st_size);
        read(fd, text, st.st_size);

        _text = std::string(text, st.st_size);

        free(text);

        return *this;
    }


    PatternSearchBenchmark & SetPatterns(std::vector<std::string> p) {
        _patterns = p;
        return *this;
    }

    void BenchmarkFind(const std::vector<int> & ans) const {
        if (_patterns.empty()) {
            return;
        }

        PatternSearchT<int> ps;

        for (size_t i = 0; i < _patterns.size(); ++i) {
            ps.Insert(_patterns[i], i);
        }
        ps.Build();

#ifdef BENCHMARK
        double start = clock();
#endif

        std::vector<int> res = ps.Find(_text);

#ifdef BENCHMARK
        std::cerr << "  BM_FIND: " << (clock() - start) / CLOCKS_PER_SEC << std::endl;
#endif

        std::set<int> ls(res.begin(), res.end());
        std::set<int> rs(ans.begin(), ans.end());

        if (ls != rs) {
            std::cerr << "expected: " << rs << std::endl;
            std::cerr << "given: " << ls << std::endl;
            std::cerr << "find doesn't work :(" << std::endl;
//            std::cerr << "text: " << _text << std::endl;

            std::cerr << "patterns: {";
            for (auto& s : _patterns) std::cerr << s << ", ";
            std::cerr << "}" << std::endl;

            exit(1);
        }
    }


private:
    std::vector<std::string> _patterns;
    std::string _text;
};

#endif
