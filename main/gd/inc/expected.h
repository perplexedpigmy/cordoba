#pragma once

/// TODO: Requires toolchain support 
// #if __cpp_lib_expected >= 202211L 
// #include <expected>
// #elif
#include <err.h>
#include <tl/expected.hpp>
#include <git2.h>
// #endif

// Temporarily use of expected outside a namespace, Move to Result
template <typename T, typename E>
using expected = tl::expected<T,E>;

template <typename T>
using expected2 = tl::expected<T, gd::Error>;

namespace gd {

  template <typename T>
  using Result = expected<T, gd::Error>;
}

namespace {
  std::string strigify_git_error() noexcept {
    const git_error *err = git_error_last();
    return std::string("[") + std::to_string(err->klass) + std::string("] ") + err->message;
  }

  #define unexpected(type, msg) tl::make_unexpected( gd::Error(__FILE__, __LINE__, type, msg) );
  #define unexpected_err(err) tl::make_unexpected( err );
  #define unexpected_git tl::make_unexpected( gd::Error(__FILE__, __LINE__, gd::ErrorType::GitError, strigify_git_error()) );
}
