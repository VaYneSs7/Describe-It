#include "crow.h"
#include <sqlite3.h>
#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <shellapi.h>

// Link the necessary libraries
#pragma comment(lib, "sqlite3.lib")
#pragma comment(lib, "shell32.lib")

sqlite3* db;

// Function to ensure the History table exists
void setupHistoryTable() {
    const char* sql = "CREATE TABLE IF NOT EXISTS History ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "Word TEXT, "
        "Description TEXT, "
        "Date DATETIME DEFAULT CURRENT_TIMESTAMP);";
    sqlite3_exec(db, sql, 0, 0, 0);
}

// Function to fetch a random word from your 466K list
std::string fetchRandomWord() {
    sqlite3_stmt* stmt;
    std::string word = "No word found";
    const char* sql = "SELECT Word FROM Words ORDER BY RANDOM() LIMIT 1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            word = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);
    return word;
}

int main() {
    // 1. Open Database
    if (sqlite3_open("dictionary.db", &db) != SQLITE_OK) {
        std::cerr << "CRITICAL: Could not open dictionary.db" << std::endl;
        return 1;
    }
    setupHistoryTable();

    crow::SimpleApp app;

    // 2. Route to serve the "tuff" Luxury Interface
    CROW_ROUTE(app, "/")
        ([]() {
        std::ifstream in("index.html", std::ios::in | std::ios::binary);
        if (!in) {
            std::cerr << "ERROR: index.html not found in project folder!" << std::endl;
            return crow::response(404, "index.html missing from server folder.");
        }
        std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return crow::response(contents);
            });

    // 3. API Route: Get random word
    CROW_ROUTE(app, "/api/random")
        ([]() {
        crow::json::wvalue response;
        response["word"] = fetchRandomWord();
        crow::response res(response);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
            });

    // 4. API Route: Save description
    CROW_ROUTE(app, "/api/submit").methods(crow::HTTPMethod::POST)
        ([](const crow::request& req) {
        auto data = crow::json::load(req.body);
        if (!data) return crow::response(400);

        std::string word = data["word"].s();
        std::string description = data["description"].s();

        sqlite3_stmt* stmt;
        const char* sql = "INSERT INTO History (Word, Description) VALUES (?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, word.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);

        std::cout << "Saved to DB: " << word << std::endl;
        crow::response res(200);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
            });

    // API Route: Fetch all history entries
    CROW_ROUTE(app, "/api/history")
        ([]() {
        sqlite3_stmt* stmt;
        const char* sql = "SELECT Word, Description, Date FROM History ORDER BY ID DESC;";
        crow::json::wvalue response;
        std::vector<crow::json::wvalue> history_list;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                crow::json::wvalue item;
                item["word"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                item["description"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                item["date"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                history_list.push_back(std::move(item));
            }
        }
        sqlite3_finalize(stmt);

        response["history"] = std::move(history_list);
        crow::response res(response);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
            });

    // 5. Auto-Launch Browser
    std::cout << "Starting Server... Launching UI." << std::endl;
    ShellExecuteA(NULL, "open", "http://localhost:18080", NULL, NULL, SW_SHOWNORMAL);

    app.port(18080).multithreaded().run();

    sqlite3_close(db);
    return 0;
}
