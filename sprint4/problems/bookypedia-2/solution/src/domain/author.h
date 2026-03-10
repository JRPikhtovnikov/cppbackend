#pragma once
#include <string>
#include <optional>
#include <vector>
#include "../util/tagged_uuid.h"

namespace domain {

namespace detail {
struct AuthorTag {};
}  // namespace detail

using AuthorId = util::TaggedUUID<detail::AuthorTag>;

class Author {
public:
    Author(AuthorId id, std::string name)
        : id_(std::move(id)), name_(std::move(name)) {}

    const AuthorId& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }

private:
    AuthorId id_;
    std::string name_;
};

class AuthorRepository {
public:
    virtual void Save(const Author& author) = 0;
    virtual void Delete(const AuthorId& author_id) = 0;
    virtual std::vector<Author> GetAllAuthors() const = 0;
    virtual std::optional<Author> GetByName(const std::string& name) const = 0;
    virtual std::optional<Author> GetById(const AuthorId& author_id) const = 0;

protected:
    ~AuthorRepository() = default;
};

}  // namespace domain