#include "database.h"
#include <stdexcept>
#include <iostream>

Database::Database(const std::string& db_url) : conn_(db_url) {
    EnsureTables();
    PrepareStatements();
}

void Database::EnsureTables() {
    pqxx::work w(conn_);
    
    // Создаем таблицу, если её нет
    w.exec(
        "CREATE TABLE IF NOT EXISTS retired_players ("
        "id SERIAL PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "score INTEGER NOT NULL,"
        "play_time_ms BIGINT NOT NULL)"
    );
    
    // Создаем индекс для быстрой сортировки по score DESC, play_time_ms ASC, name ASC
    w.exec(
        "CREATE INDEX IF NOT EXISTS idx_retired_players_score_time_name "
        "ON retired_players(score DESC, play_time_ms ASC, name ASC)"
    );
    
    w.commit();
}

void Database::PrepareStatements() {
    // Подготавливаем запросы для повышения производительности
    conn_.prepare("insert_retired_player",
        "INSERT INTO retired_players (name, score, play_time_ms) "
        "VALUES ($1, $2, $3)");
    
    conn_.prepare("get_records",
        "SELECT name, score, play_time_ms FROM retired_players "
        "ORDER BY score DESC, play_time_ms ASC, name ASC "
        "LIMIT $1 OFFSET $2");
}

void Database::SaveRetiredPlayer(const std::string& name, int score, int64_t play_time_ms) {
    try {
        pqxx::work w(conn_);
        w.exec_prepared("insert_retired_player", name, score, play_time_ms);
        w.commit();
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to save retired player: " + std::string(e.what()));
    }
}

std::vector<Record> Database::GetRecords(int start, int max_items) {
    std::vector<Record> records;
    
    try {
        pqxx::read_transaction r(conn_);
        
        auto result = r.exec_prepared("get_records", max_items, start);
        
        for (const auto& row : result) {
            Record rec;
            rec.name = row[0].as<std::string>();
            rec.score = row[1].as<int>();
            // Конвертируем миллисекунды в секунды с плавающей точкой
            rec.play_time = row[2].as<int64_t>() / 1000.0;
            records.push_back(std::move(rec));
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to get records: " + std::string(e.what()));
    }
    
    return records;
}