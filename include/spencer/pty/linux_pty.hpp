#pragma once

#include "spencer/terminal/types.hpp"
#include "spencer/util/unique_fd.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>

namespace spencer::pty {

struct SpawnOptions final {
  terminal::Dimensions dimensions{};
  std::string shell{};
  std::string working_directory{};
  std::string term{"xterm-256color"};
  bool login_shell{true};
};

class LinuxPty final {
 public:
  LinuxPty() = default;
  ~LinuxPty();

  LinuxPty(const LinuxPty&) = delete;
  LinuxPty& operator=(const LinuxPty&) = delete;
  LinuxPty(LinuxPty&&) noexcept = default;
  LinuxPty& operator=(LinuxPty&&) noexcept = default;

  static LinuxPty spawn(const SpawnOptions& options = {});

  [[nodiscard]] int descriptor() const noexcept;
  [[nodiscard]] pid_t child_pid() const noexcept;
  [[nodiscard]] bool running() const noexcept;
  [[nodiscard]] std::optional<int> exit_status();

  std::vector<std::uint8_t> read_available(std::size_t maximum_bytes = 64U * 1024U);
  std::size_t write(std::span<const std::uint8_t> bytes);
  std::size_t write(std::string_view bytes);
  void resize(terminal::Dimensions dimensions);
  void terminate(std::chrono::milliseconds grace_period = std::chrono::milliseconds{1500});

 private:
  LinuxPty(util::UniqueFd master_fd, pid_t child_pid) noexcept;
  [[nodiscard]] static std::string resolve_shell(std::string requested_shell);
  [[nodiscard]] static std::string make_login_argv0(std::string_view shell);
  void reap_non_blocking();

  util::UniqueFd master_fd_{};
  pid_t child_pid_{-1};
  std::optional<int> exit_status_{};
};

}  // namespace spencer::pty
