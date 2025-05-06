#pragma once

#include <vector>
#include <mutex>
#include <unordered_set>
#include <sstream>

#include "Node.cpp"
#include "crow.h"

// ----------------------------------------------------------------

namespace Interface {
    mutex mutex_;
    unordered_set<crow::websocket::connection*> connections;
    vector<string> messages; // JSON

    void send(const Node& output) {
        lock_guard<mutex> lock(mutex_);
        string outputString = to_string(output);

        messages.push_back(outputString);

        for(auto* connection : connections) {
            connection->send_text(outputString);
        }
    }

    void register_connection(crow::websocket::connection* connection) {
        lock_guard<mutex> lock(mutex_);

        connections.insert(connection);

        /* TODO: Do not resend everything automatically, but send critical
        for(const auto& message : messages) {
            connection->send_text(message);
        }
        */
    }

    void unregister_connection(crow::websocket::connection* connection) {
        lock_guard<mutex> lock(mutex_);

        connections.erase(connection);
    }

    void handle_message(const string& input) {
        auto cmd = crow::json::load(input);
        if (!cmd) return;

        lock_guard<mutex> lock(mutex_);

        string action = cmd["action"].s();
        if (action == "clear") {
            messages.clear();
        } else
        if (action == "removeLast") {
            if (!messages.empty()) {
                messages.pop_back();
            }
        }
    }
}