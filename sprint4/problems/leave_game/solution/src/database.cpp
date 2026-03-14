#include "database.h"
#include <chrono>
#include <stdexcept>

namespace db {

void Database::Initialize() {
    auto conn = pool_.GetConnection();
    pqxx::work tx(*conn);
    tx.exec(R"(
        CREATE TABLE IF NOT EXISTS retired_players (
            id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
            name VARCHAR(100) NOT NULL,
            score INTEGER NOT NULL,
            play_time_ms INTEGER NOT NULL
        )
    )");
    tx.exec(R"(
        CREATE INDEX IF NOT EXISTS retired_players_score_play_time_name_idx
        ON retired_players (score DESC, play_time_ms ASC, name ASC)
    )");
    tx.commit();
}

void Database::InsertRetiredPlayer(const RetiredPlayerRecord& record) {
    auto conn = pool_.GetConnection();
    pqxx::work tx(*conn);
    tx.exec_params(
        "INSERT INTO retired_players (name, score, play_time_ms) VALUES ($1, $2, $3)",
        record.name, record.score, record.play_time.count()
    );
    tx.commit();
}

std::vector<RetiredPlayerRecord> Database::GetRecords(size_t start, size_t max_items) {
    auto conn = pool_.GetConnection();
    pqxx::work tx(*conn);
    auto result = tx.exec_params(
        "SELECT name, score, play_time_ms FROM retired_players "
        "ORDER BY score DESC, play_time_ms ASC, name ASC "
        "OFFSET $1 LIMIT $2",
        start, max_items
    );
    tx.commit();

    std::vector<RetiredPlayerRecord> records;
    records.reserve(result.size());
    for (const auto& row : result) {
        RetiredPlayerRecord rec;
        rec.name = row[0].as<std::string>();
        rec.score = row[1].as<int>();
        rec.play_time = std::chrono::milliseconds(row[2].as<int>());
        records.push_back(std::move(rec));
    }
    return records;
}

} // namespace db