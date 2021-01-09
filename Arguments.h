#pragma once

#include <string>
#include <optional>

#include "fmt.h"

namespace ps
{
  struct Argument
  {
    std::string Doc;
    /// If nullopt, argument is required
    std::optional<std::string> Value;
    bool Flag = false;
    bool Given = false;
  };

  /// Name to Argument
  using ArgMap = std::unordered_map<std::string, Argument>;

  inline void merge(ArgMap& dest, const ArgMap& source)
  {
    for (auto& arg: source) {
      if (dest.find(arg.first) != end(dest)) {
        throw Exception("Duplicated argument: {}", arg.first);
      }
      dest[arg.first] = arg.second;
    }
  }

}