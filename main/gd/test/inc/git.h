#pragma once 

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>


static const char* validIcon = "✅";
static const char* invalidIcon = "❌";
static const char* criticalIcon = "⛔";

/// @brief git_oid requires Hash function to be used as key in an unordered_map
struct OidHasher {
  std::size_t operator()(const git_oid* data) const {
    std::size_t hash { 0 };
    for (size_t i = 0; i < GIT_OID_RAWSZ; ++i) {
      hash = (hash << 5) - hash + data->id[i];  // hash * 31 + [i]
    }
    return hash;
  }
};

/// @brief A Thread safe structure to capture the expected git stata
/// 
class GlycemicIt {
  public:
  using CommitId = git_oid const *;
  using Element = std::pair<std::filesystem::path, std::string>;
  using Elements = std::vector<Element>;

  /// @brief A Commit (relative) properties are parent commit and included elements
  struct CommitProps {
    CommitId parentCommitId_;
    Elements elems_;

    CommitProps(CommitId parentId, GlycemicIt& git) noexcept;
  };
  
  private:
  bool ok_;
  mutable std::shared_mutex gitData_;
  /// @brief A hash from a commit it's properties 
  std::unordered_map<CommitId, CommitProps, OidHasher> commits_;

  /// @brief a list of existing branches 
  std::vector<std::string> branches_;

  /// @brief Compares the expected exising elements and their content against the git repository
  /// @param repo an open pointer to the test git repository
  /// @return True if no problem encoutnered and the content matches
  bool valid_content(git_repository* repo) const; 

  /// @brief Only tests for the expected branch existance
  /// @param repo an open pointer to the test git repository
  /// @return True if no problem encoutnered and all the expected branches exist
  bool valid_branches(git_repository* repo) const noexcept; 

  /// @brief Verifies that removed files in a commit are not accessible in that commit
  /// @param repo an open pointer to the test git repository
  /// @return True if no removed file is accessible
  bool valid_removals(git_repository* repo) const; 

  public: 
  GlycemicIt() noexcept
  : ok_(true) {
    branches_.push_back("main"); // Add the default first branch 
  }

  bool ok() const noexcept {
    std::shared_lock<std::shared_mutex> readLock(gitData_);
    return ok_;
  }

  void nok() noexcept  {
    std::unique_lock<std::shared_mutex> writeLock(gitData_);
    ok_ = false;
  }

  /// @brief 
  /// @return 
  bool isEmpty() const noexcept {
    std::shared_lock<std::shared_mutex> readLock(gitData_);
    return commits_.empty();
  }

  /// @brief Adds a commit, propeties pair to the git bookkeeping 
  /// @param commitId the commit id in `git_oid` format
  /// @param props the commit props, i.e. parent commit id + file and content list
  void addCommit(CommitId const commitId, CommitProps&& props) noexcept {
    std::unique_lock<std::shared_mutex> writeLock(gitData_);
    commits_.insert(std::make_pair(commitId, std::move(props)));
  }

  /// @brief returns a random file from the files of a commit 
  /// @param commitId the commit id in `git_oid`
  /// @return returns a randomly selected path of a file in commit
  std::filesystem::path getRandomFileOfContext(CommitId commitId) const; 

  /// @brief Verifies the expected git content against the git repository
  /// @param repoPath A full path to the git's repository directory
  /// @return True if the two contents match, otherwise false
  /// Precondition, git_libgit2_init() was already executed, this is supposed to be true
  /// when the gitdb was already accessed.
  bool valid(const std::filesystem::path& repoPath) const; 

  /// @brief Retrieves the elements of a commit
  /// @param commitId the commit' `git_oid` 
  /// @return A vector of elements
  const Elements& elemsOf(CommitId commitId) const; 

  /// @brief Serves a branch name for an index
  /// @param branchNum the requested branch number
  /// @return a (branch name, true) if the branch is newly created, if not newly created (branch name, false)
  /// It's important to note that the same index won't necessarily yield the same branch name.
  /// For example:
  /// If the requested index is 2, but brn1 doesn't exist, it will return brn1, however, the next time index 2 is requested
  /// brn2 is returned 
  std::pair<std::string, bool> getBranch(int branchNum) noexcept;  
};

//////////////////////////////////////////
//             Validations
//////////////////////////////////////////

bool GlycemicIt::valid_content(git_repository* repo) const {
  spdlog::info("Content validation:");
  bool isValid = true;

  for (const auto& [id, props]: commits_) {
    spdlog::info("  [{}] -> [{}]:", (props.parentCommitId_ ? shortSha(props.parentCommitId_) : "ROOT"s),  shortSha(id) );
    for (const auto& [name, content] : props.elems_) {
      auto repoContent = contentOf(repo, id, name );
      if (!repoContent)  
        throw "["s + shortSha(id) + "] " + name.string() + ": " + std::string(repoContent.error());

      auto prefix = !repoContent ? criticalIcon : (*repoContent == content) ? validIcon : invalidIcon;
      isValid = isValid && !!repoContent && *repoContent == content;
      spdlog::info("   {}  {}", prefix, name);
    }
  }
  return isValid;
}


bool GlycemicIt::valid_branches(git_repository* repo) const noexcept {
  spdlog::info("Branch validation:");
  bool isValid = true;
  for (const auto& branch : branches_) {
    auto branchCommit = referenceCommit(repo, "refs/heads/"s + branch);
    spdlog::info("   {} {} [{}]", (!branchCommit ? invalidIcon : validIcon), branch, shortSha(*branchCommit));
    isValid = isValid && !!branchCommit;
  } 
  return isValid;
}


bool GlycemicIt::valid_removals(git_repository* repo) const {
  spdlog::info("Removal validation");
  bool isValid = true;

  for (const auto& [id, props]: commits_) {
    if (props.parentCommitId_) {
      if (auto prevIdx = commits_.find(props.parentCommitId_); prevIdx == commits_.end()) {
          std::cerr << "[" << shortSha(props.parentCommitId_) << "]" << " not found in testing bookkeeping" << std::endl;
          isValid = false;
      } else {
        const auto& prevProps = prevIdx->second;
        std::vector<std::string> prevElems, elems, deletedElems; 
        std::transform(prevProps.elems_.begin(), prevProps.elems_.end(), std::back_inserter(prevElems), [](const auto p) { return p.first; });
        std::transform(props.elems_.begin(), props.elems_.end(), std::back_inserter(elems), [](const auto p) { return p.first; });
        std::sort(prevElems.begin(), prevElems.end());
        std::sort(elems.begin(), elems.end());
        std::set_difference(prevElems.begin(), prevElems.end(), elems.begin(), elems.end(), std::back_inserter(deletedElems));
        for (const auto& deleted : deletedElems )  {
          auto repoContent = contentOf(repo, id, deleted);
          spdlog::info("   {} In @ [{}] gone [{}] {}", (!repoContent ? validIcon : invalidIcon), shortSha(prevIdx->first),  shortSha(id), deleted );
          isValid = isValid && !repoContent;  
        }
      }
    }
  }
  return isValid;
}


bool GlycemicIt::valid(const std::filesystem::path& repoPath) const {
  std::shared_lock<std::shared_mutex> readLock(gitData_);
  bool isValid = true;

  auto resRepo = openRepository(repoPath);
  if (!resRepo) 
    throw "Failed to open repository. "s + repoPath.string() + ": "  + std::string(resRepo.error());

  return 
    ok_ 
    && valid_branches(*resRepo) 
    && valid_content(*resRepo)
    && valid_removals(*resRepo);
}


//////////////////////////////////////////
//             Queries
//////////////////////////////////////////
const GlycemicIt::Elements& 
GlycemicIt::elemsOf(GlycemicIt::CommitId commitId) const {
  auto commit = commits_.find(commitId);

  if (commit == commits_.end()) 
    throw "(test bug) Parent commit "s + sha(commitId) + " not found in book keeping. exiting";

  return commit->second.elems_;
}


std::pair<std::string, bool> 
GlycemicIt::getBranch(int branchNum) noexcept {
  bool newBranch = false;
  if (branchNum >= branches_.size()) {
    std::unique_lock<std::shared_mutex> writeLock(gitData_);
    branches_.push_back("brn" + std::to_string(branches_.size()));
    newBranch = true;
    return std::make_pair(branches_[branches_.size() - 1], true /* New branch */);
  }
  return std::make_pair(branches_[branchNum], false /* existing branch */);
}


std::filesystem::path 
GlycemicIt::getRandomFileOfContext(GlycemicIt::CommitId commitId) const {
  std::shared_lock<std::shared_mutex> readLock(gitData_);
  auto commit = commits_.find(commitId);
  if (commit == commits_.end()) 
    throw "(test bug) Commit '"s + shortSha(commitId) + "' not found while trying to find random file of context"; 

  if (commit->second.elems_.empty() ) 
    throw "(test bug) Commit '"s + shortSha(commitId) + "' has zero elements while trying to find random file of context";

  int lastIndex = commit->second.elems_.size() - 1;
  return commit->second.elems_[ std::experimental::randint(0, lastIndex)].first;
}


GlycemicIt::CommitProps::CommitProps(GlycemicIt::CommitId parentId, GlycemicIt& git) noexcept 
:parentCommitId_{parentId} 
{
    // First commit has no parents
    if (parentId != 0) {
      std::shared_lock<std::shared_mutex> readLock(git.gitData_);
      try {
      elems_ = git.elemsOf(parentId);
      } catch (const std::string& message) {
        spdlog::error("Unable to read content of parent (Naievely assuming empty parent) commit [{}]: {}", shortSha(parentId), message);
        git.nok(); /// Unexpected error, shortcut test
      }
    }
}