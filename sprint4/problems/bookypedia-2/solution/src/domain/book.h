#pragma once
#include <string>
#include "../util/tagged_uuid.h"
#include "author.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, AuthorId author_id, std::string title, int publication_year, std::vector<std::string> tags = {})
        : id_(std::move(id)), author_id_(std::move(author_id)), title_(std::move(title)) , publication_year_(publication_year), tags_(std::move(tags)) { }

    const BookId& GetId() const noexcept {
        return id_;
    }

    const AuthorId& GetAuthorId() const noexcept {
        return author_id_;
    }

    void SetTitle(const std::string& title) {
        title_ = title;
    }

    const std::string& GetTitle() const noexcept {
        return title_;
    }

    void SetPublicationYear(int year) {
        publication_year_ = year;
    }

    int GetPublicationYear() const noexcept {
        return publication_year_;
    }

    void SetTags(std::vector<std::string> tags) {
        tags_ = std::move(tags);
    }

    const std::vector<std::string>& GetTags() const noexcept {
        return tags_;
    }

private:
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int publication_year_;
    std::vector<std::string> tags_;
};


class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual void Update(const Book& book) = 0;
    virtual void Delete(const BookId& id) = 0;
    virtual std::vector<std::pair<AuthorId, std::string>> GetAllAuthors() = 0;
    virtual std::vector<Book> GetBooks() = 0;
    virtual std::vector<Book> GetAuthorBooks(const AuthorId& author_id) = 0;
    virtual std::optional<Book> FindById(const BookId& id) = 0;
    virtual std::vector<Book> FindByTitle(const std::string& title) = 0;
    virtual ~BookRepository() = default;
};

}  // namespace domain
