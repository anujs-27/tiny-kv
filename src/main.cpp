#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <syncstream>

#include "httplib.h"
#include "kvstore.hpp"

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

    server.Put(R"(/kv/(.+))", [&store](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1];
        std::string value = req.body;
        std::optional<std::chrono::milliseconds> life_ms = std::nullopt;

        if (req.has_param("life")) {
            try {
                long long parsed = std::stoll(req.get_param_value("life"));
                if (parsed <= 0) {
                    res.status = 400;
                    res.set_content("Invalid life parameter: must be positive\n", "text/plain");
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
        std::string path = req.has_param("path") ? req.get_param_value("path") : "snapshot.bin";

        if (store.dump(path)) {
            res.status = 200;
            res.set_content("Snapshot saved\n", "text/plain");
        } else {
            res.status = 500;
            res.set_content("Failed to save snapshot\n", "text/plain");
        }
    });

    server.Post("/admin/load", [&store](const httplib::Request& req, httplib::Response& res) {
        std::string path = req.has_param("path") ? req.get_param_value("path") : "snapshot.bin";

        if (store.load(path)) {
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

    std::cout
        << "[INFO] STARTING SERVER ON PORT 8080 (CAPACITY: " << capacity << ")...\n";
    server.listen("0.0.0.0", 8080);
    return 0;
}