#include "spencer/terminal/terminal_state.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace spencer::terminal {
namespace {
constexpr std::size_t kMaximumScrollbackRows = 1'000'000;
constexpr std::size_t kMaximumTitleBytes = 512;
constexpr char32_t kReplacementCharacter = U'\uFFFD';

[[nodiscard]] std::uint8_t bounded_color_component(const int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}
}  // namespace

TerminalState::TerminalState(const Dimensions dimensions, const std::size_t scrollback_limit)
    : dimensions_(Dimensions::sanitized(dimensions.rows, dimensions.columns)),
      scrollback_limit_(std::min(scrollback_limit, kMaximumScrollbackRows)) {
  screen_.assign(dimensions_.rows, blank_row());
}

const Dimensions& TerminalState::dimensions() const noexcept { return dimensions_; }
const Cursor& TerminalState::cursor() const noexcept { return cursor_; }
const std::vector<std::vector<Cell>>& TerminalState::screen() const noexcept { return screen_; }
const std::deque<std::vector<Cell>>& TerminalState::scrollback() const noexcept { return scrollback_; }
const std::string& TerminalState::title() const noexcept { return title_; }
const TerminalModes& TerminalState::modes() const noexcept { return modes_; }
const CellStyle& TerminalState::active_style() const noexcept { return active_style_; }

void TerminalState::resize(const Dimensions requested_dimensions) {
  const Dimensions new_dimensions =
      Dimensions::sanitized(requested_dimensions.rows, requested_dimensions.columns);
  if (new_dimensions == dimensions_) {
    return;
  }

  std::vector<std::vector<Cell>> resized(new_dimensions.rows,
                                         std::vector<Cell>(new_dimensions.columns,
                                                           Cell::blank()));
  const std::size_t rows_to_copy = std::min(dimensions_.rows, new_dimensions.rows);
  const std::size_t columns_to_copy = std::min(dimensions_.columns, new_dimensions.columns);
  for (std::size_t row = 0; row < rows_to_copy; ++row) {
    for (std::size_t column = 0; column < columns_to_copy; ++column) {
      resized[row][column] = screen_[row][column];
    }
  }

  dimensions_ = new_dimensions;
  screen_ = std::move(resized);
  ensure_cursor_in_bounds();
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::set_scrollback_limit(const std::size_t requested_limit) {
  scrollback_limit_ = std::min(requested_limit, kMaximumScrollbackRows);
  while (scrollback_.size() > scrollback_limit_) {
    scrollback_.pop_front();
  }
}

void TerminalState::clear_damage() noexcept { damaged_ = false; }
bool TerminalState::damaged() const noexcept { return damaged_; }

void TerminalState::print(const char32_t codepoint) {
  if (is_combining(codepoint)) {
    append_combining_mark(codepoint);
    return;
  }
  put_printable(codepoint);
}

void TerminalState::execute_control(const std::uint8_t control) {
  switch (control) {
    case 0x08: backspace(); break;
    case 0x09: tab(); break;
    case 0x0A:
    case 0x0B:
    case 0x0C: line_feed(); break;
    case 0x0D: carriage_return(); break;
    default: break;
  }
}

void TerminalState::cursor_up(const std::size_t count) {
  cursor_.row = cursor_.row > count ? cursor_.row - count : 0;
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::cursor_down(const std::size_t count) {
  cursor_.row = std::min(cursor_.row + count, dimensions_.rows - 1);
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::cursor_forward(const std::size_t count) {
  cursor_.column = std::min(cursor_.column + count, dimensions_.columns - 1);
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::cursor_back(const std::size_t count) {
  cursor_.column = cursor_.column > count ? cursor_.column - count : 0;
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::cursor_position(const std::size_t row, const std::size_t column) {
  cursor_.row = std::min(row, dimensions_.rows - 1);
  cursor_.column = std::min(column, dimensions_.columns - 1);
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::erase_in_display(const std::size_t mode) {
  switch (mode) {
    case 0:
      erase_in_line(0);
      for (std::size_t row = cursor_.row + 1; row < dimensions_.rows; ++row) {
        clear_range(screen_[row], 0, dimensions_.columns - 1);
      }
      break;
    case 1:
      for (std::size_t row = 0; row < cursor_.row; ++row) {
        clear_range(screen_[row], 0, dimensions_.columns - 1);
      }
      clear_range(screen_[cursor_.row], 0, cursor_.column);
      break;
    case 2:
      for (auto& row : screen_) {
        clear_range(row, 0, dimensions_.columns - 1);
      }
      break;
    case 3: scrollback_.clear(); break;
    default: return;
  }
  damaged_ = true;
}

void TerminalState::erase_in_line(const std::size_t mode) {
  switch (mode) {
    case 0: clear_range(screen_[cursor_.row], cursor_.column, dimensions_.columns - 1); break;
    case 1: clear_range(screen_[cursor_.row], 0, cursor_.column); break;
    case 2: clear_range(screen_[cursor_.row], 0, dimensions_.columns - 1); break;
    default: return;
  }
  damaged_ = true;
}

void TerminalState::set_graphics_rendition(const std::span<const int> parameters) {
  const std::array<int, 1> reset_parameter{0};
  const std::span<const int> active_parameters = parameters.empty() ? std::span{reset_parameter} : parameters;

  for (std::size_t index = 0; index < active_parameters.size(); ++index) {
    const int parameter = active_parameters[index] < 0 ? 0 : active_parameters[index];
    if (parameter == 0) {
      active_style_ = {};
    } else if (parameter == 1) {
      set_attribute(CellAttribute::Bold, true);
    } else if (parameter == 2) {
      set_attribute(CellAttribute::Dim, true);
    } else if (parameter == 3) {
      set_attribute(CellAttribute::Italic, true);
    } else if (parameter == 4) {
      set_attribute(CellAttribute::Underline, true);
    } else if (parameter == 7) {
      set_attribute(CellAttribute::Inverse, true);
    } else if (parameter == 9) {
      set_attribute(CellAttribute::Strikethrough, true);
    } else if (parameter == 22) {
      set_attribute(CellAttribute::Bold, false);
      set_attribute(CellAttribute::Dim, false);
    } else if (parameter == 23) {
      set_attribute(CellAttribute::Italic, false);
    } else if (parameter == 24) {
      set_attribute(CellAttribute::Underline, false);
    } else if (parameter == 27) {
      set_attribute(CellAttribute::Inverse, false);
    } else if (parameter == 29) {
      set_attribute(CellAttribute::Strikethrough, false);
    } else if (parameter >= 30 && parameter <= 37) {
      active_style_.foreground = Color::indexed(static_cast<std::uint8_t>(parameter - 30));
    } else if (parameter == 39) {
      active_style_.foreground = Color::default_color();
    } else if (parameter >= 40 && parameter <= 47) {
      active_style_.background = Color::indexed(static_cast<std::uint8_t>(parameter - 40));
    } else if (parameter == 49) {
      active_style_.background = Color::default_color();
    } else if (parameter >= 90 && parameter <= 97) {
      active_style_.foreground = Color::indexed(static_cast<std::uint8_t>(parameter - 90 + 8));
    } else if (parameter >= 100 && parameter <= 107) {
      active_style_.background = Color::indexed(static_cast<std::uint8_t>(parameter - 100 + 8));
    } else if (parameter == 38) {
      apply_extended_color(true, active_parameters, index);
    } else if (parameter == 48) {
      apply_extended_color(false, active_parameters, index);
    }
  }
  damaged_ = true;
}

void TerminalState::set_title(const std::string_view title) {
  title_.assign(title.substr(0, kMaximumTitleBytes));
}

void TerminalState::set_private_mode(const std::span<const int> parameters, const bool enabled) {
  for (const int parameter : parameters) {
    if (parameter == 25) {
      cursor_.visible = enabled;
    } else if (parameter == 2004) {
      modes_.bracketed_paste = enabled;
    }
  }
  damaged_ = true;
}

void TerminalState::save_cursor() { saved_cursor_ = cursor_; }

void TerminalState::restore_cursor() {
  cursor_ = saved_cursor_;
  ensure_cursor_in_bounds();
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::reset() {
  cursor_ = {};
  saved_cursor_ = {};
  active_style_ = {};
  modes_ = {};
  wrap_pending_ = false;
  for (auto& row : screen_) {
    row = blank_row();
  }
  damaged_ = true;
}

std::vector<Cell> TerminalState::blank_row() const {
  return std::vector<Cell>(dimensions_.columns, Cell::blank());
}

void TerminalState::line_feed() {
  if (cursor_.row + 1 < dimensions_.rows) {
    ++cursor_.row;
  } else {
    if (scrollback_limit_ > 0) {
      scrollback_.push_back(std::move(screen_.front()));
      while (scrollback_.size() > scrollback_limit_) {
        scrollback_.pop_front();
      }
    }
    screen_.erase(screen_.begin());
    screen_.push_back(blank_row());
  }
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::carriage_return() {
  cursor_.column = 0;
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::backspace() {
  if (cursor_.column > 0) {
    --cursor_.column;
  }
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::tab() {
  const std::size_t next_stop = ((cursor_.column / 8) + 1) * 8;
  cursor_.column = std::min(next_stop, dimensions_.columns - 1);
  wrap_pending_ = false;
  damaged_ = true;
}

void TerminalState::ensure_cursor_in_bounds() {
  cursor_.row = std::min(cursor_.row, dimensions_.rows - 1);
  cursor_.column = std::min(cursor_.column, dimensions_.columns - 1);
  saved_cursor_.row = std::min(saved_cursor_.row, dimensions_.rows - 1);
  saved_cursor_.column = std::min(saved_cursor_.column, dimensions_.columns - 1);
}

void TerminalState::clear_range(std::vector<Cell>& row, const std::size_t requested_start,
                                const std::size_t requested_end) {
  const std::size_t start = std::min(requested_start, row.size() - 1);
  const std::size_t end = std::min(requested_end, row.size() - 1);
  for (std::size_t column = start; column <= end; ++column) {
    row[column] = Cell::blank();
  }
}

void TerminalState::append_combining_mark(const char32_t codepoint) {
  if (cursor_.column == 0 && cursor_.row == 0) {
    put_printable(kReplacementCharacter);
    return;
  }

  std::size_t row = cursor_.row;
  std::size_t column = cursor_.column;
  if (column == 0) {
    --row;
    column = dimensions_.columns;
  }
  --column;
  while (column > 0 && screen_[row][column].width == CellWidth::Continuation) {
    --column;
  }
  screen_[row][column].text.push_back(codepoint);
  damaged_ = true;
}

void TerminalState::put_printable(const char32_t codepoint) {
  if (wrap_pending_ && modes_.auto_wrap) {
    line_feed();
    carriage_return();
  }

  CellWidth width = width_of(codepoint);
  if (width == CellWidth::Wide && dimensions_.columns < 2) {
    width = CellWidth::Normal;
  }
  if (width == CellWidth::Wide && cursor_.column + 1 >= dimensions_.columns) {
    line_feed();
    carriage_return();
  }

  const std::size_t cells = width == CellWidth::Wide ? 2 : 1;
  if (modes_.insert) {
    auto& row = screen_[cursor_.row];
    for (std::size_t column = dimensions_.columns - 1; column >= cursor_.column + cells; --column) {
      row[column] = row[column - cells];
      if (column == cursor_.column + cells) {
        break;
      }
    }
  }

  auto& row = screen_[cursor_.row];
  row[cursor_.column] = Cell{std::u32string(1, codepoint), active_style_, width};
  if (width == CellWidth::Wide) {
    row[cursor_.column + 1] = Cell{U"", active_style_, CellWidth::Continuation};
  }

  const std::size_t next_column = cursor_.column + cells;
  if (next_column >= dimensions_.columns) {
    cursor_.column = dimensions_.columns - 1;
    wrap_pending_ = true;
  } else {
    cursor_.column = next_column;
    wrap_pending_ = false;
  }
  damaged_ = true;
}

bool TerminalState::is_combining(const char32_t codepoint) noexcept {
  return (codepoint >= 0x0300 && codepoint <= 0x036F) ||
         (codepoint >= 0x1AB0 && codepoint <= 0x1AFF) ||
         (codepoint >= 0x1DC0 && codepoint <= 0x1DFF) ||
         (codepoint >= 0x20D0 && codepoint <= 0x20FF) ||
         (codepoint >= 0xFE20 && codepoint <= 0xFE2F);
}

CellWidth TerminalState::width_of(const char32_t codepoint) noexcept {
  if ((codepoint >= 0x1100 && codepoint <= 0x115F) ||
      (codepoint >= 0x2329 && codepoint <= 0x232A) ||
      (codepoint >= 0x2E80 && codepoint <= 0xA4CF) ||
      (codepoint >= 0xAC00 && codepoint <= 0xD7A3) ||
      (codepoint >= 0xF900 && codepoint <= 0xFAFF) ||
      (codepoint >= 0xFE10 && codepoint <= 0xFE19) ||
      (codepoint >= 0xFE30 && codepoint <= 0xFE6F) ||
      (codepoint >= 0xFF00 && codepoint <= 0xFF60) ||
      (codepoint >= 0xFFE0 && codepoint <= 0xFFE6) ||
      (codepoint >= 0x1F300 && codepoint <= 0x1FAFF) ||
      (codepoint >= 0x20000 && codepoint <= 0x3FFFD)) {
    return CellWidth::Wide;
  }
  return CellWidth::Normal;
}

std::size_t TerminalState::clamp_count(const std::size_t requested) noexcept {
  return std::min(requested, std::numeric_limits<std::size_t>::max() / 2);
}

void TerminalState::set_attribute(const CellAttribute attribute, const bool enabled) {
  const std::uint16_t mask = attribute_mask(attribute);
  if (enabled) {
    active_style_.attributes |= mask;
  } else {
    active_style_.attributes &= static_cast<std::uint16_t>(~mask);
  }
}

void TerminalState::apply_extended_color(const bool foreground, const std::span<const int> parameters,
                                         std::size_t& index) {
  if (index + 1 >= parameters.size()) {
    return;
  }
  const int mode = parameters[++index];
  Color color = Color::default_color();
  bool valid = false;
  if (mode == 5 && index + 1 < parameters.size()) {
    color = Color::indexed(bounded_color_component(parameters[++index]));
    valid = true;
  } else if (mode == 2 && index + 3 < parameters.size()) {
    const std::uint8_t red = bounded_color_component(parameters[++index]);
    const std::uint8_t green = bounded_color_component(parameters[++index]);
    const std::uint8_t blue = bounded_color_component(parameters[++index]);
    color = Color::true_color(red, green, blue);
    valid = true;
  }

  if (valid) {
    if (foreground) {
      active_style_.foreground = color;
    } else {
      active_style_.background = color;
    }
  }
}

}  // namespace spencer::terminal
