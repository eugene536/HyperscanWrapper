#ifdef BENCHMARK
#ifndef BOOST_SCAN_H
#define BOOST_SCAN_H

#include <set>
#include <cassert>
#include <mutex>
#include <vector>
#include <boost/regex.hpp>

namespace Hyperscan {

template <typename DataT>
class BoostScan
{
public:
    BoostScan() {}

    void Build() {}

    size_t Size() const {
        return _regexs.size();
    }

    bool Insert(const char * pattern, size_t len, const DataT& data) {
        return Insert(std::string(pattern, pattern + len), data);
    }

    bool Insert(const std::string &pattern, const DataT& data) {
        boost::regex r(pattern);
        if (std::find(_regexs.begin(), _regexs.end(), r) != _regexs.end() &&
            std::distance(_regexs.begin(), std::find(_regexs.begin(), _regexs.end(), r)) ==
            std::distance(_data.begin(),   std::find(_data.begin(), _data.end(), data)))
        {
            return false;
        }

        _regexs.push_back(boost::regex(pattern));
        _data.push_back(data);

        return true;
    }

    bool Delete(const char * pattern, size_t len, const DataT& data) {
        return Delete(std::string(pattern, pattern + len), data);
    }

    bool Delete(const std::string &pattern, const DataT& data) {
        boost::regex p_r(pattern);
        for (size_t i = 0; i < _regexs.size(); ++i) {
            if (_regexs[i] == p_r && _data[i] == data) {
                _regexs.erase(_regexs.begin() + i);
                _data.erase(_data.begin() + i);
                return true;
            }
        }

        return false;
    }

    std::vector<DataT> Find(const char *text, size_t len) const {
        return Find(std::string(text, text + len));
    }

    std::vector<DataT> Find(const std::string &text) const {
        std::set<DataT> res;

        for (size_t i = 0; i < _regexs.size(); ++i)
            if (boost::regex_match(text, _regexs[i]))
                res.insert(_data[i]);

        return std::vector<DataT>(res.begin(), res.end());
    }

private:
    std::vector<boost::regex> _regexs;
    std::vector<DataT> _data;
};

} // StringAlgos

#endif // BOOST_SCAN_H
#endif // BENCHMARK
