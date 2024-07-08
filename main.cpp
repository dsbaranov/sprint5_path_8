#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

static regex regex_include_external(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
static regex regex_include_internal(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

path operator""_p(const char *data, std::size_t sz) {
  return path(data, data + sz);
}

string GetFileContents(string file) {
  ifstream stream(file);

  // конструируем string по двум итераторам
  return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

bool PathIsValid(path p) {
  ifstream in(p);
  if (!in) {
    return false;
  }
  return true;
}

bool IncludeRecursive(
    const path &target, const path &parent, stringstream &out,
    const string &source_filename = "",
    const vector<path> &include_directories = vector<path>()) {
  ifstream in_stream(target);
  if (in_stream) {
    string tmp_line;
    size_t line_counter{1};
    while (getline(in_stream, tmp_line)) {
      smatch match;
      if (regex_match(tmp_line, match, regex_include_external)) {
        bool include_found = false;
        path p(static_cast<string>(match[1]));
        if (PathIsValid(target.parent_path() / p)) {
          include_found =
              IncludeRecursive(target.parent_path() / p, target.parent_path(),
                               out, target.string(), include_directories);
        }
        if (!include_found) {
          for (const path &include_dir : include_directories) {
            if (!PathIsValid((include_dir / p))) {
              continue;
            }
            include_found =
                IncludeRecursive((include_dir / p), target.parent_path(), out,
                                 target.string(), include_directories);
            if (include_found) {
              break;
            }
          }
        }
        if (!include_found) {
          cout << "unknown include file " << match[1] << " at file "
               << source_filename << " at line " << line_counter << endl;
          return false;
        }
      } else if (regex_match(tmp_line, match, regex_include_internal)) {
        bool include_found = false;
        path p(static_cast<string>(match[1]));
        for (const path &include_dir : include_directories) {
          if (!PathIsValid((include_dir / p))) {
            continue;
          }
          include_found =
              IncludeRecursive((include_dir / p), parent, out, target.string(),
                               include_directories);
          if (include_found) {
            break;
          }
        }
        if (!include_found) {
          cout << "unknown include file " << match[1] << " at file "
               << source_filename << " at line " << line_counter << endl;
          return false;
        }
      } else {
        out << tmp_line << "\n";
      }
      ++line_counter;
    }
    in_stream.close();
    return true;
  } else {
    return false;
  }
  return true;
}

// напишите эту функцию
bool Preprocess(const path &in_file, const path &out_file,
                const vector<path> &include_directories) {
  stringstream out_content;
  bool result = IncludeRecursive(in_file, in_file.parent_path(), out_content,
                                 in_file.string(), include_directories);
  ofstream out_stream(out_file);
  out_stream << out_content.str();
  out_stream.close();
  return result;
};

void Test() {
  error_code err;
  filesystem::remove_all("sources"_p, err);
  filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
  filesystem::create_directories("sources"_p / "include1"_p, err);
  filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

  {
    ofstream file("sources/a.cpp");
    file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include \"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#   include<dummy.txt>\n"
            "}\n"s;
  }
  {
    ofstream file("sources/dir1/b.h");
    file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include"s;
  }
  {
    ofstream file("sources/dir1/subdir/c.h");
    file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "// text from c.h after include\n"s;
  }
  {
    ofstream file("sources/dir1/d.h");
    file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"s;
  }
  {
    ofstream file("sources/include1/std1.h");
    file << "// std1\n"s;
  }
  {
    ofstream file("sources/include2/lib/std2.h");
    file << "// std2\n"s;
  }

  assert(
      (!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                   {"sources"_p / "include1"_p, "sources"_p / "include2"_p})));

  ostringstream test_out;
  test_out << "// this comment before include\n"
              "// text from b.h before include\n"
              "// text from c.h before include\n"
              "// std1\n"
              "// text from c.h after include\n"
              "// text from b.h after include\n"
              "// text between b.h and c.h\n"
              "// text from d.h before include\n"
              "// std2\n"
              "// text from d.h after include\n"
              "\n"
              "int SayHello() {\n"
              "    cout << \"hello, world!\" << endl;\n"s;

  assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() { Test(); }