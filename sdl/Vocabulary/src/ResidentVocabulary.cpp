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
#include <fstream>
#include <boost/algorithm/string.hpp>

#include <sdl/Util/Split.hpp>
#include <sdl/Util/LineOptions.hpp>
#include <sdl/Util/ObjectCount.hpp>
#include <sdl/Vocabulary/ResidentVocabulary.hpp>
#include <sdl/Vocabulary/HelperFunctions.hpp>

namespace sdl {
namespace Vocabulary {

void ResidentVocabulary::addLeakChecks(Util::ILeakChecks& leaks) {
  leaks.add(new VocabularyUnkLeakCheck(*this));
  leaks.add(new VocabularyResidentLeakCheck(*this));
}

void ResidentVocabulary::loadTerminals(std::string const& terminalPath) {
  // TODO: test
  std::string entry;
  std::vector<std::string> strings;

  SymbolType kType = sdl::kTerminal;

  Util::Input in(terminalPath);
  while (Util::nfcline(in, entry)) {
    Util::splitSpaces(strings, entry);
    (void)getVocab(kType).add(strings[1], kType);
  }
}

void ResidentVocabulary::loadNonterminals(std::string const& nonTerminalPath) {
  // TODO: test
  std::string entry;
  std::vector<std::string> strings;

  SymbolType kType = sdl::kNonterminal;

  Util::Input inSrcFile(nonTerminalPath);
  while (Util::nfcline(inSrcFile, entry)) {
    Util::splitSpaces(strings, entry);
    (void)getVocab(kType).add(strings[1], kType);
  }
}

void ResidentVocabulary::initStarts(unsigned startingTerminal, unsigned startingNonterminal,
                                    unsigned startingViable) {
  unsigned offsetTerminal = startingTerminal;
  unsigned offsetNonterminal = startingNonterminal;

  vocabTerminal.init(kTerminal, offsetTerminal);
  vocabNonterminal.init(kNonterminal, offsetNonterminal);
  vocabVariable.init(kVariable, startingViable);
}

std::string const& ResidentVocabulary::_Str(Sym const symId) const {
  return getVocab(symId.type()).str(symId);
}

bool ResidentVocabulary::_containsSym(Sym symId) const {
  return getVocab(symId.type()).containsSym(symId);
}

bool ResidentVocabulary::_boundsSym(Sym symId) const {
  return getVocab(symId.type()).boundsSym(symId);
}

unsigned ResidentVocabulary::_GetNumSymbols(SymbolType symType) const {
  return getVocab(symType).getNumSymbols();
}

std::size_t ResidentVocabulary::_GetSize() const {
  return vocabTerminal.getSize() + vocabNonterminal.getSize() + vocabVariable.getSize();
}

void ResidentVocabulary::_Accept(IVocabularyVisitor& visitor) {
  vocabTerminal.accept(visitor);
  vocabNonterminal.accept(visitor);
  vocabVariable.accept(visitor);
}

void ResidentVocabulary::_AcceptType(IVocabularyVisitor& visitor, SymbolType type) {
  getVocab(type).accept(visitor);
}


}}
