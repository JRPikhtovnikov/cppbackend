#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <map>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

// ---------- MockAuthorRepository ----------
struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;
    std::map<domain::AuthorId, domain::Author> authors_by_id;
    std::map<std::string, domain::Author> authors_by_name;

    void Save(const domain::Author& author) override {
        saved_authors.push_back(author);
        authors_by_id[author.GetId()] = author;
        authors_by_name[author.GetName()] = author;
    }

    void Update(const domain::Author& author) override {
        // Удаляем старую запись по имени (если имя изменилось)
        for (auto it = authors_by_name.begin(); it != authors_by_name.end(); ) {
            if (it->second.GetId() == author.GetId()) {
                authors_by_name.erase(it);
                break;
            } else {
                ++it;
            }
        }
        authors_by_id[author.GetId()] = author;
        authors_by_name[author.GetName()] = author;
    }

    void Delete(const domain::AuthorId& id) override {
        auto it = authors_by_id.find(id);
        if (it != authors_by_id.end()) {
            authors_by_name.erase(it->second.GetName());
            authors_by_id.erase(it);
        }
    }

    std::vector<std::pair<domain::AuthorId, std::string>> GetAll() override {
        std::vector<std::pair<domain::AuthorId, std::string>> result;
        for (const auto& [id, author] : authors_by_id) {
            result.emplace_back(id, author.GetName());
        }
        return result;
    }

    std::optional<domain::Author> FindByName(const std::string& name) override {
        auto it = authors_by_name.find(name);
        if (it != authors_by_name.end()) return it->second;
        return std::nullopt;
    }

    std::optional<domain::Author> FindById(const domain::AuthorId& id) override {
        auto it = authors_by_id.find(id);
        if (it != authors_by_id.end()) return it->second;
        return std::nullopt;
    }
};

// ---------- MockBookRepository ----------
struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;

    void Save(const domain::Book& book) override {
        saved_books.push_back(book);
    }

    void Update(const domain::Book& book) override {
        // Для простоты можно удалить старую и добавить новую, но в тестах пока не нужно
    }

    void Delete(const domain::BookId& id) override {
        // Пока не требуется
    }

    std::vector<std::pair<domain::AuthorId, std::string>> GetAllAuthors() override {
        return {};
    }

    std::vector<domain::Book> GetBooks() override {
        return saved_books;
    }

    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetAuthorId() == author_id) result.push_back(book);
        }
        return result;
    }

    std::optional<domain::Book> FindById(const domain::BookId& id) override {
        for (const auto& book : saved_books) {
            if (book.GetId() == id) return book;
        }
        return std::nullopt;
    }

    std::vector<domain::Book> FindByTitle(const std::string& title) override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetTitle() == title) result.push_back(book);
        }
        return result;
    }
};

// ---------- MockUnitOfWork ----------
struct MockUnitOfWork : app::UnitOfWork {
    MockAuthorRepository authors;
    MockBookRepository books;
    bool committed = false;

    void Commit() override {
        committed = true;
    }

    domain::AuthorRepository& Authors() override {
        return authors;
    }

    domain::BookRepository& Books() override {
        return books;
    }
};

// ---------- Fixture ----------
struct Fixture {
    // Указатель на текущий UoW, будет заполнен фабрикой
    MockUnitOfWork* current_uow = nullptr;

    // Фабрика, создающая MockUnitOfWork и сохраняющая указатель
    auto MakeUnitOfWorkFactory() {
        return [this]() -> std::unique_ptr<app::UnitOfWork> {
            auto uow = std::make_unique<MockUnitOfWork>();
            current_uow = uow.get();
            return uow;
        };
    }
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        auto factory = MakeUnitOfWorkFactory();
        app::UseCasesImpl use_cases{std::move(factory)};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(current_uow != nullptr);
                auto& authors = current_uow->authors;
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }
    }
}