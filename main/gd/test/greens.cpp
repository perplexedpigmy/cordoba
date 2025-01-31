#include <gd/gd.h>

#include <filesystem>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <ios>
#include <set>
#include <memory>
#include <unordered_map>
#include <map>
#include <shared_mutex>
#include <format>
#include <experimental/random>
#include "out.h"

#include <openssl/sha.h>
#include "inc/wgen.h"
#include "inc/git.h"
#include "inc/crudite.h"
#include "spdlog/async.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <future>
#include <thread>
#include <CLI/CLI.hpp>

using namespace gd;
using namespace std;

static GlycemicIt sGit;

/// @brief Circular 'A'-'Z' id generator
/// @return an Id
/// The id will only be unique if there no more 26 callers 
char getLetterId() noexcept {
  static char id{0};
  return id++ % 26 + 'A';
}

/// @brief randomize the context's branch 
/// @param ctx prevous Context
/// @param numBranches the maximum number of allowed branches
/// @return Context with changed branch, unelss there is some kind of error
Result<gd::Context> randomizeBranch(char agentId, Result<Context>&& ctx, int numBranches) noexcept {
  static std::mutex branchCreation;
  // Creating a new branch before the first commit, is meaningless and problematic
  if (sGit.isEmpty()) 
    return std::move(ctx);

  auto branchNum = std::experimental::randint(0, numBranches-1);

  std::scoped_lock serialize(branchCreation);
  auto [branchName, isNew] = sGit.getBranch(branchNum);
  if (isNew) {
    ctx = std::move(ctx).and_then(createBranch(branchName));
    if (!ctx) {
      return unexpected_err(ctx.error());
    }
  }
  ctx = std::move(ctx).and_then(selectBranch(branchName));
  spdlog::info("({}) Switch to{} branch {} @ {}", agentId, (isNew ? " new" : ""), branchName, shortSha(ctx->getCommitId()));
  ctx->rebase();
  return std::move(ctx);
}

// Each agent will do exactly `numCommits` as requested by the user
// Each of the commits will have up to `num` differet actions 
// that will take place over randomly `numBranches' where the first commit takes place on the default(main) branch
// The first commit is restricted to the default branch because `git_branch_create` must have a commit to branch from.
// Surprisingly, git itself is also not able to do
//   $ git branch <newbranch> 
//   fatal: Not a valid object name: 'main'.
//
//  It will start to actively creating a branch but it can't branch from main(the default reference) because the reference 
//  doesn't exist without a commit
//
// Surprisinlgy, git allows to do 
//   $ git checkout -b <newbranch>
//   Switched to a new branch <newbranch>
//
// Because this doesn't create a new branch, but rather just changes HEAD to point at the new reference (although the later doesn't exist yet)
//   $ cat .git/HEAD
//   1:ref: refs/heads/<newbranch>
// 
// The first action must be a 'Create'
// Any consequtive answer may be of 'Create', 'Read', 'Update' and 'Delete'
Result<Context> agent(
  const std::filesystem::path& repoPath, 
  const wgen::default_syllabary& s, 
  size_t numBranches,
  size_t numCommits,
  size_t numActions,
  size_t maxDirectoryDepth,
  size_t maxFilenameLength
  ) noexcept {
    char agentId = getLetterId();
    size_t currentCommitNum = 0;
    spdlog::info("Agent ({}) Started", agentId); 

    // There are 2 critical sections in this testing code
    // 1. selectRepository may create a repository, to avoid race condition where multiple threads create the same repository
    // 2. Commits race condition can happen when multiple threads are trying to commit for the same branch serializing it, 
    //    to solve this a critical section is pairing a forcing context to tip of branch with the commit(git rebase & commit paradigm)
    //    This is a simple solution but as close as it can be to real use case, in a real use case the user 
    //    should be responsoble to make sure that this pardigm makes sense or a merge/edit is required, prior to a commit.
    static std::mutex criticalsection;

    criticalsection.lock();
    auto ctx = selectRepository(repoPath);
    criticalsection.unlock();

    if (!ctx)
      return ctx;
  
    while (sGit.ok() && !!ctx && currentCommitNum++ < numCommits) {
      GlycemicIt::CommitProps props(ctx->getCommitId(), sGit);

      for ( const auto& action : actionGenerator(s, numActions, sGit)) {
        ctx = action->applyGit(std::move(ctx), props.elems_, agentId );
      }
      // TOOD: Maybe I want to verify that only in this case it will rollback
      // 11 GIT_ERROR_OBJECT "failed to create commit: current tip is not the first parent"
      if (!props.elems_.empty()) {
        {
          std::scoped_lock serialize(criticalsection);
          if(auto isTip = ctx->isTip(); !sGit.isEmpty() && (!isTip || *isTip == false)) {
            ctx = std::move(ctx).and_then(rollback());
            spdlog::info("({}) ROLLBACK #{} [{} {}]", agentId, currentCommitNum, ctx->ref_, shortSha(ctx->getCommitId()) );
            ctx->rebase();
            --currentCommitNum;
          } else {
            ctx = std::move(ctx)
              .and_then(
                commit(
                "agent "s + agentId, 
                "agent@test.one", 
                std::format("Commit {}:{}", agentId, currentCommitNum))
            );
            if (!!ctx) {
              spdlog::info("({}) COMMIT #{} [{} {}] ", agentId, currentCommitNum, ctx->ref_, shortSha(ctx->getCommitId()) );
              sGit.addCommit(ctx->getCommitId(), std::move(props));
            }
          }
        } 
        if (!ctx) {
          sGit.nok();
        }  
      } 
    ctx = randomizeBranch(agentId, std::move(ctx), numBranches); 
  }

  return ctx;
}

/// @brief Sets logger 
/// @param logPath log file path
void setLogger(const std::filesystem::path& logPath) noexcept {
  auto glogger = spdlog::basic_logger_mt("grn", logPath);
  glogger->set_pattern("[%H:%M:%S.%f] %=8l [%t] %v");
  glogger->set_level(spdlog::level::info); 

  setLogger(glogger); // Library logger
  spdlog::set_default_logger(glogger); 
}

/**
 * Greens are good for you and desirable for any tests' results
 * 
 * The test sprouts one or more agents (db users) each of which is designated 
 * to randomly perfrom
 *   C potential updates(Commits) each of which is performed on 
 *   B branches and composed of 
 *   A random CRUD (Create, Read, Update and Delete) of files/version with
 *   D max directory depth (min 2) and 
 *   L max filename length (min 1)
 * 
 *  For testing repeatablity the the random seed is captured in the log file `/tmp/test/greesn/log`
 *  and when not explicitly specified by the user random seed is used.
 *  
 * After all agents are terminate. Thie captured GlycemicIT state is compared
 * against the repository.
 * 
 * Possible points of failure:
 *  Any of the agents' actions can fail
 *  The end result GIT != GlycemicIt
 */
int main(int argc, char** argv)
{
  CLI::App app{"Lets add some greens to the diet, and test our glycemic index"};

  std::filesystem::path testbase{ "/tmp/test"};
  int    seed = {-1};
  size_t numAgents {2};
  size_t maxBranches {3};
  size_t numCommits {10};
  size_t numActions {11};
  size_t maxDirectoryDepth{3};
  size_t maxFilenameLength{2};
  app.add_option("-t,--test", testbase, "Location for installing the repo and logs (defaults '"s + testbase.string() + ")");
  app.add_option("-s,--seed", seed, "Requested random seed (Default randmolly selected)");
  app.add_option("-g,--agents", numAgents, "Number of concurrent agents (Default "s + std::to_string(numAgents) + ")");
  app.add_option("-b,--branches", maxBranches, "Max Number of branches (Default "s + std::to_string(maxBranches) +")");
  app.add_option("-c,--commits", numCommits, "Number of Commits/Updates per agent (Default "s + std::to_string(numCommits) +")");
  app.add_option("-a,--actions", numActions, "Max Number of actions per commit (Default "s + std::to_string(numActions) + ")");
  app.add_option("-d,--depth", maxDirectoryDepth, "Max directory depth (Default " + std::to_string(maxDirectoryDepth) + ")");
  app.add_option("-l,--length", maxFilenameLength, "Max filename length (Default " + std::to_string(maxFilenameLength) + ")");
  CLI11_PARSE(app, argc, argv);

  std::filesystem::path repoPath { testbase / "greens" };
  cleanRepo(repoPath); 

  auto logFile = repoPath / "log";
  setLogger(logFile);

  if (seed == -1)
    seed = std::experimental::randint(0, 10000);
  std::experimental::reseed(seed);

  spdlog::info("Starting test with random seed {}\n{:>5} agents\n{:>5} Branches\n{:>5} Commits\n{:>5} Max actions\n{:>5} max directory depth\n{:>5} max filename", 
    seed, numAgents, maxBranches, numCommits, numActions, maxDirectoryDepth, maxFilenameLength);

  wgen::default_syllabary s(maxDirectoryDepth, maxFilenameLength);
  std::vector<std::future<Result<gd::Context>>> futures;

  for (size_t i = 0; i < numAgents; ++i) {
    futures.push_back(std::async(std::launch::async, agent, repoPath, s, maxBranches, numCommits, numActions, maxDirectoryDepth, maxFilenameLength ));
  }

  bool isError{false};
  for (auto& future : futures) {
    auto result = future.get(); // Joins and retrieves result
    if (!result) {
      isError = true;
      std::cerr << "Agent failed: " << result.error() << std::endl;
    }
  }
  if (isError) 
    exit (-1);

  try {
    if (!sGit.valid(repoPath))  {
      std::cerr << "Failure. For more details see log file " << logFile << std::endl;
      return -1;
    } else {
      std::cout << "Success" << std::endl;
    }
  } catch (const std::string& message) {
    std::cerr << "FATAL: " << message << " for more information see " << logFile << std::endl;
    spdlog::critical(message);
    return -2;
  }

  return 0;
}