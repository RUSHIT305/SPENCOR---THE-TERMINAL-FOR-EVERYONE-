#pragma once

#include "spencer/terminal/types.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace spencer::config {

struct Theme final {
  terminal::Rgb background{20, 22, 27};
  terminal::Rgb foreground{230, 233, 239};
  terminal::Rgb cursor{128, 203, 196};
  terminal::Rgb selection{57, 65, 84};
  std::array<terminal::Rgb, 16> ansi_colors{
      terminal::Rgb{40, 42, 54}, terminal::Rgb{255, 85, 85}, terminal::Rgb{80, 250, 123},
      terminal::Rgb{241, 250, 140}, terminal::Rgb{98, 114, 164}, terminal::Rgb{189, 147, 249},
      terminal::Rgb{139, 233, 253}, terminal::Rgb{248, 248, 242}, terminal::Rgb{98, 114, 164},
      terminal::Rgb{255, 110, 110}, terminal::Rgb{105, 255, 145}, terminal::Rgb{255, 255, 165},
      terminal::Rgb{135, 160, 230}, terminal::Rgb{209, 170, 255}, terminal::Rgb{170, 245, 255},
      terminal::Rgb{255, 255, 255},
  };
};

struct AppConfig final {
  std::string font_family{"Monospace"};
  int font_size{14};
  std::size_t scrollback_lines{10'000};
  std::string shell{};
  std::string working_directory{};
  int padding{8};
  Theme theme{};
};

struct LoadResult final {
  AppConfig config{};
  std::vector<std::string> warnings{};
};

[[nodiscard]] std::filesystem::path default_path();
[[nodiscard]] LoadResult load(const std::filesystem::path& path);
void write_example_if_missing(const std::filesystem::path& path);

}  // namespace spencer::config
