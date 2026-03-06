#pragma once
#include "../domain/author_fwd.h"
#include "../domain/book_fwd.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_{authors}, books_{books} {
    }

    void AddAuthor(const std::string& name) override;
    std::vector<domain::Author> GetAuthors() const override;
    void AddBook(const domain::AuthorId& author_id, const std::string& title, int year) override;
    std::vector<domain::Book> GetBooks() const override;
    std::vector<domain::Book> GetBooksByAuthor(const domain::AuthorId& author_id) const override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app