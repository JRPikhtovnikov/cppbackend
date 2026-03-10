#pragma once
#include <string>
#include <optional>
#include <vector>
#include "author.h"
#include "../util/tagged_uuid.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, AuthorId author_id, std::string title, int year)
        : id_(std::move(id)), author_id_(std::move(author_id)),
          title_(std::move(title)), year_(year) {}

    const BookId& GetId() const noexcept { return id_; }
    const AuthorId& GetAuthorId() const noexcept { return author_id_; }
    const std::string& GetTitle() const noexcept { return title_; }
    int GetPublicationYear() const noexcept { return year_; }

private:
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int year_;
};

class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual void Delete(const BookId& book_id) = 0;
    virtual void DeleteByAuthor(const AuthorId& author_id) = 0;
    virtual std::vector<Book> GetAllBooks() const = 0;
    virtual std::vector<Book> GetBooksByAuthor(const AuthorId& author_id) const = 0;
    virtual std::vector<Book> GetBooksByTitle(const std::string& title) const = 0;
    virtual std::optional<Book> GetById(const BookId& book_id) const = 0;

protected:
    ~BookRepository() = default;
};

}  // namespace domain