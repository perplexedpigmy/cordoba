#pragma once 
#include <git2.h>

#include <array>
#include <iostream>
#include <unordered_map>
#include <collector.h>
#include <spdlog/spdlog.h>

namespace
{
  template <typename K, typename V>
  char const * const stringify(K key, const std::unordered_map<K, V>& m) {
    if (auto i = m.find(key); i == m.end())
    {
      return "??????";
    }
    else
    {
      return i->second;
    }
  }

  char const * const stringify(git_object_t objectType) noexcept {
    const static std::unordered_map<git_object_t, char const *const> m = {
      {GIT_OBJECT_ANY,       "ANY    "},
      {GIT_OBJECT_INVALID,   "INVALID"},
      {GIT_OBJECT_COMMIT,    "COMMIT "},
      {GIT_OBJECT_TREE,      "TREE   "},
      {GIT_OBJECT_BLOB,      "BLOB   "},
      {GIT_OBJECT_TAG,       "TAG    "},
      {GIT_OBJECT_OFS_DELTA, "OFS Δ  "},
      {GIT_OBJECT_REF_DELTA, "REF Δ  "}};
    
    return stringify(objectType, m);
  }

  char const * const stringify(git_filemode_t fileMode) noexcept {
    const static std::unordered_map<git_filemode_t, char const *const> m = {
      {GIT_FILEMODE_UNREADABLE,      "UNREAD"},
      {GIT_FILEMODE_TREE,            "TREE  "},
      {GIT_FILEMODE_BLOB,            "BLOB  "},
      {GIT_FILEMODE_BLOB_EXECUTABLE, "EXEC  "},
      {GIT_FILEMODE_LINK,            "LINK  "},
      {GIT_FILEMODE_COMMIT,          "COMMIT"}};
    
    return stringify(fileMode, m);
  }

  static std::string sha(git_oid const *oid)
  {
    char buf[GIT_OID_HEXSZ + 1];
    return git_oid_tostr(buf, GIT_OID_HEXSZ, oid);
  }

  static std::string shortSha(git_oid const *oid)
  {
    char buf[GIT_OID_HEXSZ + 1];
    return git_oid_tostr(buf, GIT_OID_HEXSZ, oid);
  }

  std::ostream& operator<<(std::ostream &os, const git_oid &oid) noexcept
  {
    constexpr size_t BufSize = 10;

    std::array<char, BufSize> buf;
    return os << "Object(" << git_oid_tostr(buf.data(), BufSize - 1, &oid) << ")";
  }

  std::ostream &operator<<(std::ostream &os, git_tree *tree) noexcept
  {
    char buf[10];
    return os << "Tree(" << git_oid_tostr(buf, 9, git_tree_id(tree)) << ")";
  }


  std::ostream &
  operator<<(std::ostream &os, git_object_t objectType) noexcept
  {
    return os << stringify(objectType);
  }

  std::ostream &
  operator<<(std::ostream &os, git_filemode_t fileMode) noexcept
  {
    return os << stringify(fileMode);
  }

  std::ostream &operator<<(std::ostream &os, gd::ObjectUpdate const &obj) noexcept
  {
    return os << obj.oid() << "  " << obj.mod() << " " << obj.name();
  }
}

template <>
struct fmt::formatter<git_oid> :fmt::formatter<std::string> 
{
  auto format(const git_oid& oid, format_context &ctx) const -> decltype(ctx.out())
  {
    return fmt::format_to(ctx.out(), "[{}]", sha(&oid));
  }
};

template <>
struct fmt::formatter<git_filemode_t> :fmt::formatter<std::string> 
{
  auto format(git_filemode_t mode, format_context &ctx) const -> decltype(ctx.out())
  {
    return fmt::format_to(ctx.out(), "{}", stringify(mode));
  }
};

template <>
struct fmt::formatter<git_object_t> :fmt::formatter<std::string> 
{
  auto format(git_object_t type, format_context &ctx) const -> decltype(ctx.out())
  {
    return fmt::format_to(ctx.out(), "{}", stringify(type));
  }
};

template <>
struct fmt::formatter<std::filesystem::path> :fmt::formatter<std::string> 
{
  auto format(const std::filesystem::path& path, format_context &ctx) const -> decltype(ctx.out())
  {
    return fmt::format_to(ctx.out(), "{}", path.c_str());
  }
};