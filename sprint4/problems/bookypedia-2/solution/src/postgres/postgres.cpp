#include "postgres.h"
//#include <pqxx/zview.hxx>

#include <pqxx/pqxx>
#include <pqxx/result>
#include <pqxx/row>

#include <vector>
#include <string>

#include <algorithm>
#include <memory>
#include "unit_of_work_impl.h"

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    work_.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
    author.GetId().ToString(), author.GetName());
}

void AuthorRepositoryImpl::Update(const domain::Author& author) {
    auto result = work_.exec_params("UPDATE authors SET name=$2 WHERE id=$1 RETURNING id;"_zv, author.GetId().ToString(), author.GetName());
    if (result.empty()) {
        throw std::runtime_error("Author not found {Update}");
    }
}

void AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    auto result = work_.exec_params("DELETE FROM authors WHERE id=$1 RETURNING id;"_zv, id.ToString());
    if (result.empty()) {
        throw std::runtime_error("Author not found {Delete}");
    }
}

std::vector<std::pair<domain::AuthorId, std::string>> AuthorRepositoryImpl::GetAll() {
    auto result = work_.exec("SELECT id, name FROM authors ORDER BY name;"_zv);
    std::vector<std::pair<domain::AuthorId, std::string>> authors;
    authors.reserve(result.size());
    for (const auto& row : result) {
        auto id_str = row[0].as<std::string>();
        auto name = row[1].as<std::string>();
        authors.emplace_back(domain::AuthorId::FromString(id_str), std::move(name));
    }
    return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::FindById(const domain::AuthorId& id) {
    auto result = work_.exec_params("SELECT id, name FROM authors WHERE id=$1;"_zv, id.ToString());
    if (result.empty()) {
        return std::nullopt;
    }
    const auto& row = result[0];
    return domain::Author{id, row[1].as<std::string>()};
}

std::optional<domain::Author> AuthorRepositoryImpl::FindByName(const std::string& name) {
    auto result = work_.exec_params("SELECT id, name FROM authors WHERE name=$1;"_zv, name);
    if (result.empty()) {
        return std::nullopt;
    }
    const auto& row = result[0];
    auto id = domain::AuthorId::FromString(row[0].as<std::string>());
    return domain::Author{std::move(id), row[1].as<std::string>()};
}


std::vector<std::string> BookRepositoryImpl::GetBookTags(const domain::BookId& book_id) {
    auto result = work_.exec_params("SELECT tag FROM book_tags WHERE book_id=$1 ORDER BY tag;"_zv, book_id.ToString());
    std::vector<std::string> tags;
    tags.reserve(result.size());
    for (const auto& row : result) {
        tags.push_back({row[0].as<std::string>()});
    }
    return tags;
}

void BookRepositoryImpl::SaveBookTags(const domain::BookId& book_id, const std::vector<std::string>& tags) {
    for (const auto& tag : tags) {
        work_.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2);"_zv, book_id.ToString(), tag);
    }
}

void BookRepositoryImpl::DeleteBookTags(const domain::BookId& book_id) {
    work_.exec_params("DELETE FROM book_tags WHERE book_id=$1;"_zv, book_id.ToString());
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    work_.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET author_id=$2, title=$3, publication_year=$4;
)"_zv,
        book.GetId().ToString(),
        book.GetAuthorId().ToString(),
        book.GetTitle(),
        book.GetPublicationYear()
    );

    //Save tags
    DeleteBookTags(book.GetId());
    SaveBookTags(book.GetId(), book.GetTags());
}

void BookRepositoryImpl::Update(const domain::Book& book) {
    auto result = work_.exec_params("UPDATE books SET author_id=$2, title=$3, publication_year=$4 WHERE id=$1 RETURNING id;"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(), book.GetPublicationYear());
    if (result.empty()) {
        throw std::runtime_error("Book not found");
    }
    // Update tags
    DeleteBookTags(book.GetId());
    SaveBookTags(book.GetId(), book.GetTags());
}

void BookRepositoryImpl::Delete(const domain::BookId& id) {
    // Tags will be deleted automatically by ON DELETE CASCADE
    auto result = work_.exec_params("DELETE FROM books WHERE id=$1 RETURNING id;"_zv, id.ToString());

    if (result.empty()) {
        throw std::runtime_error("Book not found");
    }
}

std::vector<domain::Book> BookRepositoryImpl::GetBooks() {
    auto result = work_.exec("SELECT id, author_id, title, publication_year FROM books ORDER BY title, publication_year;"_zv);
    //auto result = work_.exec("SELECT books.id, authors.name AS author, books.title, books.pub_year FROM books JOIN authors ON books.author_id = authors.id ORDER BY books.title, authors.name, books.pub_year;"_zv);
    std::vector<domain::Book> books;
    books.reserve(result.size());
    for (const auto& row : result) {
        auto id = domain::BookId::FromString(row[0].as<std::string>());
        auto author_id = domain::AuthorId::FromString(row[1].as<std::string>());
        auto title = row[2].as<std::string>();
        auto year = row[3].as<int>();
        auto tags = GetBookTags(id);//!!!
        books.emplace_back(std::move(id), std::move(author_id), std::move(title), year, std::move(tags));
    }
    return books;
}

std::vector<std::pair<domain::AuthorId, std::string>> BookRepositoryImpl::GetAllAuthors() {
    auto result = work_.exec("SELECT id, name FROM authors ORDER BY name;"_zv);
    std::vector<std::pair<domain::AuthorId, std::string>> authors;
    authors.reserve(result.size());
    for (const auto& row : result) {
        auto id_str = row[0].as<std::string>();
        auto name = row[1].as<std::string>();
        authors.emplace_back(domain::AuthorId::FromString(id_str), std::move(name));
    }
    return authors;
}

std::vector<domain::Book> BookRepositoryImpl::GetAuthorBooks(const domain::AuthorId& author_id) {
    auto result = work_.exec_params("SELECT id, author_id, title, publication_year FROM books WHERE author_id = $1 ORDER BY publication_year, title;"_zv
        , author_id.ToString());
    std::vector<domain::Book> books;
    books.reserve(result.size());
    for (const auto& row : result) {
        auto id = domain::BookId::FromString(row[0].as<std::string>());
        auto auth_id = domain::AuthorId::FromString(row[1].as<std::string>());
        auto title = row[2].as<std::string>();
        auto year = row[3].as<int>();
        auto tags = GetBookTags(id);///!!!!
        books.emplace_back(std::move(id), std::move(auth_id), std::move(title), year, std::move(tags));
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::FindByTitle(const std::string& title) {
    auto result = work_.exec_params(
        "SELECT b.id, b.author_id, b.title, b.publication_year, a.name as author_name "
        "FROM books b "
        "JOIN authors a ON a.id = b.author_id "
        "WHERE b.title = $1 "
        "ORDER BY a.name, b.publication_year;"_zv
        , title
    );
    std::vector<domain::Book> books;
    books.reserve(result.size());
    for (const auto& row : result) {
        auto id = domain::BookId::FromString(row[0].as<std::string>());
        auto author_id = domain::AuthorId::FromString(row[1].as<std::string>());
        auto title = row[2].as<std::string>();
        auto year = row[3].as<int>();
        auto tags = GetBookTags(id);
        books.emplace_back(std::move(id), std::move(author_id), std::move(title), year, std::move(tags));
    }
    return books;
}

std::optional<domain::Book> BookRepositoryImpl::FindById(const domain::BookId& id) {
    auto result = work_.exec_params("SELECT id, author_id, title, publication_year FROM books WHERE id = $1;"_zv, id.ToString());
    if (result.empty()) {
        return std::nullopt;
    }
    const auto& row = result[0];
    auto title = row[2].as<std::string>();
    auto year = row[3].as<int>();
    auto author_id = domain::AuthorId::FromString(row[1].as<std::string>());
    auto tags = GetBookTags(id);
    return domain::Book{id, std::move(author_id), std::move(title), year, std::move(tags)};
}


Database::Database(pqxx::connection connection) : connection_{std::move(connection)} {
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
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    tag VARCHAR(30) NOT NULL,
    PRIMARY KEY(book_id, tag)
);
)"_zv);
    work.commit();
}

std::unique_ptr<app::UnitOfWork> Database::CreateUnitOfWork() {
    return std::make_unique<UnitOfWorkImpl>(connection_);
}

}  // namespace postgres
