#pragma once 

#include <format>
#include <git2.h>

template <>
struct std::formatter<git_oid const*> {
  mutable size_t digits { GIT_OID_HEXSZ };

  constexpr auto parse(std::format_parse_context& ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it == ':') {
      ++it; // Skip the ':'
      // Parse the number of characters
      digits = 0;
      while (it != ctx.end() && std::isdigit(*it)) {
        digits = digits * 10 + (*it - '0');
        ++it;
      }
    }
    return it;
  }

  auto format(git_oid const* oid, std::format_context& ctx) const {
    if (!oid) 
      return std::format_to(ctx.out(), "ROOT");

    digits = digits < 5 or digits > GIT_OID_HEXSZ ? GIT_OID_HEXSZ: digits;
    char buf[GIT_OID_HEXSZ + 1];
      
    return std::format_to(ctx.out(), "{}", git_oid_tostr(buf, digits , oid));
  }

};