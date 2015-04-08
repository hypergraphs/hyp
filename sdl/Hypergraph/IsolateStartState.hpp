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
/** \file

     if start state has inarcs, remove them by cloning the start state.
*/

#ifndef ISOLATESTARTSTATE_JG2012104_HPP
#define ISOLATESTARTSTATE_JG2012104_HPP
#pragma once



#include <sdl/Hypergraph/Transform.hpp>

namespace sdl { namespace Hypergraph {

struct AlwaysTrue
{
  template <class Any>
  bool operator()(Any const& any) const
  {
    return true;
  }
  template <class Any1, class Any2>
  bool operator()(Any1 const& any1, Any2 const& any2) const
  {
    return true;
  }
};


template <class Arc>
bool hasInArcs(IHypergraph<Arc> const& hg, StateId head)
{
  if (hg.storesInArcs())
    return hg.numInArcs(head);
  return hg.forArcsInSearch(head, AlwaysTrue(), false);
}

struct CollectArcPointers
{
  Util::PointerSet &arcPointers;
  explicit CollectArcPointers(Util::PointerSet &arcPointers) : arcPointers(arcPointers) {}

  template <class Arc>
  bool operator()(StateId, Arc *pArc) const
  {
    arcPointers.insert((intptr_t)pArc);
    return false;
  }

};

struct IsolateStartState;
struct IsolateStartStateConfig : TransformOptionsBase
{
  template <class Arc>
  struct TransformFor
  {
    typedef IsolateStartState type;
  };

  template <class Config>
  void configure(Config const& c)
  {
    c.is("isolate start state");
    c("prevents incoming arcs to start state by adding another state and cloning arcs if needed");
  }
};

/**
   if start state has inarcs, remove them by cloning the start state. this will be faster if both in and out arcs are stored (linear time otherwise)

   calling inout(inhg, &outhg, IsolateStartState()) or inplace(inouthg, IsolateStartState()) will leave the input unmodified unless the start state is the head of some arc (has in-arcs)
*/
struct IsolateStartState : TransformBase<Transform::Inplace>
{
  typedef IsolateStartStateConfig Config;
  IsolateStartState() {}
  explicit IsolateStartState(IsolateStartStateConfig const&) {}
  template <class Arc>
  bool needs(IHypergraph<Arc> const& hg) const {
    StateId start=hg.start();
    if (start==kNoState) return false;
    return hasInArcs(hg, start);
  }
  /**
     may only called by inout/inplace if needs(m).
  */
  template <class Arc>
  void inplace(IMutableHypergraph<Arc> &hg) const {
    assert(needs(hg));

    StateId oldStart=hg.start();
    assert(oldStart!=kNoState);
    StateId newStart=hg.addState();
    hg.setStart(newStart);

    Util::PointerSet arcPointers; // collect all the outarcs from start state
    hg.forArcsOutSearch(oldStart, CollectArcPointers(arcPointers));

    for (Util::PointerSet::const_iterator i=arcPointers.begin(), e=arcPointers.end();i!=e;++i) {
      Arc *pArc=(Arc *)*i;
      hg.addArc(new Arc(*pArc, oldStart, newStart)); // clone replacing in tails: old -> new start state
    }
  }
};


}}

#endif
