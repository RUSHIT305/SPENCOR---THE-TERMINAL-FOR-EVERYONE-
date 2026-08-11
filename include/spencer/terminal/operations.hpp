#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace spencer::terminal {

class TerminalOperations {
 public:
  virtual ~TerminalOperations() = default;

  virtual void print(char32_t codepoint) = 0;
  virtual void execute_control(std::uint8_t control) = 0;
  virtual void cursor_up(std::size_t count) = 0;
  virtual void cursor_down(std::size_t count) = 0;
  virtual void cursor_forward(std::size_t count) = 0;
  virtual void cursor_back(std::size_t count) = 0;
  virtual void cursor_position(std::size_t row, std::size_t column) = 0;
  virtual void erase_in_display(std::size_t mode) = 0;
  virtual void erase_in_line(std::size_t mode) = 0;
  virtual void set_graphics_rendition(std::span<const int> parameters) = 0;
  virtual void set_title(std::string_view title) = 0;
  virtual void set_private_mode(std::span<const int> parameters, bool enabled) = 0;
  virtual void save_cursor() = 0;
  virtual void restore_cursor() = 0;
  virtual void reset() = 0;
};

}  // namespace spencer::terminal
