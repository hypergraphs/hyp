// Copyright 2014 Jonathan Graehl-http://graehl.org/
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/** \file

    for configure library (see configure.hpp).

    you can add a validate member like this:

    struct M {
      void validate();
      typedef void configure_validate;
    };

    or

    namespace configure {
    validate_traits<My::M, void> { void call(My::M &); }
    }
*/

#ifndef GRAEHL_SHARED__VALIDATE_HPP
#define GRAEHL_SHARED__VALIDATE_HPP
#pragma once

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <graehl/shared/from_strings.hpp>
#include <graehl/shared/shared_ptr.hpp>
#include <graehl/shared/type_traits.hpp>
#include <graehl/shared/verbose_exception.hpp>
#include <map>
#include <set>
#include <vector>

namespace configure {

using graehl::false_type;
using graehl::true_type;
using graehl::enable_if;

template <class T, class Enable = void>
struct validate_traits {
  static void call(T&) {}
};

template <class T>
struct validate_traits<T, typename T::configure_validate> {
  static void call(T& t) { t.validate(); }
};


SIMPLE_EXCEPTION_PREFIX(config_exception, "configure: ");

template <class I>
struct bounded_range_validate {
  I begin, end;
  std::string desc;
  bounded_range_validate(I const& begin, I const& end, std::string const& desc)
      : begin(begin), end(end), desc(desc) {}
  template <class I2>
  void operator()(I2 const& i2) {
    if (i2 < begin || !(i2 < end))
      throw config_exception(desc + " value " + graehl::to_string(i2) + " - should have ["
                             + graehl::to_string(begin) + " <= value <  " + graehl::to_string(end) + ")");
  }
};

template <class I>
bounded_range_validate<I> bounded_range(I const& begin, I const& end,
                                        std::string const& desc = "value out of bounds") {
  return bounded_range_validate<I>(begin, end, desc);
}

template <class I>
struct bounded_range_inclusive_validate {
  I begin, end;
  std::string desc;
  bounded_range_inclusive_validate(I const& begin, I const& end, std::string const& desc)
      : begin(begin), end(end), desc(desc) {}
  template <class I2>
  void operator()(I2 const& i2) {
    if (i2 < begin || end < i2)
      throw config_exception(desc + " value " + graehl::to_string(i2) + " - should have ["
                             + graehl::to_string(begin) + " <= value <=  " + graehl::to_string(end) + "]");
  }
};

template <class I>
bounded_range_inclusive_validate<I> bounded_range_inclusive(I const& begin, I const& end,
                                                            std::string const& desc = "value out of bounds") {
  return bounded_range_inclusive_validate<I>(begin, end, desc);
}

struct exists {
  template <class Path>
  void operator()(Path const& pathname) {
    boost::filesystem::path path(pathname);
    if (!boost::filesystem::exists(path)) throw config_exception(path.string() + " not found.");
  }
};

struct dir_exists {
  template <class Path>
  void operator()(Path const& pathname) {
    boost::filesystem::path path(pathname);
    if (!is_directory(path)) throw config_exception("directory " + path.string() + " not found.");
  }
};

struct file_exists {
  template <class Path>
  void operator()(Path const& pathname) {
    boost::filesystem::path path((pathname));
    if (!boost::filesystem::exists(path)) throw config_exception("file " + path.string() + " not found.");
    if (is_directory(path)) throw config_exception(path.string() + " is a directory. Need a file.");
  }
};

template <class Val>
struct one_of {
  std::vector<Val> allowed;
  one_of(std::vector<Val> const& allowed) : allowed(allowed) {}
  one_of(one_of const& o) : allowed(o.allowed) {}
  one_of& operator()(Val const& v) {
    allowed.push_back(v);
    return *this;
  }
  template <class Key>
  void operator()(Key const& key) const {
    if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
      throw config_exception(to_string(key) + " not allowed - must be one of " + to_string(*this));
  }
  friend inline std::string to_string(one_of<Val> const& one) {
    return "[" + graehl::to_string_sep(one.allowed, "|") + "]";
  }
};
}

namespace adl {
/// for primitives etc - hopefully lower priority than in-T's-namespace validate(T) due to 2nd arg
template <class T>
void validate(T& t, void* lowerPriorityMatch = 0) {
  configure::validate_traits<T>::call(t);
}

/// you can call this from outside our namespace to get the more specific version via ADL if it exists
template <class T>
void adl_validate(T& t) {
  validate(t);
}
}

namespace std {
template <class T>
void validate(std::set<T> const& v) {
  for (typename std::vector<T>::const_iterator i = v.begin(), e = v.end(); i != e; ++i)
    ::adl::adl_validate(*i);
}
template <class T>
void validate(std::vector<T> const& v) {
  for (typename std::vector<T>::const_iterator i = v.begin(), e = v.end(); i != e; ++i)
    ::adl::adl_validate(*i);
}
template <class Key, class T>
void validate(std::map<Key, T> const& v) {
  for (typename std::map<Key, T>::const_iterator i = v.begin(), e = v.end(); i != e; ++i)
    ::adl::adl_validate(i->second);
}
}

namespace boost {
template <class T>
void validate(boost::optional<T> const& i) {
  if (i) ::adl::adl_validate(*i);
}
template <class T>
void validate(shared_ptr<T> const& i) {
  if (i) ::adl::adl_validate(*i);
}


}

#endif
