#define BOOST_TEST_MODULE urlencode tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

BOOST_AUTO_TEST_CASE(UrlDecode_tests) {
    using namespace std::literals;

    // Пустая строка
    BOOST_TEST(UrlDecode(""sv) == ""s);

    // Строка без специальных символов
    BOOST_TEST(UrlDecode("HelloWorld"sv) == "HelloWorld"s);

    // Пробел как '+'
    BOOST_TEST(UrlDecode("Hello+World"sv) == "Hello World"s);

    // Пробел как '%20'
    BOOST_TEST(UrlDecode("Hello%20World"sv) == "Hello World"s);

    // Несколько пробелов
    BOOST_TEST(UrlDecode("a+b+c"sv) == "a b c"s);
    BOOST_TEST(UrlDecode("a%20b%20c"sv) == "a b c"s);

    // Кодирование букв и цифр
    BOOST_TEST(UrlDecode("Hello%41"sv) == "HelloA"s);   // %41 = 'A'
    BOOST_TEST(UrlDecode("Number%31"sv) == "Number1"s); // %31 = '1'

    // Символы, которые не требуют кодирования
    BOOST_TEST(UrlDecode("-._~"sv) == "-._~"s);

    // Зарезервированные символы в явном виде (не должны меняться)
    //BOOST_TEST(UrlDecode("!#$&'()*+,/:;=?@[]"sv) == "!#$&'()*+,/:;=?@[]"s);

    // Смешанный пример
    BOOST_TEST(UrlDecode("Hello%2C%20world%21"sv) == "Hello, world!"s);

    // Процентные последовательности в разном регистре
    BOOST_TEST(UrlDecode("%2f"sv) == "/"s);
    BOOST_TEST(UrlDecode("%2F"sv) == "/"s);
    BOOST_TEST(UrlDecode("%2a"sv) == "*"s);  // строчные буквы допустимы
    BOOST_TEST(UrlDecode("%2A"sv) == "*"s);

    // Несколько последовательностей подряд
    BOOST_TEST(UrlDecode("%20%20"sv) == "  "s);

    // Комбинация '+' и закодированного '+'
    BOOST_TEST(UrlDecode("a+b%2Bc"sv) == "a b+c"s);

    // Пример из условия
    BOOST_TEST(UrlDecode("Hello%20World !"sv) == "Hello World !"s);

    // --- Проверка исключений при некорректных последовательностях ---
    // Неполный процент в конце
    BOOST_CHECK_THROW(UrlDecode("%"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("abc%"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%1"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%2"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("abc%2"sv), std::invalid_argument);

    // Некорректные шестнадцатеричные цифры
    BOOST_CHECK_THROW(UrlDecode("%1G"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%G1"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%2g"sv), std::invalid_argument); // 'g' недопустима
}