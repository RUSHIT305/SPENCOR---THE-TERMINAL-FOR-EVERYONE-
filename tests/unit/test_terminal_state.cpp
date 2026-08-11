#include "spencer/terminal/terminal_state.hpp"
#include "test_framework.hpp"

#include <array>

SPENCER_TEST(terminal_state_places_text_and_tracks_cursor) {
  spencer::terminal::TerminalState state({2, 4}, 3);

  state.print(U'A');
  state.print(U'B');

  SPENCER_REQUIRE(state.screen()[0][0].text == U"A");
  SPENCER_REQUIRE(state.screen()[0][1].text == U"B");
  SPENCER_REQUIRE(state.cursor().row == 0);
  SPENCER_REQUIRE(state.cursor().column == 2);
}

SPENCER_TEST(terminal_state_applies_sgr_indexed_and_true_colors) {
  spencer::terminal::TerminalState state({2, 8}, 3);
  const std::array<int, 1> red{31};
  state.set_graphics_rendition(red);
  state.print(U'R');

  SPENCER_REQUIRE(state.screen()[0][0].style.foreground == spencer::terminal::Color::indexed(1));

  const std::array<int, 5> rgb_background{48, 2, 10, 20, 30};
  state.set_graphics_rendition(rgb_background);
  state.print(U'B');

  SPENCER_REQUIRE(state.screen()[0][1].style.background ==
                  spencer::terminal::Color::true_color(10, 20, 30));
}

SPENCER_TEST(terminal_state_scrolls_and_bounds_scrollback) {
  spencer::terminal::TerminalState state({2, 3}, 1);
  state.print(U'A');
  state.execute_control(0x0DU);
  state.execute_control(0x0AU);
  state.print(U'B');
  state.execute_control(0x0DU);
  state.execute_control(0x0AU);
  state.print(U'C');

  SPENCER_REQUIRE(state.scrollback().size() == 1);
  SPENCER_REQUIRE(state.scrollback().front()[0].text == U"A");
  SPENCER_REQUIRE(state.screen()[0][0].text == U"B");
  SPENCER_REQUIRE(state.screen()[1][0].text == U"C");
}

SPENCER_TEST(terminal_state_handles_wide_and_combining_characters) {
  spencer::terminal::TerminalState state({2, 6}, 3);
  state.print(U'界');
  state.print(U'e');
  state.print(U'\u0301');

  SPENCER_REQUIRE(state.screen()[0][0].width == spencer::terminal::CellWidth::Wide);
  SPENCER_REQUIRE(state.screen()[0][1].width == spencer::terminal::CellWidth::Continuation);
  SPENCER_REQUIRE(state.screen()[0][2].text == U"e\u0301");
  SPENCER_REQUIRE(state.cursor().column == 3);
}

SPENCER_TEST(terminal_state_erases_requested_display_portion) {
  spencer::terminal::TerminalState state({2, 4}, 3);
  state.print(U'A');
  state.print(U'B');
  state.print(U'C');
  state.cursor_position(0, 1);
  state.erase_in_line(0);

  SPENCER_REQUIRE(state.screen()[0][0].text == U"A");
  SPENCER_REQUIRE(state.screen()[0][1].text == U" ");
  SPENCER_REQUIRE(state.screen()[0][3].text == U" ");
}
