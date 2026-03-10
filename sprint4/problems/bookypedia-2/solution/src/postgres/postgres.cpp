#include "postgres.h"
#include <pqxx/zview.hxx>
#include <pqxx/result>
#include <string>
#include <optional>

using namespace std::literals;
using pqxx::operator"" _zv;

namespace postgres {

AuthorRepositoryImpl::AuthorRepositoryImpl(pqxx::connection& connection)
    : connection_{connection} {}

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work w(connection_);
    w.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name;
)"_zv,
        author.GetId().ToString(), author.GetName());
    w.commit();
}

void AuthorRepositoryImpl::Delete(const domain::AuthorId& author_id) {
    pqxx::work w(connection_);
    w.exec_params("DELETE FROM authors WHERE id = $1;"_zv, author_id.ToString());
    w.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAllAuthors() const {
    pqxx::read_transaction r(connection_);
    auto rows = r.exec("SELECT id, name FROM authors ORDER BY name;"_zv);
    std::vector<domain::Author> authors;
    for (const auto& row : rows) {
        authors.emplace_back(
            domain::AuthorId::FromString(row[0].as<std::string>()),
            row[1].as<std::string>());
    }
    return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::GetByName(const std::string& name) const {
    pqxx::read_transaction r(connection_);
    auto result = r.exec_params("SELECT id, name FROM authors WHERE name = $1;"_zv, name);
    if (result.empty()) return std::nullopt;
    const auto& row = result[0];
    return domain::Author(domain::AuthorId::FromString(row[0].as<std::string>()), row[1].as<std::string>());
}

std::optional<domain::Author> AuthorRepositoryImpl::GetById(const domain::AuthorId& author_id) const {
    pqxx::read_transaction r(connection_);
    auto result = r.exec_params("SELECT id, name FROM authors WHERE id = $1;"_zv, author_id.ToString());
    if (result.empty()) return std::nullopt;
    const auto& row = result[0];
    return domain::Author(domain::AuthorId::FromString(row[0].as<std::string>()), row[1].as<std::string>());
}

// ---------- BookRepositoryImpl ----------

BookRepositoryImpl::BookRepositoryImpl(pqxx::connection& connection)
    : connection_{connection} {}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work w(connection_);
    w.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET author_id = EXCLUDED.author_id,
                                 title = EXCLUDED.title,
                                 publication_year = EXCLUDED.publication_year;
)"_zv,
        book.GetId().ToString(),
        book.GetAuthorId().ToString(),
        book.GetTitle(),
        book.GetPublicationYear());
    w.commit();
}

void BookRepositoryImpl::Delete(const domain::BookId& book_id) {
    pqxx::work w(connection_);
    w.exec_params("DELETE FROM books WHERE id = $1;"_zv, book_id.ToString());
    w.commit();
}

void BookRepositoryImpl::DeleteByAuthor(const domain::AuthorId& author_id) {
    pqxx::work w(connection_);
    w.exec_params("DELETE FROM books WHERE author_id = $1;"_zv, author_id.ToString());
    w.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAllBooks() const {
    pqxx::read_transaction r(connection_);
    auto rows = r.exec("SELECT id, author_id, title, publication_year FROM books ORDER BY title, publication_year;"_zv);
    std::vector<domain::Book> books;
    for (const auto& row : rows) {
        books.emplace_back(
            domain::BookId::FromString(row[0].as<std::string>()),
            domain::AuthorId::FromString(row[1].as<std::string>()),
            row[2].as<std::string>(),
            row[3].as<int>());
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetBooksByAuthor(const domain::AuthorId& author_id) const {
    pqxx::read_transaction r(connection_);
    auto rows = r.exec_params(
        "SELECT id, title, publication_year FROM books WHERE author_id = $1 ORDER BY title, publication_year;"_zv,
        author_id.ToString());
    std::vector<domain::Book> books;
    for (const auto& row : rows) {
        books.emplace_back(
            domain::BookId::FromString(row[0].as<std::string>()),
            author_id,
            row[1].as<std::string>(),
            row[2].as<int>());
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetBooksByTitle(const std::string& title) const {
    pqxx::read_transaction r(connection_);
    auto rows = r.exec_params(
        "SELECT id, author_id, title, publication_year FROM books WHERE title = $1 ORDER BY publication_year, title;"_zv,
        title);
    std::vector<domain::Book> books;
    for (const auto& row : rows) {
        books.emplace_back(
            domain::BookId::FromString(row[0].as<std::string>()),
            domain::AuthorId::FromString(row[1].as<std::string>()),
            row[2].as<std::string>(),
            row[3].as<int>());
    }
    return books;
}

std::optional<domain::Book> BookRepositoryImpl::GetById(const domain::BookId& book_id) const {
    pqxx::read_transaction r(connection_);
    auto result = r.exec_params("SELECT id, author_id, title, publication_year FROM books WHERE id = $1;"_zv, book_id.ToString());
    if (result.empty()) return std::nullopt;
    const auto& row = result[0];
    return domain::Book(
        domain::BookId::FromString(row[0].as<std::string>()),
        domain::AuthorId::FromString(row[1].as<std::string>()),
        row[2].as<std::string>(),
        row[3].as<int>());
}

// ---------- TagRepositoryImpl ----------

TagRepositoryImpl::TagRepositoryImpl(pqxx::connection& connection)
    : connection_{connection} {}

void TagRepositoryImpl::Save(const domain::Tag& tag) {
    pqxx::work w(connection_);
    w.exec_params(
        "INSERT INTO book_tags (book_id, tag) VALUES ($1, $2) ON CONFLICT DO NOTHING;"_zv,
        tag.GetBookId().ToString(),
        tag.GetTag());
    w.commit();
}

void TagRepositoryImpl::DeleteByBook(const domain::BookId& book_id) {
    pqxx::work w(connection_);
    w.exec_params("DELETE FROM book_tags WHERE book_id = $1;"_zv, book_id.ToString());
    w.commit();
}

std::vector<domain::Tag> TagRepositoryImpl::GetByBook(const domain::BookId& book_id) const {
    pqxx::read_transaction r(connection_);
    auto rows = r.exec_params(
        "SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag;"_zv,
        book_id.ToString());
    std::vector<domain::Tag> tags;
    for (const auto& row : rows) {
        tags.emplace_back(book_id, row[0].as<std::string>());
    }
    return tags;
}

// ---------- Database ----------

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)},
      authors_{connection_},
      books_{connection_},
      tags_{connection_} {
    pqxx::work w(connection_);
    w.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID PRIMARY KEY,
    name VARCHAR(100) UNIQUE NOT NULL
);
)"_zv);
    w.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID PRIMARY KEY,
    author_id UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    title VARCHAR(100) NOT NULL,
    publication_year INTEGER NOT NULL
);
)"_zv);
    w.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID REFERENCES books(id) ON DELETE CASCADE,
    tag VARCHAR(30) NOT NULL,
    PRIMARY KEY (book_id, tag)
);
)"_zv);
    w.commit();
}

}  // namespace postgres