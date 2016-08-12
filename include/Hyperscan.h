#ifndef HYPERSCAN_H
#define HYPERSCAN_H

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <queue>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <thread>
#include <mutex>

#include <hs.h>

/**
 * @defgroup Hyperscan
 * @brief Обертка над high-performance библиотекой \a %Hyperscan, которая позволяет находить множество регулярных выражений в тексте
 *
 *   Класс Hyperscan::HyperscanWrapper реализовывает основную функциональность. <br>
 * Поддерживает большую часть синтаксиса PCRE.
 *
 * @see @link Hyperscan::HyperscanWrapper::Insert(const char *, size_t, const DataT&, Error *) Insert @endlink
 * @see @link Hyperscan::HyperscanWrapper::Delete(const char *, size_t, const DataT&, Error *) Delete @endlink
 * @see @link Hyperscan::HyperscanWrapper::Build Build @endlink
 * @see @link Hyperscan::HyperscanWrapper::Find(const char *, size_t, Error *) const Find @endlink
 *
 * @file Hyperscan.h
 * @author Nemchenko Eugene
 * @date 2/08/2016
 */

/**
 *  @addtogroup Hyperscan
 *  @{
 */

//! Содержит классы для работы с библиотекой %Hyperscan и обработки ошибок
namespace Hyperscan {

/**
 * @brief enum обозначающий код ошибки
 */
enum ErrorCode : int {
    SUCCESS = 0,                //!< ошибка не произошла
    PATTERN_AND_DATA_IN_USE,    //!< в HyperscanWrapper::Insert, пара (pattern, data) уже добавлялась ранее
    PATTERN_AND_DATA_NOT_FOUND, //!< в HyperscanWrapper::Delete, не найдена пара (pattern, data)

    /**
      * @see Error::GetBadPattern
      * @see Error::GetErrorMessage
      */
    BUILD_ERROR,                //!< В HyperscanWrapper::Build, (не корректная регулярка, нет памяти и т.п.)

    SCAN_ERROR,                 //!< в HyperscanWrapper::Find, не знаю примера, чтобы данная ошибка произошла
    NO_MEMORY                   //!< в HyperscanWrapper::Find, HyperscanWrapper::Build, память закончилась
};


/**
 * @brief Для хранения ошибок, используется в методах HyperscanWrapper
 */
struct Error {
    /**
     * @brief инициализирует код ошибки значением SUCCESS
     */
    Error()
        : _code(ErrorCode::SUCCESS)
    {}

    /**
     * @brief возвращает паттерн который не смог скомпилироваться в библиотеке Hyperscan, только если Error::GetErrorCode() == ErrorCode::BUILD_ERROR
     */
    std::string GetBadPattern() {
        return _pattern;
    }

    /**
     * @brief возвращает текстовое представление ошибки
     */
    std::string GetErrorMessage() {
        switch (_code) {
            case ErrorCode::SUCCESS:
                return "all right";
            case ErrorCode::PATTERN_AND_DATA_IN_USE:
                return "(pattern, data) was previously added to database";
            case ErrorCode::PATTERN_AND_DATA_NOT_FOUND:
                return "(pattern, data) has never been added to database";
            case ErrorCode::SCAN_ERROR:
                return "Unable to scan input buffer";
            case ErrorCode::NO_MEMORY:
                return "not enough memory";
            default:
                return _message;
        }
    }

    /**
     * @return возвращает код ошибки
     */
    ErrorCode GetErrorCode() {
        return _code;
    }

private:
    explicit Error(ErrorCode code)
        : _code(code)
    {}

    Error(ErrorCode code, char const * pattern)
        : _code(code)
        , _pattern(pattern)
    {}

    Error(ErrorCode code, char const * pattern, char const * message)
        : _code(code)
        , _pattern(pattern)
        , _message(message)
    {}

private:
    ErrorCode _code;
    std::string _pattern;
    std::string _message;

private:
    template<typename DataT>
    friend class HyperscanWrapper;
};

/**
 * @brief обертка над библиотекой \a %Hyperscan
 * @tparam DataT - тип данных которые будут возвращены если соответствующий паттерн сматчился
 */
template <typename DataT>
class HyperscanWrapper {
private:
    struct Context {
        std::vector<DataT> * res;
        const std::vector<DataT> * data;
    };

    /**
     * @brief RAII класс над скомпилированной базой данных и scratch (изменяемая память выделямая для поиска в тексте)
     */
    class DatabaseWrapper {
    public:
        /**
         * @brief создает базу данных и сохраняет соответствующие данные, как бы снэпшот на текущий Build
         *
         *   В базу данных добавляются паттерны по порядку, с айдишниками соответсвенно 0, 1, 2 ... <br>
         * когда hs_scan вернет мне айдишник я смогу понять какие данные ему соответствуют <br>
         * данные нужно копировать т.к. они изменяются в другом потоке могут удаляться и добавляться <br>
         * HyperscanWrapper::Find захватывает указатель на DatabaseWrapper с ним нужно сохранить и данные <br>
         * поэтому я их и сохраняю в этом классе.
         *
         * @param patterns[in] паттерны добавленный пользователем
         * @param data[in] данные соответствующие паттернам
         * @param error[out] указатель на класс ошибки, заполняемый в случае неудачи
         */
        DatabaseWrapper(const std::vector<char *>& patterns, const std::vector<DataT>& data, Error * error = nullptr)
            : data(data)
        {
            assert(!patterns.empty());

            // flags is a vector = {HS_FLAG_SINGLEMATCH, HS_FLAG_SINGLEMATCH, ...} n times
            // ids = 1..n
            // n = max of _patterns.size() from all instances of Hyperscan
            static std::vector<unsigned> flags;
            static std::vector<unsigned> ids;

            // if we inserted new patterns to `_paterrns` from previous Init call
            if (patterns.size() > flags.size()) {
                int prev_sz = flags.size();

                flags.resize(patterns.size());
                ids.resize(patterns.size());

                std::fill_n(flags.begin() + prev_sz, flags.size() - prev_sz, HS_FLAG_SINGLEMATCH);
                std::iota(ids.begin() + prev_sz, ids.end(), prev_sz);
            }

            static const unsigned int mode = HS_MODE_BLOCK;

            hs_compile_error_t * compileErr;
            hs_error_t err = hs_compile_multi(patterns.data(), flags.data(), ids.data(),
                                              patterns.size(), mode, nullptr, &db, &compileErr);

            if (err != HS_SUCCESS) {
                if (error) {
                    *error = compileErr->expression < 0
                             ? Error(ErrorCode::BUILD_ERROR, compileErr->message, "")
                             : Error(ErrorCode::BUILD_ERROR, compileErr->message, patterns[compileErr->expression]);
                }


                // As the compileErr pointer points to dynamically allocated memory, if
                // we get an error, we must be sure to release it. This is not
                // necessary when no error is detected.
                hs_free_compile_error(compileErr);
                db = nullptr;
            }

            if (db) {
                if (hs_alloc_scratch(db, &scratch) != HS_SUCCESS) {
                    if (error) {
                        assert(err == HS_NOMEM);
                        *error = Error(ErrorCode::NO_MEMORY);
                    }

                    hs_free_database(db);
                    scratch = nullptr;
                    db = nullptr;
                }
            }
        }

        /**
          * освобождает память базы данный и scratch
          */
        ~DatabaseWrapper() {
            hs_free_database(db);
            hs_free_scratch(scratch);
        }

        /**
         * @brief скомпилируемая база данных на основе добавленных паттернов
         */
        hs_database_t * db = nullptr;

        /**
         * @brief выделенная память для hs_scan, который будет ее изменять для внутренних целей,
         *        необходимо своя для каждого потока,
         */
        hs_scratch_t * scratch = nullptr;

        /**
         * @brief пользовательские данные соответствующие паттернам
         */
        std::vector<DataT> data;
    };

    /**
     * @brief RAII обертка над hs_scracth_t, хранит склонированный scratch
     */
    struct ScratchWrapper {
        /**
         * @brief ScratchWrapper клонирует \a s и заполняет \a *error в случае неудачи
         * @param s целевой scratch который будет склонирован
         * @param error указатель на ошибку
         */
        ScratchWrapper(hs_scratch_t * s, Error * error = nullptr) {
            hs_error_t err = hs_clone_scratch(s, &scratch);

            if (err != HS_SUCCESS) {
                scratch = nullptr;

                if (error) {
                    assert(err == HS_NOMEM);
                    *error = Error(ErrorCode::NO_MEMORY);
                }
            }
        }

        /**
         * @brief ~ScratchWrapper освобождает склонированную память
         */
        ~ScratchWrapper() {
            hs_free_scratch(scratch);
        }

        hs_scratch_t * scratch = nullptr;
    };

public:
    /**
      * @brief ~HyperscanWrapper удаляет все паттерны которые были скопированны во время добавления
      */
    ~HyperscanWrapper() {
        for (char* c: _patterns) {
            delete[] c;
        }
    }

    /**
     * @brief компилирует все добавленные паттерны, обязательно вызывать после HyperscanWrapper::Insert и HyperscanWrapper::Delete
     * @param[out] error указатель на класс ошибки, здесь бывают осмысленные ошибки вида "неправильный паттерн"
     * @return true - в случае успеха, false - в случае ошибки подробности в переменной \a error
     */
    bool Build(Error * error = nullptr) {
        Error local_error;
        std::shared_ptr<DatabaseWrapper> dw;

        if (!_patterns.empty()) {
            dw = std::make_shared<DatabaseWrapper>(_patterns, _data, &local_error);

            if (local_error.GetErrorCode()) {
                if (error) *error = local_error;
                return false;
            }
        }

        _m.lock();
        _dw = dw;
        _m.unlock();

        return true;
    }

    /**
     * @brief возвращает текущее кол-во паттернов
     */
    size_t Size() const {
        return _patterns.size();
    }

    /**
     * @see Insert(const char *, size_t, const DataT&, Error *)
     */
    bool Insert(const std::string &pattern, const DataT& data, Error * error = nullptr) {
        return Insert(pattern.c_str(), pattern.size(), data, error);
    }

    /**
     * @brief добавляет (паттерн, данные), необходимо после вызвать HyperscanWrapper::Build.
     *
     *   Можно добавлять один и тот же паттерн с разными данными все они будут возвращены в случае успеха <br>
     * например: Insert("bomba", 1) -> Insert("bomba", 2) -> Build() -> Find("bomba") -> {1, 2} <br> <br>
     * можно добавлять разные паттерны с одинаковыми айдишниками, но после файнда они будут не различимы <br>
     * например: Insert("bomba", 1) -> Insert("Putin", 1) -> Build() -> Find("bomba Putin") -> {1, 1} <br> <br>
     * в случае:  Insert("bomba", 1) -> Insert("bomba", 1) -> ErrorCode::PATTERN_AND_DATA_IN_USE <br>
     *
     * @remark single writer
     * @param[in] pattern указатель на начало паттерна
     * @param[in] len  длина паттерна
     * @param[in] data данные которые будут возвращены, если данный паттерн сматчился в тексте
     * @param[out] error может быть записано ErrorCode::PATTERN_AND_DATA_IN_USE
     * @return true в случае успеха, false в случае неудачи смотри \a error
     */
    virtual bool Insert(const char *pattern, size_t len, const DataT& data, Error * error = nullptr) {
        if (error) *error = Error();

        for (size_t i = 0; i < _patterns.size(); ++i) {
            if (strcmp(_patterns[i], pattern) == 0 && _data[i] == data) {
                if (error) *error = Error(ErrorCode::PATTERN_AND_DATA_IN_USE, pattern);
                return false;
            }
        }

        char * cpy = new char[len + 1];
        strncpy(cpy, pattern, len + 1);
        _patterns.push_back(cpy);
        _data.push_back(data);

        return true;
    }

    /**
     * @see Delete(const char *, size_t, const DataT&, Error *)
     */
    bool Delete(const std::string &pattern, const DataT& data, Error * error = nullptr) {
        return Delete(pattern.c_str(), pattern.size(), data, error);
    }

    /**
     * @brief удаляет (паттерн, данные), необходимо после вызвать HyperscanWrapper::Build.
     *
     *   В случае:  Insert("bomba", 1) -> Delete("bomba", 1) -> Delete("bomba", 1) -> ErrorCode::PATTERN_AND_DATA_NOT_FOUND
     * @remark single writer
     * @param[in] pattern указатель на начало паттерна
     * @param[in] len  длина паттерна
     * @param[in] data данные которые будут возвращены, если данный паттерн сматчился в тексте
     * @param[out] error может быть записано ErrorCode::PATTERN_AND_DATA_IN_USE
     * @return true в случае успеха, false в случае неудачи смотри \a error
     */
    virtual bool Delete(const char *pattern, size_t len, const DataT& data, Error * error = nullptr) {
        if (error) *error = Error();

        for (size_t i = 0; i < _patterns.size(); ++i) {
            if (strcmp(_patterns[i], pattern) == 0 && _data[i] == data) {

                std::swap(_patterns[i], _patterns.back());
                std::swap(_data[i], _data.back());

                delete[] _patterns.back();
                _patterns.pop_back();
                _data.pop_back();

                return true;
            }
        }

        if (error) *error = Error(ErrorCode::PATTERN_AND_DATA_NOT_FOUND);
        return false;
    }

    /**
     * @see InsertAndBuild(char const *, size_t, const DataT&, Error *)
     */
    bool InsertAndBuild(const std::string& pattern, const DataT& data, Error * error = nullptr) {
        return InsertAndBuild(pattern.c_str(), pattern.size(), data, error);
    }

    /**
     * @brief Insert + Build
     * @param[in] pattern указатель на начало паттерна
     * @param[in] len  длина паттерна
     * @param[in] data данные которые будут возвращены, если данный паттерн сматчился в тексте
     * @param[out] error может быть записано ErrorCode::PATTERN_AND_DATA_IN_USE или ErrorCode::BUILD_ERROR
     * @return true в случае успеха, false в случае неудачи смотри \a error
     */
    bool InsertAndBuild(char const * pattern, size_t len, const DataT& data, Error * error = nullptr) {
        return Insert(pattern, len, data, error) && Build(error);
    }

    /**
     * @see DeleteAndBuild(char const *, size_t, const DataT&, Error *)
     */
    bool DeleteAndBuild(const std::string& pattern, const DataT& data, Error * error = nullptr) {
        return DeleteAndBuild(pattern.c_str(), pattern.size(), data, error);
    }

    /**
     * @brief Delete + Build
     * @param[in] pattern указатель на начало паттерна
     * @param[in] len  длина паттерна
     * @param[in] data данные которые будут возвращены, если данный паттерн сматчился в тексте
     * @param[out] error может быть записано ErrorCode::PATTERN_AND_DATA_NOT_FOUND или ErrorCode::BUILD_ERROR
     * @return true в случае успеха, false в случае неудачи смотри \a error
     */
    bool DeleteAndBuild(char const * pattern, size_t len, const DataT& data, Error * error = nullptr) {
        return Delete(pattern, len, data, error) && Build(error);
    }

    /**
     * @see Find(const char *, size_t, Error *) const
     */
    std::vector<DataT> Find(const std::string &text, Error * error = nullptr) const {
        return Find(text.c_str(), text.size(), error);
    }

    /**
     * @brief Find ищет в тексте добавленные паттерны
     *
     *   Каждый раз копирует указатель на текущее состояние = DatabaseWrapper <br>
     * клонирует необходимую память, которую он будет изменять в ходе поиска <br>
     * создает контекст с указателем на ответ и пользовательские данные
     * который будет передаваться в callback(FindHandler) вызываемый из hs_scan
     *
     * @remark thread-safe, multiple readers
     * @param[in] text указатель на начало текста
     * @param[in] len  длина текста
     * @param[out] error может быть записано ErrorCode::SCAN_ERROR
     * @return вектор данных соответствующих паттернам которые сматчились во время поиска
     */
    std::vector<DataT> Find(const char *text, size_t len, Error * error = nullptr) const {
        if (error) *error = Error();

        _m.lock();
        std::shared_ptr<DatabaseWrapper> dw = _dw;
        _m.unlock();

        std::vector<DataT> res;
        if (!dw) return res;

        assert(dw->scratch);
        ScratchWrapper sw(dw->scratch);
        Context ctx{&res, &dw->data};

        if (hs_scan(dw->db, text, len, 0, sw.scratch, FindHandler, (void*) &ctx) != HS_SUCCESS && error) {
            *error = Error(ErrorCode::SCAN_ERROR);
        }

        return res;
    }

private:
    /**
     * @brief FindHandler callback вызываемый функцией hs_scan
     *
     *   Сохраняет в ответ данные соответствующие айдишнику паттерна который сматчился
     *
     * @param id соответсвтвующий паттерну добавленному в базу данных
     * @param from позиция в тексте начиная с которой сматчился паттерн
     * @param to позиция до которой в тексте сматчился паттерн
     * @param flags флаги
     * @param ctx указатель на Context
     * @return возвращает ноль чтобы продолжить поиск, не ноль чтобы остановить поиск
     */
    static int FindHandler(unsigned int id, unsigned long long from,
                            unsigned long long to, unsigned int flags, void * ctx) {
        Context * context = reinterpret_cast<Context *>(ctx);
        context->res->push_back((*context->data)[id]);

        return 0;
    }

private:
    /**
     * @brief паттерны добавленные пользователем
     */
    std::vector<char *> _patterns;

    /**
     * @brief данные соответсвтующие паттернам
     */
    std::vector<DataT> _data;

    /**
     * @brief указатель на скомпилированную базу данных и продублированные данные
     */
    std::shared_ptr<DatabaseWrapper> _dw;

    /**
     * @brief временное решение для реализации single Build multiple Find ;)
     */
    mutable std::mutex _m;
};

} // namespace Hyperscan

/*! @} End of Doxygen Groups*/

#endif // HYPERSCAN_H
