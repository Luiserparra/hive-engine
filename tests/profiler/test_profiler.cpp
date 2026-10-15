#include "jobs/scheduler.h"
#include "profiler/chrome_trace.h"
#include "profiler/profiler.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

TEST_CASE("profiler scopes are no-ops without init") {
    hive::profiler::shutdown();
    {
        PROFILE_SCOPE("nope");
    }
    REQUIRE(hive::profiler::events_for_this_thread().empty());
}

TEST_CASE("profiler one scope records one event with the literal name pointer") {
    hive::profiler::init(64);
    const char* literal = "one_scope";
    {
        PROFILE_SCOPE(literal);
    }
    const auto events = hive::profiler::events_for_this_thread();
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].name == literal);
    REQUIRE(events[0].end_us >= events[0].start_us);
    hive::profiler::shutdown();
}

TEST_CASE("profiler nested scopes nest in time") {
    hive::profiler::init(64);
    {
        PROFILE_SCOPE("outer");
        {
            PROFILE_SCOPE("inner");
        }
    }
    const auto events = hive::profiler::events_for_this_thread();
    REQUIRE(events.size() == 2);

    const hive::profiler::Event* outer = nullptr;
    const hive::profiler::Event* inner = nullptr;
    for (const auto& e : events) {
        if (e.name != nullptr && std::string_view(e.name) == "outer") {
            outer = &e;
        }
        if (e.name != nullptr && std::string_view(e.name) == "inner") {
            inner = &e;
        }
    }
    REQUIRE(outer != nullptr);
    REQUIRE(inner != nullptr);
    REQUIRE(inner->start_us >= outer->start_us);
    REQUIRE(inner->end_us <= outer->end_us);
    hive::profiler::shutdown();
}

TEST_CASE("profiler threads do not mix events") {
    hive::profiler::init(32);
    {
        PROFILE_SCOPE("main");
    }

    // Catch2 REQUIRE throws. An uncaught exception on a std::thread calls
    // std::terminate() -> abort() and pops an MSVC dialog. Capture on the
    // worker, assert on the main thread.
    std::size_t worker_count = 0;
    const char* worker_name = nullptr;
    std::thread worker([&] {
        hive::profiler::init(32);
        {
            PROFILE_SCOPE("worker");
        }
        const auto ev = hive::profiler::events_for_this_thread();
        worker_count = ev.size();
        if (!ev.empty()) {
            worker_name = ev[0].name;
        }
        hive::profiler::shutdown();
    });
    worker.join();

    REQUIRE(worker_count == 1);
    REQUIRE(worker_name != nullptr);
    REQUIRE(std::string_view(worker_name) == "worker");

    const auto ev = hive::profiler::events_for_this_thread();
    REQUIRE(ev.size() == 1);
    REQUIRE(ev[0].name != nullptr);
    REQUIRE(std::string_view(ev[0].name) == "main");
    hive::profiler::shutdown();
}

TEST_CASE("profiler ring overwrites oldest and stays at capacity") {
    const char* names[] = {"e0", "e1", "e2", "e3", "e4", "e5"};
    hive::profiler::init(4);
    for (const char* n : names) {
        PROFILE_SCOPE(n);
    }
    const auto events = hive::profiler::events_for_this_thread();
    REQUIRE(events.size() == 4);
    REQUIRE(std::string_view(events[0].name) == "e2");
    REQUIRE(std::string_view(events[1].name) == "e3");
    REQUIRE(std::string_view(events[2].name) == "e4");
    REQUIRE(std::string_view(events[3].name) == "e5");
    hive::profiler::shutdown();
}

TEST_CASE("profiler chrome trace empty file contains traceEvents") {
    hive::profiler::init(16);
    const auto path = std::filesystem::temp_directory_path() / "hive_empty_trace.json";
    hive::profiler::write_chrome_trace(path.string().c_str());
    std::ifstream in(path);
    REQUIRE(in.good());
    const std::string body((std::istreambuf_iterator<char>(in)), {});
    REQUIRE(body.find("\"traceEvents\"") != std::string::npos);
    hive::profiler::shutdown();
}

TEST_CASE("profiler chrome trace includes recorded scope names") {
    hive::profiler::init(16);
    {
        PROFILE_SCOPE("Arena.allocate");
    }
    const auto path = std::filesystem::temp_directory_path() / "hive_scope_trace.json";
    hive::profiler::write_chrome_trace(path.string().c_str());
    std::ifstream in(path);
    REQUIRE(in.good());
    const std::string body((std::istreambuf_iterator<char>(in)), {});
    REQUIRE(body.find("Arena.allocate") != std::string::npos);
    hive::profiler::shutdown();
}

TEST_CASE("profiler snapshot_all_threads covers worker tids") {
    hive::profiler::init(256);
    hive::jobs::Scheduler sched{2};
    hive::jobs::Counter done;
    std::atomic<int> spins{0};
    for (int i = 0; i < 128; ++i) {
        sched.schedule(
            [&spins] {
                PROFILE_SCOPE("worker_job");
                int x = 0;
                for (int k = 0; k < 20000; ++k) {
                    x += k;
                }
                spins.fetch_add(x != 0 ? 1 : 0, std::memory_order_relaxed);
            },
            &done);
    }
    sched.wait(done);
    REQUIRE(spins.load(std::memory_order_relaxed) == 128);

    const std::vector<hive::profiler::Event> events = hive::profiler::snapshot_all_threads();
    std::set<std::uint32_t> tids;
    bool saw_worker_job = false;
    for (const hive::profiler::Event& e : events) {
        tids.insert(e.tid);
        if (e.name != nullptr && std::string_view(e.name) == "worker_job") {
            saw_worker_job = true;
        }
    }
    REQUIRE(saw_worker_job);
    REQUIRE(tids.size() >= 2);

    const auto path = std::filesystem::temp_directory_path() / "hive_mt_trace.json";
    hive::profiler::write_chrome_trace(path.string().c_str());
    std::ifstream in(path);
    REQUIRE(in.good());
    const std::string body((std::istreambuf_iterator<char>(in)), {});
    REQUIRE(body.find("worker_job") != std::string::npos);

    hive::profiler::shutdown();
}

TEST_CASE("profiler snapshot_all_threads_since is empty with no events") {
    hive::profiler::init(16);
    REQUIRE(hive::profiler::snapshot_all_threads_since(0).empty());
    hive::profiler::shutdown();
}

TEST_CASE("profiler snapshot_all_threads_since cutoff mid-history") {
    hive::profiler::init(32);
    const char* names[] = {"a", "b", "c", "d", "e"};
    for (const char* n : names) {
        PROFILE_SCOPE(n);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const std::vector<hive::profiler::Event> all = hive::profiler::snapshot_all_threads();
    REQUIRE(all.size() == 5);

    const std::uint64_t cutoff = all[2].end_us;
    const std::vector<hive::profiler::Event> since =
        hive::profiler::snapshot_all_threads_since(cutoff);
    REQUIRE_FALSE(since.empty());
    for (const hive::profiler::Event& event : since) {
        REQUIRE(event.end_us >= cutoff);
    }

    std::size_t expected = 0;
    for (const hive::profiler::Event& event : all) {
        if (event.end_us >= cutoff) {
            ++expected;
        }
    }
    REQUIRE(since.size() == expected);
    hive::profiler::shutdown();
}

TEST_CASE("profiler snapshot_all_threads_since all-old and all-new") {
    hive::profiler::init(16);
    {
        PROFILE_SCOPE("only");
    }
    const std::vector<hive::profiler::Event> all = hive::profiler::snapshot_all_threads();
    REQUIRE(all.size() == 1);

    const std::vector<hive::profiler::Event> all_new =
        hive::profiler::snapshot_all_threads_since(0);
    REQUIRE(all_new.size() == 1);
    REQUIRE(std::string_view(all_new[0].name) == "only");

    const std::vector<hive::profiler::Event> all_old =
        hive::profiler::snapshot_all_threads_since(all[0].end_us + 1);
    REQUIRE(all_old.empty());
    hive::profiler::shutdown();
}

TEST_CASE("profiler snapshot_all_threads_since survives ring wrap") {
    hive::profiler::init(4);
    const char* names[] = {"e0", "e1", "e2", "e3", "e4", "e5"};
    for (const char* n : names) {
        PROFILE_SCOPE(n);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const std::vector<hive::profiler::Event> all = hive::profiler::snapshot_all_threads();
    REQUIRE(all.size() == 4);
    REQUIRE(std::string_view(all[0].name) == "e2");
    REQUIRE(std::string_view(all[3].name) == "e5");

    const std::uint64_t cutoff = all[1].end_us;
    const std::vector<hive::profiler::Event> since =
        hive::profiler::snapshot_all_threads_since(cutoff);
    REQUIRE_FALSE(since.empty());
    for (const hive::profiler::Event& event : since) {
        REQUIRE(event.end_us >= cutoff);
    }

    std::size_t expected = 0;
    for (const hive::profiler::Event& event : all) {
        if (event.end_us >= cutoff) {
            ++expected;
        }
    }
    REQUIRE(since.size() == expected);
    hive::profiler::shutdown();
}
