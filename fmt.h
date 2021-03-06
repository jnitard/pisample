#pragma once

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <exception>
#include <string>

namespace ps
{
  class Exception : public std::exception
  {
  public:
    template <class ... Args>
    Exception(const char* format, Args&& ... args)
      : _what(fmt::format(format, std::forward<Args>(args)...))
    { }

    const char* what() const noexcept override
    {
      return _what.c_str();
    }

  private:
    std::string _what;
  };
}
