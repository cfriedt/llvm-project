// Copyright (c) 2021 Friedt Professional Engineering Services, Inc
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <stdio.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "clang/Basic/TargetInfo.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/Support/Base64.h"
#include "llvm/Support/SHA256.h"

using namespace clang;
using namespace llvm;

namespace std {
template <> struct hash<StringRef> {
  std::size_t operator()(const StringRef &k) const { return hash_value(k); }
};
} // namespace std

std::mutex HashMutex;
// Provide storage for Hash strings
static std::unordered_set<std::string> HashStorage;
// Cache known hashes (i.e. "Section" -> "Hash")
static std::unordered_map<StringRef, StringRef> HashForward;

// the algorithm used for hashing long section names
static std::string hashit(const StringRef &Name, unsigned N) {
  SHA256 Hasher;
  Hasher.update(Name);
  return encodeBase64(Hasher.result()).substr(0, N);
}

static void writeHashedSectionNameToFile(const std::string &filename,
                                         const StringRef &Section,
                                         const StringRef &Hash,
                                         bool IsWindows) {
  FILE *fp;

  fp = fopen(filename.c_str(), "a");
  if (NULL == fp) {
    return;
  }

  fprintf(fp, "%s\t%s%s", Section.str().c_str(), Hash.str().c_str(),
          IsWindows ? "\r\n" : "\n");
  fclose(fp);
}

static StringRef hashSectionName(const StringRef &Section, unsigned N,
                                     const std::string &OutputFile,
                                     bool IsWindows) {
  StringRef Hash;

  assert(N > 0);

  auto it = HashForward.find(Section);
  if (HashForward.end() != it) {
    Hash = it->second;
    return Hash;
  }

  std::string hash = hashit(Section, N);
  HashStorage.insert(hash);
  Hash = StringRef(*HashStorage.find(hash));
  HashForward[Section] = Hash;

  writeHashedSectionNameToFile(OutputFile, Section, Hash, IsWindows);

  return Hash;
}

// replace 'from' with 'to' inside of str
static void replace(std::string &str, const StringRef &from,
                    const StringRef &to) {
  size_t start_pos = str.find(from.str());
  if (start_pos == std::string::npos)
    return;
  str.replace(start_pos, from.size(), to.str());
}

// Common code for transforming Attributes

static void
hashForAttrCommon(unsigned N, bool IsDarwin, bool IsWindows,
                  StringRef (*GetDarwinSection)(const StringRef &),
                  std::unordered_map<StringRef, std::string> &NameCache,
                  const std::string &OutputFileName, StringRef &Name) {
  StringRef Hash;
  StringRef Section = Name;

  if (IsDarwin) {
    Section = GetDarwinSection(Name);
  }

  if (N == 0 || Section.size() < N) {
    return;
  }

  std::lock_guard<decltype(HashMutex)> lock(HashMutex);

  Hash = hashSectionName(Section, N, OutputFileName, IsWindows);
  if (Hash == Section) {
    return;
  }

  std::string tmp = Name.str();
  replace(tmp, Section, Hash);
  NameCache[Name] = tmp;
  Name = StringRef(NameCache[Name]);
}

// Code for transforming SectionlAttr
// for __attribute__(section("__RODATA,ThisSectionNameIsTooLong"))
//  => __attribute__(section("__RODATA,ip9RNVxH27rCS+Ix"))

static StringRef getMachOSectionFromSectionAttr(const StringRef &Name) {
  StringRef Segment, Section;
  unsigned TAA, StubSize;
  bool HasTAA;

  auto err = MCSectionMachO::ParseSectionSpecifier(Name, Segment, Section, TAA,
                                                   HasTAA, StubSize, true);
  assert(!err && "MCSectionMachO::ParseSectionSpecifier() failed");

  return Section;
}

void Sema::hashSectionNameForSectionAttr(StringRef &Name) {
  static std::unordered_map<StringRef, std::string> NameCache;
  unsigned N = Context.getLangOpts().hashSectionNames();
  bool IsDarwin = Context.getTargetInfo().getTriple().isOSDarwin();
  bool IsWindows = Context.getTargetInfo().getTriple().isOSWindows();
  auto GetDarwinSection = getMachOSectionFromSectionAttr;
  std::string OutputFileName =
      Context.getLangOpts().getHashedSectionNamesOutputFile();

  hashForAttrCommon(N, IsDarwin, IsWindows, GetDarwinSection, NameCache,
                    OutputFileName, Name);
}

// Code for transforming AsmLabelAttr
// e.g. __asm("section$start$__RODATA$ThisSectionNameIsTooLong")
//   => __asm("section$start$__RODATA$ip9RNVxH27rCS+Ix")

static StringRef getMachOSectionFromMachOAsmLabelAttr(const StringRef &Name) {
  size_t a, b;

  for (a = 0, b = 0; a < Name.size() && b < 3; ++a) {
    if (Name[a] == '$') {
      ++b;
    }
  }

  for (b = a; b < Name.size(); ++b) {
    if (Name[b] == '$') {
      break;
    }
  }

  if (b >= Name.size()) {
    b = StringRef::npos;
  }

  return Name.substr(a, b);
}

void Sema::hashSectionNameForAsmLabelAttr(StringRef &Name) {
  static std::unordered_map<StringRef, std::string> NameCache;
  unsigned N = Context.getLangOpts().hashSectionNames();
  bool IsDarwin = Context.getTargetInfo().getTriple().isOSDarwin();
  bool IsWindows = Context.getTargetInfo().getTriple().isOSWindows();
  auto GetDarwinSection = getMachOSectionFromMachOAsmLabelAttr;
  std::string OutputFileName =
      Context.getLangOpts().getHashedSectionNamesOutputFile();

  hashForAttrCommon(N, IsDarwin, IsWindows, GetDarwinSection, NameCache,
                    OutputFileName, Name);
}
