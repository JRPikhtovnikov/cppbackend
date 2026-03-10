#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>
#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/tag.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection);
    void Save(const domain::Author& author) override;
    void Delete(const domain::AuthorId& author_id) override;
    std::vector<domain::Author> GetAllAuthors() const override;
    std::optional<domain::Author> GetByName(const std::string& name) const override;
    std::optional<domain::Author> GetById(const domain::AuthorId& author_id) const override;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection);
    void Save(const domain::Book& book) override;
    void Delete(const domain::BookId& book_id) override;
    void DeleteByAuthor(const domain::AuthorId& author_id) override;
    std::vector<domain::Book> GetAllBooks() const override;
    std::vector<domain::Book> GetBooksByAuthor(const domain::AuthorId& author_id) const override;
    std::vector<domain::Book> GetBooksByTitle(const std::string& title) const override;
    std::optional<domain::Book> GetById(const domain::BookId& book_id) const override;

private:
    pqxx::connection& connection_;
};

class TagRepositoryImpl : public domain::TagRepository {
public:
    explicit TagRepositoryImpl(pqxx::connection& connection);
    void Save(const domain::Tag& tag) override;
    void DeleteByBook(const domain::BookId& book_id) override;
    std::vector<domain::Tag> GetByBook(const domain::BookId& book_id) const override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);
    pqxx::connection& GetConnection() { return connection_; }
    AuthorRepositoryImpl& GetAuthors() & { return authors_; }
    BookRepositoryImpl& GetBooks() & { return books_; }
    TagRepositoryImpl& GetTags() & { return tags_; }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_;
    BookRepositoryImpl books_;
    TagRepositoryImpl tags_;
};

}  // namespace postgres