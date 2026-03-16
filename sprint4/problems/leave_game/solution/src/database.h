#pragma once

#include <pqxx/pqxx>
#include <string>
#include <vector>

// Определяем структуру Record до класса Database
struct Record {
    std::string name;
    int score;
    double play_time; // в секундах
};

class Database {
public:
    explicit Database(const std::string& db_url);
    
    // Запрещаем копирование
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    // Разрешаем перемещение
    Database(Database&&) = default;
    Database& operator=(Database&&) = default;

    void SaveRetiredPlayer(const std::string& name, int score, int64_t play_time_ms);
    std::vector<Record> GetRecords(int start, int max_items);

private:
    void EnsureTables();
    void PrepareStatements();

    pqxx::connection conn_;
};