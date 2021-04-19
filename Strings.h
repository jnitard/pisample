#pragma once

#include <string>
#include <filesystem>


namespace ps
{
  /// Read a text file and return its contents as a string.
  /// Not ideal for huge files.
  std::string readTextFile(const std::filesystem::path file);

  /// Split a string_view into parts, over a single character.
  void split(std::vector<std::string_view>& out, std::string_view in, char delim);

  /// Remove trading and leading instance of a matching characters from a string_view.
  /// Defaults to removing only spaces.
  template <class Pred = bool (*)(char)>
  std::string_view trim(
      std::string_view in,
      Pred&& p = [](char c){ return c == ' '; })
  {
    while (in.size() and std::forward<Pred>(p)(in.front())) {
      in = in.substr(1);
    }
    while (in.size() and std::forward<Pred>(p)(in.back())) {
      in = in.substr(0, in.size() - 1);
    }
    return in;
  }

  /// No from chars in this GCC version (8.3.0) :'(
  /// Going the slow way as this is not needed in the fastpath.
  /// Throw on errors.
  inline int svtoi(std::string_view in)
  {
    return std::stoi(std::string(in));
  }
}