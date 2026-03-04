#include <iostream>
#include <string>
#include <string_view>
#include <sstream>
#include <pqxx/pqxx>
#include <boost/json.hpp>

namespace json = boost::json;

using namespace std::literals;

// Оператор для работы с pqxx (zero-terminated string literals)
using pqxx::operator"" _zv;

void CreateBooksTable(pqxx::connection& conn) {
    pqxx::work txn(conn);
    txn.exec(
        R"(
            CREATE TABLE IF NOT EXISTS books (
                id SERIAL PRIMARY KEY,
                title VARCHAR(100) NOT NULL,
                author VARCHAR(100) NOT NULL,
                year INTEGER NOT NULL,
                ISBN CHAR(13) UNIQUE
            )
        )"_zv
    );
    txn.commit();
}

void HandleAddBook(pqxx::connection& conn, const json::object& payload) {
    std::string title = payload.at("title").as_string().c_str();
    std::string author = payload.at("author").as_string().c_str();
    int year = payload.at("year").as_int64();

    // ISBN может быть null
    std::optional<std::string> isbn;
    if (!payload.at("ISBN").is_null()) {
        isbn = payload.at("ISBN").as_string().c_str();
    }

    try {
        pqxx::work txn(conn);
        std::string query = "INSERT INTO books (title, author, year, ISBN) VALUES (" +
                            txn.quote(title) + ", " + txn.quote(author) + ", " +
                            txn.quote(year) + ", " +
                            (isbn ? txn.quote(*isbn) : "NULL") + ")";
        txn.exec(query.c_str());
        txn.commit();
        std::cout << R"({"result":true})" << std::endl;
    } catch (const pqxx::sql_error&) {
        std::cout << R"({"result":false})" << std::endl;
    } catch (const std::exception&) {
        std::cout << R"({"result":false})" << std::endl;
    }
}

void HandleAllBooks(pqxx::connection& conn) {
    pqxx::read_transaction txn(conn);
    auto result = txn.exec(
        R"(
            SELECT id, title, author, year, ISBN
            FROM books
            ORDER BY year DESC, title ASC, author ASC, ISBN ASC
        )"_zv
    );

    json::array books;
    for (const auto& row : result) {
        json::object book;
        book["id"] = row[0].as<int>();
        book["title"] = row[1].as<std::string>();
        book["author"] = row[2].as<std::string>();
        book["year"] = row[3].as<int>();

        if (row[4].is_null()) {
            book["ISBN"] = nullptr;
        } else {
            book["ISBN"] = row[4].as<std::string>();
        }

        books.push_back(std::move(book));
    }

    std::cout << json::serialize(books) << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: book_manager <connection-string>" << std::endl;
        return EXIT_FAILURE;
    }

    try {
        pqxx::connection conn(argv[1]);

        // Создаём таблицу при старте
        CreateBooksTable(conn);

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            try {
                json::value request = json::parse(line);
                const json::object& obj = request.as_object();
                std::string action = obj.at("action").as_string().c_str();
                const json::object& payload = obj.at("payload").as_object();

                if (action == "add_book") {
                    HandleAddBook(conn, payload);
                } else if (action == "all_books") {
                    HandleAllBooks(conn);
                } else if (action == "exit") {
                    break;
                }
            } catch (const std::exception& e) {
                // По условию входные данные корректны, но на всякий случай:
                continue;
            }
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}