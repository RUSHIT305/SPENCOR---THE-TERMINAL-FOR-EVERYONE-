#include "spencer/pty/linux_pty.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <thread>

#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

namespace spencer::pty {
namespace {
[[noreturn]] void throw_system_error(const std::string_view context) {
  throw std::system_error(errno, std::generic_category(), std::string(context));
}

[[nodiscard]] unsigned short terminal_extent(const std::size_t value) {
  return static_cast<unsigned short>(std::min(value, static_cast<std::size_t>(
                                                      std::numeric_limits<unsigned short>::max())));
}

[[nodiscard]] bool executable_absolute_path(const std::string& path) {
  return !path.empty() && path.front() == '/' && ::access(path.c_str(), X_OK) == 0;
}

[[nodiscard]] bool should_spawn_flatpak_host_shell() {
  const char* const flatpak_id = ::getenv("FLATPAK_ID");
  return flatpak_id != nullptr && *flatpak_id != '\0' &&
         ::access("/usr/bin/flatpak-spawn", X_OK) == 0;
}
}  // namespace

LinuxPty::LinuxPty(util::UniqueFd master_fd, const pid_t child_pid) noexcept
    : master_fd_(std::move(master_fd)), child_pid_(child_pid) {}

LinuxPty::~LinuxPty() {
  try {
    terminate();
  } catch (...) {
    // Destructors must not throw. The master file descriptor still closes through RAII.
  }
}

LinuxPty LinuxPty::spawn(const SpawnOptions& options) {
  const std::string shell = resolve_shell(options.shell);
  const int raw_master = ::posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (raw_master < 0) {
    throw_system_error("Could not open POSIX PTY master");
  }
  util::UniqueFd master_fd(raw_master);

  if (::grantpt(master_fd.get()) != 0 || ::unlockpt(master_fd.get()) != 0) {
    throw_system_error("Could not grant or unlock POSIX PTY");
  }

  std::array<char, 256> slave_path{};
  if (::ptsname_r(master_fd.get(), slave_path.data(), slave_path.size()) != 0) {
    throw_system_error("Could not resolve POSIX PTY slave path");
  }
  util::UniqueFd slave_fd(::open(slave_path.data(), O_RDWR | O_NOCTTY | O_CLOEXEC));
  if (!slave_fd) {
    throw_system_error("Could not open POSIX PTY slave");
  }

  const auto set_dimensions = [&options](const int descriptor) {
    const terminal::Dimensions dimensions = terminal::Dimensions::sanitized(
        options.dimensions.rows, options.dimensions.columns);
    const winsize size{
        .ws_row = terminal_extent(dimensions.rows),
        .ws_col = terminal_extent(dimensions.columns),
        .ws_xpixel = 0,
        .ws_ypixel = 0,
    };
    if (::ioctl(descriptor, TIOCSWINSZ, &size) != 0) {
      throw_system_error("Could not set PTY dimensions");
    }
  };
  set_dimensions(master_fd.get());

  const pid_t child = ::fork();
  if (child < 0) {
    throw_system_error("Could not fork shell process");
  }

  if (child == 0) {
    // Child: establish the slave as the controlling terminal, then execute the shell.
    if (::setsid() < 0 || ::ioctl(slave_fd.get(), TIOCSCTTY, 0) != 0) {
      _exit(127);
    }
    if (::dup2(slave_fd.get(), STDIN_FILENO) < 0 || ::dup2(slave_fd.get(), STDOUT_FILENO) < 0 ||
        ::dup2(slave_fd.get(), STDERR_FILENO) < 0) {
      _exit(127);
    }

    master_fd.reset();
    if (slave_fd.get() > STDERR_FILENO) {
      slave_fd.reset();
    }
    if (!options.working_directory.empty() && ::chdir(options.working_directory.c_str()) != 0) {
      _exit(127);
    }
    if (::setenv("TERM", options.term.empty() ? "xterm-256color" : options.term.c_str(), 1) != 0) {
      _exit(127);
    }

    const std::string argv0 = options.login_shell ? make_login_argv0(shell) : shell;
    if (should_spawn_flatpak_host_shell()) {
      // Flatpak terminals need an explicit portal-mediated host launch to access the user's real shell.
      ::execl("/usr/bin/flatpak-spawn", "flatpak-spawn", "--host", shell.c_str(), "-i",
              static_cast<char*>(nullptr));
    }
    ::execl(shell.c_str(), argv0.c_str(), "-i", static_cast<char*>(nullptr));
    _exit(127);
  }

  slave_fd.reset();
  const int current_flags = ::fcntl(master_fd.get(), F_GETFL);
  if (current_flags < 0 || ::fcntl(master_fd.get(), F_SETFL, current_flags | O_NONBLOCK) < 0) {
    const int saved_errno = errno;
    (void)::kill(-child, SIGHUP);
    (void)::waitpid(child, nullptr, 0);
    errno = saved_errno;
    throw_system_error("Could not make PTY master non-blocking");
  }

  return LinuxPty(std::move(master_fd), child);
}

int LinuxPty::descriptor() const noexcept { return master_fd_.get(); }
pid_t LinuxPty::child_pid() const noexcept { return child_pid_; }
bool LinuxPty::running() const noexcept { return child_pid_ > 0 && !exit_status_.has_value(); }

std::optional<int> LinuxPty::exit_status() {
  reap_non_blocking();
  return exit_status_;
}

std::vector<std::uint8_t> LinuxPty::read_available(const std::size_t maximum_bytes) {
  if (!master_fd_) {
    return {};
  }

  std::vector<std::uint8_t> result;
  result.reserve(std::min(maximum_bytes, static_cast<std::size_t>(4096)));
  std::array<std::uint8_t, 4096> buffer{};

  while (result.size() < maximum_bytes) {
    const std::size_t remaining = maximum_bytes - result.size();
    const ssize_t read_count = ::read(master_fd_.get(), buffer.data(), std::min(buffer.size(), remaining));
    if (read_count > 0) {
      result.insert(result.end(), buffer.begin(), buffer.begin() + read_count);
      continue;
    }
    if (read_count == 0) {
      reap_non_blocking();
      break;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EIO) {
      reap_non_blocking();
      break;
    }
    throw_system_error("Could not read PTY output");
  }
  return result;
}

std::size_t LinuxPty::write(const std::span<const std::uint8_t> bytes) {
  if (!master_fd_ || bytes.empty()) {
    return 0;
  }
  while (true) {
    const ssize_t written = ::write(master_fd_.get(), bytes.data(), bytes.size());
    if (written >= 0) {
      return static_cast<std::size_t>(written);
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EIO) {
      return 0;
    }
    throw_system_error("Could not write PTY input");
  }
}

std::size_t LinuxPty::write(const std::string_view bytes) {
  return write(std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
}

void LinuxPty::resize(const terminal::Dimensions requested_dimensions) {
  if (!master_fd_) {
    throw std::runtime_error("Cannot resize a closed PTY");
  }
  const terminal::Dimensions dimensions = terminal::Dimensions::sanitized(
      requested_dimensions.rows, requested_dimensions.columns);
  const winsize size{
      .ws_row = terminal_extent(dimensions.rows),
      .ws_col = terminal_extent(dimensions.columns),
      .ws_xpixel = 0,
      .ws_ypixel = 0,
  };
  if (::ioctl(master_fd_.get(), TIOCSWINSZ, &size) != 0) {
    throw_system_error("Could not resize PTY");
  }
}

void LinuxPty::terminate(const std::chrono::milliseconds grace_period) {
  reap_non_blocking();
  if (child_pid_ <= 0 || exit_status_.has_value()) {
    return;
  }

  (void)::kill(-child_pid_, SIGHUP);
  const auto deadline = std::chrono::steady_clock::now() + grace_period;
  while (std::chrono::steady_clock::now() < deadline) {
    reap_non_blocking();
    if (exit_status_.has_value()) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }

  (void)::kill(-child_pid_, SIGTERM);
  const auto terminate_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{250};
  while (std::chrono::steady_clock::now() < terminate_deadline) {
    reap_non_blocking();
    if (exit_status_.has_value()) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }

  (void)::kill(-child_pid_, SIGKILL);
  while (::waitpid(child_pid_, nullptr, 0) < 0 && errno == EINTR) {
  }
  child_pid_ = -1;
  exit_status_ = 128 + SIGKILL;
}

std::string LinuxPty::resolve_shell(std::string requested_shell) {
  if (requested_shell.empty()) {
    const char* const environment_shell = ::getenv("SHELL");
    if (environment_shell != nullptr) {
      requested_shell = environment_shell;
    }
  }
  if (executable_absolute_path(requested_shell)) {
    return requested_shell;
  }

  // Merged-/usr systems and minimal distributions can expose the POSIX shell
  // at different conventional paths. Probe only absolute, known-safe paths.
  constexpr std::array<std::string_view, 4> fallback_shells{
      "/bin/sh", "/usr/bin/sh", "/bin/bash", "/usr/bin/bash"};
  for (const std::string_view fallback : fallback_shells) {
    if (executable_absolute_path(std::string(fallback))) {
      return std::string(fallback);
    }
  }
  throw std::runtime_error("No executable shell is available from $SHELL or the standard Linux shell paths");
}

std::string LinuxPty::make_login_argv0(const std::string_view shell) {
  const std::size_t slash = shell.find_last_of('/');
  return "-" + std::string(slash == std::string_view::npos ? shell : shell.substr(slash + 1));
}

void LinuxPty::reap_non_blocking() {
  if (child_pid_ <= 0 || exit_status_.has_value()) {
    return;
  }
  int status = 0;
  const pid_t reaped = ::waitpid(child_pid_, &status, WNOHANG);
  if (reaped == child_pid_) {
    child_pid_ = -1;
    if (WIFEXITED(status)) {
      exit_status_ = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      exit_status_ = 128 + WTERMSIG(status);
    } else {
      exit_status_ = status;
    }
  } else if (reaped < 0 && errno == ECHILD) {
    child_pid_ = -1;
    exit_status_ = -1;
  }
}

}  // namespace spencer::pty
