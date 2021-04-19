#include "Strings.h"
#include "fmt.h"

#include <fstream>

using namespace ps;
using namespace std;

string ps::readTextFile(const filesystem::path file)
{
  auto entry = filesystem::directory_entry(file);

  if (not entry.exists()) {
    throw Exception("File '{}' does not exist or no permissions to read it",
        static_cast<string>(file));
  }

  if (not entry.is_regular_file() or not entry.is_symlink()) {
    throw Exception("Not supporting reading '{}', it’s not a normal file or a "
        "symlink.", static_cast<string>(file));
  }

  string result;
  result.resize(entry.file_size());

  ifstream in(file);
  if (not in) {
    throw Exception("Failed to open '{}' for reading", static_cast<string>(file));
  }

  in.read(result.data(), result.size());
  if (not in) {
    throw Exception("Failed to read '{}'", static_cast<string>(file));
  }

  return result;
}

void ps::split(vector<string_view>& out, string_view in, char delim)
{
  out.clear();

  auto pos = in.find(delim);
  while (pos != string_view::npos) {
    out.push_back(string_view(begin(in), pos));
    in = in.substr(pos + 1);
    pos = in.find(delim);
  }
  out.push_back(string_view(begin(in), end(in) - begin(in)));
}