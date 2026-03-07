#include "postgres.h"

#include <pqxx/zview.hxx>
#include <pqxx/result> 

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    // Пока каждое обращение к репозиторию выполняется внутри отдельной транзакции
    // В будущих уроках вы узнаете про паттерн Unit of Work, при помощи которого сможете несколько
    // запросов выполнить в рамках одной транзакции.
    // Вы также может самостоятельно почитать информацию про этот паттерн и применить его здесь.
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAllAuthors() const {
    pqxx::read_transaction r{connection_};
    auto rows = r.exec("SELECT id, name FROM authors ORDER BY name"_zv);
    std::vector<domain::Author> authors;
    authors.reserve(rows.size());
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        const auto& row = *it;
        auto id = domain::AuthorId::FromString(row[0].c_str());
        authors.emplace_back(std::move(id), row[1].c_str());
    }
    return authors;
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET author_id=$2, title=$3, publication_year=$4;
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(), book.GetPublicationYear());
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAllBooks() const {
    pqxx::read_transaction r{connection_};
    auto rows = r.exec("SELECT id, author_id, title, publication_year FROM books ORDER BY title"_zv);
    std::vector<domain::Book> books;
    books.reserve(rows.size());
    for (const auto& row : rows) {
        auto id = domain::BookId::FromString(row[0].c_str());
        auto author_id = domain::AuthorId::FromString(row[1].c_str());
        books.emplace_back(std::move(id), std::move(author_id), row[2].c_str(), row[3].as<int>());
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetBooksByAuthor(const domain::AuthorId& author_id) const {
    pqxx::read_transaction r{connection_};
    auto rows = r.exec_params(
        "SELECT id, title, publication_year FROM books WHERE author_id = $1 ORDER BY publication_year, title"_zv,
        author_id.ToString());
    std::vector<domain::Book> books;
    books.reserve(rows.size());
    for (const auto& row : rows) {
        auto id = domain::BookId::FromString(row[0].c_str());
        books.emplace_back(std::move(id), author_id, row[1].c_str(), row[2].as<int>());
    }
    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
    CREATE TABLE IF NOT EXISTS authors (
        id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
        name varchar(100) UNIQUE NOT NULL
    );
    )"_zv);
    
    work.exec(R"(
    CREATE TABLE IF NOT EXISTS books (
        id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
        author_id UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
        title varchar(100) NOT NULL,
        publication_year integer NOT NULL
    );
    )"_zv);

    // коммитим изменения
    work.commit();
}

}  // namespace postgres