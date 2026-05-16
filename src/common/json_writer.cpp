#include "common/json_writer.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace algobench
{
namespace
{

std::string json_escape(const std::string& input)
{
    std::ostringstream os;
    for (const char ch : input)
    {
        switch (ch)
        {
        case '"': os << "\\\""; break;
        case '\\': os << "\\\\"; break;
        case '\b': os << "\\b"; break;
        case '\f': os << "\\f"; break;
        case '\n': os << "\\n"; break;
        case '\r': os << "\\r"; break;
        case '\t': os << "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20)
            {
                os << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                   << static_cast<int>(static_cast<unsigned char>(ch));
            }
            else
            {
                os << ch;
            }
        }
    }
    return os.str();
}

void write_string_field(std::ostringstream& os, const char* key, const std::string& value, bool comma = true)
{
    os << '"' << key << "\":\"" << json_escape(value) << '"';
    if (comma)
    {
        os << ',';
    }
}

void write_number_field(std::ostringstream& os, const char* key, double value, bool comma = true)
{
    os << '"' << key << "\":" << std::fixed << std::setprecision(6) << value;
    if (comma)
    {
        os << ',';
    }
}

} // namespace

std::string to_json_line(const BenchmarkResult& result)
{
    std::ostringstream os;
    os << '{';

    write_string_field(os, "benchmark", result.benchmark);
    write_string_field(os, "variant", result.variant);
    write_string_field(os, "preset", result.preset);

    os << "\"repeat\":" << result.repeat << ',';
    os << "\"warmup\":" << result.warmup << ',';

    os << "\"input_size\":{";
    bool first = true;
    for (const auto& [key, value] : result.input_size)
    {
        if (!first)
        {
            os << ',';
        }
        first = false;
        os << '"' << json_escape(key) << "\":" << value;
    }
    os << "},";

    write_number_field(os, "total_ms", result.total_ms);
    write_number_field(os, "h2d_ms", result.h2d_ms);
    write_number_field(os, "kernel_ms", result.kernel_ms);
    write_number_field(os, "d2h_ms", result.d2h_ms);

    os << "\"correct\":" << (result.correct ? "true" : "false") << ',';

    write_string_field(os, "device", result.device);
    write_string_field(os, "notes", result.notes, result.metadata.empty() ? false : true);

    if (!result.metadata.empty())
    {
        os << "\"metadata\":{";
        first = true;
        for (const auto& [key, value] : result.metadata)
        {
            if (!first)
            {
                os << ',';
            }
            first = false;
            os << '"' << json_escape(key) << "\":\"" << json_escape(value) << '"';
        }
        os << '}';
    }

    os << '}';
    return os.str();
}

void write_jsonl(const std::string& path, const std::vector<BenchmarkResult>& results, bool append)
{
    if (path.empty())
    {
        return;
    }

    const std::filesystem::path output_path(path);
    if (!output_path.parent_path().empty())
    {
        std::filesystem::create_directories(output_path.parent_path());
    }

    std::ofstream file(path, append ? std::ios::app : std::ios::trunc);
    if (!file)
    {
        throw std::runtime_error("failed to open output file: " + path);
    }

    for (const auto& result : results)
    {
        file << to_json_line(result) << '\n';
    }
}

void print_result_table(std::ostream& os, const std::vector<BenchmarkResult>& results)
{
    os << std::left
       << std::setw(24) << "benchmark"
       << std::setw(10) << "variant"
       << std::setw(22) << "preset"
       << std::right
       << std::setw(14) << "total_ms"
       << std::setw(12) << "kernel_ms"
       << std::setw(10) << "correct"
       << "  device\n";

    os << std::string(104, '-') << '\n';

    for (const auto& r : results)
    {
        os << std::left
           << std::setw(24) << r.benchmark
           << std::setw(10) << r.variant
           << std::setw(22) << r.preset
           << std::right
           << std::setw(14) << std::fixed << std::setprecision(3) << r.total_ms
           << std::setw(12) << std::fixed << std::setprecision(3) << r.kernel_ms
           << std::setw(10) << (r.correct ? "yes" : "no")
           << "  " << r.device << '\n';
    }
}

} // namespace algobench
