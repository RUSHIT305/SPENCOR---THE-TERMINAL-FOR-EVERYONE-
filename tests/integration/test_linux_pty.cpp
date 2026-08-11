#include "spencer/pty/linux_pty.hpp"
#include "test_framework.hpp"

#include <chrono>
#include <string>
#include <thread>

namespace {

std::string read_until(spencer::pty::LinuxPty& pty, const std::string& marker,
                       const std::chrono::milliseconds timeout) {
  std::string output;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto bytes = pty.read_available();
    output.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (output.find(marker) != std::string::npos) {
      return output;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }
  return output;
}

void write_all(spencer::pty::LinuxPty& pty, const std::string& input) {
  std::size_t offset = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
  while (offset < input.size() && std::chrono::steady_clock::now() < deadline) {
    offset += pty.write(std::string_view(input).substr(offset));
    if (offset < input.size()) {
      std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
  }
  SPENCER_REQUIRE(offset == input.size());
}

}  // namespace

SPENCER_TEST(linux_pty_launches_shell_relays_output_resizes_and_reaps) {
  spencer::pty::SpawnOptions options;
  options.shell = "/bin/sh";
  options.login_shell = false;
  options.dimensions = {24, 80};
  spencer::pty::LinuxPty pty = spencer::pty::LinuxPty::spawn(options);

  write_all(pty, "printf '__SPENCER_PTY_OK__\\n'\n");
  const std::string initial_output =
      read_until(pty, "__SPENCER_PTY_OK__\r\n", std::chrono::seconds{2});
  SPENCER_REQUIRE_MESSAGE(initial_output.find("__SPENCER_PTY_OK__") != std::string::npos,
                          "interactive shell did not emit expected PTY marker");

  pty.resize({40, 100});
  write_all(pty, "stty size; printf '__SPENCER_PTY_RESIZE__\\n'; exit\n");
  const std::string resized_output =
      read_until(pty, "__SPENCER_PTY_RESIZE__\r\n", std::chrono::seconds{2});
  SPENCER_REQUIRE_MESSAGE(resized_output.find("40 100") != std::string::npos,
                          std::string("PTY did not report requested terminal dimensions; output: ") +
                              resized_output);

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
  while (!pty.exit_status().has_value() && std::chrono::steady_clock::now() < deadline) {
    (void)pty.read_available();
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }
  SPENCER_REQUIRE(pty.exit_status().has_value());
  SPENCER_REQUIRE(!pty.running());
}
