#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"
#include "../app/unit_of_work.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::work& work) : work_(work) { }
    void Save(const domain::Author& author) override;
    void Update(const domain::Author& author) override;
    void Delete(const domain::AuthorId& id) override;
    std::vector<std::pair<domain::AuthorId, std::string>> GetAll() override;
    std::optional<domain::Author> FindById(const domain::AuthorId& id) override;
    std::optional<domain::Author> FindByName(const std::string& name) override;
private:
     pqxx::work& work_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::work& work) : work_(work) { }

    void Save(const domain::Book& book) override;
    void Update(const domain::Book& book) override;
    void Delete(const domain::BookId& id) override;
    std::vector<std::pair<domain::AuthorId, std::string>> GetAllAuthors() override;
    std::vector<domain::Book> GetBooks() override;
    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) override;
    std::optional<domain::Book> FindById(const domain::BookId& id) override;
    std::vector<domain::Book> FindByTitle(const std::string& title) override;
private:
    std::vector<std::string> GetBookTags(const domain::BookId& book_id);
    void SaveBookTags(const domain::BookId& book_id, const std::vector<std::string>& tags);
    void DeleteBookTags(const domain::BookId& book_id);
    pqxx::work& work_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    std::unique_ptr<app::UnitOfWork> CreateUnitOfWork();

private:
    pqxx::connection connection_;
};

}  // namespace postgres
