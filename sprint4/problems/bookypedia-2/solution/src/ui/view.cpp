#include "view.h"
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <regex>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include "../app/use_cases.h"
#include "../menu/menu.h"
#include "../domain/author.h" 
#include "../domain/book.h"  

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << " by " << book.author_name << ", " << book.publication_year;
    return out;
}

}  // namespace detail

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}, use_cases_{use_cases}, input_{input}, output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s,
                    std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                    std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s,
                    std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s,
                    std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                    std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Delete author and all his/her books"s,
                    std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "[name]"s, "Edit author's name"s,
                    std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "[title]"s, "Delete a book"s,
                    std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "[title]"s, "Edit book details"s,
                    std::bind(&View::EditBook, this, ph::_1));
    menu_.AddAction("ShowBook"s, "[title]"s, "Show book details"s,
                    std::bind(&View::ShowBook, this, ph::_1));
}

// ---------- Вспомогательные методы ----------

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> result;
    for (const auto& author : use_cases_.GetAllAuthors()) {
        result.push_back({author.GetId().ToString(), author.GetName()});
    }
    return result;
}

std::optional<detail::AuthorInfo> View::SelectAuthor() const {
    auto authors = GetAuthors();
    if (authors.empty()) return std::nullopt;
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel:" << std::endl;
    std::string line;
    if (!std::getline(input_, line) || line.empty()) return std::nullopt;
    int idx = std::stoi(line) - 1;
    if (idx < 0 || idx >= static_cast<int>(authors.size()))
        throw std::runtime_error("Invalid author number");
    return authors[idx];
}

std::optional<domain::AuthorId> View::SelectAuthorId() const {
    auto author_opt = SelectAuthor();
    if (!author_opt) return std::nullopt;
    return domain::AuthorId::FromString(author_opt->id);
}

std::vector<detail::BookInfo> View::GetBooks() const {
    std::vector<detail::BookInfo> result;
    for (const auto& book : use_cases_.GetAllBooks()) {
        auto author = use_cases_.GetAuthorById(book.GetAuthorId());
        if (author) {
            result.push_back({book.GetId().ToString(),
                              book.GetTitle(),
                              author->GetName(),
                              book.GetPublicationYear()});
        }
    }
    std::sort(result.begin(), result.end(),
        [](const detail::BookInfo& a, const detail::BookInfo& b) {
            if (a.title != b.title) return a.title < b.title;
            if (a.author_name != b.author_name) return a.author_name < b.author_name;
            return a.publication_year < b.publication_year;
        });
    return result;
}

std::vector<detail::BookInfo> View::GetAuthorBooks(const domain::AuthorId& author_id) const {
    std::vector<detail::BookInfo> result;
    auto author_opt = use_cases_.GetAuthorById(author_id);
    if (!author_opt) return result;
    for (const auto& book : use_cases_.GetBooksByTitle("")) { // TODO: исправить
        // На самом деле нужно получать книги по автору
        // Для этого добавим метод в use_cases: GetBooksByAuthorId
        // Пока заглушка
    }
    return result;
}

std::optional<detail::BookInfo> View::SelectBookByTitle(const std::string& title) const {
    auto books = GetBooks();
    std::vector<detail::BookInfo> matching;
    for (const auto& b : books) {
        if (b.title == title) matching.push_back(b);
    }
    if (matching.empty()) return std::nullopt;
    if (matching.size() == 1) return matching[0];
    PrintVector(output_, matching);
    output_ << "Enter the book # or empty line to cancel:" << std::endl;
    std::string line;
    if (!std::getline(input_, line) || line.empty()) return std::nullopt;
    int idx = std::stoi(line) - 1;
    if (idx < 0 || idx >= static_cast<int>(matching.size()))
        throw std::runtime_error("Invalid book number");
    return matching[idx];
}

std::optional<detail::BookInfo> View::SelectBookFromList() const {
    auto books = GetBooks();
    if (books.empty()) return std::nullopt;
    PrintVector(output_, books);
    output_ << "Enter the book # or empty line to cancel:" << std::endl;
    std::string line;
    if (!std::getline(input_, line) || line.empty()) return std::nullopt;
    int idx = std::stoi(line) - 1;
    if (idx < 0 || idx >= static_cast<int>(books.size()))
        throw std::runtime_error("Invalid book number");
    return books[idx];
}

std::vector<std::string> View::NormalizeTags(const std::string& input) const {
    std::vector<std::string> tags;
    boost::split(tags, input, boost::is_any_of(","), boost::token_compress_on);
    std::vector<std::string> result;
    for (auto& t : tags) {
        boost::algorithm::trim(t);
        // заменяем множественные пробелы на один
        t = std::regex_replace(t, std::regex("\\s+"), " ");
        if (!t.empty()) result.push_back(t);
    }
    // удаляем дубликаты
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

// ---------- Команды ----------

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        if (name.empty()) throw std::runtime_error("Empty author name");
        use_cases_.AddAuthor(name);
    } catch (const std::exception&) {
        output_ << "Failed to add author" << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        int year;
        if (!(cmd_input >> year)) throw std::runtime_error("Invalid year");
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        if (title.empty()) throw std::runtime_error("Empty title");

        output_ << "Enter author name or empty line to select from list:" << std::endl;
        std::string author_input;
        std::getline(input_, author_input);
        boost::algorithm::trim(author_input);

        domain::AuthorId author_id;
        if (author_input.empty()) {
            // Выбор из списка
            auto author_opt = SelectAuthorId();
            if (!author_opt) {
                // Отмена – ничего не выводим
                return true;
            }
            author_id = *author_opt;
        } else {
            // Поиск по имени
            auto author_opt = use_cases_.GetAuthorByName(author_input);
            if (author_opt) {
                author_id = author_opt->GetId();
            } else {
                output_ << "No author found. Do you want to add " << author_input << " (y/n)?" << std::endl;
                std::string answer;
                std::getline(input_, answer);
                boost::algorithm::trim(answer);
                if (answer == "y" || answer == "Y") {
                    use_cases_.AddAuthor(author_input);
                    author_opt = use_cases_.GetAuthorByName(author_input);
                    if (!author_opt) throw std::runtime_error("Failed to create author");
                    author_id = author_opt->GetId();
                } else {
                    output_ << "Failed to add book" << std::endl;
                    return true;
                }
            }
        }

        output_ << "Enter tags (comma separated):" << std::endl;
        std::string tags_line;
        std::getline(input_, tags_line);
        auto tags = NormalizeTags(tags_line);

        use_cases_.AddBook(author_id, title, year, tags);
    } catch (const std::exception&) {
        output_ << "Failed to add book" << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::optional<domain::AuthorId> author_id;
        if (name.empty()) {
            author_id = SelectAuthorId();
        } else {
            auto author_opt = use_cases_.GetAuthorByName(name);
            if (author_opt) author_id = author_opt->GetId();
        }

        if (!author_id) {
            output_ << "Failed to delete author" << std::endl;
            return true;
        }

        use_cases_.DeleteAuthor(*author_id);
    } catch (const std::exception&) {
        output_ << "Failed to delete author" << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        bool with_name = !name.empty();
        std::optional<domain::AuthorId> author_id;
        if (name.empty()) {
            author_id = SelectAuthorId();
        } else {
            auto author_opt = use_cases_.GetAuthorByName(name);
            if (author_opt) author_id = author_opt->GetId();
        }

        if (!author_id) {
            if (with_name) {
                output_ << "Failed to edit author" << std::endl;
            }
            return true;
        }

        output_ << "Enter new name:" << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        if (new_name.empty()) throw std::runtime_error("Empty name");

        use_cases_.EditAuthor(*author_id, new_name);
    } catch (const std::exception&) {
        output_ << "Failed to edit author" << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        bool with_title = !title.empty();
        std::optional<detail::BookInfo> book;
        if (with_title) {
            book = SelectBookByTitle(title);
        } else {
            book = SelectBookFromList();
        }

        if (!book) {
            if (with_title) {
                output_ << "Book not found" << std::endl;
            }
            return true;
        }

        use_cases_.DeleteBook(domain::BookId::FromString(book->id));
    } catch (const std::exception&) {
        output_ << "Failed to delete book" << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        bool with_title = !title.empty();
        std::optional<detail::BookInfo> book;
        if (title.empty()) {
            book = SelectBookFromList();
        } else {
            book = SelectBookByTitle(title);
        }

        if (!book) {
            if (with_title) {
                output_ << "Book not found" << std::endl;
            }
            return true;
        }

        auto book_id = domain::BookId::FromString(book->id);
        auto current_book = use_cases_.GetBookById(book_id);
        if (!current_book) {
            output_ << "Book not found" << std::endl;
            return true;
        }

        // Название
        output_ << "Enter new title or empty line to use the current one (" << current_book->GetTitle() << "):" << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        if (new_title.empty()) new_title = current_book->GetTitle();

        // Год
        output_ << "Enter publication year or empty line to use the current one (" << current_book->GetPublicationYear() << "):" << std::endl;
        std::string year_str;
        std::getline(input_, year_str);
        boost::algorithm::trim(year_str);
        int new_year = current_book->GetPublicationYear();
        if (!year_str.empty()) {
            new_year = std::stoi(year_str);
        }

        // Теги
        auto current_tags = use_cases_.GetTagsByBook(book_id);
        std::string tags_display;
        for (size_t i = 0; i < current_tags.size(); ++i) {
            if (i > 0) tags_display += ", ";
            tags_display += current_tags[i];
        }
        output_ << "Enter tags (current tags: " << tags_display << "):" << std::endl;
        std::string tags_line;
        std::getline(input_, tags_line);
        boost::algorithm::trim(tags_line);
        std::vector<std::string> new_tags;
        if (tags_line.empty()) {
            new_tags = current_tags;  // сохраняем текущие
        } else {
            new_tags = NormalizeTags(tags_line);
        }

        use_cases_.EditBook(book_id, new_title, new_year, new_tags);
    } catch (const std::exception&) {
        output_ << "Failed to edit book" << std::endl;
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::optional<detail::BookInfo> book;
        if (title.empty()) {
            book = SelectBookFromList();
        } else {
            book = SelectBookByTitle(title);
        }

        if (!book) {
            output_ << "Book not found" << std::endl;
            return true; 
        }

        auto book_id = domain::BookId::FromString(book->id);
        auto current_book = use_cases_.GetBookById(book_id);
        if (!current_book) return true;

        output_ << "Title: " << current_book->GetTitle() << std::endl;
        output_ << "Author: " << book->author_name << std::endl;
        output_ << "Publication year: " << current_book->GetPublicationYear() << std::endl;

        auto tags = use_cases_.GetTagsByBook(book_id);
        if (!tags.empty()) {
            output_ << "Tags: ";
            for (size_t i = 0; i < tags.size(); ++i) {
                if (i > 0) output_ << ", ";
                output_ << tags[i];
            }
            output_ << std::endl;
        }
    } catch (const std::exception&) {
        // игнорируем ошибки
    }
    return true;
}

bool View::ShowAuthors() const {
    PrintVector(output_, GetAuthors());
    return true;
}

bool View::ShowBooks() const {
    PrintVector(output_, GetBooks());
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        auto author_id_opt = SelectAuthorId();
        if (!author_id_opt) return true;
        auto author = use_cases_.GetAuthorById(*author_id_opt);
        if (!author) return true;
        auto books = use_cases_.GetBooksByAuthor(*author_id_opt);
        std::vector<detail::BookInfo> book_infos;
        for (const auto& b : books) {
            book_infos.emplace_back(
                b.GetId().ToString(),
                b.GetTitle(),
                author->GetName(),
                b.GetPublicationYear()
            );
        }
        PrintVector(output_, book_infos);
    } catch (const std::exception&) {
        output_ << "Failed to show author books" << std::endl;
    }
    return true;
}

}  // namespace ui