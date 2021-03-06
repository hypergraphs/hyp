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

    for two hypergraphs annotated with input spans, merge the parts sharing the
    same span (giving more derivations than a simple top-level union).
*/

#ifndef HYP__HYPERGRAPH_SUBUNION_HPP
#define HYP__HYPERGRAPH_SUBUNION_HPP
#pragma once

#include <sdl/Hypergraph/IHypergraph.hpp>
#include <sdl/Hypergraph/MutableHypergraph.hpp>
#include <sdl/Hypergraph/Span.hpp>
#include <sdl/Hypergraph/Union.hpp>
#include <sdl/Util/CartesianProduct.hpp>
#include <sdl/Util/Constants.hpp>
#include <sdl/Util/LogHelper.hpp>
#include <sdl/IVocabulary.hpp>
#include <map>
#include <stack>
#include <tuple>
#include <vector>

namespace sdl {
namespace Hypergraph {

// TODO: could put everything into a class => less parameters to pass
// around

struct SubUnionOptions {
  bool requirePathOverlap;
  bool addStandardUnion;
  SubUnionOptions() : requirePathOverlap(false), addStandardUnion(true) {}
  static inline std::string usage() {
    return "Create sub-component-level union of two hypergraphs; this creates "
           "additional paths that are not contained in standard union.";
  }
  template <class Config>
  void configure(Config& config) {
    config.is("SubUnionOptions");
    config("require-path-overlap", &requirePathOverlap)("require path overlap?");
    config("add-standard-union", &addStandardUnion)("add standard union?");
  }
};

namespace SubUnionUtil {

// TODO: boost (smart) pointer vector?
typedef std::map<Span, StateIdContainer> SpanToStateIds;
typedef std::map<StateId, Span> StateIdToSpan;
typedef std::pair<bool, StateIdContainer> NewStateInfo;

/**
   Tries to get span from label name, which is, by convention,
   something like this: "2-3". Some don't have it, though.
*/
template <class Arc>
Span inferSpanFromLabel(IHypergraph<Arc> const& hg, Sym symId) {
  if (symId == NoSymbol) return kNullSpan;
  // e.g., "0-3"
  std::string const& symStr = hg.getVocabulary()->str(symId);
  // we also have strings like "0-3.b", which is also span (0,3)
  // remove the ".b" part
  std::string::size_type dotPos = symStr.find(".");
  Span r;
  if (dotPos == std::string::npos)
    r.set(symStr);
  else {
    char const* s = symStr.data();
    r.set(s, s + dotPos);
  }
  return r;
}

template <class Key, class Value>
typename std::map<Key, Value>::const_iterator getMaxValueIter(std::map<Key, Value> const& aMap) {
  typedef typename std::map<Key, Value>::const_iterator Iter;
  Iter it = aMap.begin();
  Iter best = it;
  ++it;
  for (; it != aMap.end(); ++it) {
    if (it->second > best->second) {
      best = it;
    }
  }
  return best;
}

/**
   Traverses trees post-order so that span info bubbles from
   bottom to top
*/
template <class Arc>
Span getSourceSpansBubbleUp(IHypergraph<Arc> const& hg, StateId s, StateIdToSpan* stateIdToSpan) {
  // Already done?
  StateIdToSpan::const_iterator found = stateIdToSpan->find(s);
  if (found != stateIdToSpan->end()) {
    return found->second;
  }
  if (hg.hasLexicalLabel(s)) {
    // TODO@MD: why was below commented out?
    // Span resultingSpan(inferSpanFromLabel(hg, hg.outputLabel(s)));
    return (*stateIdToSpan)[s] = kNullSpan;
  }
  SDL_DEBUG(Hypergraph.SubUnion, "getSourceSpansBubbleUp(s=" << s << ")");

  std::map<StateId, StateId> votesForLeft, votesForRight;

  // Look at the spans of children
  for (ArcId arcid : hg.inArcIds(s)) {
    Arc* arc = hg.inArc(s, arcid);
    SDL_DEBUG(Hypergraph.SubUnion, "Processing arc " << *arc);
    StateIdContainer tailIds = arc->tails();
    std::vector<Span> tailSpans;
    Span coverSpan((kNullSpan));
    for (TailId i = 0, end = tailIds.size(); i < end; ++i) {
      Span sp = getSourceSpansBubbleUp(hg, tailIds[i], stateIdToSpan);
      tailSpans.push_back(sp);
      coverSpan.growIfDefined(sp);
    }

    if (Util::defined(coverSpan.left)) ++votesForLeft[coverSpan.left];

    if (Util::defined(coverSpan.right)) ++votesForRight[coverSpan.right];
  }

  // Look at own span annotation if children are unreliable
  if (votesForLeft.size() != 1 || votesForRight.size() != 1) {
    Span sSpan = inferSpanFromLabel(hg, hg.inputLabel(s));
    ++votesForLeft[sSpan.left];
    ++votesForRight[sSpan.right];
  }

  Span resultingSpan(getMaxValueIter(votesForLeft)->first, getMaxValueIter(votesForRight)->first);

  (*stateIdToSpan)[s] = resultingSpan;
  SDL_DEBUG(Hypergraph.SubUnion, "s: " << s << " has " << resultingSpan);
  return resultingSpan;
}

/**
   Propagates span info down
*/
template <class Arc>
void getSourceSpansBubbleDown(IHypergraph<Arc> const& hg, StateId s, Span sSpan, StateIdToSpan* stateIdToSpan) {
  SDL_DEBUG(Hypergraph.SubUnion, "getSourceSpansBubbleDown(s=" << s << ")");
  for (ArcId arcid : hg.inArcIds(s)) {
    Arc* arc = hg.inArc(s, arcid);
    StateIdContainer tailIds = arc->tails();
    for (TailId i = 0, end = tailIds.size(); i < end; ++i) {
      StateIdToSpan::iterator found = stateIdToSpan->find(tailIds[i]);
      if (found == stateIdToSpan->end())
        SDL_THROW_LOG(Hypergraph.SubUnion, ProgrammerMistakeException,
                      tailIds[i] << " should have had a span stored");

      if (i == 0) Util::setUnlessDefined(found->second.left, sSpan.left);

      if (i == end - 1) Util::setUnlessDefined(found->second.right, sSpan.right);

      SDL_DEBUG(Hypergraph.SubUnion, "Assigned to " << tailIds[i] << ": " << found->second);

      getSourceSpansBubbleDown(hg, tailIds[i], found->second, stateIdToSpan);
    }
  }
}

/**
   Removes the span of a tail if it's the same as the span of
   its head.
*/
template <class Arc>
struct InvalidArcSpansRemover {

  InvalidArcSpansRemover(IHypergraph<Arc> const& hg_, StateIdToSpan* _) : hg(hg_), m(_) {
    assert(!hg.storesAllOutArcs());
  }

  void operator()(ArcBase& arc) const {
    StateIdContainer const& tails = arc.tails();
    StateIdToSpan::iterator foundHeadSpan = m->find(arc.head());
    if (foundHeadSpan == m->end()) {
      return;
    }
    for (TailId i = 0, end = tails.size(); i < end; ++i) {
      StateIdToSpan::iterator found = m->find(tails[i]);
      const bool tailIsLexical = hg.hasLexicalLabel(tails[i]);
      if (!tailIsLexical && found->second == foundHeadSpan->second) {
        m->erase(found);
      }
    }
  }
  void operator()(ArcBase* arc) const { (*this)(*arc); }

  IHypergraph<Arc> const& hg;
  StateIdToSpan* m;
};

/**
   Removes invalid spans, e.g., local cycles that will result
   from "(8-9) <- (8-9.1) (8-9.2)"
*/
template <class Arc>
void removeInvalidSpans(IHypergraph<Arc> const& hg, StateIdToSpan* stateIdToSpan) {
  InvalidArcSpansRemover<Arc> fct(hg, stateIdToSpan);
  hg.forArcs(fct);

  for (StateId s = 0, e = hg.size(); s != e; ++s) {
    StateIdToSpan::iterator found = stateIdToSpan->find(s);
    if (found != stateIdToSpan->end())
      if (isNull(found->second)) stateIdToSpan->erase(found);
  }
}

/**
   Traverses tree top-down depth-first, gets mapping from state
   source spans to state IDs.  States are often annotated with the
   span as the state symbol, but these are unreliable and sometimes
   missing.
*/
template <class Arc>
void getSourceSpans(IHypergraph<Arc> const& hg, StateIdToSpan* stateIdToSpan) {
  // Run bubble-up first b/c the hg might contain better annotations
  // for smaller (i.e., lower) spans
  Span finalSpan = getSourceSpansBubbleUp(hg, hg.final(), stateIdToSpan);
  getSourceSpansBubbleDown(hg, hg.final(), finalSpan, stateIdToSpan);
  removeInvalidSpans(hg, stateIdToSpan);
}

template <class Arc>
NewStateInfo* addStatesRecurse(IHypergraph<Arc> const& hg, StateId head, Span parentSpan,
                               StateIdToSpan const& hgStateIdToSpan, IMutableHypergraph<Arc>* result,
                               SpanToStateIds const& resultSpanToStateIds, std::set<StateIdContainer>* resultArcs,
                               std::map<StateId, NewStateInfo*>* newStateInfos, SubUnionOptions& opts) {

  IVocabulary* voc = hg.vocab();
  // Memoized result
  std::map<StateId, NewStateInfo*>::const_iterator foundResult = newStateInfos->find(head);
  if (foundResult != newStateInfos->end()) {
    return foundResult->second;
  }

  bool isUnion = false;
  const bool hasLexicalLabel = hg.hasLexicalLabel(head);

  StateIdToSpan::const_iterator foundSpan = hgStateIdToSpan.find(head);
  const bool didFindHeadSpan = foundSpan != hgStateIdToSpan.end();
  Span headSpan = didFindHeadSpan ? foundSpan->second : kNullSpan;
  if (didFindHeadSpan && !headSpan.smaller(parentSpan) && !hasLexicalLabel) headSpan = kNullSpan;

  std::vector<std::vector<StateIdContainer>> newStatesForTailsPerArc;
  for (ArcId arcid : hg.inArcIds(head)) {
    std::vector<StateIdContainer> newStatesForTails;
    bool allTailsAreUnion = true;
    Arc* arc = hg.inArc(head, arcid);
    for (StateId tailId : arc->tails()) {
      NewStateInfo* p = addStatesRecurse(hg, tailId, headSpan, hgStateIdToSpan, result, resultSpanToStateIds,
                                         resultArcs, newStateInfos, opts);
      bool isUnion = p->first;
      if (!isUnion) allTailsAreUnion = false;
      newStatesForTails.push_back(p->second);
    }
    if (allTailsAreUnion) isUnion = true;
    newStatesForTailsPerArc.push_back(newStatesForTails);
  }

  StateIdContainer newStates;
  if (didFindHeadSpan && headSpan != kNullSpan) {
    SDL_DEBUG(Hypergraph.SubUnion, head << ": found span " << foundSpan->second);
    SpanToStateIds::const_iterator foundStates = resultSpanToStateIds.find(foundSpan->second);
    if (foundStates != resultSpanToStateIds.end()) {
      StateIdContainer const& statesVec = foundStates->second;
      for (StateId s : statesVec) {
        if (result->hasLexicalLabel(s)) {
          if (result->inputLabel(s) == hg.inputLabel(head)) {
            isUnion = true;
            newStates.push_back(s);
            SDL_DEBUG(Hypergraph.SubUnion, "State " << head << ": Found corresponding state (lex) " << s);
          }
        } else if (!hasLexicalLabel) {
          SDL_DEBUG(Hypergraph.SubUnion, "State " << head << ": Found corresponding state (nonlex) " << s);
          newStates.push_back(s);
        }
      }
    }
  } else {
    NewStateInfo* info = new NewStateInfo(false, newStates);
    // return std::make_pair(false, newStates);
    (*newStateInfos)[head] = info;
    return info;
  }

  if (newStates.empty() || (opts.requirePathOverlap && !isUnion)) {
    Sym label;
    label = hasLexicalLabel ? hg.inputLabel(head) : voc->add(foundSpan->second.str(), kNonterminal);
    StateId newState = result->addState(label, label);
    newStates.clear();
    newStates.push_back(newState);
  }

  // Add all resulting arcs to the result machine
  for (StateId newHead : newStates) {
    for (std::size_t a = 0; a < newStatesForTailsPerArc.size(); ++a) {
      std::vector<StateIdContainer> newStatesForTails = newStatesForTailsPerArc[a];
      for (std::size_t i = 0, end = newStatesForTails.size(); i < end; ++i) {
        if (newStatesForTails[i].empty()) {
          continue;
        }
      }
      std::vector<StateIdContainer> cartProd;
      Util::cartesianProduct(newStatesForTails, &cartProd);
      for (std::size_t i = 0, N = cartProd.size(); i < N; ++i) {
        // avoid duplicate arcs
        StateIdContainer arcStates(cartProd[i]);
        arcStates.push_back(newHead);
        if (resultArcs->find(arcStates) == resultArcs->end()) {
          Arc* arc = new Arc(newHead, cartProd[i]);
          SDL_DEBUG(Hypergraph.SubUnion, "Adding arc " << *arc);
          result->addArc(arc);
          resultArcs->insert(arcStates);
        }
      }
    }
  }

  SDL_DEBUG(Hypergraph.SubUnion, "Result " << head << ": " << (isUnion ? "UNION" : "NO"));
  for (StateId s : newStates) {
    SDL_DEBUG(Hypergraph.SubUnion, " new state " << s);
  }

  // return std::make_pair(isUnion, newStates);
  NewStateInfo* info = new NewStateInfo(isUnion, newStates);
  (*newStateInfos)[head] = info;
  return info;
}

template <class Arc>
void addStates(IHypergraph<Arc> const& hg, StateIdToSpan const& hgStateIdToSpan, IMutableHypergraph<Arc>* result,
               SpanToStateIds const& resultSpanToStateIds, SubUnionOptions& opts) {
  std::set<StateIdContainer> resultArcs;
  for (StateId s : hg.getStateIds()) {
    for (ArcId arcid : hg.inArcIds(s)) {
      Arc* arc = hg.inArc(s, arcid);
      StateIdContainer v(arc->tails());
      v.push_back(arc->head());
      resultArcs.insert(v);
    }
  }

  // To memoize results
  std::map<StateId, NewStateInfo*> newStateInfos;

  addStatesRecurse(hg, hg.final(), kNullSpan, hgStateIdToSpan, result, resultSpanToStateIds, &resultArcs,
                   &newStateInfos, opts);

  for (std::map<StateId, NewStateInfo*>::const_iterator it = newStateInfos.begin(); it != newStateInfos.end();
       ++it) {
    delete it->second;
  }
}
}

/**
   Takes a union of 2 hypergraphs by taking union of smaller
   subnetworks. This was implemented for hyter but might be useful for
   system combination, etc., as well.
*/
template <class Arc>
void subUnion(IHypergraph<Arc> const& hg1, IHypergraph<Arc> const& hg2, IMutableHypergraph<Arc>* result,
              SubUnionOptions opts = SubUnionOptions()) {
  using namespace SubUnionUtil;
  SDL_DEBUG(Hypergraph.SubUnion, "subUnion called with");
  SDL_DEBUG(Hypergraph.SubUnion, "hg1:\n" << hg1);
  SDL_DEBUG(Hypergraph.SubUnion, "hg2:\n" << hg2);

  if (opts.requirePathOverlap) {
    SDL_DEBUG(Hypergraph.SubUnion, "Requiring path overlap!");
  }
  if (opts.addStandardUnion) {
    SDL_DEBUG(Hypergraph.SubUnion, "Add standard union!");
  }

  SDL_DEBUG(Hypergraph.SubUnion, "StateIdToSpan1");
  StateIdToSpan stateIdToSpan1;
  getSourceSpans(hg1, &stateIdToSpan1);
  SDL_DEBUG(Hypergraph.SubUnion, "Spans Hg1:");
  SpanToStateIds spanToStateIds1;
  for (StateIdToSpan::const_iterator it = stateIdToSpan1.begin(); it != stateIdToSpan1.end(); ++it) {
    spanToStateIds1[it->second].push_back(it->first);
    SDL_DEBUG(Hypergraph.SubUnion, it->first << ": " << it->second);
  }

  SDL_DEBUG(Hypergraph.SubUnion, "StateIdToSpan2");
  StateIdToSpan stateIdToSpan2;
  getSourceSpans(hg2, &stateIdToSpan2);
  for (StateIdToSpan::const_iterator it = stateIdToSpan1.begin(); it != stateIdToSpan1.end(); ++it) {
    SDL_DEBUG(Hypergraph.SubUnion, it->first << ": " << it->second);
  }

  copyHypergraph(hg1, result);
  addStates(hg2, stateIdToSpan2, result, spanToStateIds1, opts);

  // In addition, do normal union
  if (opts.addStandardUnion) {
    hgUnion(hg2, result);
  }
}


}}

#endif
