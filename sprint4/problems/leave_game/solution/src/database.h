#pragma once

#include <pqxx/pqxx>
#include <string>
#include <vector>

class Database {
public:
    explicit Database(const std::string& db_url);

    void SaveRetiredPlayer(const std::string& name, int score, int64_t play_time_ms);
    std::vector<Record> GetRecords(int start, int max_items);

private:
    void EnsureTables();

    pqxx::connection conn_;
};

struct Record {
    std::string name;
    int score;
    double play_time; // в секундах
};