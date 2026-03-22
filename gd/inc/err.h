#pragma once 
#include <string>
#include <source_location>
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
    :_file(file), _line(line), _type(type), _msg(msg), _gitKlass{0}, _func("") {}

    Error(char const * const file, int line, ErrorType type, git_error const * err) noexcept
    :_file(file), _line(line), _type(type), _msg(err->message), _gitKlass{err->klass}, _func("") {}

    Error(ErrorType type, const std::string&& msg, std::source_location loc = std::source_location::current()) noexcept
    :_file(loc.file_name()), _line(loc.line()), _func(loc.function_name()), _type(type), _msg(msg), _gitKlass{0} {}

    Error(ErrorType type, git_error const * err, std::source_location loc = std::source_location::current()) noexcept
    :_file(loc.file_name()), _line(loc.line()), _func(loc.function_name()), _type(type), _msg(err->message), _gitKlass{err->klass} {}

    char const * _file;
    char const * _func;
    int          _line;
    ErrorType    _type;
    std::string  _msg;
    int          _gitKlass;

    operator std::string() const noexcept 
    {
      return  ""s + _file + ":" + std::to_string(_line) + " '" + _func + "' " + _msg;
    }
  };
}