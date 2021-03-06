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
/*
 Created on: May 2, 2012
     Author: skohli
 */
#ifndef SDL_CONFIGURE__YAMLCONFIGPROCESSOR_HPP
#define SDL_CONFIGURE__YAMLCONFIGPROCESSOR_HPP
#pragma once

#include <sdl/Config/Config.hpp>
#include <sdl/Util/PrintRange.hpp>
#include <sdl/Util/StringBuilder.hpp>
#include <sdl/Path.hpp>
#include <boost/algorithm/string.hpp>
#include <map>
#include <sstream>
#include <utility>

class ProcessYAMLAccess;  // see TestProcessYAML

namespace sdl {
namespace Config {

/**
   \class YAMLConfigProcessor
   Config Processor to reolve keywords like 'basis', 'replace' and merge config files in
   to a single config node.
 */
struct YAMLConfigProcessor {
  /// memoized loadConfig - typical case: config file w/ many instances of same basis: x
  ConfigNode loadConfigOnce(Path const& path, OptPath const& forPath = OptPath()) const;

  /**
      Constructor
   */
  YAMLConfigProcessor(OptPath const& forPath = OptPath())
      : optPath_(forPath), optPathInitialDepth_(optPath_.size()) {}

  /**
     \return a fully processed copy of in (not modifying the input), expanding
     "*-path" values relative to fsPrefix. see
     /docs/xmt-configure-with-yaml-userdoc.md for a full description of the
     processing.
  */
  ConfigNode process(ConfigNode const& in, Path fsPrefix);

  /**
      Set file path, to be used for logging.
     *

      \param filePath, Path to the file being processed.
   */
  void setFilePath(Path const& filePath) { filePath_ = filePath; }

  /**
      Returns the path (including file-name) as a string for reporting errors.
   */
  std::string getFilePath() const { return filePath_.string(); }

  /**
      Push the Key of the node being processed on to the stack. for path() log msgs only
   */
  void push(std::string const& key) const {
    optPath_.push_back(key);
    // SDL_DEBUG(Config.PushPop, "push +" << optPath_.back() << " => " << optPath_.size());
  }

  /**
      Pop a key.
   */
  void pop() const {
    // SDL_DEBUG(Config.PushPop, "popped " << optPath_.back() << " -1 => " << optPath_.size() - 1);
    optPath_.pop_back(false);
  }

  /**
      Dump the stack in to a string, for use Pop a key.
   */
  std::string path() const {
    Util::StringBuilder ss;
    if (!filePath_.empty()) ss(filePath_)(':');
    ss.range(optPath_, '.');
    return ss.str();
  }

  OptPath const& optPath() const { return optPath_; }

  friend class ::ProcessYAMLAccess;


  /**
      Resolves paths for all nodes that have a key ending in '-path' substring.
   */
  ConfigNode resolvePaths(ConfigNode const&, Path const&) const;
  /**
      Merges two nodes, Part of the basis expansion.
      Overriding the values for duplicate keys in the first node with values from the second node.
      Any nodes not present in node1 and prsent in node2 are added to node1.
      any nodes in node1 that are not found in node2 are left as is.
   */
  ConfigNode mergeNodes(ConfigNode const& base, ConfigNode const& overwrite, bool skipBasisOverwrite) const;
  enum { kCopyBasisKey = 0, kSkipBasisKey = 1 };
  /// skipBasisOverwrite = 1 has the effect of removing overwrite["basis"] - subtrees are still merged
  /// including 'basis' - although in the context of global processing we always bottom-up interpret+remove
  /// basis first before merging, so that shouldn't matter

  typedef std::string InstanceName;
  typedef std::string Category;
  typedef std::pair<Category, ConfigNode> Instance;
  typedef unordered_map<InstanceName, Instance> Instances;

 protected:
  /**
      throws if there are duplicate declarations of Resource, modules or pipeline instances.
   */
  void registerInstance(std::string const&, std::string const&, ConfigNode const&);

  ConfigNode mergeMaps(ConfigNode const&, ConfigNode const&, bool) const;
  ConfigNode mergeSeqs(ConfigNode const& base, ConfigNode const& append) const;

  /**
      Processes basis nodes be overlaying them/
     *
      Processes basis nodes such as:
      basis: [a.yml, b.yml, c.yml, ...
      as follows:
      temp = a.yml <- b.yml [b is overlaid on a]
      temp = temp <- c.yml [c is overlaid on temp] and so on.
      This method calls mergeNodes to do the overlay.
      During this operation sequences in the overlaid node get append to the base node.
      Scalar values are over-written.
      Maps are traversed and one of the above two operations are perfromed at each leaf node.
     *
   */
  ConfigNode expandBasis(ConfigNode const&, Path const&) const;

  void addBasisElement(std::vector<ConfigNode>& basisNodes, ConfigNode const& inBasis, Path const& fsPrefix,
                       YAMLConfigProcessor const& proc) const;
  /**
      Handles node categories like 'resource', 'module' & 'pipeline'.
   */
  ConfigNode resolveCategories(ConfigNode const& in);
  /**
      Process replace keyword and converts them into mapped values as per configuration document.
     *
      Translates:
      main:
        replace name: newVal
      to
      main:
        replace:
          name: newVal
   */
  ConfigNode resolveReplaceNodes(ConfigNode const& in) const;
  /**
      Process replace node generated in resolveReplaceNodes call and do actual replacements.
     *
      Processes nodes such as:
      main:
        name: oldVal
        replace:
          name: newVal
      to
      main:
        name: newVal
   */
  ConfigNode processReplaceNodes(ConfigNode const& in);

  Path filePath_;
  mutable OptPath optPath_;  // for log messages. mutable because of insanity - DFS push/pop discipline. to
  // keep refs sane, OptPath uses StableVector
  mutable unordered_map<Path, ConfigNode> loadedConfig_;
  std::size_t const optPathInitialDepth_;
  Instances instances_;
};


}}

#endif
