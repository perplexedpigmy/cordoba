#pragma once 
#include <git2.h>

#include <array>
#include <iostream>
#include <unordered_map>

class Object;

static std::string sha(git_oid const * oid) 
{ 
  char buf[GIT_OID_HEXSZ+1];
  return git_oid_tostr(buf, GIT_OID_HEXSZ, oid); 
} 

std::ostream& operator<<(std::ostream& os, const git_oid& oid) noexcept
{
   constexpr size_t BufSize = 10;

   std::array<char, BufSize> buf;
   return os << "Object(" << git_oid_tostr(buf.data(), BufSize - 1, &oid) <<")" ;
}

std::ostream& operator<<(std::ostream& os, git_tree* tree) noexcept
{
   char buf[10];
   return os << "Tree(" << git_oid_tostr(buf, 9, git_tree_id(tree)) <<")" ;
} 

std::ostream& operator<<(std::ostream& os, git_filemode_t mode) noexcept
{
  const static std::unordered_map<git_filemode_t, char const *>  s = {
    {0000000, "UNRE"},
    {0040000, "TREE"},
    {0100644, "BLOB"},
    {0100755, "EXEC"},
    {0120000, "LINK"},
    {0160000, "CMMT"},
  };
  if (auto i = s.find(mode); i == s.end()) {
    os << "????";
  } else {
    os << i->second;
  }
} 
