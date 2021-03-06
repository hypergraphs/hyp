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
#include <sdl/Hypergraph/Arc.hpp>
#include <sdl/Hypergraph/ArcParserFct.hpp>
#include <sdl/Hypergraph/MutableHypergraph.hpp>
#include <sdl/Hypergraph/Weight.hpp>
#include <sdl/Hypergraph/fs/WriteOpenFstFormat.hpp>
#include <iostream>
#include <sstream>
#include <string>

#ifndef NDEBUG
#include <sdl/Hypergraph/HypergraphWriter.hpp>
#endif

#include <sdl/Hypergraph/HypergraphMain.hpp>
#include <sdl/Vocabulary/HelperFunctions.hpp>
#include <sdl/Util/Input.hpp>
#include <sdl/Util/Locale.hpp>
#include <sdl/Util/ProgramOptions.hpp>
#include <sdl/IVocabulary.hpp>
#include <sdl/SharedPtr.hpp>

namespace sdl {
namespace Hypergraph {

struct HypToOpenFstText {
  static int run_main(int ac, char* av[]) {
    namespace po = boost::program_options;
    try {
      po::options_description generic("Allowed options");

      add(generic)("config-file,c", po::value<std::string>(), "config file name");
      add(generic)("help,h", "produce help message");

      po::options_description hidden("Hidden options");
      hidden.add_options()("input-file", po::value<std::string>(), "input file");

      po::options_description cmdline_options;
      cmdline_options.add(generic).add(hidden);

      po::options_description config_file_options;
      config_file_options.add(generic).add(hidden);

      po::positional_options_description p;
      p.add("input-file", -1);

      po::variables_map vm;
      store(po::command_line_parser(ac, av).options(cmdline_options).positional(p).run(), vm);

      if (vm.count("config-file")) {
        Util::Input ifs(vm["config-file"].as<std::string>());
        store(parse_config_file(*ifs, config_file_options), vm);
        notify(vm);
      }

      if (vm.count("help")) {
        std::cout << generic << "\n\n";
        std::cout << "Convert FSM (in hypergraph format) to OpenFst text format" << '\n';
        return EXIT_FAILURE;
      }

      std::string file;
      if (vm.count("input-file")) {
        file = vm["input-file"].as<std::string>();
      }

      typedef ViterbiWeightTpl<float> Weight;
      typedef Hypergraph::ArcTpl<Weight> Arc;

      IVocabularyPtr pVoc = Vocabulary::createDefaultVocab();

      Util::Input in_(file);

      MutableHypergraph<Arc> hg;
      hg.setVocabulary(pVoc);
      parseText(*in_, file, &hg);

      writeOpenFstFormat(std::cout, hg);

      assert(hg.checkValid());
    } catch (std::exception& e) {
      std::cerr << e.what() << '\n';
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
};
}
}

HYPERGRAPH_NAMED_MAIN(ToOpenFstText)
