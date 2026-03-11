#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <iostream>

#include "../app/use_cases.h"
#include "../menu/menu.h"
#include "../util/tag_util.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << ", " << book.publication_year;
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


void SortBooks(std::vector<detail::BookInfo>& books) {
    std::sort(books.begin(), books.end(), [](const detail::BookInfo& a, const detail::BookInfo& b) {

        if (a.title != b.title) {
            return a.title < b.title;
        }
        if (a.author_name != b.author_name) {
            return a.author_name < b.author_name;
        }
        return a.publication_year < b.publication_year;
    });
}


void PrintBooks(std::ostream& out, const std::vector<detail::BookInfo>& books) {
    int i = 1;
    for (const auto& v : books) {
        out << i++ << " " << v.title << " by " << v.author_name << ", " << v.publication_year << std::endl;
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}, use_cases_{use_cases}, input_{input}, output_{output}
{
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "[name]"s, "Edits author"s, std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Deletes author"s, std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s, std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "[title]"s, "Edits book"s, std::bind(&View::EditBook, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "[title]"s, "Deletes book"s, std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowBook"s, "[title]"s, "Show book details"s, std::bind(&View::ShowBook, this, ph::_1));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        if (name.empty()) {
            output_ << "Failed to add author"sv << std::endl;
            return true;
        }
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string author_input;
        std::getline(cmd_input, author_input);
        boost::algorithm::trim(author_input);
        std::optional<std::string> author_id;
        if (author_input.empty()) {
            author_id = SelectAuthor();
        } else {
            author_id = GetAuthorIdFromInput(author_input);
        }
        if (!author_id) {
            output_ << "Failed to edit author" << std::endl;
            return true;
        }
        output_ << "Enter new name: " << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        if (new_name.empty()) {
            output_ << "Failed to edit author: name cannot be empty" << std::endl;
            return true;
        }
        use_cases_.EditAuthor(*author_id, new_name);
    } catch (const std::exception& e) {
        output_ << "Failed to edit author" << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string author_input;
        std::getline(cmd_input, author_input);
        boost::algorithm::trim(author_input);
        std::optional<std::string> author_id;
        if (author_input.empty()) {
            author_id = SelectAuthor();
        } else {
            author_id = GetAuthorIdFromInput(author_input);
        }
        if (!author_id) {
            output_ << "Failed to delete author" << std::endl;
            return true;//!!!
        }
        use_cases_.DeleteAuthor(*author_id);
    } catch (const std::exception& e) {

        return true;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        auto params = GetBookParams(cmd_input);
        if (params) {
            use_cases_.AddBook(params->title, params->publication_year, params->author_id, params->tags);
        }
    } catch (const std::exception& e) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title_input;
        std::getline(cmd_input, title_input);
        boost::algorithm::trim(title_input);
        std::optional<std::string> book_id;
        if (title_input.empty()) {
            book_id = SelectBook();
        } else {
            book_id = SelectBook(title_input);
        }
        if (!book_id) {
            output_ << "Book not found" << std::endl;
            return true;
        }
        auto book_info = use_cases_.GetBookById(*book_id);
        if (!book_info) {
            output_ << "Book not found" << std::endl;
            return true;
        }

        output_ << "Enter new title or empty line to use the current one ("
                << book_info->title << "): " << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        if (new_title.empty()) {
            new_title = book_info->title;
        }

        output_ << "Enter publication year or empty line to use the current one ("
                << book_info->publication_year << "): " << std::endl;
        std::string year_str;
        std::getline(input_, year_str);
        boost::algorithm::trim(year_str);
        int new_year = book_info->publication_year;
        if (!year_str.empty()) {
            try {
                new_year = std::stoi(year_str);
            } catch (...) {
                output_ << "Invalid year, keeping current value" << std::endl;
            }
        }

        std::string current_tags = utils::JoinTags(book_info->tags);
        output_ << "Enter tags (current tags: " << current_tags << "): " << std::endl;
        std::string tags_input;
        std::getline(input_, tags_input);

        std::vector<std::string> new_tags;

        new_tags = utils::NormalizeTags(tags_input);

        use_cases_.EditBook(*book_id, new_title, new_year, new_tags);
    } catch (const std::exception& e) {
        output_ << "Failed to edit book" << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title_input;
        std::getline(cmd_input, title_input);
        boost::algorithm::trim(title_input);
        std::optional<std::string> book_id;
        if (title_input.empty()) {
            book_id = SelectBook();
        } else {
            book_id = SelectBook(title_input);
        }
        if (!book_id) {

            return true;
        }
        use_cases_.DeleteBook(*book_id);
    } catch (const std::exception& e) {

    }
    return true;
}

bool View::ShowAuthors() const {
    auto authors = GetAuthors();
    if (!authors.empty()) {
        PrintVector(output_, authors);
    }
    return true;
}

bool View::ShowBooks() const {
    auto books = GetBooks();
    SortBooks(books);
    if (!books.empty()) {
        PrintBooks(output_, books);
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title_input;
        std::getline(cmd_input, title_input);
        boost::algorithm::trim(title_input);
        std::optional<std::string> book_id;
        if (title_input.empty()) {
            book_id = SelectBook();
        } else {
            book_id = SelectBook(title_input);
        }
        if (!book_id) {
            return true;
        }
        auto book_info = use_cases_.GetBookById(*book_id);
        if (!book_info) {
            output_ << "Book not found" << std::endl;
            return true;
        }
        output_ << "Title: " << book_info->title << std::endl;
        output_ << "Author: " << book_info->author_name << std::endl;
        output_ << "Publication year: " << book_info->publication_year << std::endl;
        if (!book_info->tags.empty()) {
            output_ << "Tags: " << utils::JoinTags(book_info->tags) << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show book" << std::endl;
    }
    return true;
}


std::optional<detail::AddBookParams> View::GetBookParams(std::istream& cmd_input) const {
    detail::AddBookParams params;

    if (!(cmd_input >> params.publication_year)) {
        return std::nullopt;
    }
    std::getline(cmd_input, params.title);
    boost::algorithm::trim(params.title);
    if (params.title.empty()) {
        return std::nullopt;
    }

    output_ << "Enter author name or empty line to select from list:" << std::endl;
    std::string author_input;
    std::getline(input_, author_input);
    boost::algorithm::trim(author_input);
    auto author_id = HandleAuthorInput(author_input);
    if (!author_id) {
        return std::nullopt;
    }
    params.author_id = *author_id;
    params.tags = PromptForTags();
    return params;
}

std::optional<std::string> View::HandleAuthorInput(const std::string& author_input) const {
    if (author_input.empty()) {
        return SelectAuthor("Select author:");
    }
    auto existing_author = use_cases_.FindAuthorByName(author_input);
    if (existing_author) {
        return existing_author->id;
    }
    output_ << "No author found. Do you want to add " << author_input << " (y/n)? " << std::endl;
    std::string answer;
    std::getline(input_, answer);
    boost::algorithm::trim(answer);
    if (answer != "y" && answer != "Y") {
        throw std::invalid_argument("Cancel add author");
    }
    use_cases_.AddAuthor(author_input);
    auto new_author = use_cases_.FindAuthorByName(author_input);
    if (new_author) {
        return new_author->id;
    }
    return std::nullopt;
}

std::optional<std::string> View::SelectAuthor(const std::string& prompt) const {
    output_ << prompt << std::endl;
    auto authors = GetAuthors();
    if (authors.empty()) {
        output_ << "No authors found" << std::endl;
        return std::nullopt;
    }
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;
    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }
    try {
        int author_idx = std::stoi(str) - 1;
        if (author_idx >= 0 && author_idx < static_cast<int>(authors.size())) {
            return authors[author_idx].id;
        }
    } catch (...) {}

    return std::nullopt;
}

std::optional<std::string> View::SelectBook(const std::string& title) const {
    std::vector<detail::BookInfo> books;
    if (title.empty()) {
        books = GetBooks();
    } else {
        auto app_books = use_cases_.FindBooksByTitle(title);
        books.reserve(app_books.size());
        for (const auto& book : app_books) {
            books.push_back({book.id, book.title, book.author_name, book.publication_year, {}});
        }
    }
    if (books.empty()) {
        return std::nullopt;
    }
    if (books.size() == 1 && !title.empty()) {
        return books[0].id;
    }
    SortBooks(books);
    PrintBooks(output_, books);
    output_ << "Enter the book # or empty line to cancel: " << std::endl;
    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }
    try {
        int book_idx = std::stoi(str) - 1;
        if (book_idx >= 0 && book_idx < static_cast<int>(books.size())) {
            return books[book_idx].id;
        }
    } catch (...) {}

    return std::nullopt;
}

std::optional<std::string> View::GetAuthorIdFromInput(const std::string& input) const {

    try {
        int author_idx = std::stoi(input) - 1;
        auto authors = GetAuthors();
        if (author_idx >= 0 && author_idx < static_cast<int>(authors.size())) {
            return authors[author_idx].id;
        }
    } catch (...) {}

    auto author = use_cases_.FindAuthorByName(input);
    if (!author) {
        return std::nullopt;
    }

    return author->id;
}

std::vector<std::string> View::PromptForTags(const std::string& current_tags) const {
    output_ << "Enter tags (comma separated): " << std::endl;
    std::string tags_input;
    std::getline(input_, tags_input);
    boost::algorithm::trim(tags_input);
    if (tags_input.empty()) {
        return {};
    }
    return utils::NormalizeTags(tags_input);
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> result;
    auto authors = use_cases_.GetAllAuthors();
    result.reserve(authors.size());

    for (const auto& author : authors) {
        result.push_back({author.id, author.name});
    }
    return result;
}

std::vector<detail::BookInfo> View::GetBooks() const {
    std::vector<detail::BookInfo> result;
    auto books = use_cases_.GetAllBooks();
    result.reserve(books.size());

    for (const auto& book : books) {
        result.push_back({
            book.id,
            book.title,
            book.author_name,
            book.publication_year,
            book.tags
        });
    }
    return result;
}


}  // namespace ui
