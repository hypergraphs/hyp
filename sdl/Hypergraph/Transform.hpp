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

    boilerplate for 'maybe hypergraph needs to be copied then in-place
    transformed, maybe the hypergraph needs these properties to be in-place
    transformed, or maybe the hypergraph already satisfies the postcondition of
    the transform'

    if you call transform.inplace(h) or .inout(h, &out) directly the
    work-avoiding logic (needs() etc) is skipped, but calling free fns as in the
    following example will intelligently avoid some work:

    usage:

    // given a transform, e.g.
    LmRescore addlm;
    addlm.reset("LWLM","file.lwlm", 3);

    // and input inhg
    MutableHypergraph<A> inhg, outhg;
    shared_ptr<IHypergraph<A> > rescored1, rescored2;

    // then, one of:
    rescored1 = transformed(inhg, addlm);
    rescored2 = clone(inhg);
    inplace(rescored2, addlm);
    inout(inhg, &outhg, addlm);
    inplace(inhg, addlm); // will silently create a temporary copy of the input hg
    // (out of necessity; LmRescore doesn't support in-place hg tranformation)

    a Transform e.g. LmRescore:

    1. may define properties that must be on or off in the input hg and the output hg.

    2. may define a check on the input which may skip the transform (e.g. the desired result is already the
    case in the input). no matter what, the required output properties are present/absent in the output.
    further, the output gets the vocab of the input, even if the transform wasn't needed

    3. may implement either an in-place modification of input, or output to a new hg, or both (with a
    preference for one or the other). the transform elects its preferred/possible methods by setting Inplace
   or
    OptionalInplace bool members

    4. may be used via inplace(hg, transform) or inout(inhg, &outhg, transform), no matter what. for inplace,
    hg may be a ref to mutable hg or a ref to shared_ptr to hg. the shared_ptr is inspected to see whether hg
    itself is mutable, for a possible actual in-place update.

    5. if you define 'typedef void IsSimpleTransform;' then xmt modules won't do
    need locking/copying/etc before action (easiest to inherit from
    SimpleTransform<...> in this case

    note: OptionalInplace should be true only if Inplace is true, i.e. it means
    prefer inout except when input->output is a mutable hg. OptionalInplace is
    irrelevant if !Inplace (Transform::Inout)

*/

// FIXME: kFsm is getting cleared e.g. HgConcat fsm*fsm*fsm - thinks (fsm*fsm) -> cfg. not sure why. not
// setting the kFsm property when copying? but why copying?

/*

  idea:
  an alg may require some property of HG.

  for the special case of unary transforms (1 in 1 out), maybe we, the caller,
  want to forget (destroy) the original input, or maybe the transform is nondestructive,
  but adds an index. in that case, we want to update in place. but some HG
  aren't mutable. so hold by smart_ptr and maybe update the ptr if mutation isn't supported.

  TODO: some expensive transforms might be important enough that you'd like to
  cache whether needs() in hg.properties(). This isn't done very cleanly yet;
  there's special code for each such case.

  some algs are more naturally expressed by mutating the input HG. others
  require copying. our purpose in Transform.hpp is to support both ways of
  calling, when just one is implemented

  pass HGs by smart_ptr which may be updated to a new HG (copy of IHG) as
  needed. or if the pointed-to-HG was mutable (and pointer was non-const), make
  the required change. also, second version which always makes a copy.

  when we have more than one type of HG impl, (right now it's just Mutable
  underlying), will need a factory object to make the suggested type of
  replacement HG if the smart_ptr must be updated. can also allocate for you in the always copy version

*/

/* integrate w/ TransformMain ? let's handle only unary transforms here, and make it easy to adapt one to a
   TransformMain:

   struct T : TransformBase;

   struct TM : TransformBase, TransformMainBase<TM> // bad: would have to restate overriding TransformBase
   methods in TM-conflicts from two superclasses - fix: could do impl().transform().x instead of impl().x or
   in TM typedef TransformBase Transform;

   struct TM : TransformBase, TransformMainBase<TransformBase> // does static_cast in TMB succeeed?
   struct TM : TransformBase, TransformMainBase<TM, TransformBase> // two static_cast-would definitely work

*/

#ifndef HYP__HG_TRANSFORM_HPP
#define HYP__HG_TRANSFORM_HPP
#pragma once

#include <sdl/Hypergraph/FinalOutputState.hpp>
#include <sdl/Hypergraph/ForceArcs.hpp>
#include <sdl/Hypergraph/HypergraphCopyBasic.hpp>
#include <sdl/Hypergraph/MutableHypergraph.hpp>
#include <sdl/Hypergraph/Transform-fwd.hpp>
#include <sdl/Util/LogHelper.hpp>
#include <sdl/SharedPtr.hpp>
#include <boost/pointer_cast.hpp>
#include <boost/thread/tss.hpp>

namespace sdl {

namespace {
std::string const outputVocabularyUsage(
    "(OPTIONAL) Vocabulary resource name for output hg vocab. In most cases the input hg's vocab should be "
    "used");
}

namespace Hypergraph {

struct PrepareArcType {
  /**
     (for xmt/CopyingHypergraphTransform.hpp)

     override this if you have input-arc type initialization in xmt pipeline
     context (via TransformAsModule) that needs to precede any actual input
     processing, e.g. persistent vocab symbols added before non-persistent. of
     course, you can do this in loadResources if you have a unique transform
     type for each arc type
  */
  template <class ArcType>
  void prepareArcType(ArcType*) const {}

  template <class ResourceManager, class ArcType>
  void prepareArcTypeThread(ResourceManager&, ArcType*) const {}
};

template <class Arc>
MutableHypergraph<Arc>* newEmptySameVocabulary(IHypergraph<Arc> const& hg) {
  MutableHypergraph<Arc>* r = new MutableHypergraph<Arc>;
  r->setVocabulary(hg.getVocabulary());
  return r;
}

template <class Arc>
MutableHypergraph<Arc>* newEmptyHg(IVocabularyPtr const& voc) {
  MutableHypergraph<Arc>* r = new MutableHypergraph<Arc>;
  r->setVocabulary(voc);
  return r;
}

template <class Arc>
bool emptyInToOut(IHypergraph<Arc> const& hg, IMutableHypergraph<Arc>* presult_hg) {
  presult_hg->setVocabulary(hg.getVocabulary());
  if (hg.prunedEmpty()) {
    presult_hg->setEmpty();
    return true;
  } else
    return false;
}

template <class Arc>
bool emptyInToOut(IHypergraph<Arc> const& hg, shared_ptr<IMutableHypergraph<Arc>>& presult_hg) {
  if (hg.prunedEmpty()) {
    presult_hg.reset(newEmptySameVocabulary(hg));
    return true;
  } else
    return false;
}

template <class Arc, class OutputArc>
bool emptyInToOut(IHypergraph<Arc> const& hg, shared_ptr<IHypergraph<OutputArc>>& presult_hg) {
  if (hg.prunedEmpty()) {
    presult_hg.reset(newEmptyHg<OutputArc>(hg.getVocabulary()));
    return true;
  } else
    return false;
}

// transform Transform & is mutable in case you want needs() to do some computation which is saved (that will
// speed up the transform if it's needed)

static const Properties maybeClearProps = 0;  // kAllProperties

// we seem to be forcing props repeatedly, but at least one (sort) may need re-forcing after adding arcs.

/**
   throw ConfigException if input hg fails transform.checkInputs(hg).
*/
template <class Transform, class Arc>
void checkInputs(IHypergraph<Arc> const& hg, Transform const& t) {
  if (!t.checkInputs(hg))
    THROW_LOG_NAMESTR("sdl." + t.type(), ConfigException,
                      "input hypergraph " << t.checkInputsHelp() << " for transform " << t.type());
}


/// no matter whether transform supports inplace natively, we update m <- transform(m). does not check
/// t.needs(m)
template <class Transform, class A>
void inplace_always(IMutableHypergraph<A>& m, Transform const& t) {
  Properties cpProp = t.inAddProps() | t.newOutAddProps();
  if (t.Inplace) {
    bool needsc = t.needsCopy(m);
    if (needsc && !t.OptionalInplace)
      SDL_THROW2(SelfModifyException, "unimplemented: needsCopy(hg) for inplace transform(hg)", t.type());
    if (!needsc) {
      m.forceProperties(t.inAddProps());
      t.inplace(m);
      m.forcePropertiesOnOff(t.outAddProps(), t.outSubProps());
      return;
    }
  }
  Properties newInplaceProps = cpProp & ~t.outSubProps();
  MutableHypergraph<A> i(newInplaceProps);
  copyHypergraph(m, &i, kNoClear);
  t.inout(i, &m);
  m.forcePropertiesOnOff(t.outAddProps(), t.outSubProps());
}

/// see inplace_always. properties are forced whether or not the transform was needed.
template <class Transform, class A>
bool inplace(IMutableHypergraph<A>& m, Transform const& t) {
  if (t.needs(m)) {
    inplace_always(m, t);
    return true;
  } else {
    m.forcePropertiesOnOff(t.outAddProps(), t.outSubProps());
    return false;
  }
}

template <class Transform, class A>
bool inplace(IMutableHypergraph<A>& m) {
  Transform t;
  return inplace(m, t);
}

namespace {
/// without checking t.needs(i), copy if needed for required input props, then o <- t(i)
template <class Transform, class A>
void inout_impl(IHypergraph<A> const& i, IMutableHypergraph<A>* o, Transform const& t) {
  o->setVocabulary(t.getVocab(i.getVocabulary()));
  shared_ptr<IHypergraph<A> const> pi = ensureProperties(i, t.inAddProps(), maybeClearProps, 0);
  o->forcePropertiesOnOff(t.outAddProps(), t.outSubProps());
  t.inout(*pi, o);
  o->forcePropertiesOnOff(t.outAddProps(), t.outSubProps());
}
}

template <class Transform, class A>
void inout_by_inplace(IHypergraph<A> const& i, IMutableHypergraph<A>* o, Transform const& t) {
  assert(t.Inplace);
  copyEnsuringProperties(i, o, t.inAddProps());  // optional, but may be more efficient than forceprops later.
  inplace_always(*o, t);
}


/**
   precondition: t.needs(i) (or you don't care that the transformation gets performed needlessly)
*/
template <class Transform, class A>
void inoutNeeds(IHypergraph<A> const& i, IMutableHypergraph<A>* o, Transform const& t) {
  if (t.Inplace && !t.OptionalInplace) {
    inout_by_inplace(i, o, t);
  } else {
    inout_impl(i, o, t);
  }
}

template <class Transform, class A>
void inout(IHypergraph<A> const& i, IMutableHypergraph<A>* o, Transform const& t) {
  if (!t.needs(i)) {
    copyEnsuringProperties(i, o, t.outAddProps(), t.outSubProps());  // sets vocab
    return;
  }
  inoutNeeds(i, o, t);
}


template <class A>
void clone(shared_ptr<IHypergraph<A>>& phg) {
  phg.reset(new MutableHypergraph<A>(*phg));
}

template <class A>
shared_ptr<IHypergraph<A>> clone(IHypergraph<A> const& inhg) {
  return shared_ptr<IHypergraph<A>>(new MutableHypergraph<A>(inhg));
}

template <class A>
bool copyIfSame(shared_ptr<IHypergraph<A>>& pl, IHypergraph<A> const& r) {
  if (pl.get() == &r) {
    clone(pl);
    return true;
  }
  return false;
}

/// shared_ptr to hg is updated with result t(hg) - this is a copy if needed, but if
template <class Transform, class A>
bool inplace(shared_ptr<IHypergraph<A> const>& cpi, Transform const& t) {
  shared_ptr<IHypergraph<A> const> holdi = cpi;
  IHypergraph<A> const& i = *holdi;
  if (!t.needs(i)) {
    cpi = ensureProperties(i, t.outAddProps(), 0, t.outSubProps());
    return false;
  }
  MutableHypergraph<A>* o = new MutableHypergraph<A>(t.newOutAddProps());
  cpi.reset(o);
  if (t.Inplace && !t.OptionalInplace) {
    copyEnsuringProperties(
        i, o, t.inAddProps(),
        maybeClearProps);  // could add out props too, but may be cheaper after inplace xform
    inplace_always(*o, t);
  } else {
    shared_ptr<IHypergraph<A> const> i2 = ensureProperties(i, t.inAddProps(), maybeClearProps);
    inout_impl(*i2, o, t);
  }
  return true;
}

// note: non-const pointer may still not actually be mutable - will check isMutable()
template <class Transform, class A>
bool inplace(shared_ptr<IMutableHypergraph<A>>& pi, Transform const& t) {
  return inplace(*pi, t);
}

// return false iff !t.needs(*pi), modify pi
template <class Transform, class A>
bool inplace(shared_ptr<IHypergraph<A>>& pi, Transform const& t) {
  typedef IHypergraph<A> Hg;
  if (pi->isMutable()) {
    typedef IMutableHypergraph<A> MHg;
    shared_ptr<MHg> pm(static_pointer_cast<MHg>(pi));
    if (inplace(pm, t)) {
      pi = static_pointer_cast<Hg>(pm);
      return true;
    } else
      return false;
  }
  shared_ptr<Hg const> cpi = pi;
  inplace(cpi, t);
  pi = const_pointer_cast<Hg>(cpi);
  return false;
}

template <class Transform, class A>
bool maybeInplace(IHypergraph<A>& i, Transform const& t) {
  if (i.isMutable()) {
    inplace(static_cast<IMutableHypergraph<A>*>(&i));
    return true;
  }
  return false;
}

template <class Transform, class A>
void tryInplace(IHypergraph<A>& i, Transform const& t) {
  if (!maybeInplace(i, t))
    SDL_THROW_LOG(Hypergraph.Transform, ImmutableHypergraphException,
                  "input hypergraph is not mutable attempting in-place " << t.type() << " transform.");
}

template <class Transform, class A>
MutableHypergraph<A>* transformedNew(IHypergraph<A> const& hg, Transform const& t,
                                     IVocabularyPtr pVoc = IVocabularyPtr()) {
  Properties cpProp = t.inAddProps() | t.newOutAddProps();
  typedef MutableHypergraph<A> MH;
  MH* m = new MH(cpProp);
  m->setVocabulary(pVoc ? pVoc : t.getVocab(hg.getVocabulary()));
  inout(hg, m, t);
  return m;
}

template <class Transform, class A>
MutableHypergraph<A>* transformedNewNeeds(IHypergraph<A> const& hg, Transform const& t,
                                          IVocabularyPtr pVoc = IVocabularyPtr()) {
  Properties cpProp = t.inAddProps() | t.newOutAddProps();
  typedef MutableHypergraph<A> MH;
  MH* m = new MH(cpProp);
  m->setVocabulary(pVoc ? pVoc : t.getVocab(hg.getVocabulary()));
  inoutNeeds(hg, m, t);
  return m;
}

/**
   \return hg transformed by t. always returns a new hg even if !t.needs(hg).
*/
template <class Transform, class A>
shared_ptr<IHypergraph<A>> transformedCopy(IHypergraph<A> const& hg, Transform const& t,
                                           IVocabularyPtr pVoc = IVocabularyPtr()) {
  return shared_ptr<IHypergraph<A>>(transformedNew(hg, t, pVoc));
}

/**
   \return hg transformed by t. may return reference to hg (so must not use past hg's validity) if
   !t.needs(hg)
*/
template <class Transform, class A>
shared_ptr<IHypergraph<A> const> transformed(IHypergraph<A> const& hg, Transform const& t,
                                             IVocabularyPtr pVoc = IVocabularyPtr()) {
  if (!t.needs(hg)) return ptrNoDelete(hg);
  return shared_ptr<IHypergraph<A> const>(transformedNewNeeds(hg, t, pVoc));
}

/**
   \return hg transformed by t. may return reference to hg (so must not use past hg's validity) if
   !t.needs(hg)
*/
template <class Transform, class A>
shared_ptr<IHypergraph<A>> transformedMaybeInplace(IHypergraph<A>& hg, Transform const& t,
                                                   IVocabularyPtr pVoc = IVocabularyPtr()) {
  if (!t.needs(hg)) return ptrNoDelete(hg);
  if (t.Inplace && hg.isMutable()) {
    t.inplace(static_cast<IMutableHypergraph<A>&>(hg));
    return ptrNoDelete(hg);
  }
  return shared_ptr<IHypergraph<A>>(transformedNewNeeds(hg, t, pVoc));
}

Properties const NewOutAddProps = kStoreInArcs;
Properties const OutSubProps = Transform::NoProperties;
Properties const OutAddProps = Transform::NoProperties;

// since we use templates, you don't actually need to inherit from this. but provide these members.
template <bool Inplace_ = Transform::Inout, Properties InAddProps = Transform::NoProperties>
struct TransformBase : PrepareArcType {
  TransformBase() { defaultVocab = "input-vocab"; }

  template <class A>
  bool checkInputs(IHypergraph<A> const& h) const {
    return true;
  }

  /// input hypergraph [checkInputsHelp] for stransform [name].
  std::string checkInputsHelp() const { return "is unsuitable"; }

  // NOTE: if you say h doesn't need the transform, the properties InAddProps are still ensured. we may want
  // an option for that :)
  template <class A>
  bool needs(IHypergraph<A> const& h) const {
    return true;
  }
  template <class A>
  bool needsCopy(IHypergraph<A> const& h) const {
    return false;  // e.g. if &h=&rhs for binary transform
  }

  // named differently so we don't hide when we override same name
  template <class A>
  void inplace(IMutableHypergraph<A>& m) const {
    SDL_THROW_LOG(Hypergraph, InvalidInputException, "unimplemented in-place transform");
  }
  template <class A>
  void inout(IHypergraph<A> const& h, IMutableHypergraph<A>* o) const {
    SDL_THROW_LOG(Hypergraph, InvalidInputException, "unimplemented in->out transform");
  }

  enum { Inplace = Inplace_, OptionalInplace = false };
  Properties inAddProps() const { return InAddProps; }
  Properties outAddProps() const { return OutAddProps; }
  Properties newOutAddProps() const { return NewOutAddProps | OutAddProps; }
  Properties outSubProps() const { return OutSubProps; }
  // flag for Transform to force using this instead of inout input hg vocab?

  void setVocabulary(IVocabularyPtr const& pvoc) const { pVoc.get() = pvoc; }

  /**
     TODO: pull RescourceManager out of xmt, instead of this template

     make sure you call TransformBase<...>::loadResources from your subclass loadResources
  */
  template <class ResourceManager>
  void loadResourcesThread(ResourceManager& mgr) const {
    if (!defaultVocab.empty()) {
      IVocabularyPtr& voc = pVoc.get();
      if (!voc) {
        SDL_DEBUG(Hypergraph.Transform.loadResourcesThread, "using resource 'vocabulary: " << defaultVocab << "'");
        mgr.maybeGetResource(defaultVocab, voc);
        if (!voc) {
          SDL_WARN(Hypergraph.Transform.loadResourcesThread,
                   "couldn't find resource 'vocabulary: " << defaultVocab << "'");
        }
      }
    }
  }

  mutable boost::thread_specific_ptr<FinalOutputState> applyFinalOutput;

  template <class ResourceManager>
  void loadResources(ResourceManager& mgr) {
    initProcess();
  }

  virtual void initProcess() {}

  template <class ResourceManager>
  void baseLoadResourcesThread(ResourceManager& mgr) const {
    loadResourcesThread(mgr);
  }

  template <class HG>
  IVocabularyPtr inputVocabMatches(HG const& hg) const {
    IVocabularyPtr const& hgVoc = hg.getVocabulary();
    IVocabularyPtr const& trVoc = pVoc.get();
    SDL_DEBUG(Transform, "transform input hg vocabulary @ " << hgVoc.get() << " should match vocabulary @"
                                                            << trVoc.get());
    if (trVoc && trVoc != hgVoc)
      SDL_THROW_LOG(Hypergraph.Transform, ConfigException,
                    "vocabulary resource '" << defaultVocab
                                            << "' didn't match input hypergraph's vocabulary");
    return hgVoc;
  }

  IVocabularyPtr const& getVocab() const { return pVoc; }

  void resetCaches() const {}

  /**
     if set before loadResources, pVoc will get vocab resource mgr[name].
  */
  void setDefaultVocabName(std::string const& name) { defaultVocab = name; }

  // so the word vocab configured by resource manager gets used in preference to the input hg's (warning:
  // don't assume ids are the same for same string, then - e.g. capitalize convert chars to tokens which
  // create new symbols
  IVocabularyPtr const& getVocab(IVocabularyPtr const& defaultVoc) const {
    IVocabularyPtr& voc = pVoc.get();
    if (!voc) voc = defaultVoc;
    assert(voc);
    return voc;
  }

  IVocabulary& vocab() const { return *getVocab(); }

 protected:
  std::string defaultVocab;  // comes from xmt/TransformAsModule config_.vocabulary
  mutable IVocabularyPtrPerThread pVoc;  // TODO: once we have per-process vocabulary, remove ThreadSpecific

 private:
  void operator=(TransformBase const&);
  TransformBase(TransformBase const& o);
};

// optional base class.
struct TransformOptionsBase : ForceArcs {
  Hypergraph::Properties inputProperties() const {
    Hypergraph::Properties p = ForceArcs::arcProperties();
    return p ? p
             : Hypergraph::kStoreInArcs;  // you might want kStoreFirstTailOutArcs for an fst-based transform
  }

  template <class Config>
  void configure(Config& config) {
    ForceArcs::configure(config);
  }

  /// e.g. stat tok wants chars splitting for its strings, but most modules would prefer space
  bool splitOnWhitespaceDefault() const { return true; }

  /// should module config have 'vocabulary' resource string cfg (true), or do we get
  /// the vocab through other means (e.g. associated resource) (false)?
  bool configureVocabulary() const { return true; }

  /// heading for options/help
  static char const* caption() { return "Options"; }

  void validate() {}
};

template <class Arc, class TransformOptions>
TransformHolder transformFor(TransformOptions const& opt) {
  typedef TransformFor<TransformOptions, Arc> TFor;
  if (TFor::Simple)
    return TransformHolder((void*)TFor::getSimple(opt), DoNothing());
  else
    return TFor::getComplex(opt);
}

template <class Arc, class TransformOptions>
typename TransformFor<TransformOptions, Arc>::type const& useTransform(TransformHolder const& transformHolder) {
  return *static_cast<typename TransformFor<TransformOptions, Arc>::type*>(transformHolder.get());
}

template <class Arc, class TransformOptions>
void inplaceFromOptions(TransformOptions const& opt, IMutableHypergraph<Arc>& hg) {
  typedef typename TransformFor<TransformOptions, Arc>::type Transform;
  Transform t(opt);
  inplace(hg, t);
}

template <class Options>
struct InplaceForOptions {
  template <class Arc>
  void inplace(Options const& opt, IMutableHypergraph<Arc>& hg) const {
    opt.inplace(hg);
  }
};

template <class Arc, class Options>
void inplaceTransform(Options const& opt, IHypergraph<Arc>& hg) {
  if (!hg.isMutable())
    SDL_THROW_LOG(Hypergraph, ProgrammerMistakeException,
                  "don't call in-place modules on non-mutable hypergraphs");
  InplaceForOptions<Options>().inplace(opt, static_cast<IMutableHypergraph<Arc>&>(hg));
}


template <class Arc, class TransformOptions>
void inoutFromOptions(TransformOptions const& opt, IHypergraph<Arc> const& i, IMutableHypergraph<Arc>* o) {
  typedef typename TransformFor<TransformOptions, Arc>::type Transform;
  Transform t(opt);
  inout(i, o, t);
}

/**
   convenience: declare a config-less transform suitable for xmt/TransformAsModule.
*/
template <class CRTP, bool InplaceOrInout, bool HasOptionalInplace = true>
struct SimpleTransform : TransformBase<InplaceOrInout> {
  enum { OptionalInplace = HasOptionalInplace };
  template <class Arc>
  struct TransformFor {
    typedef CRTP type;
  };
  typedef void IsSimpleTransform;
};

template <class Options>
struct InplaceTransform : TransformBase<Transform::Inplace> {
  static char const* type() { return Options::type(); }
  InplaceTransform(Options const& opt) : opt(opt) {}
  Options const& opt;
  template <class Arc>
  void inplace(IMutableHypergraph<Arc>& m) const {
    opt.inplace(m);
  }
};

template <class Options>
struct OptionalInplaceTransform : TransformBase<Transform::Inplace> {
  enum { OptionalInplace = true };
  static char const* type() { return Options::type(); }
  OptionalInplaceTransform(Options const& opt) : opt(opt) {}
  Options const& opt;
  template <class Arc>
  void inplace(IMutableHypergraph<Arc>& m) const {
    opt.inplace(m);
  }
  template <class A>
  void inout(IHypergraph<A> const& h, IMutableHypergraph<A>* o) const {
    opt.inout(h, o);
  }
};
}


}

#endif
