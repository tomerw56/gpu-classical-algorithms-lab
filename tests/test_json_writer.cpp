#include "common/json_writer.hpp"

#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

algobench::BenchmarkResult make_result(const std::string& variant)
{
    algobench::BenchmarkResult result;
    result.benchmark = "foundation_smoke";
    result.variant = variant;
    result.preset = "tiny";
    result.repeat = 2;
    result.warmup = 1;
    result.input_size["values"] = 1024;
    result.total_ms = 1.25;
    result.h2d_ms = 0.1;
    result.kernel_ms = 0.2;
    result.d2h_ms = 0.3;
    result.correct = true;
    result.device = "test device";
    result.notes = "note with \"quotes\" and slash \\";
    result.metadata["checksum"] = "123.456";
    return result;
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

int main()
{
    const auto result = make_result("cpu");
    const std::string line = algobench::to_json_line(result);

    TEST_CHECK(line.find("\"benchmark\":\"foundation_smoke\"") != std::string::npos);
    TEST_CHECK(line.find("\"variant\":\"cpu\"") != std::string::npos);
    TEST_CHECK(line.find("\"values\":1024") != std::string::npos);
    TEST_CHECK(line.find("\"correct\":true") != std::string::npos);
    TEST_CHECK(line.find("\\\"quotes\\\"") != std::string::npos);
    TEST_CHECK(line.find("\\\\") != std::string::npos);

    const auto temp_dir = std::filesystem::temp_directory_path() / "gpu_algobench_tests";
    std::filesystem::create_directories(temp_dir);
    const auto output_path = temp_dir / "json_writer_test.jsonl";
    std::filesystem::remove(output_path);

    algobench::write_jsonl(output_path.string(), {make_result("cpu")}, false);
    algobench::write_jsonl(output_path.string(), {make_result("gpu")}, true);

    const std::string file_text = read_file(output_path);
    TEST_CHECK(file_text.find("\"variant\":\"cpu\"") != std::string::npos);
    TEST_CHECK(file_text.find("\"variant\":\"gpu\"") != std::string::npos);

    std::ostringstream table;
    algobench::print_result_table(table, {make_result("cpu")});
    TEST_CHECK(table.str().find("benchmark") != std::string::npos);
    TEST_CHECK(table.str().find("foundation_smoke") != std::string::npos);
    TEST_CHECK(table.str().find("yes") != std::string::npos);

    std::filesystem::remove(output_path);

    return algobench::test::finish();
}
