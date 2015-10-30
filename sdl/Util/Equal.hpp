// Copyright 2014-2015 SDL plc
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/** \file

    mostly for unit testing - compare objects via string representation.
*/

#ifndef SDL_UTIL_EQUAL_HPP
#define SDL_UTIL_EQUAL_HPP
#pragma once

#include <cmath>
#include <set>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <type_traits>
#include <sdl/LexicalCast.hpp>
#include <sdl/Util/LogHelper.hpp>
#include <sdl/Util/PrintRange.hpp>
#include <sdl/Util/AsciiCase.hpp>
#include <sdl/Util/Math.hpp>
#include <sdl/Util/Chomp.hpp>
#include <graehl/shared/split.hpp>

namespace sdl {
namespace Util {

template <class T>
bool floatEqualWarn(T v1, T v2, T epsilon = (T)1e-6, char const* name1 = "GOT", char const* name2 = "REF") {
  bool const eq = floatEqual(v1, v2, epsilon);
  if (!eq)
    SDL_WARN(Util.floatEqual, '(' << v1 << " = " << name1 << ") != (" << name2 << " = " << v2
                                  << ") with tolerance epsilon=" << epsilon);
  return eq;
}

using graehl::chomped_lines;

/**
   \return whether sets of lines are the same.
*/
bool StringsUnorderedEqual(std::vector<std::string> const& lines1, std::vector<std::string> const& lines2,
                           bool sortWords = false, char const* name1 = "GOT", char const* name2 = "REF",
                           bool warn = true);

inline bool LinesUnorderedEqual(std::istream& stream1, std::istream& stream2, bool sortWords = false,
                                char const* name1 = "GOT", char const* name2 = "REF", bool warn = true) {
  return StringsUnorderedEqual(chomped_lines(stream1), chomped_lines(stream2), sortWords, name1, name2, warn);
}

inline bool LinesUnorderedEqual(std::istream& stream1, std::string const& lines2, bool sortWords = false,
                                char const* name1 = "GOT", char const* name2 = "REF", bool warn = true) {
  return StringsUnorderedEqual(chomped_lines(stream1), graehl::chomped_lines(lines2), sortWords, name1, name2,
                               warn);
}

inline bool LinesUnorderedEqual(std::string const& str1, std::string const& str2, bool sortWords = false,
                                char const* name1 = "GOT", char const* name2 = "REF", bool warn = true) {
  return StringsUnorderedEqual(chomped_lines(str1), chomped_lines(str2), sortWords, name1, name2, warn);
}

template <class Val1>
inline typename std::enable_if<!std::is_base_of<std::istream, Val1>::value, bool>::type
LinesUnorderedEqual(Val1 const& val1, std::string const& val2, bool sortWords = false,
                    char const* name1 = "GOT", char const* name2 = "REF", bool warn = true) {
  std::stringstream stream1;
  stream1 << val1;
  return StringsUnorderedEqual(chomped_lines(stream1), chomped_lines(val2), sortWords, name1, name2, warn);
}

inline void ReplaceDigits(std::string& str, char replaceDigitsBy = '#') {
  for (std::string::iterator i = str.begin(), e = str.end(); i != e; ++i)
    if (isAsciiDigit(*i)) *i = replaceDigitsBy;
}

/**
   return str with runs of digits replaced by 'replaceIntegersBy', so long as
   the digits are not:

   immediately preceeded or followed by a decimal point '.',

   preceded by '-'

   followed by end of line or '['

   (the last is a hack for comparing hypergraph output)
*/
std::string ReplacedIntegers(std::string const& str, std::string const& replaceIntegersBy = "#");

inline bool LinesUnorderedEqualIgnoringDigits(std::string str1, std::string str2, bool sortWords = false,
                                              char const* name1 = "GOT", char const* name2 = "REF",
                                              char replaceDigitsBy = '#') {
  if (LinesUnorderedEqual(str1, str2, sortWords, name1, name2, false)) return true;
  SDL_TRACE(Util.Equal, "not exactly equal - trying with digits replaced: [(test)" << str1 << " != " << str2
                                                                                   << "(reference)]\n");
  ReplaceDigits(str1, replaceDigitsBy);
  ReplaceDigits(str2, replaceDigitsBy);
  bool ok = LinesUnorderedEqual(str1, str2, sortWords, name1, name2);
  if (ok)
    SDL_WARN(Util.Equal, " OK - Equal after digit replacement.");
  else
    SDL_WARN(Util.Equal, "not exactly equal even after digit replacement and ignoring line order: [(test)"
                             << str1 << "  !=  " << str2 << " (reference)]\n");
  return ok;
}

inline bool LinesUnorderedEqualIgnoringIntegers(std::string str1, std::string str2, bool sortWords = false,
                                                char const* name1 = "GOT", char const* name2 = "REF",
                                                std::string const& replaceIntegersBy = "#") {
  if (LinesUnorderedEqual(str1, str2, sortWords, name1, name2, false)) return true;
  SDL_TRACE(Util.Equal, "not exactly equal - trying with digits replaced: [(test)" << str1 << " != " << str2
                                                                                   << "(reference)]\n");
  str1 = ReplacedIntegers(str1, replaceIntegersBy);
  str2 = ReplacedIntegers(str2, replaceIntegersBy);
  bool ok = LinesUnorderedEqual(str1, str2, sortWords, name1, name2);
  if (!ok)
    SDL_WARN(Util.Equal, "not exactly equal even after integer replacement and ignoring line order: [(test)\n"
                             << str1 << "\n  !=  \n" << str2 << "\n (reference)]\n");
  else
    SDL_WARN(Util.Equal, " OK - Equal after integer replacement.");
  return ok;
}

template <class Val1>
bool LinesUnorderedEqualIgnoringDigits(Val1 const& val1, std::string const& ref, bool sortWords = false,
                                       char const* name1 = "GOT", char const* name2 = "REF",
                                       char replaceDigitsBy = '#') {
  std::stringstream ss;
  ss << val1;
  return LinesUnorderedEqualIgnoringDigits(ss.str(), ref, sortWords, name1, name2, replaceDigitsBy);
}

template <class Val1>
bool LinesUnorderedEqualIgnoringIntegers(Val1 const& val1, std::string const& ref, bool sortWords = false,
                                         char const* name1 = "GOT", char const* name2 = "REF",
                                         std::string const& replaceIntegersBy = "#") {
  std::stringstream ss;
  ss << val1;
  return LinesUnorderedEqualIgnoringIntegers(ss.str(), ref, sortWords, name1, name2, replaceIntegersBy);
}

template <class Val, class Val2>
bool LinesUnorderedEqualIgnoringIntegers(Val const& val, Val2 const& val2, bool sortWords = false,
                                         char const* name1 = "GOT", char const* name2 = "REF",
                                         std::string const& replaceIntegersBy = "#") {
  std::stringstream ss;
  ss << val;
  std::stringstream ss2;
  ss2 << val2;
  return LinesUnorderedEqualIgnoringIntegers(ss.str(), ss2.str(), sortWords, name1, name2, replaceIntegersBy);
}

/**
   \return true if the object (printed using operator<<) equals the
   string.
*/
template <class T>
bool isStrEqual(T const& val1, std::string const& val2, char const* name1 = "GOT",
                char const* name2 = "REF") {
  std::stringstream ss;
  ss << val1;
  bool ok = val2 == ss.str();
  if (!ok) {
    SDL_WARN(Util, "[" << name1 << "] NOT EQUAL [" << name2 << "]:\n"
                       << "  " << ss.str() << " [" << name1 << "]\n"
                       << "  " << val2 << " [" << name2 << ']');
  }
  return ok;
}

template <class T>
bool byStrEqual(T const& val1, T const& val2, char const* name1 = "", char const* name2 = "") {
  std::stringstream ss;
  ss << val2;
  return isStrEqual(val1, ss.str(), name1, name2);
}

void normalizeLine(std::string& line, bool sortWords, char wordsep = ' ');
}
}


#define SDL_ARE_EQUAL_STR(obj, val2) sdl::Util::byStrEqual(obj, val2, #obj, #val2)
#define SDL_REQUIRE_EQUAL_STR(obj, val2) BOOST_REQUIRE(SDL_ARE_EQUAL_STR(obj, val2))
#define SDL_CHECK_EQUAL_STR(obj, val2) BOOST_CHECK(SDL_ARE_EQUAL_STR(obj, val2))

#endif
