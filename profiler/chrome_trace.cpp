#include "profiler/chrome_trace.h"

#include "core/assert.h"
#include "profiler/profiler.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ostream>
#include <string_view>

namespace hive::profiler {
namespace {

void write_escaped(std::ostream& out, std::string_view text) {
    out << '"';
    for (char c : text) {
        switch (c) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << c;
            break;
        }
    }
    out << '"';
}

} // namespace

void write_chrome_trace(const char* path) {
    HIVE_ASSERT(path != nullptr);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return;
    }

    const std::span<const Event> events = events_for_this_thread();

    out << "{\n  \"displayTimeUnit\": \"ns\",\n  \"traceEvents\": [\n";
    for (std::size_t i = 0; i < events.size(); ++i) {
        const Event& e = events[i];
        const std::uint64_t start = e.start_us;
        const std::uint64_t end = e.end_us >= start ? e.end_us : start;
        const char* name = e.name != nullptr ? e.name : "unnamed";

        out << "    {\"name\": ";
        write_escaped(out, name);
        out << ", \"cat\": \"function\", \"ph\": \"X\", \"ts\": " << start
            << ", \"dur\": " << (end - start) << ", \"pid\": 1, \"tid\": " << e.tid
            << "}";
        if (i + 1 < events.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << "  ]\n}\n";
}

} // namespace hive::profiler
