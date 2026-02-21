#include "urldecode.h"

#include <charconv>
#include <stdexcept>

#include <string>
#include <string_view>
#include <cctype>
#include <stdexcept>

std::string UrlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    auto is_hex = [](char ch) noexcept {
        return (ch >= '0' && ch <= '9') ||
               (ch >= 'A' && ch <= 'F') ||
               (ch >= 'a' && ch <= 'f');
    };

    auto hex_to_val = [](char ch) noexcept -> unsigned char {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        return 0;
    };

    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];
        if (c == '%') {
            if (i + 2 >= str.size()) {
                throw std::invalid_argument("Incomplete percent encoding at end of string");
            }
            char h1 = str[i + 1];
            char h2 = str[i + 2];
            if (!is_hex(h1) || !is_hex(h2)) {
                throw std::invalid_argument("Invalid hex digit in percent encoding");
            }
            unsigned char val = (hex_to_val(h1) << 4) | hex_to_val(h2);
            result.push_back(static_cast<char>(val));
            i += 2;
        } else if (c == '+') {
            result.push_back(' ');
        } else {
            result.push_back(c);
        }
    }

    return result;
}
