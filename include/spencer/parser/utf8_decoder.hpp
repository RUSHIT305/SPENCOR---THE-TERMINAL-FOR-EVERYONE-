#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace spencer::parser {

class Utf8Decoder final {
 public:
  using Consumer = std::function<void(char32_t)>;

  explicit Utf8Decoder(Consumer consumer);

  void feed(std::uint8_t byte);
  void flush();
  void reset() noexcept;

 private:
  void emit(char32_t codepoint) const;
  void process(std::uint8_t byte);

  Consumer consumer_;
  char32_t accumulator_{0};
  char32_t minimum_value_{0};
  std::size_t remaining_{0};
};

}  // namespace spencer::parser
