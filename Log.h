#pragma once

#include "fmt.h"
#include "Log.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <chrono>
#include <functional>

#include <fmt/format.h>

namespace ps
{
  class Log;

  namespace details
  {
    template <class PrintFuncT>
    struct LogTimer
    {
      LogTimer(PrintFuncT&& print)
          : _printer(std::move(print))
      { }

      LogTimer(const LogTimer&) = delete;
      LogTimer& operator=(const LogTimer&) = delete;

      template <class Func>
      void operator=(Func&&);

    private:
      PrintFuncT _printer;
    };
  }

  template <class OStream>
  OStream& operator<<(OStream& out, const std::filesystem::path& path)
  {
    out << path.c_str();
    return out;
  }

  #define PS_LOG_TIME(logger, format, args) \
    logger.execTime(format, args) = [&]


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

    template <class ... Args>
    [[gnu::noinline]] void throw_(const char* format, Args&& ... args )
    {
      throw Exception(format, std::forward<Args>(args)...);
    }
    
    /// Usage:
    /// logger.execTime("loading file {}", filename) = [&]{ loadFile(fileName); };
    template <class ... Args>
    auto execTime(const char* format, Args&& ... args);

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

  template <class ... Args>
  auto Log::execTime(const char* format, Args&& ... args)
  {
    auto formatted = fmt::format(format, std::forward<Args>(args)... );
    return ::ps::details::LogTimer(
      [&](std::chrono::milliseconds ms) {
          std::cout << _prefix << _levelStrings[Info]
              << " run took " << ms.count() << "ms for " << formatted << '\n';
      }
    );
  }

}

template <class PrintFuncT>
template <class MeasureFuncT>
void ::ps::details::LogTimer<PrintFuncT>::operator=(MeasureFuncT&& func)
{
  namespace c = std::chrono;
  c::steady_clock::time_point start = c::steady_clock::now();
  // TODO: some barrier to ensure parts of func don’t run before the clock ?
  std::forward<MeasureFuncT>(func)();
  c::steady_clock::time_point stop = c::steady_clock::now();
  _printer(c::duration_cast<c::milliseconds>(stop - start));
}