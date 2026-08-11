#pragma once

#include "spencer/parser/utf8_decoder.hpp"
#include "spencer/terminal/operations.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace spencer::parser {

class TerminalParser final {
 public:
  explicit TerminalParser(terminal::TerminalOperations& operations);

  void feed(const std::uint8_t* bytes, std::size_t size);
  void feed(std::string_view bytes);
  void flush();
  void reset();

 private:
  enum class State : std::uint8_t {
    Ground,
    Escape,
    Csi,
    Osc,
    OscEscape,
    String,
    StringEscape,
  };

  void process_byte(std::uint8_t byte);
  void process_ground(std::uint8_t byte);
  void process_escape(std::uint8_t byte);
  void process_csi(std::uint8_t byte);
  void process_osc(std::uint8_t byte);
  void process_string(std::uint8_t byte);
  void emit_codepoint(char32_t codepoint);
  void execute_csi(char final);
  void execute_osc();
  void enter_csi();
  void enter_osc();
  void enter_string();
  void clear_sequence_buffers();
  [[nodiscard]] std::vector<int> parse_parameters() const;
  [[nodiscard]] static int parameter_or_default(const std::vector<int>& parameters,
                                                std::size_t index, int default_value);

  terminal::TerminalOperations& operations_;
  Utf8Decoder decoder_;
  State state_{State::Ground};
  std::string parameters_;
  std::string osc_payload_;
  bool private_mode_{false};
  std::size_t sequence_bytes_{0};

  static constexpr std::size_t kMaximumSequenceBytes = 4096;
  static constexpr std::size_t kMaximumParameterCount = 32;
};

}  // namespace spencer::parser
