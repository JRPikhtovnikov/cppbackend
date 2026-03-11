#pragma once
#include "../domain/author_fwd.h"
#include "../domain/book_fwd.h"

namespace app {

class UnitOfWork {
public:
    virtual ~UnitOfWork() = default;

    virtual void Commit() = 0;
    virtual domain::AuthorRepository& Authors() = 0;
    virtual domain::BookRepository& Books() = 0;
};

}  // namespace domain
