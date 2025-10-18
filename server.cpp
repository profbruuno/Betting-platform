#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <sqlite3.h>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

class BettingServer {
private:
    sqlite3* db;
    int server_fd;
    
public:
    BettingServer() : db(nullptr), server_fd(-1) {}
    
    bool initialize() {
        // Initialize database
        if (sqlite3_open("betting.db", &db) != SQLITE_OK) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        
        createTables();
        
        // Initialize socket
        #ifdef _WIN32
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                std::cerr << "WSAStartup failed" << std::endl;
                return false;
            }
        #endif
        
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "Socket creation failed" << std::endl;
            return false;
        }
        
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(8080);
        
        if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Bind failed" << std::endl;
            return false;
        }
        
        if (listen(server_fd, 10) < 0) {
            std::cerr << "Listen failed" << std::endl;
            return false;
        }
        
        std::cout << "🚀 Server running on http://localhost:8080" << std::endl;
        return true;
    }
    
    void createTables() {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS users (
                user_id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT UNIQUE,
                balance REAL DEFAULT 0.0,
                total_winnings REAL DEFAULT 0.0,
                is_admin BOOLEAN DEFAULT 0,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            );
            
            CREATE TABLE IF NOT EXISTS matches (
                match_id INTEGER PRIMARY KEY AUTOINCREMENT,
                team_a TEXT NOT NULL,
                team_b TEXT NOT NULL,
                status TEXT DEFAULT 'upcoming',
                result TEXT DEFAULT 'pending',
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            );
            
            CREATE TABLE IF NOT EXISTS bets (
                bet_id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                match_id INTEGER,
                prediction TEXT,
                stake_amount REAL DEFAULT 0.5,
                status TEXT DEFAULT 'pending',
                potential_winnings REAL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY(user_id) REFERENCES users(user_id),
                FOREIGN KEY(match_id) REFERENCES matches(match_id)
            );
            
            CREATE TABLE IF NOT EXISTS transactions (
                transaction_id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                type TEXT,
                amount REAL,
                description TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            );
            
            INSERT OR IGNORE INTO users (username, balance, is_admin) 
            VALUES ('admin', 1000.0, 1);
            
            INSERT OR IGNORE INTO users (username, balance) 
            VALUES ('demo_user', 50.0);
        )";
        
        char* errMsg;
        if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "SQL error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
    }
    
    void run() {
        while (true) {
            sockaddr_in client_addr;
            #ifdef _WIN32
                int addrlen = sizeof(client_addr);
            #else
                socklen_t addrlen = sizeof(client_addr);
            #endif
            
            int client_socket = accept(server_fd, (sockaddr*)&client_addr, &addrlen);
            if (client_socket < 0) {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }
            
            // Handle client in separate thread
            std::thread client_thread(&BettingServer::handleClient, this, client_socket);
            client_thread.detach();
        }
    }
    
    void handleClient(int client_socket) {
        char buffer[4096] = {0};
        recv(client_socket, buffer, 4096, 0);
        
        std::string request(buffer);
        std::string response = processRequest(request);
        
        send(client_socket, response.c_str(), response.length(), 0);
        
        #ifdef _WIN32
            closesocket(client_socket);
        #else
            close(client_socket);
        #endif
    }
    
    std::string processRequest(const std::string& request) {
        // Parse request
        std::istringstream iss(request);
        std::string method, path;
        iss >> method >> path;
        
        // Extract JSON from POST request
        std::string json_data;
        size_t json_pos = request.find("\r\n\r\n");
        if (json_pos != std::string::npos) {
            json_data = request.substr(json_pos + 4);
        }
        
        // Route requests
        if (path == "/user/data") {
            return getUserData();
        } else if (path == "/user/deposit") {
            return processDeposit(json_data);
        } else if (path == "/matches/list") {
            return getMatchesList();
        } else if (path == "/bets/place") {
            return placeBet(json_data);
        } else if (path == "/bets/history") {
            return getBetHistory();
        } else if (path == "/admin/add_match") {
            return addMatch(json_data);
        } else {
            // Serve HTML file
            return serveHTML();
        }
    }
    
    std::string serveHTML() {
        // In a real application, you'd read this from a file
        std::string html = R"(<!DOCTYPE html><html>...)</html>)"; // Your HTML content here
        
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html\r\n";
        response += "Content-Length: " + std::to_string(html.length()) + "\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "\r\n";
        response += html;
        
        return response;
    }
    
    std::string jsonResponse(bool success, const std::string& message = "", const std::string& data = "{}") {
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        
        std::string json = "{\"success\":" + std::string(success ? "true" : "false");
        if (!message.empty()) {
            json += ",\"message\":\"" + message + "\"";
        }
        if (!data.empty() && data != "{}") {
            json += ",\"data\":" + data;
        }
        json += "}";
        
        response += "Content-Length: " + std::to_string(json.length()) + "\r\n";
        response += "\r\n";
        response += json;
        
        return response;
    }
    
    std::string getUserData() {
        // For demo, use user_id 2 (demo_user)
        const char* sql = "SELECT user_id, username, balance, total_winnings, is_admin FROM users WHERE user_id = 2";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string data = "{";
                data += "\"user_id\":" + std::to_string(sqlite3_column_int(stmt, 0)) + ",";
                data += "\"username\":\"" + std::string((char*)sqlite3_column_text(stmt, 1)) + "\",";
                data += "\"balance\":" + std::to_string(sqlite3_column_double(stmt, 2)) + ",";
                data += "\"total_winnings\":" + std::to_string(sqlite3_column_double(stmt, 3)) + ",";
                data += "\"is_admin\":" + std::string(sqlite3_column_int(stmt, 4) ? "true" : "false");
                data += "}";
                
                sqlite3_finalize(stmt);
                return jsonResponse(true, "", data);
            }
            sqlite3_finalize(stmt);
        }
        
        return jsonResponse(false, "User not found");
    }
    
    std::string processDeposit(const std::string& json_data) {
        // Parse amount from JSON (simplified)
        double amount = 10.0; // Default
        size_t amount_pos = json_data.find("\"amount\":");
        if (amount_pos != std::string::npos) {
            std::string amount_str = json_data.substr(amount_pos + 8);
            amount = std::stod(amount_str);
        }
        
        const char* sql = "UPDATE users SET balance = balance + ? WHERE user_id = 2";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_double(stmt, 1, amount);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                sqlite3_finalize(stmt);
                return jsonResponse(true, "Deposit successful");
            }
            sqlite3_finalize(stmt);
        }
        
        return jsonResponse(false, "Deposit failed");
    }
    
    std::string getMatchesList() {
        const char* sql = "SELECT match_id, team_a, team_b, status, result FROM matches WHERE status = 'upcoming'";
        sqlite3_stmt* stmt;
        
        std::string data = "[";
        bool first = true;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) data += ",";
                first = false;
                
                data += "{";
                data += "\"match_id\":" + std::to_string(sqlite3_column_int(stmt, 0)) + ",";
                data += "\"team_a\":\"" + std::string((char*)sqlite3_column_text(stmt, 1)) + "\",";
                data += "\"team_b\":\"" + std::string((char*)sqlite3_column_text(stmt, 2)) + "\",";
                data += "\"status\":\"" + std::string((char*)sqlite3_column_text(stmt, 3)) + "\",";
                data += "\"result\":\"" + std::string((char*)sqlite3_column_text(stmt, 4)) + "\"";
                data += "}";
            }
            sqlite3_finalize(stmt);
        }
        
        data += "]";
        return jsonResponse(true, "", data);
    }
    
    std::string placeBet(const std::string& json_data) {
        // Parse bet data (simplified)
        int match_id = 1;
        std::string prediction = "team_a";
        double stake = 0.5;
        
        // Check user balance first
        double balance = 0.0;
        const char* balance_sql = "SELECT balance FROM users WHERE user_id = 2";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, balance_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                balance = sqlite3_column_double(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        
        if (balance < stake) {
            return jsonResponse(false, "Insufficient balance");
        }
        
        // Place bet
        const char* bet_sql = "INSERT INTO bets (user_id, match_id, prediction, stake_amount) VALUES (2, ?, ?, ?)";
        if (sqlite3_prepare_v2(db, bet_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, match_id);
            sqlite3_bind_text(stmt, 2, prediction.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 3, stake);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                // Deduct stake from balance
                const char* update_sql = "UPDATE users SET balance = balance - ? WHERE user_id = 2";
                sqlite3_stmt* update_stmt;
                
                if (sqlite3_prepare_v2(db, update_sql, -1, &update_stmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_double(update_stmt, 1, stake);
                    sqlite3_step(update_stmt);
                    sqlite3_finalize(update_stmt);
                }
                
                sqlite3_finalize(stmt);
                return jsonResponse(true, "Bet placed successfully");
            }
            sqlite3_finalize(stmt);
        }
        
        return jsonResponse(false, "Failed to place bet");
    }
    
    std::string getBetHistory() {
        const char* sql = R"(
            SELECT b.bet_id, m.team_a, m.team_b, b.prediction, b.stake_amount, b.status, b.potential_winnings
            FROM bets b
            JOIN matches m ON b.match_id = m.match_id
            WHERE b.user_id = 2
            ORDER BY b.created_at DESC
            LIMIT 10
        )";
        
        sqlite3_stmt* stmt;
        std::string data = "[";
        bool first = true;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) data += ",";
                first = false;
                
                data += "{";
                data += "\"bet_id\":" + std::to_string(sqlite3_column_int(stmt, 0)) + ",";
                data += "\"team_a\":\"" + std::string((char*)sqlite3_column_text(stmt, 1)) + "\",";
                data += "\"team_b\":\"" + std::string((char*)sqlite3_column_text(stmt, 2)) + "\",";
                data += "\"prediction\":\"" + std::string((char*)sqlite3_column_text(stmt, 3)) + "\",";
                data += "\"stake\":" + std::to_string(sqlite3_column_double(stmt, 4)) + ",";
                data += "\"status\":\"" + std::string((char*)sqlite3_column_text(stmt, 5)) + "\",";
                data += "\"winnings\":" + std::to_string(sqlite3_column_double(stmt, 6));
                data += "}";
            }
            sqlite3_finalize(stmt);
        }
        
        data += "]";
        return jsonResponse(true, "", data);
    }
    
    std::string addMatch(const std::string& json_data) {
        // Parse team names (simplified)
        std::string team_a = "Team A";
        std::string team_b = "Team B";
        
        const char* sql = "INSERT INTO matches (team_a, team_b) VALUES (?, ?)";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, team_a.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, team_b.c_str(), -1, SQLITE_STATIC);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                sqlite3_finalize(stmt);
                return jsonResponse(true, "Match added successfully");
            }
            sqlite3_finalize(stmt);
        }
        
        return jsonResponse(false, "Failed to add match");
    }
    
    ~BettingServer() {
        if (db) sqlite3_close(db);
        #ifdef _WIN32
            if (server_fd != -1) closesocket(server_fd);
            WSACleanup();
        #else
            if (server_fd != -1) close(server_fd);
        #endif
    }
};

int main() {
    BettingServer server;
    
    if (server.initialize()) {
        server.run();
    } else {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }
    
    return 0;
}
