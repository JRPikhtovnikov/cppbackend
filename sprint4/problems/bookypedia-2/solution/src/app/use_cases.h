#pragma once

#include <string>
#include <vector>
#include <optional>

namespace app {


struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string id;
    std::string author_name;
    std::string title;
    int publication_year;
    std::vector<std::string> tags;
};

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual void EditAuthor(const std::string& author_id, const std::string& new_name) = 0;
    virtual void DeleteAuthor(const std::string& author_id) = 0;
    virtual std::vector<AuthorInfo> GetAllAuthors() = 0;
    virtual std::optional<AuthorInfo> FindAuthorByName(const std::string& name) = 0;
    virtual void AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::vector<std::string>& tags={}) = 0;
    virtual void EditBook(const std::string& book_id, const std::string& title, int publication_year, const std::vector<std::string>& tags) = 0;
    virtual void DeleteBook(const std::string& book_id) = 0;
    virtual std::vector<BookInfo> GetAllBooks() = 0;
    virtual std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) = 0;
    virtual std::vector<BookInfo> FindBooksByTitle(const std::string& title) = 0;
    virtual std::optional<BookInfo> GetBookById(const std::string& book_id) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
