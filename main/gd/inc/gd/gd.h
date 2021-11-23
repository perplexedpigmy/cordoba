#pragma once

#include <git2.h>
#include <tl/expected.hpp>

#include <string>
#include <set>
#include <memory>


/**
 * TODO: Consider  RAII.  tradeoffs (flexibility vs explicit cleanup)
 * TODO: Timing and scalabilty checks
 *       - naive, 10,000 files per directory = ~50files/s    100 files per directory = ~1000/s
 *       - index,
 *       - local caching until commit.
 * TODO: Allow to chain createBranch-> addFile (where the file is added to the new Branch's context)
 * TODO: Convert Error from string -> (errString, errNo, origin)
 *
 * TODO: Support for sharding
 * TODO: Missing interface
 *         merge
 *         tag
 *
 * TODO: Implement rollback
 * TODO: More tests
 * TODO: lift & chain implementation
 * TODO: Display hash constructs
 * TODO: Tags support
 * TODO: Metadata support (notes)
 **/
namespace gd
{
 constexpr char const * defaultRef = "HEAD";

 /**
  * A structure to convey the context for git chains calls.
  * Except for the initial commit at least a repo and a branch are needed,
  * While the rest are interal information for chain calls
  **/

  struct context;
  namespace internal {
    struct Node {

      Node() noexcept = default;

      Node(Node&& other) noexcept;
      Node& operator=(Node&& other) noexcept;

      void reset() noexcept;
      static tl::expected<::gd::context, std::string> init(::gd::context&& ctx) noexcept;

      ~Node() {  reset();  }

      git_commit* commit_{nullptr};
      git_tree*   tree_{nullptr};
    };
  };

  struct context
  {
    context(git_repository* repo,
            const std::string& branch = defaultRef ) noexcept
    : repo_(repo), ref_(branch) {}

    context(context&& other ) noexcept  = default;
    context& operator=(context&& other) noexcept  = default;

    void setRepo(git_repository* repo) noexcept;
    void setBranch(const std::string& fullPathRef) noexcept;

    tl::expected<::gd::context, std::string> updateCommitTree(const git_oid& treeOid) noexcept;

    git_repository* repo_;         /*  git repository             */
    std::string ref_;              /*  git reference (branch tip) */
    bool dirty_{false};            /*  Has non-comitted updates   */

    internal::Node tip_;           /* Internal call chaining information */
  };


  /**
   *  Updating git repository(NI - Naieve implementation)
   *  Naively updates all trees in the path for each file.
   *  So the same logical tree may be updated multiple times.
   *
   *  TODO: Add required interface for complete implementation
   **/
  namespace ni
  {
    tl::expected<context, std::string> selectBranch(context&& ctx, const std::string& name) noexcept;
    tl::expected<context, std::string> add(context&& ctx, const std::string& fullpath, const std::string& content) noexcept;
    tl::expected<context, std::string> del(context&& ctx, const std::string& fullpath) noexcept;
    tl::expected<context, std::string> mv(context&& ctx, const std::string& fullpath, const std::string& toFullpath) noexcept;
    tl::expected<context, std::string> createBranch(context&& ctx, const git_oid& commitId, const std::string& name) noexcept;
    tl::expected<git_oid, std::string> commit(context&& ctx, const std::string& author, const std::string& email, const std::string& message) noexcept;
    tl::expected<context, std::string> rollback(context&& ctx) noexcept;

    tl::expected<std::string, std::string> read(context&& ctx, const std::string& fullpath) noexcept;
  }


 /**
  * selecting a repository for chained calls
  *
  * @param repo[Repository] : The repository to work with
  **/
  tl::expected<context, std::string>
  selectRepository(const std::string& fullpath, const std::string& name = "") noexcept;

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
  inline auto add(const std::string& fullpath, const std::string& content) noexcept
  {
    return [&fullpath, &content](context&& ctx) -> tl::expected<context, std::string> {
      return ni::add(std::move(ctx), fullpath, content);
    };
  }

 /**
  * Removes a file in the current context (repo, branch)
  * Only regular files(BLOB) can be removed.
  *
  * @param fullpath[std::string] : File name full path (relative to root)
  **/
  inline auto del(const std::string& fullpath) noexcept
  {
    return [&fullpath](context&& ctx) -> tl::expected<context, std::string> {
      return ni::del(std::move(ctx), fullpath);
    };
  }

 /**
  * Move/Rename a file in the current context (repo, branch)
  * Only regular files(Blobs) are supported
  *
  * @param fullpath[std::string] : File name full path (relative to root)
  **/
  inline auto mv(const std::string& fullpath, const std::string& toFullpath) noexcept
  {
    return [&fullpath, &toFullpath](context&& ctx) -> tl::expected<context, std::string> {
      return ni::mv(std::move(ctx), fullpath, toFullpath);
    };
  }

 /**
  * Adds files and their contents
  *
  * @param fileAndContents[set<std::pair<std::string, ,std::string>>]   A Set of files and content pairs
  **/
  inline auto add(const std::set<std::pair<std::string, std::string> >& filesAndContents) noexcept
  {;
    return [&filesAndContents](context&& ctx) -> tl::expected<context, std::string> {
      tl::expected<context, std::string>  foldingCtx(std::move(ctx));

      for (const auto& [fullpath, content] : filesAndContents)
      {
        if (!foldingCtx)
          return foldingCtx; // on error shortcut

        auto localCtx(ni::add(std::move(*foldingCtx), fullpath, content));
        foldingCtx.swap(localCtx);
      }
      return foldingCtx;
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
   * Commit previous updates to the context(Repository/Branch)
   *
   * @param author[std::string]  :  commiter's name (assuming commiter == author)
   * @param email[std::string]   :  commiter's email
   * @param message[std::string] :  Commit's message
   **/
  inline auto rollback() noexcept
  {
    return [](context&& ctx) -> tl::expected<context, std::string>  {
       return ni::rollback(std::move(ctx));
    };
  }
  /**
   * Creates a branch in a given repository
   *
   * @param commitId[git_oid] : The originating commit for the branch
   * @param name[std::string] : Branch name
   **/
  inline auto createBranch(const git_oid& commitId, const std::string& name) noexcept
  {
     return [&commitId, &name](context&& ctx) -> tl::expected<context, std::string> {
       return ni::createBranch(std::move(ctx), commitId, name);
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

  namespace shorthand {

    template <typename L, typename F>
    auto operator >>(tl::expected<L, std::string>&& lhs, F&& f)
    {
      return std::move(lhs).and_then(std::forward<F>(f));
    }

    template <typename L, typename F>
    auto operator ||(tl::expected<L, std::string>&& lhs, F&& f)
    {
      return std::move(lhs).or_else(std::forward<F>(f));
    }

    /**
     *  ThreadChainingContext enables disjoint gd commands using the last context.
     *
     *  Example:
     *
     *  // While chaining keeps the same context
     *  selectRepository(...) >> selectBranch(...) >> addFile(...) ... >> commit(...)
     *
     * // A more common use constitues disjointed calles to the library in different parts of the code
     * // db keeps the context for such cases.
     * selectRepository(...);
     * db >> selectBranch(...);
     * db >> addFile(...);
     * db >> rollback();
     *
     ***/
    class ThreadChainingContext {};
    static ThreadChainingContext db;

    tl::expected<context, std::string> getThreadContext() noexcept;

    template <typename F>
    auto operator >>(ThreadChainingContext, F&& f)
    {
      return std::move(getThreadContext()).and_then(std::forward<F>(f));
    }

    template <typename F>
    auto operator ||(ThreadChainingContext, F&& f)
    {
      return std::move(getThreadContext()).or_else(std::forward<F>(f));
    }

    /**
     * there is upport for call chaining i.e.
     *   selectRepository(...) >> addFile(...) >> commit(...)
     *
     * As well as via kept context i.e.
     *
     *  1.  auto db = selectRepository(...)
     *  2.  db >> addFile(...)
     *  3.  db >> commit(...)
     *
     * The ealier is serviced by the above operator >> and || where the context is moved from one call to another.
     * But allowing the kept context means that we are handling an lvalue.
     *
     * This operator handles the lvalue when its provided predicate F is ** the same ** Payload 'L'
     * as the return value, in that case the lvalue lhs can be updated directly, so it can be farther used,
     * as depicted in lines 2 and 3 above.
     *
     * Note that db >> commit(...) does not return a context and thus farther chaining is not possible.
     **/
    template <typename L, typename F>
    auto operator >>(tl::expected<L, std::string>& lhs, F&& f) ->
    std::enable_if_t<
      std::is_same<
        L,
        typename std::decay_t<decltype(std::move(lhs).and_then(std::forward<F>(f)))>::value_type
      >::value,
      decltype(std::move(lhs).and_then(std::forward<F>(f)))&
    >
    {
      lhs = std::move(lhs).and_then(std::forward<F>(f));
      return lhs;
    }

    /**
     * there is upport for call chaining i.e.
     *   selectRepository(...) >> addFile(...) >> commit(...)
     *
     * As well as via kept context i.e.
     *
     *  1.  auto db = selectRepository(...)
     *  2.  db >> addFile(...)
     *  3.  auto commitId = db >> commit(...)
     *  4.  auto content = db >> readFile(...)
     *
     * The ealier is serviced by the above operator >> and || where the context is moved from one call to another.
     * But allowing the kept context means that we are handling an lvalue.
     *
     * This operator handles the lvalue when its provided predicate F is ** different ** than Payload 'L'
     * In this case we can not update lhs, but we have to return a new tl::expected
     *
     * TODO: the following sentence is an ideal but can't yet be implemented.
     *        L's lhs will have to be updated, but it's already moved (and lost) in f
     *        Example, In line 3 the db's context was moved and the commit updated it,
     *                 but currently there is no way to update db to refelct the changed context.
     * Note that it's possible to continue using the db as shown on line 4.
     **/
    template <typename L, typename F>
    auto operator >>(tl::expected<L, std::string>& lhs, F&& f) ->
    std::enable_if_t<
      !std::is_same<
        L,
        typename std::decay_t<decltype(std::move(lhs).and_then(std::forward<F>(f)))>::value_type
      >::value,
      decltype(std::move(lhs).and_then(std::forward<F>(f)))
    >
    {
      return std::move(lhs).and_then(std::forward<F>(f));
    }

  }
};
