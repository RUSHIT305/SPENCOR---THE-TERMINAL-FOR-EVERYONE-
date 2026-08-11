#include "spencer/config/config.hpp"
#include "spencer/parser/terminal_parser.hpp"
#include "spencer/pty/linux_pty.hpp"
#include "spencer/terminal/terminal_state.hpp"

#include <gtk/gtk.h>
#include <glib-unix.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view kApplicationId = "io.github.rushit305.Spencer";
constexpr std::string_view kDefaultTitle = "SPENCER — The Terminal for Everyone";

[[nodiscard]] std::string to_utf8(const std::u32string& text) {
  gchar* const converted = g_ucs4_to_utf8(reinterpret_cast<const gunichar*>(text.data()),
                                           static_cast<glong>(text.size()), nullptr, nullptr, nullptr);
  if (converted == nullptr) {
    return "?";
  }
  std::string result(converted);
  g_free(converted);
  return result;
}

[[nodiscard]] double component(const std::uint8_t value) {
  return static_cast<double>(value) / 255.0;
}

[[nodiscard]] spencer::terminal::Rgb xterm_color(const std::uint8_t index) {
  if (index < 16) {
    return {};
  }
  if (index >= 232) {
    const std::uint8_t shade = static_cast<std::uint8_t>(8 + (index - 232) * 10);
    return {shade, shade, shade};
  }
  const std::uint8_t value = static_cast<std::uint8_t>(index - 16);
  constexpr std::array<std::uint8_t, 6> levels{0, 95, 135, 175, 215, 255};
  return {levels[value / 36], levels[(value / 6) % 6], levels[value % 6]};
}

class SpencerApplication final {
 public:
  int run(const int argc, char** argv) {
    application_ = gtk_application_new(kApplicationId.data(), G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(application_, "activate", G_CALLBACK(&SpencerApplication::activate_callback), this);
    const int status = g_application_run(G_APPLICATION(application_), argc, argv);
    shutdown();
    g_object_unref(application_);
    application_ = nullptr;
    return status;
  }

 private:
  static void activate_callback(GtkApplication*, const gpointer user_data) {
    static_cast<SpencerApplication*>(user_data)->activate();
  }

  static gboolean pty_callback(const gint, const GIOCondition, const gpointer user_data) {
    return static_cast<SpencerApplication*>(user_data)->read_pty();
  }

  static gboolean blink_callback(const gpointer user_data) {
    auto* const self = static_cast<SpencerApplication*>(user_data);
    self->cursor_blink_visible_ = !self->cursor_blink_visible_;
    if (self->drawing_area_ != nullptr) {
      gtk_widget_queue_draw(self->drawing_area_);
    }
    return G_SOURCE_CONTINUE;
  }

  static void draw_callback(GtkDrawingArea*, cairo_t* context, const int width, const int height,
                            const gpointer user_data) {
    static_cast<SpencerApplication*>(user_data)->draw(context, width, height);
  }

  static gboolean key_pressed_callback(GtkEventControllerKey*, const guint keyval, const guint,
                                       const GdkModifierType state, const gpointer user_data) {
    return static_cast<SpencerApplication*>(user_data)->key_pressed(keyval, state);
  }

  static gboolean scroll_callback(GtkEventControllerScroll*, const double, const double delta_y,
                                  const gpointer user_data) {
    return static_cast<SpencerApplication*>(user_data)->scroll(delta_y);
  }

  static gboolean close_request_callback(GtkWindow*, const gpointer user_data) {
    static_cast<SpencerApplication*>(user_data)->shutdown();
    return FALSE;
  }

  void activate() {
    const std::filesystem::path config_path = spencer::config::default_path();
    spencer::config::write_example_if_missing(config_path);
    const spencer::config::LoadResult loaded_config = spencer::config::load(config_path);
    config_ = loaded_config.config;

    window_ = GTK_WINDOW(gtk_application_window_new(application_));
    gtk_window_set_default_size(window_, 1000, 640);
    gtk_window_set_title(window_, kDefaultTitle.data());
    g_signal_connect(window_, "close-request", G_CALLBACK(&SpencerApplication::close_request_callback), this);

    drawing_area_ = gtk_drawing_area_new();
    gtk_widget_set_hexpand(drawing_area_, TRUE);
    gtk_widget_set_vexpand(drawing_area_, TRUE);
    gtk_widget_set_focusable(drawing_area_, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area_), &SpencerApplication::draw_callback, this,
                                   nullptr);

    auto* const key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(&SpencerApplication::key_pressed_callback), this);
    gtk_widget_add_controller(drawing_area_, GTK_EVENT_CONTROLLER(key_controller));

    // GTK exposes bit flags through a C enum; the OR combination is required by its API.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    const auto scroll_flags = static_cast<GtkEventControllerScrollFlags>(
        GTK_EVENT_CONTROLLER_SCROLL_VERTICAL | GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
    auto* const scroll_controller = gtk_event_controller_scroll_new(scroll_flags);
    g_signal_connect(scroll_controller, "scroll", G_CALLBACK(&SpencerApplication::scroll_callback), this);
    gtk_widget_add_controller(drawing_area_, GTK_EVENT_CONTROLLER(scroll_controller));

    gtk_window_set_child(window_, drawing_area_);

    try {
      terminal_state_ = std::make_unique<spencer::terminal::TerminalState>(
          spencer::terminal::Dimensions{24, 80}, config_.scrollback_lines);
      parser_ = std::make_unique<spencer::parser::TerminalParser>(*terminal_state_);
      spencer::pty::SpawnOptions options;
      options.shell = config_.shell;
      options.working_directory = config_.working_directory;
      options.dimensions = terminal_state_->dimensions();
      pty_ = std::make_unique<spencer::pty::LinuxPty>(spencer::pty::LinuxPty::spawn(options));
      // GIOCondition is a GLib C bit-flag enum; this combination watches normal data and terminal closure.
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      const auto pty_conditions = static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR);
      pty_source_id_ = g_unix_fd_add_full(G_PRIORITY_DEFAULT, pty_->descriptor(), pty_conditions,
                                          &SpencerApplication::pty_callback, this, nullptr);
      blink_source_id_ = g_timeout_add(500, &SpencerApplication::blink_callback, this);
    } catch (const std::exception& error) {
      show_error(error.what());
    }

    gtk_window_present(window_);
    gtk_widget_grab_focus(drawing_area_);
  }

  void show_error(const std::string_view detail) {
    GtkAlertDialog* const dialog =
        gtk_alert_dialog_new("SPENCER could not start the terminal session.");
    const std::string detail_copy(detail);
    gtk_alert_dialog_set_detail(dialog, detail_copy.c_str());
    gtk_alert_dialog_show(dialog, window_);
    g_object_unref(dialog);
  }

  gboolean read_pty() {
    if (!pty_ || !parser_ || !terminal_state_) {
      return G_SOURCE_REMOVE;
    }
    try {
      const std::vector<std::uint8_t> bytes = pty_->read_available();
      if (!bytes.empty()) {
        parser_->feed(bytes.data(), bytes.size());
        update_window_title();
        if (terminal_state_->damaged()) {
          terminal_state_->clear_damage();
          gtk_widget_queue_draw(drawing_area_);
        }
      }
      if (pty_->exit_status().has_value()) {
        pty_source_id_ = 0;
        return G_SOURCE_REMOVE;
      }
      return G_SOURCE_CONTINUE;
    } catch (const std::exception& error) {
      show_error(error.what());
      pty_source_id_ = 0;
      return G_SOURCE_REMOVE;
    }
  }

  [[nodiscard]] bool key_pressed(const guint keyval, const GdkModifierType modifiers) {
    if (!pty_) {
      return FALSE;
    }

    std::string sequence;
    switch (keyval) {
      case GDK_KEY_Return:
      case GDK_KEY_KP_Enter: sequence = "\r"; break;
      case GDK_KEY_BackSpace: sequence = "\x7f"; break;
      case GDK_KEY_Tab: sequence = "\t"; break;
      case GDK_KEY_Escape: sequence = "\x1b"; break;
      case GDK_KEY_Up: sequence = "\x1b[A"; break;
      case GDK_KEY_Down: sequence = "\x1b[B"; break;
      case GDK_KEY_Right: sequence = "\x1b[C"; break;
      case GDK_KEY_Left: sequence = "\x1b[D"; break;
      case GDK_KEY_Home: sequence = "\x1b[H"; break;
      case GDK_KEY_End: sequence = "\x1b[F"; break;
      case GDK_KEY_Insert: sequence = "\x1b[2~"; break;
      case GDK_KEY_Delete: sequence = "\x1b[3~"; break;
      case GDK_KEY_F1: sequence = "\x1bOP"; break;
      case GDK_KEY_F2: sequence = "\x1bOQ"; break;
      case GDK_KEY_F3: sequence = "\x1bOR"; break;
      case GDK_KEY_F4: sequence = "\x1bOS"; break;
      case GDK_KEY_F5: sequence = "\x1b[15~"; break;
      case GDK_KEY_F6: sequence = "\x1b[17~"; break;
      case GDK_KEY_F7: sequence = "\x1b[18~"; break;
      case GDK_KEY_F8: sequence = "\x1b[19~"; break;
      case GDK_KEY_F9: sequence = "\x1b[20~"; break;
      case GDK_KEY_F10: sequence = "\x1b[21~"; break;
      case GDK_KEY_Page_Up:
        adjust_viewport(5);
        return TRUE;
      case GDK_KEY_Page_Down:
        adjust_viewport(-5);
        return TRUE;
      default: {
        const gunichar character = gdk_keyval_to_unicode(keyval);
        if (character == 0) {
          return FALSE;
        }
        if ((modifiers & GDK_CONTROL_MASK) != 0 && character < 128) {
          const char control = static_cast<char>(g_ascii_tolower(static_cast<gchar>(character)) & 0x1F);
          if (control != 0) {
            sequence.assign(1, control);
          }
        } else {
          gchar buffer[8]{};
          const gint length = g_unichar_to_utf8(character, buffer);
          sequence.assign(buffer, static_cast<std::size_t>(length));
        }
        break;
      }
    }

    if ((modifiers & GDK_ALT_MASK) != 0 && keyval != GDK_KEY_Escape) {
      sequence.insert(sequence.begin(), '\x1b');
    }
    if (sequence.empty()) {
      return FALSE;
    }

    try {
      write_to_pty(sequence);
      viewport_offset_ = 0;
      gtk_widget_queue_draw(drawing_area_);
      return TRUE;
    } catch (const std::exception& error) {
      show_error(error.what());
      return TRUE;
    }
  }

  [[nodiscard]] bool scroll(const double delta_y) {
    if (delta_y > 0.0) {
      adjust_viewport(3);
    } else if (delta_y < 0.0) {
      adjust_viewport(-3);
    }
    return TRUE;
  }

  void write_to_pty(const std::string_view input) {
    std::size_t offset = 0;
    while (offset < input.size()) {
      const std::size_t written = pty_->write(input.substr(offset));
      if (written == 0) {
        throw std::runtime_error("Terminal input queue is temporarily full; please try again.");
      }
      offset += written;
    }
  }

  void draw(cairo_t* const context, const int width, const int height) {
    if (!terminal_state_) {
      cairo_set_source_rgb(context, 0.08, 0.09, 0.11);
      cairo_paint(context);
      return;
    }

    calculate_cell_metrics(context);
    resize_terminal(width, height);

    cairo_set_source_rgb(context, component(config_.theme.background.red), component(config_.theme.background.green),
                         component(config_.theme.background.blue));
    cairo_paint(context);

    const auto rows = visible_rows();
    const double usable_height = std::max(0, height - 2 * config_.padding);
    const std::size_t max_rows = static_cast<std::size_t>(usable_height / cell_height_);
    const std::size_t rows_to_draw = std::min(rows.size(), max_rows);
    for (std::size_t row = 0; row < rows_to_draw; ++row) {
      const auto* const cells = rows[row];
      draw_row(context, *cells, row);
    }
    draw_cursor(context);
  }

  void calculate_cell_metrics(cairo_t* const context) {
    PangoLayout* const layout = pango_cairo_create_layout(context);
    PangoFontDescription* const description = pango_font_description_from_string(config_.font_family.c_str());
    pango_font_description_set_absolute_size(description, static_cast<double>(config_.font_size) * PANGO_SCALE);
    pango_layout_set_font_description(layout, description);
    pango_layout_set_text(layout, "M", -1);
    int measured_width = 0;
    int measured_height = 0;
    pango_layout_get_pixel_size(layout, &measured_width, &measured_height);
    cell_width_ = std::max(1, measured_width);
    cell_height_ = std::max(1, measured_height + 2);
    pango_font_description_free(description);
    g_object_unref(layout);
  }

  void resize_terminal(const int width, const int height) {
    const int horizontal_space = std::max(1, width - 2 * config_.padding);
    const int vertical_space = std::max(1, height - 2 * config_.padding);
    const auto columns = static_cast<std::size_t>(std::max(1, horizontal_space / cell_width_));
    const auto rows = static_cast<std::size_t>(std::max(1, vertical_space / cell_height_));
    const spencer::terminal::Dimensions dimensions = spencer::terminal::Dimensions::sanitized(rows, columns);
    if (dimensions == terminal_state_->dimensions()) {
      return;
    }
    terminal_state_->resize(dimensions);
    if (pty_) {
      pty_->resize(dimensions);
    }
  }

  [[nodiscard]] std::vector<const std::vector<spencer::terminal::Cell>*> visible_rows() const {
    std::vector<const std::vector<spencer::terminal::Cell>*> all_rows;
    all_rows.reserve(terminal_state_->scrollback().size() + terminal_state_->screen().size());
    for (const auto& row : terminal_state_->scrollback()) {
      all_rows.push_back(&row);
    }
    for (const auto& row : terminal_state_->screen()) {
      all_rows.push_back(&row);
    }

    const std::size_t screen_rows = terminal_state_->dimensions().rows;
    const std::size_t maximum_offset = all_rows.size() > screen_rows ? all_rows.size() - screen_rows : 0;
    const std::size_t offset = std::min(viewport_offset_, maximum_offset);
    const std::size_t start = all_rows.size() > screen_rows + offset ? all_rows.size() - screen_rows - offset : 0;
    const std::size_t end = std::min(all_rows.size(), start + screen_rows);
    return {all_rows.begin() + static_cast<std::ptrdiff_t>(start),
            all_rows.begin() + static_cast<std::ptrdiff_t>(end)};
  }

  [[nodiscard]] spencer::terminal::Rgb resolve_color(const spencer::terminal::Color& color,
                                                       const bool foreground) const {
    if (color.kind == spencer::terminal::ColorKind::Default) {
      return foreground ? config_.theme.foreground : config_.theme.background;
    }
    if (color.kind == spencer::terminal::ColorKind::Rgb) {
      return color.rgb;
    }
    if (color.index < config_.theme.ansi_colors.size()) {
      return config_.theme.ansi_colors[color.index];
    }
    return xterm_color(color.index);
  }

  void draw_row(cairo_t* const context, const std::vector<spencer::terminal::Cell>& row,
                const std::size_t row_index) {
    for (std::size_t column = 0; column < row.size(); ++column) {
      const auto& cell = row[column];
      if (cell.width == spencer::terminal::CellWidth::Continuation) {
        continue;
      }
      spencer::terminal::Rgb foreground = resolve_color(cell.style.foreground, true);
      spencer::terminal::Rgb background = resolve_color(cell.style.background, false);
      if (spencer::terminal::has_attribute(cell.style.attributes, spencer::terminal::CellAttribute::Inverse)) {
        std::swap(foreground, background);
      }
      const double x = static_cast<double>(config_.padding + static_cast<int>(column) * cell_width_);
      const double y = static_cast<double>(config_.padding + static_cast<int>(row_index) * cell_height_);
      const int cell_span = cell.width == spencer::terminal::CellWidth::Wide ? 2 : 1;

      cairo_set_source_rgb(context, component(background.red), component(background.green), component(background.blue));
      cairo_rectangle(context, x, y, static_cast<double>(cell_width_ * cell_span), static_cast<double>(cell_height_));
      cairo_fill(context);

      if (cell.text == U" ") {
        continue;
      }
      if (spencer::terminal::has_attribute(cell.style.attributes, spencer::terminal::CellAttribute::Dim)) {
        foreground.red = static_cast<std::uint8_t>((static_cast<unsigned int>(foreground.red) + background.red) / 2U);
        foreground.green = static_cast<std::uint8_t>((static_cast<unsigned int>(foreground.green) + background.green) / 2U);
        foreground.blue = static_cast<std::uint8_t>((static_cast<unsigned int>(foreground.blue) + background.blue) / 2U);
      }

      PangoLayout* const layout = pango_cairo_create_layout(context);
      PangoFontDescription* const description = pango_font_description_from_string(config_.font_family.c_str());
      pango_font_description_set_absolute_size(description, static_cast<double>(config_.font_size) * PANGO_SCALE);
      if (spencer::terminal::has_attribute(cell.style.attributes, spencer::terminal::CellAttribute::Bold)) {
        pango_font_description_set_weight(description, PANGO_WEIGHT_BOLD);
      }
      if (spencer::terminal::has_attribute(cell.style.attributes, spencer::terminal::CellAttribute::Italic)) {
        pango_font_description_set_style(description, PANGO_STYLE_ITALIC);
      }
      pango_layout_set_font_description(layout, description);
      const std::string text = to_utf8(cell.text);
      pango_layout_set_text(layout, text.c_str(), -1);
      cairo_set_source_rgb(context, component(foreground.red), component(foreground.green), component(foreground.blue));
      cairo_move_to(context, x, y);
      pango_cairo_show_layout(context, layout);

      if (spencer::terminal::has_attribute(cell.style.attributes, spencer::terminal::CellAttribute::Underline)) {
        cairo_set_line_width(context, 1.0);
        cairo_move_to(context, x, y + cell_height_ - 2);
        cairo_line_to(context, x + cell_width_ * cell_span, y + cell_height_ - 2);
        cairo_stroke(context);
      }
      if (spencer::terminal::has_attribute(cell.style.attributes, spencer::terminal::CellAttribute::Strikethrough)) {
        cairo_set_line_width(context, 1.0);
        cairo_move_to(context, x, y + cell_height_ / 2.0);
        cairo_line_to(context, x + cell_width_ * cell_span, y + cell_height_ / 2.0);
        cairo_stroke(context);
      }
      pango_font_description_free(description);
      g_object_unref(layout);
    }
  }

  void draw_cursor(cairo_t* const context) const {
    if (!terminal_state_->cursor().visible || !cursor_blink_visible_ || viewport_offset_ != 0) {
      return;
    }
    const auto& cursor = terminal_state_->cursor();
    const double x = static_cast<double>(config_.padding + static_cast<int>(cursor.column) * cell_width_);
    const double y = static_cast<double>(config_.padding + static_cast<int>(cursor.row) * cell_height_);
    cairo_set_source_rgb(context, component(config_.theme.cursor.red), component(config_.theme.cursor.green),
                         component(config_.theme.cursor.blue));
    cairo_set_line_width(context, 1.5);
    cairo_rectangle(context, x + 0.75, y + 0.75, static_cast<double>(cell_width_ - 1),
                    static_cast<double>(cell_height_ - 1));
    cairo_stroke(context);
  }

  void adjust_viewport(const int delta) {
    if (!terminal_state_) {
      return;
    }
    const std::size_t available = terminal_state_->scrollback().size();
    if (delta > 0) {
      viewport_offset_ = std::min(available, viewport_offset_ + static_cast<std::size_t>(delta));
    } else {
      const std::size_t reduction = static_cast<std::size_t>(-delta);
      viewport_offset_ = viewport_offset_ > reduction ? viewport_offset_ - reduction : 0;
    }
    gtk_widget_queue_draw(drawing_area_);
  }

  void update_window_title() {
    if (terminal_state_->title().empty()) {
      gtk_window_set_title(window_, kDefaultTitle.data());
      return;
    }
    const std::string title = terminal_state_->title() + " — SPENCER";
    gtk_window_set_title(window_, title.c_str());
  }

  void shutdown() {
    if (pty_source_id_ != 0) {
      g_source_remove(pty_source_id_);
      pty_source_id_ = 0;
    }
    if (blink_source_id_ != 0) {
      g_source_remove(blink_source_id_);
      blink_source_id_ = 0;
    }
    parser_.reset();
    if (pty_) {
      pty_->terminate();
      pty_.reset();
    }
  }

  GtkApplication* application_{nullptr};
  GtkWindow* window_{nullptr};
  GtkWidget* drawing_area_{nullptr};
  spencer::config::AppConfig config_{};
  std::unique_ptr<spencer::terminal::TerminalState> terminal_state_{};
  std::unique_ptr<spencer::parser::TerminalParser> parser_{};
  std::unique_ptr<spencer::pty::LinuxPty> pty_{};
  guint pty_source_id_{0};
  guint blink_source_id_{0};
  int cell_width_{8};
  int cell_height_{18};
  std::size_t viewport_offset_{0};
  bool cursor_blink_visible_{true};
};

}  // namespace

int main(const int argc, char** argv) {
  SpencerApplication application;
  return application.run(argc, argv);
}
