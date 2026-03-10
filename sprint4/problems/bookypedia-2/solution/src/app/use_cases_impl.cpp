#include "use_cases_impl.h"
#include <pqxx/transaction>

namespace app {

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save(domain::Author(domain::AuthorId::New(), name));
}

void UseCasesImpl::DeleteAuthor(const domain::AuthorId& author_id) {
    pqxx::work w(connection_);
    w.exec_params("DELETE FROM book_tags WHERE book_id IN (SELECT id FROM books WHERE author_id = $1)", author_id.ToString());
    w.exec_params("DELETE FROM books WHERE author_id = $1", author_id.ToString());
    w.exec_params("DELETE FROM authors WHERE id = $1", author_id.ToString());
    w.commit();
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
    
    w.exec_params(
        "INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)",
        book_id.ToString(), author_id.ToString(), title, year);
    
    for (const auto& tag : tags) {
        w.exec_params(
            "INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)",
            book_id.ToString(), tag);
    }
    
    w.commit();
}

void UseCasesImpl::DeleteBook(const domain::BookId& book_id) {
    pqxx::work w(connection_);
    w.exec_params("DELETE FROM book_tags WHERE book_id = $1", book_id.ToString());
    w.exec_params("DELETE FROM books WHERE id = $1", book_id.ToString());
    w.commit();
}

void UseCasesImpl::EditBook(const domain::BookId& book_id,
                            const std::string& new_title,
                            int new_year,
                            const std::vector<std::string>& new_tags) {
    // Проверка существования книги (можно выполнить в той же транзакции, но для простоты оставим)
    auto book_opt = books_.GetById(book_id);
    if (!book_opt) return;

    pqxx::work w(connection_);
    
    w.exec_params(
        "UPDATE books SET title = $1, publication_year = $2 WHERE id = $3",
        new_title, new_year, book_id.ToString());
    
    w.exec_params("DELETE FROM book_tags WHERE book_id = $1", book_id.ToString());
    
    for (const auto& tag : new_tags) {
        w.exec_params(
            "INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)",
            book_id.ToString(), tag);
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