#pragma once

#include <unistd.h>

namespace spencer::util {

class UniqueFd final {
 public:
  constexpr UniqueFd() noexcept = default;
  explicit constexpr UniqueFd(const int descriptor) noexcept : descriptor_(descriptor) {}

  ~UniqueFd() { reset(); }

  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;

  UniqueFd(UniqueFd&& other) noexcept : descriptor_(other.release()) {}

  UniqueFd& operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  [[nodiscard]] int get() const noexcept { return descriptor_; }
  [[nodiscard]] bool valid() const noexcept { return descriptor_ >= 0; }
  explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] int release() noexcept {
    const int released = descriptor_;
    descriptor_ = -1;
    return released;
  }

  void reset(const int replacement = -1) noexcept {
    if (descriptor_ >= 0) {
      (void)::close(descriptor_);
    }
    descriptor_ = replacement;
  }

 private:
  int descriptor_{-1};
};

}  // namespace spencer::util
