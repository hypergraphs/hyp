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

    for acyclic FSM only, push costs as far toward arcs leaving the start state
    (or arcs leaving states) as possible, without changing the structure or
    changing the weight of any derivation[1]. for weights with inverse (so not
    Feature) - e.g. log or viterbi only

    related but not yet implemented: (for HG or FSM): push costs toward final state

    note: [1] because we don't have initial or final state weights, we might have
    some constant weight that's been factored out (e.g. when using
    probabilities). by default we attempt to put this weight back on the arcs
    but if the graph isn't acyclic we may have to leave it off. you can retrieve
    the weight and insert a new state with an epsilon arc using that wieght if
    you like.

    acyclic restriction can be removed by use of viterbi best-paths algs instead
    of DAG-only inside/outside (or a more expensive algorithm, or approximate
    solution, for log weight)

    TODO: define multiplicative right-inverse to handle noncommutative semirings. for
    now this is only for viterbi and log weight
*/

#ifndef PUSHWEIGHTS_JG2013220_HPP
#define PUSHWEIGHTS_JG2013220_HPP
#pragma once

#include <sdl/Hypergraph/IHypergraph.hpp>
#include <sdl/Hypergraph/InsideAlgorithm.hpp>
#include <sdl/Hypergraph/InsideCosts.hpp>
#include <sdl/Hypergraph/OutsideCosts.hpp>
#include <sdl/Hypergraph/Transform.hpp>
#include <sdl/Hypergraph/WeightUtil.hpp>
#include <graehl/shared/nan.hpp>

namespace sdl {
namespace Hypergraph {

enum { kPushWeightsInsideAxiom = false };

struct PushWeights : SimpleTransform<PushWeights, Transform::Inplace, false> {
  template <class Arc>
  void inplace(IMutableHypergraph<Arc>& hg) const;

  Properties inAddProps() const { return kStoreInArcs | (pushToFinal ? 0 : kStoreFirstTailOutArcs); }
  static char const* type() { return "PushWeights"; }
  static char const* caption() {
    return "Modify Arc Weights (real-valued costs), optionally (in order 1-5):";
  }

  Properties inputProperties() const { return inAddProps(); }
  void validate() {}
  bool splitOnWhitespaceDefault() { return true; }

  PushWeights() : pushToFinal() {}
  bool pushToFinal;
  template <class Config>
  void configure(Config& config) {
    config.is(type());
    config(
        "for acyclic hg, push weights to final state; for acyclic graph only, push weights to start state "
        "keeping local normalization [sum(arc weight)=1] except for start state which gets the residual");
    config("push-to-final", &pushToFinal)
        .defaulted()("push weights toward final state instead of start state (also supports acyclic HG)");
  }
};


/**
   for acyclic FSM only (so far), push costs as far toward arcs leaving the start state (or arcs leaving
   states) as possible, without changing the structure

   for arcs that aren't connected through start->final, set weight to 0

   commutative semirings (w/ division) only

   usage: PushCostsToStart<Arc>(hg, PushWeightsOptions());
*/

template <class Arc>
struct PushCostsToStart {
  typedef typename Arc::Weight Weight;
  typedef IHypergraph<Arc> HG;
  HG& hg;
  typedef IMutableHypergraph<Arc> MHG;
  MHG* mhg;
  StateId N, start, final;
  Util::AutoDeleteArray<SdlFloat> inside, outside;
  SdlFloat* outside0;
  PushCostsToStart(HG& hg, PushWeights const& = PushWeights())
      : hg(hg)
      , mhg(hg.isMutable() ? static_cast<MHG*>(&hg) : 0)
      , N(hg.sizeForHeads())
      , start(hg.start())
      , final(hg.final())
      , inside(N)
      , outside(N, HUGE_VAL) {
    if (hg.prunedEmpty()) return;
    if (!hg.isGraph() || start == kNoState)
      SDL_THROW_LOG(Hypergraph.PushCostsToStart, ConfigException,
                    "PushWeights=>start requires an acyclic graph");
    hg.forceFirstTailOutArcs();
    if (!N) goto empty;
    insideCosts(hg, &inside[0], N);
    if (graehl::is_inf(inside[final])) {
    empty:
      if (mhg)
        mhg->setEmpty();
      else
        SDL_THROW_LOG(Hypergraph.PushCostsToStart, ConfigException,
                      "non-mutable hg had no finite-cost paths - can't PushCostsToStart");
      return;
    }
    outside0 = outside;
    assert(graehl::is_posinf(*outside0));
    outsideCosts(hg, outside0, inside, N, (float)HUGE_VAL, false);
    assert(start < N);
    outside0[start] = inside[final];
    assert(outside0[final] == 0);
    hg.forArcs(*this);
  }

  void operator()(Arc* pArc) const {
    Arc& arc = *pArc;
    StateId head = arc.head_;
    StateId tail = arc.tails_[0];
    SdlFloat& cost = arc.weight_.value_;
    assert(head < N);
    assert(tail < N);
    // we want the new outsides to be as small as possible without going
    // negative. TODO: prove correct for cyclic graphs
    cost += outside0[head] - (tail != start ? outside0[tail] : 0);
  }
};

/**
   as with PushCostsToStart, but works on acyclic hg, moving inside costs up
   toward final (toward heads). commutative semirings (w/ division) only

   usage: PushWeightsToFinal<Arc>(hg, PushWeights());

*/
template <class Arc>
struct PushWeightsToFinal {
  typedef typename Arc::Weight Weight;
  typedef IHypergraph<Arc> HG;
  typedef boost::ptr_vector<Weight> PtrWeights;
  HG& hg;
  typedef IMutableHypergraph<Arc> MHG;
  MHG* mhg;
  StateId N, final;
  Weight const kZero;
  PtrWeights inside;
  PushWeightsToFinal(HG& hg, PushWeights const& = PushWeights())
      : hg(hg)
      , mhg(hg.isMutable() ? static_cast<MHG*>(&hg) : 0)
      , N(hg.sizeForHeads())
      , final(hg.final())
      , kZero(Weight::zero())
      , inside(N) {
    insideAlgorithm(hg, &inside, kPushWeightsInsideAxiom);
    if (isZero(inside[final])) {
      if (mhg)
        mhg->setEmpty();
      else
        SDL_THROW_LOG(Hypergraph.PushCostsToStart, ConfigException,
                      "non-mutable hg had no finite-cost paths - can't PushCostsToStart");
    }
    hg.forArcs(*this);
  }
  /**
     telescoping: for best hyperpath F(C, D) wf, C(A, B) wc, leaf arcs A wa, B wb, D wd, we have:

     for the best arcs into each state, the relationship between inside and arc weight is simple:

     wf = inside[F]/(inside[C]*inside[D])
     wc = inside[C]/(inside[A]*inside[B])
     wa = inside[A]
     wb = inside[B]
     wd = inside[D]

     but for non-best arcs, we need something different, based on the previous derivation of head:

     arc.w' = arc.w*prod {inside[tails]}/inside[head] (except don't divide for head=final,
     since we have no final weights)

     this does the same thing for best arcs as above. it can be proven correct
     by a telescoping argument (every state occurs an equal number of times as a
     head and a tail in the derivation, except for the leaves, which have inside
     of one, by definition.


     remember that inside[F]=wf*(inside[C]*inside[D]), inside[A]=wa, etc.

     so the weight for any path is the same (commutative semiring w/ division, e.g. log or viterbi)

     that is, for any arc, the new weight is inside[head] / prod(inside[tails]))
  */
  void operator()(ArcBase* pArc) const {
    Arc& arc = *(Arc*)pArc;
    StateId head = arc.head_;
    Weight& weight = arc.weight();
    StateId ninside = inside.size();
    assert(ninside <= N);
    if (head < ninside) {
      if (head != final) {
        Weight const& divhead = inside[head];
        if (isZero(divhead)) {  // avoid zero division
          setZero(weight);
          return;
        }
        divideBy(divhead, weight);
      }
      StateIdContainer const& tails = arc.tails();
      for (typename StateIdContainer::const_iterator i = tails.begin(), e = tails.end(); i != e; ++i) {
        StateId const tail = *i;
        if (!hg.isAxiom(tail)) timesBy(inside[tail], weight);
      }
    } else
      weight = kZero;
  }
};

template <class Arc>
void pushWeightsToStart(IHypergraph<Arc>& hg, PushWeights const& config = PushWeights()) {
  PushCostsToStart<Arc> push(hg, config);
}

template <class Arc>
void PushWeights::inplace(IMutableHypergraph<Arc>& hg) const {
  if (pushToFinal)
    PushWeightsToFinal<Arc>(hg, *this);
  else
    PushCostsToStart<Arc> push(hg, *this);
}


}}

#endif
