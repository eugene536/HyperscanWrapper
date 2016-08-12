#ifndef LINEARSEARCH_H
#define LINEARSEARCH_H

#include <set>
#include <cassert>
#include <mutex>
#include <vector>

namespace Hyperscan {

template <typename DataT>
class LinearSearch
{
public:
    LinearSearch() {}

    void Build() {}

    size_t Size() const {
        return _patterns.size();
    }

    bool Insert(const char * pattern, size_t len, const DataT& data) {
        return Insert(std::string(pattern, pattern + len), data);
    }

    bool Insert(const std::string &pattern, const DataT& data) {
        auto r = _patterns.insert(std::make_pair(pattern, data));
        return r.second;
    }

    bool Delete(const char * pattern, size_t len, const DataT& data) {
        return Delete(std::string(pattern, pattern + len), data);
    }

    bool Delete(const std::string &pattern, const DataT& data) {
        auto r = _patterns.erase(std::make_pair(pattern, data));
        return r;
    }

    std::vector<DataT> Find(const char *text, size_t len) const {
        return Find(std::string(text, text + len));
    }

    std::vector<DataT> Find(const std::string &text) const {
        std::set<DataT> res;

        for (auto& pp: _patterns) {
            const std::string& pattern = pp.first;

            if (text.find(pattern) != std::string::npos) {
                res.insert(pp.second);
            }
        }

        return std::vector<DataT>(res.begin(), res.end());
    }

private:
    std::set<std::pair<std::string, DataT>> _patterns;
};

} // StringAlgos

#endif // LINEARSEARCH_H
