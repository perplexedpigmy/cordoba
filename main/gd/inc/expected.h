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
using Result = expected<T, gd::Error>;


  namespace
  {
    /// @brief creates unexpected results with user data
    /// @param type type of the unexpected
    /// @param msg description
    /// @param loc source location
    /// @return the newly created unexpected 
    tl::unexpected<gd::Error>
    unexpected(gd::ErrorType type, const std::string&& msg, std::source_location loc = std::source_location::current()) {
      return tl::make_unexpected( gd::Error(type, std::move(msg), loc));
    }
  
    /// @brief returns an unexpected from result
    /// @param result The result containing an error
    /// @return the moved unexpected value 
    template <typename T>
    tl::unexpected<gd::Error>
    unexpected(Result<T>&& result) {
      return tl::unexpected<gd::Error>(std::move(result.error()));
    }
  
    /// @brief creates an unexpected with git error
    /// @param loc source location
    /// @return the newly created unexpected 
    tl::unexpected<gd::Error>
    unexpected(std::source_location loc = std::source_location::current()){
      return tl::make_unexpected(gd::Error(gd::ErrorType::GitError, git_error_last(), loc));
    }
  }
