#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>

namespace spencer::terminal {

struct Rgb final {
  std::uint8_t red{0};
  std::uint8_t green{0};
  std::uint8_t blue{0};

  auto operator<=>(const Rgb&) const = default;
};

enum class ColorKind : std::uint8_t {
  Default,
  Indexed,
  Rgb,
};

struct Color final {
  ColorKind kind{ColorKind::Default};
  std::uint8_t index{0};
  Rgb rgb{};

  [[nodiscard]] static constexpr Color default_color() noexcept { return {}; }

  [[nodiscard]] static constexpr Color indexed(const std::uint8_t value) noexcept {
    return Color{ColorKind::Indexed, value, {}};
  }

  [[nodiscard]] static constexpr Color true_color(const std::uint8_t red,
                                                  const std::uint8_t green,
                                                  const std::uint8_t blue) noexcept {
    return Color{ColorKind::Rgb, 0, {red, green, blue}};
  }

  auto operator<=>(const Color&) const = default;
};

enum class CellWidth : std::uint8_t {
  Normal = 1,
  Wide = 2,
  Continuation = 0,
};

enum class CellAttribute : std::uint16_t {
  None = 0,
  Bold = 1U << 0U,
  Dim = 1U << 1U,
  Italic = 1U << 2U,
  Underline = 1U << 3U,
  Inverse = 1U << 4U,
  Strikethrough = 1U << 5U,
};

[[nodiscard]] constexpr std::uint16_t attribute_mask(const CellAttribute attribute) noexcept {
  return static_cast<std::uint16_t>(attribute);
}

[[nodiscard]] constexpr bool has_attribute(const std::uint16_t attributes,
                                           const CellAttribute sought) noexcept {
  return (attributes & attribute_mask(sought)) != 0;
}

struct CellStyle final {
  Color foreground{Color::default_color()};
  Color background{Color::default_color()};
  std::uint16_t attributes{0};

  auto operator<=>(const CellStyle&) const = default;
};

struct Cell final {
  std::u32string text{U" "};
  CellStyle style{};
  CellWidth width{CellWidth::Normal};

  [[nodiscard]] static Cell blank(const CellStyle& style = {}) {
    return Cell{U" ", style, CellWidth::Normal};
  }
};

struct Dimensions final {
  std::size_t rows{24};
  std::size_t columns{80};

  [[nodiscard]] static constexpr Dimensions sanitized(const std::size_t requested_rows,
                                                      const std::size_t requested_columns) noexcept {
    return Dimensions{requested_rows == 0 ? 1 : requested_rows,
                      requested_columns == 0 ? 1 : requested_columns};
  }

  auto operator<=>(const Dimensions&) const = default;
};

struct Cursor final {
  std::size_t row{0};
  std::size_t column{0};
  bool visible{true};

  auto operator<=>(const Cursor&) const = default;
};

struct TerminalModes final {
  bool auto_wrap{true};
  bool insert{false};
  bool origin{false};
  bool bracketed_paste{false};
  bool alternate_screen{false};
};

}  // namespace spencer::terminal
