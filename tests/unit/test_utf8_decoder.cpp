#include "spencer/parser/utf8_decoder.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <vector>

SPENCER_TEST(utf8_decoder_emits_ascii_and_multibyte_scalars) {
  std::vector<char32_t> result;
  spencer::parser::Utf8Decoder decoder([&result](const char32_t value) { result.push_back(value); });

  decoder.feed(static_cast<std::uint8_t>('A'));
  decoder.feed(0xE2U);
  decoder.feed(0x82U);
  decoder.feed(0xACU);

  SPENCER_REQUIRE(result.size() == 2);
  SPENCER_REQUIRE(result[0] == U'A');
  SPENCER_REQUIRE(result[1] == U'€');
}

SPENCER_TEST(utf8_decoder_preserves_fragmented_sequences_between_feeds) {
  std::vector<char32_t> result;
  spencer::parser::Utf8Decoder decoder([&result](const char32_t value) { result.push_back(value); });

  decoder.feed(0xF0U);
  decoder.feed(0x9FU);
  SPENCER_REQUIRE(result.empty());
  decoder.feed(0x98U);
  decoder.feed(0x80U);

  SPENCER_REQUIRE(result.size() == 1);
  SPENCER_REQUIRE(result.front() == U'😀');
}

SPENCER_TEST(utf8_decoder_replaces_malformed_and_incomplete_sequences) {
  std::vector<char32_t> result;
  spencer::parser::Utf8Decoder decoder([&result](const char32_t value) { result.push_back(value); });

  decoder.feed(0xC0U);
  decoder.feed(0xE2U);
  decoder.feed(static_cast<std::uint8_t>('x'));
  decoder.feed(0xF0U);
  decoder.feed(0x9FU);
  decoder.flush();

  SPENCER_REQUIRE(result.size() == 4);
  SPENCER_REQUIRE(result[0] == U'\uFFFD');
  SPENCER_REQUIRE(result[1] == U'\uFFFD');
  SPENCER_REQUIRE(result[2] == U'x');
  SPENCER_REQUIRE(result[3] == U'\uFFFD');
}
