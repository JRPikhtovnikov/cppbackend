#pragma once
#include <string>

#include "../util/tagged_uuid.h"
#include <vector>
#include <optional>

namespace domain {

namespace detail {
struct AuthorTag {};
}  // namespace detail

using AuthorId = util::TaggedUUID<detail::AuthorTag>;

class Author {
public:
    Author(AuthorId id, std::string name) : id_(std::move(id)), name_(std::move(name)) { }

    const AuthorId& GetId() const noexcept {
        return id_;
    }

    void SetName(const std::string& name) {
        name_ = name;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

private:
    AuthorId id_;
    std::string name_;
};


class AuthorRepository {
public:
    virtual void Save(const Author& author) = 0;
    virtual void Update(const Author& author) = 0;
    virtual void Delete(const AuthorId& id) = 0;
    virtual std::vector<std::pair<AuthorId, std::string>> GetAll() = 0;
    virtual std::optional<Author> FindByName(const std::string& name) = 0;
    virtual std::optional<Author> FindById(const AuthorId& id) = 0;
    virtual ~AuthorRepository() = default;
};

}  // namespace domain
