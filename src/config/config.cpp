#include "spencer/config/config.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>

namespace spencer::config {
namespace {
constexpr std::size_t kMaximumScrollbackLines = 1'000'000;
constexpr int kMinimumFontSize = 6;
constexpr int kMaximumFontSize = 72;
constexpr int kMaximumPadding = 100;

[[nodiscard]] std::string trim(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](const unsigned char character) {
    return std::isspace(character) != 0;
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char character) {
    return std::isspace(character) != 0;
  }).base();
  if (first >= last) {
    return {};
  }
  return {first, last};
}

[[nodiscard]] std::optional<int> parse_int(const std::string& value) {
  int result = 0;
  const auto conversion = std::from_chars(value.data(), value.data() + value.size(), result);
  if (conversion.ec != std::errc{} || conversion.ptr != value.data() + value.size()) {
    return std::nullopt;
  }
  return result;
}

void warning(LoadResult& result, const std::size_t line, const std::string& message) {
  result.warnings.push_back("line " + std::to_string(line) + ": " + message);
}
}  // namespace

std::filesystem::path default_path() {
  if (const char* const xdg_config_home = std::getenv("XDG_CONFIG_HOME"); xdg_config_home != nullptr &&
      *xdg_config_home != '\0') {
    return std::filesystem::path(xdg_config_home) / "spencer" / "config";
  }
  if (const char* const home = std::getenv("HOME"); home != nullptr && *home != '\0') {
    return std::filesystem::path(home) / ".config" / "spencer" / "config";
  }
  return std::filesystem::path(".spencer-config");
}

LoadResult load(const std::filesystem::path& path) {
  LoadResult result;
  std::ifstream input(path);
  if (!input.is_open()) {
    if (std::filesystem::exists(path)) {
      result.warnings.push_back("could not read configuration file: " + path.string());
    }
    return result;
  }

  std::string raw_line;
  std::size_t line_number = 0;
  while (std::getline(input, raw_line)) {
    ++line_number;
    const std::string line = trim(raw_line);
    if (line.empty() || line.front() == '#') {
      continue;
    }
    const std::size_t separator = line.find('=');
    if (separator == std::string::npos) {
      warning(result, line_number, "expected key = value");
      continue;
    }
    const std::string key = trim(line.substr(0, separator));
    const std::string value = trim(line.substr(separator + 1));
    if (key.empty() || value.empty()) {
      warning(result, line_number, "empty configuration key or value");
      continue;
    }

    if (key == "font_family") {
      if (value.size() > 128) {
        warning(result, line_number, "font_family exceeds 128 characters; ignoring it");
      } else {
        result.config.font_family = value;
      }
    } else if (key == "font_size") {
      const auto parsed = parse_int(value);
      if (!parsed || *parsed < kMinimumFontSize || *parsed > kMaximumFontSize) {
        warning(result, line_number, "font_size must be between 6 and 72");
      } else {
        result.config.font_size = *parsed;
      }
    } else if (key == "scrollback_lines") {
      const auto parsed = parse_int(value);
      if (!parsed || *parsed < 0 || static_cast<std::size_t>(*parsed) > kMaximumScrollbackLines) {
        warning(result, line_number, "scrollback_lines must be between 0 and 1000000");
      } else {
        result.config.scrollback_lines = static_cast<std::size_t>(*parsed);
      }
    } else if (key == "shell") {
      if (value.front() != '/' || value.size() > 4096) {
        warning(result, line_number, "shell must be a reasonable absolute path");
      } else {
        result.config.shell = value;
      }
    } else if (key == "working_directory") {
      if (value.front() != '/' || value.size() > 4096) {
        warning(result, line_number, "working_directory must be a reasonable absolute path");
      } else {
        result.config.working_directory = value;
      }
    } else if (key == "padding") {
      const auto parsed = parse_int(value);
      if (!parsed || *parsed < 0 || *parsed > kMaximumPadding) {
        warning(result, line_number, "padding must be between 0 and 100");
      } else {
        result.config.padding = *parsed;
      }
    } else {
      warning(result, line_number, "unknown key '" + key + "'");
    }
  }
  return result;
}

void write_example_if_missing(const std::filesystem::path& path) {
  if (std::filesystem::exists(path)) {
    return;
  }
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) {
    return;
  }
  std::ofstream output(path);
  if (!output.is_open()) {
    return;
  }
  output << "# SPENCER configuration\n"
         << "# Invalid values are ignored and SPENCER starts with safe defaults.\n"
         << "font_family = Monospace\n"
         << "font_size = 14\n"
         << "scrollback_lines = 10000\n"
         << "# shell = /bin/bash\n"
         << "# working_directory = /home/you\n"
         << "padding = 8\n";
}

}  // namespace spencer::config
