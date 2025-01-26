#pragma once 
#include <string>
using namespace std::literals;

namespace gd {
  enum class ErrorType {
    MissingRepository,
    BadDir,
    BadFile,
    BadCommit,
    EmptyCommit,
    BlobError,
    GitError,
    InitialContext,
    Deleted, 
    NotFound,
    Application, /* Generic Application error */
  };

  struct Error {
    Error(char const * const file, int line, ErrorType type, const std::string&& msg) noexcept
    :_file(file), _line(line), _type(type), _msg(msg) {}

    char const * _file;
    int          _line;
    ErrorType    _type;
    std::string  _msg;

    operator std::string() const noexcept 
    {
      return  ""s + _file + ":" + std::to_string(_line) + "  " + _msg;
    }
  };
}