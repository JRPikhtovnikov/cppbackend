#pragma once
#include "use_cases.h"
#include "../postgres/postgres.h"  
#include <pqxx/connection>

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::AuthorRepository& authors,
                          domain::BookRepository& books,
                          domain::TagRepository& tags,
                          pqxx::connection& connection)
        : authors_{authors}, books_{books}, tags_{tags}, connection_{connection} {}

    // Авторы
    void AddAuthor(const std::string& name) override;
    void DeleteAuthor(const domain::AuthorId& author_id) override;
    void EditAuthor(const domain::AuthorId& author_id, const std::string& new_name) override;
    std::vector<domain::Author> GetAllAuthors() const override;
    std::optional<domain::Author> GetAuthorByName(const std::string& name) const override;
    std::optional<domain::Author> GetAuthorById(const domain::AuthorId& author_id) const override;

    // Книги
    void AddBook(const domain::AuthorId& author_id,
                 const std::string& title,
                 int year,
                 const std::vector<std::string>& tags) override;
    void DeleteBook(const domain::BookId& book_id) override;
    void EditBook(const domain::BookId& book_id,
                  const std::string& new_title,
                  int new_year,
                  const std::vector<std::string>& new_tags) override;
    std::vector<domain::Book> GetAllBooks() const override;
    std::vector<domain::Book> GetBooksByTitle(const std::string& title) const override;
    std::optional<domain::Book> GetBookById(const domain::BookId& book_id) const override;
    std::vector<domain::Book> GetBooksByAuthor(const domain::AuthorId& author_id) const override;

    // Теги
    std::vector<std::string> GetTagsByBook(const domain::BookId& book_id) const override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
    domain::TagRepository& tags_;
    pqxx::connection& connection_;
};

}  // namespace app