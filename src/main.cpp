#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <syncstream>

#include "httplib.h"
#include "kvstore.hpp"

namespace {
httplib::Server* global_server = nullptr;

void handle_signal(int) {
    if (global_server) {
        global_server->stop();
    }
}

bool is_safe_filename(std::string_view filename) {
    if (filename.empty() || filename.find('/') != std::string_view::npos ||
        filename.find('\\') != std::string_view::npos ||
        filename.find("..") != std::string_view::npos) {
        return false;
    }
    return true;
}
}  // namespace

int main(int argc, char const* argv[]) {
    size_t capacity = 1000;

    if (argc > 1) {
        try {
            unsigned long val = std::stoul(argv[1]);
            if (val == 0) {
                std::cerr << "Error: Capacity must be greater than 0.\n";
                return 1;
            }
            capacity = static_cast<size_t>(val);
        } catch (...) {
            std::cerr << "Usage: " << argv[0] << " [capacity]\n";
            return 1;
        }
    }

    KVStore store(capacity);
    httplib::Server server;
    global_server = &server;

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    server.set_payload_max_length(16 * 1024 * 1024);

    server.Put(R"(/kv/(.+))", [&store](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1];
        if (key.size() > 64 * 1024) {
            res.status = 400;
            res.set_content("Key too long\n", "text/plain");
            return;
        }

        std::string value = req.body;
        std::optional<std::chrono::milliseconds> life_ms = std::nullopt;

        if (req.has_param("life")) {
            try {
                long long parsed = std::stoll(req.get_param_value("life"));
                constexpr long long MAX_LIFE_MS = 365LL * 24 * 60 * 60 * 1000;
                if (parsed <= 0 || parsed > MAX_LIFE_MS) {
                    res.status = 400;
                    res.set_content("Invalid life parameter: out of allowed range\n", "text/plain");
                    return;
                }
                life_ms = std::chrono::milliseconds(parsed);
            } catch (...) {
                res.status = 400;
                res.set_content("Invalid life parameter\n", "text/plain");
                return;
            }
        }

        store.set(key, value, life_ms);
        res.status = 200;
        res.set_content("OK\n", "text/plain");
    });

    server.Get(R"(/kv/(.+))", [&store](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1];
        auto value = store.get(key);
        if (value) {
            res.status = 200;
            res.set_content(*value, "text/plain");
        } else {
            res.status = 404;
            res.set_content("Key not found\n", "text/plain");
        }
    });

    server.Delete(R"(/kv/(.+))", [&store](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1];
        if (store.del(key)) {
            res.status = 200;
            res.set_content("Deleted\n", "text/plain");
        } else {
            res.status = 404;
            res.set_content("Key not found\n", "text/plain");
        }
    });

    server.Post("/admin/dump", [&store](const httplib::Request& req, httplib::Response& res) {
        std::string filename = req.has_param("path") ? req.get_param_value("path") : "snapshot.bin";
        if (!is_safe_filename(filename)) {
            res.status = 400;
            res.set_content("Invalid filename: directory traversal detected\n", "text/plain");
            return;
        }

        if (store.dump(filename)) {
            res.status = 200;
            res.set_content("Snapshot saved\n", "text/plain");
        } else {
            res.status = 500;
            res.set_content("Failed to save snapshot\n", "text/plain");
        }
    });

    server.Post("/admin/load", [&store](const httplib::Request& req, httplib::Response& res) {
        std::string filename = req.has_param("path") ? req.get_param_value("path") : "snapshot.bin";

        if (!is_safe_filename(filename)) {
            res.status = 400;
            res.set_content("Invalid filename: directory traversal detected\n", "text/plain");
            return;
        }

        if (store.load(filename)) {
            res.status = 200;
            res.set_content("Snapshot loaded\n", "text/plain");
        } else {
            res.status = 500;
            res.set_content("Failed to load snapshot\n", "text/plain");
        }
    });

    server.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::osyncstream(std::cout) << "[INFO] " << req.method << " " << req.path << " -> " << res.status << "\n";
    });

    std::cout << "[INFO] STARTING SERVER ON PORT 8080 (CAPACITY: " << capacity << ")...\n";
    server.listen("0.0.0.0", 8080);
    return 0;
}