// Copyright 2014 SDL plc
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
#define HG_TRANSFORM_MAIN
#include <sdl/Hypergraph/TransformMain.hpp>
#include <sdl/Hypergraph/OpenFstDraw.hpp>
#include <sdl/Hypergraph/HypergraphWriter.hpp>

#include <sdl/IVocabulary.hpp>
#include <sdl/Vocabulary/HelperFunctions.hpp>

#include <sdl/SharedPtr.hpp>
#include <sdl/Util/Forall.hpp>

namespace sdl {
namespace Hypergraph {
#if HAVE_OPENFST
# define USAGEFST " (using openfst draw if fsm)"
#else
# define USAGEFST ""
#endif

#define USAGE "Print graphviz (dot) equivalent of hypergraph unless --out=-0 " USAGEFST

#define VERSION "v1"

struct HypFsmDraw : TransformMain<HypFsmDraw> {
  DrawOptions dopt;
  typedef TransformMain<HypFsmDraw> Base;
  HypFsmDraw() : Base("HypFsmDraw", USAGE, VERSION) {
    opt.allow_ins();
  }
  void declare_configurable() {
    this->configurable(&dopt);
  }

  Properties properties(int i) const {
    return kStoreInArcs;
  }
  bool printFinal() const { return false; }
  enum { has_transform1 = false, has_transform2 = false, has_inplace_input_transform = true };
  template <class Arc>
  bool inputTransformInPlace(IHypergraph<Arc> const& i, unsigned) {
    dopt.o = out_file.is_none()?0:out_file.get();
    dopt.draw(i);
    if (out_file)
      out_file.stream() << '\n';
    return true;
  }
  void validate_parameters_more()
  {
    dopt.validate();
  }
};

}}

INT_MAIN(sdl::Hypergraph::HypFsmDraw)
