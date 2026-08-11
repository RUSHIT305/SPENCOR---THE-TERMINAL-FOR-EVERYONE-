#pragma once

#include "spencer/terminal/operations.hpp"
#include "spencer/terminal/types.hpp"

#include <cstddef>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace spencer::terminal {

class TerminalState final : public TerminalOperations {
 public:
  explicit TerminalState(Dimensions dimensions = {}, std::size_t scrollback_limit = 10'000);

  [[nodiscard]] const Dimensions& dimensions() const noexcept;
  [[nodiscard]] const Cursor& cursor() const noexcept;
  [[nodiscard]] const std::vector<std::vector<Cell>>& screen() const noexcept;
  [[nodiscard]] const std::deque<std::vector<Cell>>& scrollback() const noexcept;
  [[nodiscard]] const std::string& title() const noexcept;
  [[nodiscard]] const TerminalModes& modes() const noexcept;
  [[nodiscard]] const CellStyle& active_style() const noexcept;

  void resize(Dimensions dimensions);
  void set_scrollback_limit(std::size_t limit);
  void clear_damage() noexcept;
  [[nodiscard]] bool damaged() const noexcept;

  void print(char32_t codepoint) override;
  void execute_control(std::uint8_t control) override;
  void cursor_up(std::size_t count) override;
  void cursor_down(std::size_t count) override;
  void cursor_forward(std::size_t count) override;
  void cursor_back(std::size_t count) override;
  void cursor_position(std::size_t row, std::size_t column) override;
  void erase_in_display(std::size_t mode) override;
  void erase_in_line(std::size_t mode) override;
  void set_graphics_rendition(std::span<const int> parameters) override;
  void set_title(std::string_view title) override;
  void set_private_mode(std::span<const int> parameters, bool enabled) override;
  void save_cursor() override;
  void restore_cursor() override;
  void reset() override;

 private:
  [[nodiscard]] std::vector<Cell> blank_row() const;
  void line_feed();
  void carriage_return();
  void backspace();
  void tab();
  void ensure_cursor_in_bounds();
  void clear_range(std::vector<Cell>& row, std::size_t start, std::size_t end);
  void append_combining_mark(char32_t codepoint);
  void put_printable(char32_t codepoint);
  [[nodiscard]] static bool is_combining(char32_t codepoint) noexcept;
  [[nodiscard]] static CellWidth width_of(char32_t codepoint) noexcept;
  [[nodiscard]] static std::size_t clamp_count(std::size_t requested) noexcept;
  void set_attribute(CellAttribute attribute, bool enabled);
  void apply_extended_color(bool foreground, std::span<const int> parameters, std::size_t& index);

  Dimensions dimensions_;
  std::size_t scrollback_limit_;
  std::vector<std::vector<Cell>> screen_;
  std::deque<std::vector<Cell>> scrollback_;
  Cursor cursor_{};
  Cursor saved_cursor_{};
  CellStyle active_style_{};
  TerminalModes modes_{};
  std::string title_{};
  bool wrap_pending_{false};
  bool damaged_{true};
};

}  // namespace spencer::terminal
