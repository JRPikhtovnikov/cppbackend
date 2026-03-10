#include "use_cases_impl.h"
#include <pqxx/transaction>

namespace app {

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save(domain::Author(domain::AuthorId::New(), name));
}

void UseCasesImpl::DeleteAuthor(const domain::AuthorId& author_id) {
    authors_.Delete(author_id);
}

void UseCasesImpl::EditAuthor(const domain::AuthorId& author_id, const std::string& new_name) {
    auto author = authors_.GetById(author_id);
    if (!author) return;
    authors_.Save(domain::Author(author_id, new_name));  // обновление через Save с тем же id
}

std::vector<domain::Author> UseCasesImpl::GetAllAuthors() const {
    return authors_.GetAllAuthors();
}

std::optional<domain::Author> UseCasesImpl::GetAuthorByName(const std::string& name) const {
    return authors_.GetByName(name);
}

std::optional<domain::Author> UseCasesImpl::GetAuthorById(const domain::AuthorId& author_id) const {
    return authors_.GetById(author_id);
}

void UseCasesImpl::AddBook(const domain::AuthorId& author_id,
                           const std::string& title,
                           int year,
                           const std::vector<std::string>& tags) {
    pqxx::work w(connection_);
    domain::BookId book_id = domain::BookId::New();
    books_.Save(domain::Book(book_id, author_id, title, year));
    for (const auto& t : tags) {
        tags_.Save(domain::Tag(book_id, t));
    }
    w.commit();
}

void UseCasesImpl::DeleteBook(const domain::BookId& book_id) {
    books_.Delete(book_id);

void UseCasesImpl::EditBook(const domain::BookId& book_id,
                            const std::string& new_title,
                            int new_year,
                            const std::vector<std::string>& new_tags) {
    pqxx::work w(connection_);
    auto book_opt = books_.GetById(book_id);
    if (!book_opt) return;
    books_.Save(domain::Book(book_id, book_opt->GetAuthorId(), new_title, new_year));
    tags_.DeleteByBook(book_id);
    for (const auto& t : new_tags) {
        tags_.Save(domain::Tag(book_id, t));
    }
    w.commit();
}

std::vector<domain::Book> UseCasesImpl::GetAllBooks() const {
    return books_.GetAllBooks();
}

std::vector<domain::Book> UseCasesImpl::GetBooksByTitle(const std::string& title) const {
    return books_.GetBooksByTitle(title);
}

std::optional<domain::Book> UseCasesImpl::GetBookById(const domain::BookId& book_id) const {
    return books_.GetById(book_id);
}

std::vector<domain::Book> UseCasesImpl::GetBooksByAuthor(const domain::AuthorId& author_id) const {
    return books_.GetBooksByAuthor(author_id);
}

std::vector<std::string> UseCasesImpl::GetTagsByBook(const domain::BookId& book_id) const {
    std::vector<std::string> result;
    auto tags = tags_.GetByBook(book_id);
    for (const auto& t : tags) {
        result.push_back(t.GetTag());
    }
    return result;
}

}  // namespace app