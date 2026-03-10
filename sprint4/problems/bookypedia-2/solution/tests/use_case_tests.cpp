#include <catch2/catch_test_macros.hpp>
#include <pqxx/pqxx>
#include <cstdlib>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"
#include "../src/domain/tag.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }
    void Delete(const domain::AuthorId&) override {}
    std::vector<domain::Author> GetAllAuthors() const override { return {}; }
    std::optional<domain::Author> GetByName(const std::string&) const override { return std::nullopt; }
    std::optional<domain::Author> GetById(const domain::AuthorId&) const override { return std::nullopt; }
};

struct MockBookRepository : domain::BookRepository {
    void Save(const domain::Book&) override {}
    void Delete(const domain::BookId&) override {}
    void DeleteByAuthor(const domain::AuthorId&) override {}
    std::vector<domain::Book> GetAllBooks() const override { return {}; }
    std::vector<domain::Book> GetBooksByAuthor(const domain::AuthorId&) const override { return {}; }
    std::vector<domain::Book> GetBooksByTitle(const std::string&) const override { return {}; }
    std::optional<domain::Book> GetById(const domain::BookId&) const override { return std::nullopt; }
};

struct MockTagRepository : domain::TagRepository {
    void Save(const domain::Tag&) override {}
    void DeleteByBook(const domain::BookId&) override {}
    std::vector<domain::Tag> GetByBook(const domain::BookId&) const override { return {}; }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
    MockTagRepository tags;
    pqxx::connection conn;

    Fixture() : conn([]{
        const char* url = std::getenv("BOOKYPEDIA_DB_URL");
        if (!url) throw std::runtime_error("BOOKYPEDIA_DB_URL not set");
        return pqxx::connection(url);
    }()) {}
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{authors, books, tags, conn};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }
    }
}