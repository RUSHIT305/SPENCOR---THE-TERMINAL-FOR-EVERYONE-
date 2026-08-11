#include "spencer/parser/terminal_parser.hpp"

#include <algorithm>
#include <charconv>
#include <string_view>

namespace spencer::parser {
namespace {
constexpr std::uint8_t kEscape = 0x1BU;
constexpr std::uint8_t kBell = 0x07U;
constexpr std::uint8_t kCancel = 0x18U;
constexpr std::uint8_t kSubstitute = 0x1AU;
constexpr std::uint8_t kCsi = 0x9BU;
constexpr std::uint8_t kOsc = 0x9DU;
constexpr std::uint8_t kDcs = 0x90U;
constexpr std::uint8_t kStringTerminator = 0x9CU;

[[nodiscard]] bool is_c0_control(const std::uint8_t byte) noexcept {
  return byte < 0x20U || byte == 0x7FU;
}

[[nodiscard]] bool is_csi_parameter_byte(const std::uint8_t byte) noexcept {
  return byte >= 0x30U && byte <= 0x3FU;
}

[[nodiscard]] bool is_csi_final_byte(const std::uint8_t byte) noexcept {
  return byte >= 0x40U && byte <= 0x7EU;
}
}  // namespace

TerminalParser::TerminalParser(terminal::TerminalOperations& operations)
    : operations_(operations), decoder_([this](const char32_t codepoint) { emit_codepoint(codepoint); }) {}

void TerminalParser::feed(const std::uint8_t* bytes, const std::size_t size) {
  for (std::size_t index = 0; index < size; ++index) {
    process_byte(bytes[index]);
  }
}

void TerminalParser::feed(const std::string_view bytes) {
  feed(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

void TerminalParser::flush() {
  decoder_.flush();
  if (state_ != State::Ground) {
    state_ = State::Ground;
    clear_sequence_buffers();
  }
}

void TerminalParser::reset() {
  decoder_.reset();
  state_ = State::Ground;
  clear_sequence_buffers();
}

void TerminalParser::process_byte(const std::uint8_t byte) {
  if (byte == kCancel || byte == kSubstitute) {
    decoder_.flush();
    state_ = State::Ground;
    clear_sequence_buffers();
    return;
  }

  switch (state_) {
    case State::Ground: process_ground(byte); break;
    case State::Escape: process_escape(byte); break;
    case State::Csi: process_csi(byte); break;
    case State::Osc:
    case State::OscEscape: process_osc(byte); break;
    case State::String:
    case State::StringEscape: process_string(byte); break;
  }
}

void TerminalParser::process_ground(const std::uint8_t byte) {
  if (byte == kEscape) {
    decoder_.flush();
    state_ = State::Escape;
    return;
  }
  if (byte == kCsi) {
    decoder_.flush();
    enter_csi();
    return;
  }
  if (byte == kOsc) {
    decoder_.flush();
    enter_osc();
    return;
  }
  if (byte == kDcs) {
    decoder_.flush();
    enter_string();
    return;
  }
  if (is_c0_control(byte)) {
    operations_.execute_control(byte);
    return;
  }
  decoder_.feed(byte);
}

void TerminalParser::process_escape(const std::uint8_t byte) {
  state_ = State::Ground;
  switch (byte) {
    case '[': enter_csi(); return;
    case ']': enter_osc(); return;
    case 'P':
    case '^':
    case '_': enter_string(); return;
    case '7': operations_.save_cursor(); return;
    case '8': operations_.restore_cursor(); return;
    case 'c': operations_.reset(); return;
    case 'D': operations_.execute_control(0x0AU); return;
    case 'E':
      operations_.execute_control(0x0AU);
      operations_.execute_control(0x0DU);
      return;
    default: return;
  }
}

void TerminalParser::process_csi(const std::uint8_t byte) {
  if (byte == '?' && parameters_.empty()) {
    private_mode_ = true;
    parameters_.push_back(static_cast<char>(byte));
    return;
  }
  if (is_c0_control(byte)) {
    operations_.execute_control(byte);
    return;
  }
  if (is_csi_parameter_byte(byte)) {
    if (parameters_.size() < kMaximumSequenceBytes) {
      parameters_.push_back(static_cast<char>(byte));
    } else {
      state_ = State::Ground;
      clear_sequence_buffers();
    }
    return;
  }
  if (byte >= 0x20U && byte <= 0x2FU) {
    if (parameters_.size() < kMaximumSequenceBytes) {
      parameters_.push_back(static_cast<char>(byte));
    }
    return;
  }
  if (is_csi_final_byte(byte)) {
    execute_csi(static_cast<char>(byte));
  }
  state_ = State::Ground;
  clear_sequence_buffers();
}

void TerminalParser::process_osc(const std::uint8_t byte) {
  if (state_ == State::OscEscape) {
    if (byte == '\\') {
      execute_osc();
      state_ = State::Ground;
      clear_sequence_buffers();
      return;
    }
    if (osc_payload_.size() + 2 <= kMaximumSequenceBytes) {
      osc_payload_.push_back(static_cast<char>(kEscape));
      osc_payload_.push_back(static_cast<char>(byte));
      state_ = State::Osc;
    } else {
      state_ = State::String;
    }
    return;
  }

  if (byte == kBell) {
    execute_osc();
    state_ = State::Ground;
    clear_sequence_buffers();
    return;
  }
  if (byte == kEscape) {
    state_ = State::OscEscape;
    return;
  }
  if (byte == kStringTerminator) {
    execute_osc();
    state_ = State::Ground;
    clear_sequence_buffers();
    return;
  }
  if (osc_payload_.size() < kMaximumSequenceBytes) {
    osc_payload_.push_back(static_cast<char>(byte));
  } else {
    state_ = State::String;
  }
}

void TerminalParser::process_string(const std::uint8_t byte) {
  if (state_ == State::StringEscape) {
    state_ = byte == '\\' ? State::Ground : State::String;
    return;
  }
  if (byte == kEscape) {
    state_ = State::StringEscape;
  } else if (byte == kStringTerminator) {
    state_ = State::Ground;
  }
}

void TerminalParser::emit_codepoint(const char32_t codepoint) {
  operations_.print(codepoint);
}

void TerminalParser::execute_csi(const char final) {
  const std::vector<int> parameters = parse_parameters();
  const auto one_based = [&](const std::size_t index) {
    return static_cast<std::size_t>(std::max(1, parameter_or_default(parameters, index, 1)) - 1);
  };
  const auto count = [&](const std::size_t index) {
    return static_cast<std::size_t>(std::max(1, parameter_or_default(parameters, index, 1)));
  };

  switch (final) {
    case 'A': operations_.cursor_up(count(0)); break;
    case 'B': operations_.cursor_down(count(0)); break;
    case 'C': operations_.cursor_forward(count(0)); break;
    case 'D': operations_.cursor_back(count(0)); break;
    case 'H':
    case 'f': operations_.cursor_position(one_based(0), one_based(1)); break;
    case 'J':
      operations_.erase_in_display(static_cast<std::size_t>(std::max(0, parameter_or_default(parameters, 0, 0))));
      break;
    case 'K':
      operations_.erase_in_line(static_cast<std::size_t>(std::max(0, parameter_or_default(parameters, 0, 0))));
      break;
    case 'm': operations_.set_graphics_rendition(parameters); break;
    case 'h':
      if (private_mode_) {
        operations_.set_private_mode(parameters, true);
      }
      break;
    case 'l':
      if (private_mode_) {
        operations_.set_private_mode(parameters, false);
      }
      break;
    case 's': operations_.save_cursor(); break;
    case 'u': operations_.restore_cursor(); break;
    default: break;
  }
}

void TerminalParser::execute_osc() {
  const std::size_t separator = osc_payload_.find(';');
  if (separator == std::string::npos) {
    return;
  }
  const std::string_view command(osc_payload_.data(), separator);
  const std::string_view value(osc_payload_.data() + separator + 1,
                               osc_payload_.size() - separator - 1);
  if (command == "0" || command == "2") {
    operations_.set_title(value);
  }
}

void TerminalParser::enter_csi() {
  state_ = State::Csi;
  clear_sequence_buffers();
}

void TerminalParser::enter_osc() {
  state_ = State::Osc;
  clear_sequence_buffers();
}

void TerminalParser::enter_string() {
  state_ = State::String;
  clear_sequence_buffers();
}

void TerminalParser::clear_sequence_buffers() {
  parameters_.clear();
  osc_payload_.clear();
  private_mode_ = false;
  sequence_bytes_ = 0;
}

std::vector<int> TerminalParser::parse_parameters() const {
  std::vector<int> result;
  if (parameters_.empty()) {
    return result;
  }

  std::size_t start = 0;
  if (parameters_.front() == '?') {
    start = 1;
  }
  while (start <= parameters_.size() && result.size() < kMaximumParameterCount) {
    const std::size_t end = parameters_.find(';', start);
    const std::size_t length = (end == std::string::npos ? parameters_.size() : end) - start;
    int value = -1;
    if (length > 0) {
      const char* first = parameters_.data() + start;
      const char* last = first + length;
      const auto conversion = std::from_chars(first, last, value);
      if (conversion.ec != std::errc{} || conversion.ptr != last) {
        value = -1;
      }
    }
    result.push_back(value);
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return result;
}

int TerminalParser::parameter_or_default(const std::vector<int>& parameters, const std::size_t index,
                                         const int default_value) {
  if (index >= parameters.size() || parameters[index] < 0) {
    return default_value;
  }
  return parameters[index];
}

}  // namespace spencer::parser
