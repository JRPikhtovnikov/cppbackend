#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"
#include <stdexcept>

#include <iostream>

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    auto uow = uow_factory_();
    auto& authors = uow->Authors();
    authors.Save({domain::AuthorId::New(), name});
    uow->Commit();
}

void UseCasesImpl::EditAuthor(const std::string& author_id, const std::string& new_name) {
    auto uow = uow_factory_();
    auto& authors = uow->Authors();
    AuthorId id = AuthorId::FromString(author_id);
    auto author = authors.FindById(id);
    if (!author) {
        throw std::runtime_error("Author not found");
    }
    author->SetName(new_name);
    authors.Update(*author);
    uow->Commit();
}

void UseCasesImpl::DeleteAuthor(const std::string& author_id) {
   try {
        auto uow = uow_factory_();
        auto& authors = uow->Authors();
        AuthorId id = AuthorId::FromString(author_id);
        authors.Delete(id);
        uow->Commit();
   }
   catch(const std::runtime_error& e) {
       throw std::runtime_error(e.what());
   }
}

std::vector<AuthorInfo> UseCasesImpl::GetAllAuthors() {
    auto uow = uow_factory_();
    auto& authors = uow->Authors();
    auto author_pairs = authors.GetAll();
    std::vector<AuthorInfo> result;
    result.reserve(author_pairs.size());
    for (const auto& [id, name] : author_pairs) {
        result.push_back({id.ToString(), name});
    }
    return result;
}

std::optional<AuthorInfo> UseCasesImpl::FindAuthorByName(const std::string& name) {
    auto uow = uow_factory_();
    auto& authors = uow->Authors();
    auto author = authors.FindByName(name);
    if (!author) {
        return std::nullopt;
    }
    return AuthorInfo{author->GetId().ToString(), author->GetName()};
}



void UseCasesImpl::AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::vector<std::string>& tags) {
    auto uow = uow_factory_();
    auto& books = uow->Books();
    AuthorId auth_id = AuthorId::FromString(author_id);
    std::vector<std::string> tag_infos;
    tag_infos.reserve(tags.size());
    for (const auto& tag : tags) {
        tag_infos.push_back(tag);
    }
    Book book{BookId::New(), auth_id, title, publication_year, std::move(tag_infos)};
    books.Save(book);
    uow->Commit();
}

void UseCasesImpl::EditBook(const std::string& book_id, const std::string& title, int publication_year, const std::vector<std::string>& tags) {
    auto uow = uow_factory_();
    auto& books = uow->Books();
    BookId id = BookId::FromString(book_id);
    auto book = books.FindById(id);
    if (!book) {
        throw std::runtime_error("Book not found");
    }
    book->SetTitle(title);
    book->SetPublicationYear(publication_year);
    std::vector<std::string> tag_infos;
    tag_infos.reserve(tags.size());
    for (const auto& tag : tags) {
        tag_infos.push_back({tag});
    }
    book->SetTags(std::move(tag_infos));
    books.Update(*book);
    uow->Commit();
}

void UseCasesImpl::DeleteBook(const std::string& book_id) {
    try{
        auto uow = uow_factory_();
        auto& books = uow->Books();
        BookId id = BookId::FromString(book_id);
        books.Delete(id);
        uow->Commit();
    } catch(const std::runtime_error& e) {
        throw std::runtime_error(e.what());
    }
}

std::vector<BookInfo> UseCasesImpl::GetAllBooks() {
    auto uow = uow_factory_();
    auto& books = uow->Books();
    auto book_list = books.GetBooks();
    std::vector<BookInfo> result;
    result.reserve(book_list.size());
    for (const auto& book : book_list) {
        std::vector<std::string> tags;
        tags.reserve(book.GetTags().size());
        for (const auto& tag : book.GetTags()) {
            tags.push_back(tag);
        }
        auto& authors = uow->Authors();
        auto author = authors.FindById(book.GetAuthorId());
        result.push_back({
            book.GetId().ToString(),
            author ? author->GetName() : "Unknown",
            book.GetTitle(),
            book.GetPublicationYear(),
            std::move(tags)
        });
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) {
    AuthorId auth_id = AuthorId::FromString(author_id);
    auto uow = uow_factory_();
    auto& books = uow->Books();
    auto book_list = books.GetAuthorBooks(auth_id);
    std::vector<BookInfo> result;
    result.reserve(book_list.size());
    auto& authors = uow->Authors();
    auto author = authors.FindById(auth_id);
    std::string author_name = author ? author->GetName() : "Unknown";
    for (const auto& book : book_list) {
        std::vector<std::string> tags;
        tags.reserve(book.GetTags().size());
        for (const auto& tag : book.GetTags()) {
            tags.push_back(tag);
        }
        result.push_back({
            book.GetId().ToString(),
            author_name,
            book.GetTitle(),
            book.GetPublicationYear(),
            std::move(tags)
        });
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::FindBooksByTitle(const std::string& title) {
    auto uow = uow_factory_();
    auto& books = uow->Books();
    //auto book_list = books.FindBooksByTitle(title);
    auto book_list = books.FindByTitle(title);
    std::vector<BookInfo> result;
    result.reserve(book_list.size());
    auto& authors = uow->Authors();
    for (const auto& book : book_list) {
        std::vector<std::string> tags;
        tags.reserve(book.GetTags().size());
        for (const auto& tag : book.GetTags()) {
            tags.push_back(tag);
        }
        auto author = authors.FindById(book.GetAuthorId());
        result.push_back({
            book.GetId().ToString(),
            //book.GetAuthorId().ToString(),
            author ? author->GetName() : "Unknown",
            book.GetTitle(),
            book.GetPublicationYear(),
            std::move(tags)
        });
    }
    return result;
}

std::optional<BookInfo> UseCasesImpl::GetBookById(const std::string& book_id) {
    BookId id = BookId::FromString(book_id);
    auto uow = uow_factory_();
    auto& books = uow->Books();
    auto book = books.FindById(id);
    if (!book) {
        return std::nullopt;
    }
    std::vector<std::string> tags;
    tags.reserve(book->GetTags().size());
    for (const auto& tag : book->GetTags()) {
        tags.push_back(tag);
    }
    auto& authors = uow->Authors();
    auto author = authors.FindById(book->GetAuthorId());
    return BookInfo{
        book->GetId().ToString(),
        //book->GetAuthorId().ToString(),
        author ? author->GetName() : "Unknown",
        book->GetTitle(),
        book->GetPublicationYear(),
        std::move(tags)
    };
}

}  // namespace app
