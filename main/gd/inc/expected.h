#pragma once

#include <expected>
#include <git2.h>
#include <err.h>

template <typename T, typename E>
using expected = std::expected<T,E>;

template <typename T>
using Result = expected<T, gd::Error>;


  namespace
{
  /// @brief creates unexpected results with user data
  /// @param type type of the unexpected
  /// @param msg description
  /// @param loc source location
  /// @return the newly created unexpected 
  std::unexpected<gd::Error>
  gd_unexpected(gd::ErrorType type, const std::string&& msg, std::source_location loc = std::source_location::current()) {
    return std::unexpected<gd::Error>(gd::Error(type, std::move(msg), loc));
  }

  /// @brief returns an unexpected from result
  /// @param result The result containing an error
  /// @return the moved unexpected value 
  template <typename T>
  std::unexpected<gd::Error>
  gd_unexpected(Result<T>&& result) {
    return std::unexpected<gd::Error>(std::move(result.error()));
  }

  /// @brief returns an unexpected from an error
  /// @param err The error to wrap
  /// @return the unexpected value 
  inline std::unexpected<gd::Error>
  gd_unexpected(const gd::Error& err) {
    return std::unexpected<gd::Error>(err);
  }

  /// @brief creates an unexpected with git error
  /// @param loc source location
  /// @return the newly created unexpected 
  std::unexpected<gd::Error>
  gd_unexpected(std::source_location loc = std::source_location::current()){
    return std::unexpected<gd::Error>(gd::Error(gd::ErrorType::GitError, git_error_last(), loc));
  }
}
