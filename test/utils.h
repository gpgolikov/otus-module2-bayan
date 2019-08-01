#pragma once

#include <catch2/catch.hpp>

#include <sstream>
#include <string>
#include <locale>

template<typename T>
class equals : public Catch::MatcherBase<T> {
    T value_;

public:
    explicit equals(T value) : value_(std::move(value)) {}

    virtual bool match(const T& to_match) const override {
        return value_ == to_match;
    }

    virtual std::string describe() const override {
        std::ostringstream os;
        os << "\nequals: \n\"" << value_ << '"';
        return os.str();
    }
};

template <typename T>
inline auto Equals(T value) {
    return equals<T>(std::move(value));
}

inline auto Equals(const char* str) {
    return equals<std::string_view>(str);
}

namespace std {

ostream&
operator<<(ostream& os, const wstring& value) {
    mbstate_t state {};
    auto value_data = value.data();
    size_t len = wcsrtombs(nullptr, &value_data, 0, &state);
    string str(len, 0);
    wcsrtombs(str.data(), &value_data, str.size(), &state);
    return os << str;
}

template<typename T1, typename T2>
ostream&
operator<<(ostream& os, const pair<T1, T2>& p) {
    return os << '{' << get<0>(p) << ';' << get<1>(p) << '}';
}

}
