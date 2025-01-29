#pragma once
#include <git2.h>

#include <string>
#include <set>
#include <memory>
#include <ostream>
#include <filesystem>
#include <map>
#include <err.h>
#include <expected.h>
#include <guard.h>
#include <collector.h>
#include <spdlog/spdlog.h>

/**
 * TODO: Review functionality of Thread local context
 *       First the updates are not shared between local variable and local thread context 
 *       Re-think the semantics of local thread context, 
 *        is it interchangable 
 *        when should it be destructed? Additional API with callers responsiblity.
 * TODO: Review considerations:
 *    Currently reading non-commited added files or updates is not possible (Any use case for that)
 *    Updating the same file more than once (Including creation + update) is undefined behavior (Should it be supported?)
 * 
 * TODO: Timing and scalabilty checks
 *       - naive, 10,000 files per directory = ~50files/s    100 files per directory = ~1000/s
 *       - index,
 *       - local caching until commit. 
 * TODO: Convert Error from string -> (errString, errNo, origin)
 *
 * TODO: Support for sharding. 2 years later, what did I mean, what for?
 * TODO: Missing interface
 *         merge
 *         tag
 * TODO: Error, to keep all elements of git_error (klass)
 * TODO: lift & chain implementation
 * TODO: Tags support
 * TODO: Metadata support (notes)
 * 
 * TODO: Testing, testing and more testing 
 *       test rollback
 *       test multithreading
 * TODO: Split Node, into Node && NodeWithTip or something to that effect. Why??
 *       
 *       
 **/
namespace gd
{
  struct Context;

  constexpr char const * defaultRef = "HEAD";

  namespace internal {

    /** 
     * A abstraction node on the DAG
     **/
    struct Node {

      Node() noexcept = default;
      Node(Node&& other) noexcept;
      Node& operator=(Node&& other) noexcept;

      /**
       * @brief Updates a context to latest of reference 
       * @param ctx The old context, with valid repository/reference 
       * @return The context with new node pointing at the tip of ref
       **/ 
      static Result<gd::Context> init(gd::Context&& ctx) noexcept;

      /**
       * @brief Updates the tip to given commit by commitId
       * @param ctx The old context, with valid repository/reference 
       **/ 
      Result<void> update(const gd::Context& ctx, git_oid const* commitId) noexcept;

      /**
       * @brief Updates the tip to the latest commit of the current reference
       * @param ctx The old context, with valid repository/reference 
       * @warning if multiple threads contribute to the same reference's evolution, serizlization is required by caller, or 
       * the update risks not refelecting the last of reference. 
       **/ 
      Result<void> rebase(const gd::Context& ctx) noexcept;

      /// @brief The commit used
      commit_t commit_;

      /// @brief Root tree of the commit
      tree_t root_;
    };
  };

  /**
   * A context tracking struct, for call chaining 
   * Updates are collected and dumped on commit, 
   * This aggregates all directory updates into one git write.
   **/
  struct Context
  {
    repository_t* repo_;     /*  git repository             */
    std::string   ref_;      /*  git reference (branch tip) */
    TreeCollector updates_;  /*  Collects updates           */

    internal::Node tip_;     /* Internal call chaining information */

    Context(repository_t* repo,
            const std::string& branch = defaultRef ) noexcept
    : repo_(repo), ref_(branch) {}

    Context(Context&& other ) noexcept  = default;
    Context& operator=(Context&& other) noexcept  = default;

    void setRepo(gd::repository_t* repo) noexcept;
    void setBranch(const std::string& fullPathRef) noexcept;

    Result<gd::Context> updateCommitTree(const git_oid& treeOid) noexcept;

    git_oid const * getCommitId() const noexcept;

    Result<void> update(git_oid const * commitId) noexcept { return tip_.update(*this, commitId); }
    Result<void> rebase() noexcept { return tip_.rebase(*this); }

  };

  class ReadContext : public Context {
    std::string content_;

    public:
    ReadContext(Context&& ctx, std::string&& content) noexcept
    : Context{ std::move(ctx) }, content_{ std::move(content)}
    { }

    const std::string& content() const noexcept { return content_; }
  };

  namespace ni
  {
    Result<Context> selectBranch(Context&& ctx, const std::string& name) noexcept;
    Result<Context> add(Context&& ctx, const std::filesystem::path& fullpath, const std::string& content) noexcept;
    Result<Context> rm(Context&& ctx, const std::string& fullpath) noexcept;
    Result<Context> mv(Context&& ctx, const std::string& fullpath, const std::string& toFullpath) noexcept;
    Result<Context> createBranch(Context&& ctx, const git_oid* commitId, const std::string& name) noexcept;
    Result<Context> createBranch(Context&& ctx, const std::string& name) noexcept;
    Result<Context> commit(Context&& ctx, const std::string& author, const std::string& email, const std::string& message) noexcept;
    Result<Context> rollback(Context&& ctx) noexcept;

    Result<ReadContext> read(Context&& ctx, const std::filesystem::path& fullpath) noexcept;
    Result<gd::ReadContext> readblob(gd::Context&& ctx, git_blob* blob, const std::filesystem::path& fullpath) noexcept;
  }

  /// @brief Sets a user spdLog::Logger to accomodate for application needs
  /// @param newLogger The new spdlog::Logger
  /// @return the old logger
  std::shared_ptr<spdlog::logger>
  setLogger(std::shared_ptr<spdlog::logger> newLogger) noexcept;

  /// @brief Removes a repository
  /// @param repoFullPath full path to repository 
  /// @return return true if a filesystem repo was removed
  bool 
  cleanRepo(const std::filesystem::path& repoFullPath) noexcept;

  /// @brief Opens or creates a repository, and getting a context to work with
  /// @param fullpath Fullpath to the repository
  /// @param name creator's name, in case of creation the repository's creator will be 'name'. [Optional]
  /// @return On success, a context to work with repository, otherwise an Error
  Result<Context>
  selectRepository(const std::filesystem::path& fullpath, const std::string& name = "") noexcept;

  /// @brief selects a differen branch
  /// @param name the name of the branch to move to
  /// @return On success returns a context for continuation, otherwise an Error
  inline auto selectBranch(const std::string& name) noexcept
  {
    return [&name](Context&& ctx) -> Result<Context> {
      return ni::selectBranch(std::move(ctx), name);
    };
  }

  /// @brief adds a file(blob in git speak) with its content in a  given 'fullpath'
  /// @param fullpath the fullpath including the file name
  /// @param content the files content 
  /// @return On success returns a context for continuation, otherwise an Error
  inline auto add(const std::string& fullpath, const std::string& content) noexcept
  {
    return [&fullpath, &content](Context&& ctx) -> Result<Context> {
      return ni::add(std::move(ctx), fullpath, content);
    };
  }

  /// @brief Removes a file or a direcotry by fullpath
  /// @param fullpath The full path of the file to remove
  /// @return On success returns a context for continuation, otherwise an Error
  inline auto del(const std::string& fullpath) noexcept
  {
    return [&fullpath](Context&& ctx) -> Result<Context> {
      return ni::rm(std::move(ctx), fullpath);
    };
  }

  /// @brief Renames and/or moves a files in the git repository tree 
  /// @param fullpath The full path including the file/direcotry name to move from
  /// @param toFullpath The full path including the new file/directory name to move to
  /// @return On success returns a context for continuation, otherwise an Error
  inline auto mv(const std::string& fullpath, const std::string& toFullpath) noexcept
  {
    return [&fullpath, &toFullpath](Context&& ctx) -> Result<Context> {
      return ni::mv(std::move(ctx), fullpath, toFullpath);
    };
  }

  /// @brief Adds a set of files and their contents 
  /// @param filesAndContents fullpath, content pair set
  /// @return On success returns a context for continuation, otherwise an Error
  inline auto add(const std::set<std::pair<std::string, std::string> >& filesAndContents) noexcept
  {;
    return [&filesAndContents](Context&& ctx) -> Result<Context> {
      Result<Context>  foldingCtx(std::move(ctx));

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

  /// @brief Commit previous updates to the context(Repository/Branch)
  /// @param author commiter's name (assuming commiter == author)
  /// @param email commiter's email
  /// @param message Commit's message
  /// @return On success returns a context for continuation, otherwise an Error
  inline auto commit(const std::string& author, const std::string& email, const std::string& message) noexcept
  {
    return [&author, &email, &message](Context &&ctx) -> Result<Context> {
      return ni::commit(std::move(ctx), author, email, message);
    };
  }

  /// @brief Rollback all updates since last commit 
  /// @return On success returns a context for continuation, otherwise an Error
  inline auto rollback() noexcept
  {
    return [](Context &&ctx) -> Result<Context> {
      return ni::rollback(std::move(ctx));
    };
  }

  /// @brief Branch from a `commitId` 
  /// @param commitId The commit Id to branch from
  /// @param name The new branch's name
  /// @return On success returns a context for continuation, otherwise an Error
  inline auto createBranch(const git_oid* commitId, const std::string& name) noexcept
  {
    return [&commitId, &name](Context &&ctx) -> Result<Context> {
      return ni::createBranch(std::move(ctx), commitId, name);
    };
  }

  /**
   * Reads file content in a given context
   * @param fullpath[string] : Full path of file
   **/
  /// @brief Branch from currnt Context's tip
  /// @param name The new branch's name
  /// @return On success returns a context for continuation, otherwise an Error
  ///
  /// NOTE: Branch creation doesn't change the Context's branch, to do so execute `setBranch`
  inline auto createBranch(const std::string& name) noexcept
  {
    return [&name](Context &&ctx) -> Result<Context> {
      return ni::createBranch(std::move(ctx), name);
    };
  }

  /// @brief Read a file(blob)'s contents 
  /// @param fullpath The fullpath of the file in the repository
  /// @return The file contents as a string
  inline auto read(const std::filesystem::path& fullpath) noexcept
  {
    return [&fullpath](Context&& ctx) -> Result<ReadContext> {
      return ni::read(std::move(ctx), fullpath);
    };
  }

  /// @brief Access support command chaining via the expect primitives of and_then/or_else
  /// shorthand replaces them with the operators >> and || for increased readablity
  namespace shorthand {

    template <typename L, typename F>
    auto operator >>(Result<L>&& lhs, F&& f)
    {
      return std::move(lhs).and_then(std::forward<F>(f));
    }

    template <typename L, typename F>
    auto operator ||(Result<L>&& lhs, F&& f)
    {
      return std::move(lhs).or_else(std::forward<F>(f));
    }

    /**
     * EXPERIMENAL:
     *  ThreadChainingContext enables disjoint gd commands using the last context of the reference.
     *
     *  Example:
     *
     *  While chaining keeps the same context
     *    selectRepository(...) >> selectBranch(...) >> addFile(...) ... >> commit(...) >> read(..)
     *
     * An alernative use can use the `db` that always points at the latest of the local threads repo/ref
     * selectRepository(...);   // Sets `db` thread context 
     * db >> selectBranch(...);
     * db >> addFile(...);
     * db >> rollback();
     *
     ***/
    class ThreadChainingContext {};
    static ThreadChainingContext db;

    Result<Context> getThreadContext() noexcept;

    /**
     * Support for call chaining, 
     *   >>  on successful execution (and_then)
     *   ||  on failure (or_else)
     *   i.e. 
     *      selectRepository(...) >> addFile(...) >> commit(...) || logError(...)
     *   
     *   it's also possible to keep it in varible and chain over multiple lines 
     *    auto ctx = selectRepository(...);
     *    ctx >> addFile(..);
     *    ctx || logError();
     * 
     * Disjoints calls can use the `db` construct that keeps a context per thread, providing context creation via `selectResository`
     *
     *    1.  auto db = selectRepository(...)
     *    2.  db >> addFile(...)
     *    3.  db >> commit(...)
     *
     * Design decision
     *  Chaining a RValue propagates the return value
     *  i.e.  
     *    selectRepository(...) >> addFile(...) ...
     *  
     *  While in the case of aLValue `ctx`
     *     auto ctx = selectRepository
     *   
     *  it is updated
     *     ctx >> addFile(...)
     * 
     *  This can only happen when the Return type `L` is the same as the LValue.
     **/

    /**
     * RValue chaining for operator >> (and_then)
     */
    template <typename F>
    auto operator >>(ThreadChainingContext, F&& f)
    {
      return std::move(getThreadContext()).and_then(std::forward<F>(f));
    }

    /**
     * RValue chaining for operator || (or_else)
     */
    template <typename F>
    auto operator ||(ThreadChainingContext, F&& f)
    {
      return std::move(getThreadContext()).or_else(std::forward<F>(f));
    }

    /**
     * LValue handling for >> (and_then) when the LValue type == L
     */
    template <typename L, typename F>
    auto operator >>(Result<L>& lhs, F&& f) ->
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
     * LValue handling for >> (and_then) when the LValue type != L
     */
    template <typename L, typename F>
    auto operator >>(Result<L>& lhs, F&& f) ->
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

    /**
     * LValue handling for || (or_else) when the LValue type == L
     */
    template <typename L, typename F>
    auto operator ||(Result<L>& lhs, F&& f) ->
    std::enable_if_t<
      std::is_same<
        L,
        typename std::decay_t<decltype(std::move(lhs).or_else(std::forward<F>(f)))>::value_type
      >::value,
      decltype(std::move(lhs).or_else(std::forward<F>(f)))&
    >
    {
      lhs = std::move(lhs).or_else(std::forward<F>(f));
      return lhs;
    }

    /**
     * LValue handling for || (or_else) when the LValue type != L
     */
    template <typename L, typename F>
    auto operator ||(Result<L>& lhs, F&& f) ->
    std::enable_if_t<
      !std::is_same<
        L,
        typename std::decay_t<decltype(std::move(lhs).or_else(std::forward<F>(f)))>::value_type
      >::value,
      decltype(std::move(lhs).or_else(std::forward<F>(f)))
    >
    {
      return std::move(lhs).or_else(std::forward<F>(f));
    }

  }
};

std::ostream& operator<<(std::ostream&, const gd::Error&) noexcept;