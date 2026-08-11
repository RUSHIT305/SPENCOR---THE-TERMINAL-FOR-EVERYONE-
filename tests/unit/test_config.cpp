#include "spencer/config/config.hpp"
#include "test_framework.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path temporary_config_path(const std::string& suffix) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("spencer-config-" + suffix + "-" + std::to_string(stamp));
}

}  // namespace

SPENCER_TEST(config_uses_defaults_when_file_is_missing) {
  const auto path = temporary_config_path("missing");
  const spencer::config::LoadResult result = spencer::config::load(path);

  SPENCER_REQUIRE(result.config.font_family == "Monospace");
  SPENCER_REQUIRE(result.config.font_size == 14);
  SPENCER_REQUIRE(result.config.scrollback_lines == 10'000);
  SPENCER_REQUIRE(result.warnings.empty());
}

SPENCER_TEST(config_applies_valid_values_and_skips_malformed_values) {
  const auto path = temporary_config_path("values");
  {
    std::ofstream output(path);
    output << "font_family = JetBrains Mono\n"
           << "font_size = 16\n"
           << "scrollback_lines = 25000\n"
           << "shell = /bin/sh\n"
           << "working_directory = /tmp\n"
           << "padding = 12\n"
           << "font_size = 999\n"
           << "unknown = value\n";
  }

  const spencer::config::LoadResult result = spencer::config::load(path);
  std::filesystem::remove(path);

  SPENCER_REQUIRE(result.config.font_family == "JetBrains Mono");
  SPENCER_REQUIRE(result.config.font_size == 16);
  SPENCER_REQUIRE(result.config.scrollback_lines == 25'000);
  SPENCER_REQUIRE(result.config.shell == "/bin/sh");
  SPENCER_REQUIRE(result.config.working_directory == "/tmp");
  SPENCER_REQUIRE(result.config.padding == 12);
  SPENCER_REQUIRE(result.warnings.size() == 2);
}

SPENCER_TEST(config_writes_a_safe_example_only_when_missing) {
  const auto directory = temporary_config_path("example");
  const auto path = directory / "config";
  spencer::config::write_example_if_missing(path);

  SPENCER_REQUIRE(std::filesystem::exists(path));
  const spencer::config::LoadResult result = spencer::config::load(path);
  SPENCER_REQUIRE(result.warnings.empty());
  SPENCER_REQUIRE(result.config.font_size == 14);

  spencer::config::write_example_if_missing(path);
  std::filesystem::remove_all(directory);
}
