#include "database.h"

Database::Database(const std::string& db_url) : conn_(db_url) {
    EnsureTables();
}

void Database::EnsureTables() {
    pqxx::work w(conn_);
    w.exec("CREATE TABLE IF NOT EXISTS retired_players ("
           "id SERIAL PRIMARY KEY,"
           "name TEXT NOT NULL,"
           "score INTEGER NOT NULL,"
           "play_time_ms BIGINT NOT NULL)");
    w.exec("CREATE INDEX IF NOT EXISTS idx_retired_players_score_time_name "
           "ON retired_players(score DESC, play_time_ms ASC, name ASC)");
    w.commit();
}

void Database::SaveRetiredPlayer(const std::string& name, int score, int64_t play_time_ms) {
    pqxx::work w(conn_);
    w.exec_params("INSERT INTO retired_players (name, score, play_time_ms) VALUES ($1, $2, $3)",
                  name, score, play_time_ms);
    w.commit();
}

std::vector<Record> Database::GetRecords(int start, int max_items) {
    pqxx::read_transaction r(conn_);
    auto result = r.exec_params(
        "SELECT name, score, play_time_ms FROM retired_players "
        "ORDER BY score DESC, play_time_ms ASC, name ASC "
        "LIMIT $1 OFFSET $2",
        max_items, start
    );
    std::vector<Record> records;
    for (const auto& row : result) {
        records.push_back({
            row[0].as<std::string>(),
            row[1].as<int>(),
            row[2].as<int64_t>() / 1000.0
        });
    }
    return records;
}