#include "spencer/parser/utf8_decoder.hpp"

namespace spencer::parser {
namespace {
constexpr char32_t kReplacementCharacter = U'\uFFFD';
constexpr char32_t kMaximumScalarValue = 0x10FFFF;
constexpr char32_t kHighSurrogate = 0xD800;
constexpr char32_t kLowSurrogate = 0xDFFF;
}  // namespace

Utf8Decoder::Utf8Decoder(Consumer consumer) : consumer_(std::move(consumer)) {}

void Utf8Decoder::feed(const std::uint8_t byte) {
  process(byte);
}

void Utf8Decoder::flush() {
  if (remaining_ != 0) {
    emit(kReplacementCharacter);
  }
  reset();
}

void Utf8Decoder::reset() noexcept {
  accumulator_ = 0;
  minimum_value_ = 0;
  remaining_ = 0;
}

void Utf8Decoder::emit(const char32_t codepoint) const {
  consumer_(codepoint);
}

void Utf8Decoder::process(const std::uint8_t byte) {
  if (remaining_ == 0) {
    if (byte <= 0x7FU) {
      emit(static_cast<char32_t>(byte));
      return;
    }

    if (byte >= 0xC2U && byte <= 0xDFU) {
      accumulator_ = static_cast<char32_t>(byte & 0x1FU);
      minimum_value_ = 0x80;
      remaining_ = 1;
      return;
    }

    if (byte >= 0xE0U && byte <= 0xEFU) {
      accumulator_ = static_cast<char32_t>(byte & 0x0FU);
      minimum_value_ = 0x800;
      remaining_ = 2;
      return;
    }

    if (byte >= 0xF0U && byte <= 0xF4U) {
      accumulator_ = static_cast<char32_t>(byte & 0x07U);
      minimum_value_ = 0x10000;
      remaining_ = 3;
      return;
    }

    emit(kReplacementCharacter);
    return;
  }

  if ((byte & 0xC0U) != 0x80U) {
    emit(kReplacementCharacter);
    reset();
    process(byte);
    return;
  }

  accumulator_ = static_cast<char32_t>((accumulator_ << 6U) | (byte & 0x3FU));
  --remaining_;
  if (remaining_ != 0) {
    return;
  }

  const char32_t decoded = accumulator_;
  const bool is_surrogate = decoded >= kHighSurrogate && decoded <= kLowSurrogate;
  if (decoded < minimum_value_ || decoded > kMaximumScalarValue || is_surrogate) {
    emit(kReplacementCharacter);
  } else {
    emit(decoded);
  }
  reset();
}

}  // namespace spencer::parser
