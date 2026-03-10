#pragma once
#include <string>
#include <vector>
#include <optional>
#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/tag.h"

namespace app {

class UseCases {
public:
    // Авторы
    virtual void AddAuthor(const std::string& name) = 0;
    virtual void DeleteAuthor(const domain::AuthorId& author_id) = 0;
    virtual void EditAuthor(const domain::AuthorId& author_id, const std::string& new_name) = 0;
    virtual std::vector<domain::Author> GetAllAuthors() const = 0;
    virtual std::optional<domain::Author> GetAuthorByName(const std::string& name) const = 0;
    virtual std::optional<domain::Author> GetAuthorById(const domain::AuthorId& author_id) const = 0;

    // Книги
    virtual void AddBook(const domain::AuthorId& author_id,
                         const std::string& title,
                         int year,
                         const std::vector<std::string>& tags) = 0;
    virtual void DeleteBook(const domain::BookId& book_id) = 0;
    virtual void EditBook(const domain::BookId& book_id,
                          const std::string& new_title,
                          int new_year,
                          const std::vector<std::string>& new_tags) = 0;
    virtual std::vector<domain::Book> GetAllBooks() const = 0;
    virtual std::vector<domain::Book> GetBooksByTitle(const std::string& title) const = 0;
    virtual std::optional<domain::Book> GetBookById(const domain::BookId& book_id) const = 0;
    virtual std::vector<domain::Book> GetBooksByAuthor(const domain::AuthorId& author_id) const = 0;

    // Теги
    virtual std::vector<std::string> GetTagsByBook(const domain::BookId& book_id) const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app