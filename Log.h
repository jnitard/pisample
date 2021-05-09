#pragma once

#include <iostream>
#include <string>
#include <filesystem>

#include <fmt/format.h>

namespace ps
{
  template <class OStream>
  OStream& operator<<(OStream& out, const std::filesystem::path& path)
  {
    out << path.c_str();
    return out;
  }

  class Log
  {
  public:
    Log(const char* prefix)
      : _prefix(fmt::format("[{}]", prefix))
    { }

    template <class ... Args>
    void debug(const char* format, Args&& ... args )
    {
      log(Debug, format, std::forward<Args>(args)...);
    }

    template <class ... Args>
    void info(const char* format, Args&& ... args )
    {
      log(Info, format, std::forward<Args>(args)...);
    }

    template <class ... Args>
    void warn(const char* format, Args&& ... args )
    {
      log(Warn, format, std::forward<Args>(args)...);
    }
  
    template <class ... Args>
    void error(const char* format, Args&& ... args )
    {
      log(Error, format, std::forward<Args>(args)...);
    }

  private:
    std::string _prefix;
    
    enum Level
    {
      Debug,
      Info,
      Warn,
      Error
    };

    static inline constexpr std::array _levelStrings = {
      " DBG : ",
      " INF : ",
      " WRN : ",
      " ERR : "
    };

    template <class ... Args>
    void log(Level level, const char* format, Args&& ... args )
    {
      std::cout << _prefix << _levelStrings[level]
                << fmt::format(format, std::forward<Args>(args)... ) << '\n';
    }
  };
}