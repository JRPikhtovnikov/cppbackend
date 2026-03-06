#include "use_cases_impl.h"
#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({domain::AuthorId::New(), name});
}

std::vector<domain::Author> UseCasesImpl::GetAuthors() const {
    return authors_.GetAllAuthors();
}

void UseCasesImpl::AddBook(const domain::AuthorId& author_id, const std::string& title, int year) {
    books_.Save({domain::BookId::New(), author_id, title, year});
}

std::vector<domain::Book> UseCasesImpl::GetBooks() const {
    return books_.GetAllBooks();
}

std::vector<domain::Book> UseCasesImpl::GetBooksByAuthor(const domain::AuthorId& author_id) const {
    return books_.GetBooksByAuthor(author_id);
}

}  // namespace app