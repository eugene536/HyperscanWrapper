#ifndef HYPERSCANWITHESCAPEDCHARACTER_H
#define HYPERSCANWITHESCAPEDCHARACTER_H

#include <set>
#include <map>
#include <string>
#include <Hyperscan.h>

namespace Hyperscan {

/**
 * @brief обертка над классом HyperscanWrapper которая по умолчанию экранирует паттерны.
 *
 *   Экранирует специальные символы зарезервированные за регулярными выражениями <br>
 * и преобразовывает паттерны (требуемые заказчиком) в соответствующие regexp <br>
 *
 * Ex:
 * \*bomba\* -> .*bomba.* <br>
 * bom?b?* -> bom.b..* <br>
 * #bom$ba -> \\#bom\\$ba
 *
 * @tparam DataT - тип данных которые будут возвращены если соответствующий паттерн сматчился
 */
template <typename DataT>
struct HyperscanWithEscapedCharacter : public HyperscanWrapper<DataT> {
    using HyperscanWrapper<DataT>::Insert;
    using HyperscanWrapper<DataT>::Delete;

    bool Insert(const char *pattern, size_t len, const DataT& data, Error * error = nullptr) override {
        std::string temp = CreateEscapedString(std::string(pattern, len));
        return HyperscanWrapper<DataT>::Insert(temp.c_str(), temp.size(), data, error);
    }

    bool Delete(const char *pattern, size_t len, const DataT& data, Error * error = nullptr) override {
        std::string temp = CreateEscapedString(std::string(pattern, len));
        return HyperscanWrapper<DataT>::Delete(temp.c_str(), temp.size(), data, error);
    }

private:
    /*
     * Ex: *bomba* -> .*bomba.*
     *     bom?b?* -> bom.b..*
     *     #bom$ba -> \#bom\$ba
     */
    std::string CreateEscapedString(const std::string pattern) {
        const std::set<char> specials = {'-' ,'[' ,']' ,'/' ,'{' ,'}' ,'(' ,')' ,'*' ,'+' ,'?' ,'^' ,'$' ,'|', '.', ',', '#'};

        const std::map<char, std::string> converter = {{'*', ".*"},
                                                       {'?', "."}};


        std::string res;
        res.reserve(pattern.size() * 2);

        bool escaped = false;
        for (char c: pattern) {
            if (!escaped) {
                if (c == '\\') {
                    escaped = true;
                } else if (converter.count(c)) {
                    res.append(converter.at(c));
                    continue;
                } else  if (specials.count(c)) {
                    res.push_back('\\');
                }
            } else {
                escaped = false;
            }

            res.push_back(c);
        }

        return res;
    }
};

} // StringAlgos


#endif // HYPERSCANWITHESCAPEDCHARACTER_H
