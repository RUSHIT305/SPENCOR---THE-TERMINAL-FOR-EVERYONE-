#include "spencer/parser/terminal_parser.hpp"
#include "spencer/terminal/terminal_state.hpp"
#include "test_framework.hpp"

SPENCER_TEST(terminal_parser_renders_printable_text_across_fragmented_input) {
  spencer::terminal::TerminalState state({2, 12}, 3);
  spencer::parser::TerminalParser parser(state);

  parser.feed("hel");
  parser.feed("lo");

  SPENCER_REQUIRE(state.screen()[0][0].text == U"h");
  SPENCER_REQUIRE(state.screen()[0][4].text == U"o");
}

SPENCER_TEST(terminal_parser_applies_sgr_and_cursor_controls) {
  spencer::terminal::TerminalState state({3, 8}, 3);
  spencer::parser::TerminalParser parser(state);

  parser.feed("\x1b[31mR\x1b[0mX\x1b[2;4HZ");

  SPENCER_REQUIRE(state.screen()[0][0].text == U"R");
  SPENCER_REQUIRE(state.screen()[0][0].style.foreground == spencer::terminal::Color::indexed(1));
  SPENCER_REQUIRE(state.screen()[0][1].text == U"X");
  SPENCER_REQUIRE(state.screen()[0][1].style.foreground == spencer::terminal::Color::default_color());
  SPENCER_REQUIRE(state.screen()[1][3].text == U"Z");
}

SPENCER_TEST(terminal_parser_interprets_osc_title_with_bell_and_st_terminators) {
  spencer::terminal::TerminalState state({2, 8}, 3);
  spencer::parser::TerminalParser parser(state);

  parser.feed("\x1b]2;first title\a");
  SPENCER_REQUIRE(state.title() == "first title");

  parser.feed("\x1b]0;second title\x1b\\");
  SPENCER_REQUIRE(state.title() == "second title");
}

SPENCER_TEST(terminal_parser_sets_tested_private_modes) {
  spencer::terminal::TerminalState state({2, 8}, 3);
  spencer::parser::TerminalParser parser(state);

  parser.feed("\x1b[?25l\x1b[?2004h");

  SPENCER_REQUIRE(!state.cursor().visible);
  SPENCER_REQUIRE(state.modes().bracketed_paste);
}

SPENCER_TEST(terminal_parser_discards_oversized_osc_payload_safely) {
  spencer::terminal::TerminalState state({2, 8}, 3);
  spencer::parser::TerminalParser parser(state);
  std::string sequence = "\x1b]2;";
  sequence.append(5000, 'x');
  sequence.push_back('\a');
  parser.feed(sequence);

  SPENCER_REQUIRE(state.title().empty());
}
