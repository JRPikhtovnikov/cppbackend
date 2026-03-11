#include "unit_of_work_impl.h"
#include <stdexcept>

namespace postgres {

UnitOfWorkImpl::UnitOfWorkImpl(pqxx::connection& connection) : connection_(connection)
    , work_(std::make_unique<pqxx::work>(connection_)), authors_(*work_), books_(*work_) {}

UnitOfWorkImpl::~UnitOfWorkImpl() {
    if (!committed_ && work_) {
        try {
            work_->abort();
        } catch (...) {
            // Игнорируем ошибки в деструкторе
        }
    }
}

void UnitOfWorkImpl::Commit() {
    if (!work_) {
        throw std::logic_error("Unit of work already committed");
    }
    work_->commit();
    committed_ = true;
    work_.reset();
}

domain::AuthorRepository& UnitOfWorkImpl::Authors() {
    return authors_;
}

domain::BookRepository& UnitOfWorkImpl::Books() {
    return books_;
}

}  // namespace postgres
