#pragma once 
#include <gd/gd.h>

#include "git.h"
#include "generator.h"
#include "wgen.h"
#include "out.h"
#include "spdlog/async.h"

#include <memory>
using namespace gd;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Crudité - French for vegtables slices added to the meal, just like a db CRUD they are the basis of healthy nutrition
// This class has the honor of abstracting the basis for all DB operations starting with
// CRUD - Create, Read, Update and Delete
class Crudité {
  public:
    virtual Result<Context> apply(Result<Context>&&, GlycemicIt::Elements& elems, char agentId) const noexcept = 0;
};

/// @brief CRTP construct whose sole purpose is adding getters to CRUD (and potentially other DB actions) 
template <typename D>
class Getter : public Crudité {
  public:
  static std::unique_ptr<Crudité> get(const wgen::syllabary& s) noexcept {
    return std::make_unique<D>(D(s));
  }
};

////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a new file with content, filename and content are randomally generated
class Create : public Getter<Create> {
  std::filesystem::path name_;
  std::string content_;

public:
  Create(const wgen::syllabary& s) noexcept
  :name_{s.random_unique_filename()}, content_{s.random_content()} {}

  Result<Context> apply(Result<Context>&& ctx, GlycemicIt::Elements& elems, char agentId) const noexcept override {
    spdlog::info("({}) [{} {}] CREATE {} (size: {})", agentId, ctx->ref_, shortSha(ctx->getCommitId()), name_, content_.size());
    elems.emplace_back(make_pair(name_, content_));
    return std::move(ctx).and_then(add(name_, content_));
  }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Udpates existing files in the current context, files is chosen randommaly and so is the generated content
class Update : public Getter<Update> {
  std::string content_;

public:
  Update(const wgen::syllabary& s) noexcept
  :content_{s.random_content()} {}

  Result<Context> apply(Result<Context>&& ctx, GlycemicIt::Elements& elems, char agentId) const noexcept override {
    if (elems.empty()) 
      return std::move(ctx);

    auto updateIdx = std::experimental::randint(0, static_cast<int>(elems.size() - 1));
    std::filesystem::path name  = elems[updateIdx].first;
      
    spdlog::info("({}) [{} {}] UPDATE {} (size: {})", agentId, ctx->ref_, shortSha(ctx->getCommitId()), name, content_.size());
    elems[updateIdx] = std::make_pair(name, content_);
    return std::move(ctx).and_then(add(name, content_));
  }
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Randomally deletes a file from context
class Delete : public Getter<Delete> {
  std::string content_;

public:
  Delete(const wgen::syllabary& _) noexcept {}

  Result<Context> apply(Result<Context>&& ctx, GlycemicIt::Elements& elems, char agentId) const noexcept override {
    if (elems.empty()) 
      return std::move(ctx);

    auto delIdx = std::experimental::randint(0, static_cast<int>(elems.size() - 1));
    std::filesystem::path name  = elems[delIdx].first;
      
    spdlog::info("({}) [{} {}] DELETE {}", agentId, ctx->ref_, shortSha(ctx->getCommitId()), name);
    elems.erase( elems.begin() + delIdx);
    return std::move(ctx).and_then(del(name));
  }
};

/////////////////////////////////////////////////////////////////////////////////////////
/// @brief Reads a random file of current context, read content is silently discarded
class Read : public Getter<Read> {
  std::string content_;

public:
  Read(const wgen::syllabary& _) noexcept {}

  Result<Context> apply(Result<Context>&& ctx, GlycemicIt::Elements& elems, char agentId) const noexcept override {
    if (elems.empty()) 
      return std::move(ctx);

    auto delIdx = std::experimental::randint(0, static_cast<int>(elems.size() - 1));
    std::filesystem::path name  = elems[delIdx].first;
      
    auto readCtx = std::move(ctx).and_then(read(name));
    spdlog::info("({}) [{} {}] READ {} (size: {})", agentId, readCtx->ref_, shortSha(readCtx->getCommitId()), name, readCtx->content().size());
    return std::move(*readCtx);
  }
};

constexpr double operator"" _percent(unsigned long long val) { return static_cast<double>(val) / 100.0; }
constexpr double operator"" _percent(long double val) { return val / 100.0; }

/////////////////////////////////////////////////////////////
/// @brief Generates a stream of actions where the last is a commit
/// @param numActions max num actions, where at lesat one as Create/Update or Delete and the last one is a Commit 
/// @return a Generator that yields up to `numAction` Actions
/// Each Action has a certain hardcoded probability assigned to it, that could change in the future if probablility testing 
/// is required (Maybe some problems are more frequent in different action salad ratio??)
///
/// NOTE: First action of all time is always a CREATE
generator<std::unique_ptr<Crudité>> 
actionGenerator(const wgen::syllabary& s, int numActions, const GlycemicIt& git) noexcept {
  using Getter = std::unique_ptr<Crudité>(*)(const wgen::syllabary&) noexcept;
  using ProbabilityGetter= std::pair<double, Getter>;

  std::vector<ProbabilityGetter> getters = {
    {20_percent, Create::get},
    {30_percent, Update::get},
    {10_percent, Delete::get},
    {40_percent, Read::get},
  };

  std::vector<double> probabilities;
  std::transform(getters.begin(), getters.end(), std::back_inserter(probabilities), [](const auto p) { return p.first; });
  std::discrete_distribution<> dist{ probabilities.begin(), probabilities.end() };
  std::mt19937 engine;

  /// The very first action for a repository must be a `Create` otherwise there is nothing to update, delete or read
  if (git.isEmpty()) {
    co_yield Create::get(s); 
    --numActions;
  }

  while (numActions-- > 0) {
    auto getter = getters[ dist(engine) ].second;
    co_yield getter(s);
  }
};
