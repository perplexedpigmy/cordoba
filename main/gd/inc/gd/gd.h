#pragma once

#include <git2.h>
#include <tl/expected.hpp>

#include <iostream>
#include <string>


/**
 * TODO: Consider introducing RAII. runtime impact?
 * TODO: Add tests
 * TODO: Timing and scalabilty checks
 *       - naieve,
 *       - index,
 *       - local caching until commit, cache cleanup (???)
 * TODO: exception less repository creation(pros, cons)
 **/
namespace gd
{
  inline static std::string sRefRoot = "refs/heads/";

 /**
  * Repository access abstraction
  * connets to an existing repositry if not existing creates a new bare repository
  * in the indicated path, providing there path has write premissions.
  *
  * Failure to create or connect to a repository throws an exception
  **/
  class Repository
  {
    private:
      git_repository* repo_ = nullptr;
      const std::string    path_;
      const std::string    name_;

      /**
       * Creates a repository named 'repoName' at the location 'path'
       *
       * @param path[string] : Location of the repository
       * @param repoName     : Repository name
       *
       * throws an excpetion on failure
       **/
      void create(const std::string& path, const std::string& repoName);

    public:
      Repository(const std::string& path, const std::string& name);
      ~Repository();

      /**
       * Returns true if a repository exists at 'repoPath'
       **/
      static bool exists(const std::string& repoPath) noexcept;

      operator git_repository*() const noexcept { return repo_; }

  };

 /**
  * A structure to convey the context for git chains calls.
  * Except for the initial commit at least a repo and a branch are needed,
  * While the rest are interal information for chain calls
  **/
  struct context
  {
    context(Repository& r, const std::string& ref )
    :repo_(r), ref_(ref), prevCommitTree_(nullptr), parents_{nullptr, nullptr}, numParents_(0) {}

    Repository& repo_;  /*  git repository             */
    std::string ref_;   /*  git reference (branch tip) */

    // Internal chaining information
    git_tree* prevCommitTree_;       /*  Commit tree       */
    git_commit const *parents_[2];   /*  Commit parents    */
    size_t  numParents_;             /*  number of parents */
  };

 /**
  * Retrieves a tree relative to a root tree.
  * null is returned if the tree does not exists. Any other error is reported
  *
  * @param repo[Repository]  : The repository
  * @param path[std::string] : The relative path
  * @param root[git_tree]    : Pointer to a root git_tree
  *
  * Returned git_tree* must be freed by the caller
  **/
  tl::expected<git_tree*, std::string>
  getTree(Repository& repo, const std::string& path, git_tree const * root) noexcept;

 /**
  * Retrieves a blob relative to a root tree.
  * null is returned if the tree does not exists. Any other error is reported
  *
  * @param repo[Repository]  : The repository
  * @param path[std::string] : The relative path
  * @param root[git_tree]    : Pointer to a root git_tree
  *
  * Returned git_object* must be freed by the caller
  **/
  tl::expected<git_blob*, std::string>
  getBlob(Repository& repo, const std::string& path, git_tree const * root) noexcept;

  /**
   *  Updating git repository(NI - Naieve implementation)
   *  Naively updates all trees in the path for each file.
   *  So the same logical tree may be updated multiple times.
   **/
  namespace ni
  {
    tl::expected<context, std::string> selectBranch(context&& ctx, const std::string& name) noexcept;
    tl::expected<context, std::string> addFile(context&& ctx, const std::string& fullpath, const std::string& content) noexcept;
    tl::expected<git_oid, std::string> commit(context&& ctx, const std::string& author, const std::string& email, const std::string& message) noexcept;
    tl::expected<context, std::string> fork(context&& ctx, const git_oid& commitId, const std::string& name) noexcept;
    tl::expected<std::string, std::string> read(context&& ctx, const std::string& fullpath) noexcept;
  }


 /**
  * selecting a repository for chained calls
  *
  * @param repo[Repository] : The repository to work with
  **/
  tl::expected<context, std::string>
  selectRepository(Repository& repo) noexcept;

 /**
  * selecting a branch for chained calles
  *
  * @param name[std::string] : The branch's name
  **/
  inline auto selectBranch(const std::string& name) noexcept
  {
    return [&name](context&& ctx) -> tl::expected<context, std::string> {
      return ni::selectBranch(std::move(ctx), name);
    };
  }


 /**
  * Adds a file in the current context (repo, branch)
  *
  * @param fullpath[std::string] : File name full path (relative to root)
  * @param content[std::string]  : File contents blob
  **/
  inline auto addFile(const std::string& fullpath, const std::string& content) noexcept
  {
    return [&fullpath, &content](context&& ctx) -> tl::expected<context, std::string> {
      return ni::addFile(std::move(ctx), fullpath, content);
    };
  }

  /**
   * Commit previous updates to the context(Repository/Branch)
   *
   * @param author[std::string]  :  commiter's name (assuming commiter == author)
   * @param email[std::string]   :  commiter's email
   * @param message[std::string] :  Commit's message
   **/
  inline auto commit(const std::string& author, const std::string& email, const std::string& message) noexcept
  {
    return [&author, &email, &message](context&& ctx) -> tl::expected<git_oid, std::string>  {
       return ni::commit(std::move(ctx), author, email, message);
    };
  }

  /**
   * Creates a branch in a given repository
   *
   * @param commitId[git_oid] : The originating commit for the branch
   * @param name[std::string] : Branch name
   **/
  inline auto fork(const git_oid& commitId, const std::string& name) noexcept
  {
     return [&commitId, &name](context&& ctx) -> tl::expected<context, std::string> {
       return ni::fork(std::move(ctx), commitId, name);
     };
  }

  /**
   * Reads file content in a given context
   * @param fullpath[string] : Full path of file
   **/
  inline auto read(const std::string& fullpath) noexcept
  {
    return [&fullpath](context&& ctx) -> tl::expected<std::string, std::string> {
      return ni::read(std::move(ctx), fullpath);
    };
  }
};
