#pragma once
#include "unit_of_work.h"
#include "use_cases.h"
#include <memory>
#include <functional>

namespace app {
class UseCasesImpl : public UseCases {
public:
    using UnitOfWorkFactory = std::function<std::unique_ptr<UnitOfWork>()>;
    explicit UseCasesImpl(UnitOfWorkFactory factory) : uow_factory_(std::move(factory)) { }
    void AddAuthor(const std::string& name) override;
    void EditAuthor(const std::string& author_id, const std::string& new_name) override;
    void DeleteAuthor(const std::string& author_id) override;
    std::vector<AuthorInfo> GetAllAuthors() override;
    std::optional<AuthorInfo> FindAuthorByName(const std::string& name) override;
    void AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::vector<std::string>& tags={}) override;
    void EditBook(const std::string& book_id, const std::string& title, int publication_year, const std::vector<std::string>& tags) override;
    void DeleteBook(const std::string& book_id) override;
    std::vector<BookInfo> GetAllBooks() override;
    std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) override;
    std::vector<BookInfo> FindBooksByTitle(const std::string& title) override;
    std::optional<BookInfo> GetBookById(const std::string& book_id) override;

private:
    UnitOfWorkFactory uow_factory_;
};


}  // namespace app
