#pragma once
#include "../app/unit_of_work.h"
#include "../domain/author_fwd.h"
#include "../domain/book_fwd.h"
#include <pqxx/connection>
#include <pqxx/transaction>
#include "postgres.h"

namespace postgres {

class UnitOfWorkImpl : public app::UnitOfWork {
public:
    explicit UnitOfWorkImpl(pqxx::connection& connection);
    ~UnitOfWorkImpl() override;

    void Commit() override;
    domain::AuthorRepository& Authors() override;
    domain::BookRepository& Books() override;

private:
    pqxx::connection& connection_;
    std::unique_ptr<pqxx::work> work_;
    AuthorRepositoryImpl authors_;
    BookRepositoryImpl books_;
    bool committed_ = false;
};

}  // namespace postgres
