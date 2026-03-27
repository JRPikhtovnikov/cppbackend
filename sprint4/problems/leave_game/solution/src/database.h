#pragma once

#include <pqxx/pqxx>
#include <string>
#include <vector>

struct Record {
    std::string name;
    int score;
    double play_time;
};

class Database {
public:
    explicit Database(const std::string& db_url);
    
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    Database(Database&&) = default;
    Database& operator=(Database&&) = default;

    void SaveRetiredPlayer(const std::string& name, int score, int64_t play_time_ms);
    std::vector<Record> GetRecords(int start, int max_items);

private:
    void EnsureTables();
    void PrepareStatements();

    pqxx::connection conn_;
};