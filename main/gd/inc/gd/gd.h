#pragma once

#include <git2.h>

#include <string>
#include <set>
#include <memory>
#include <ostream>
#include <filesystem>
#include <iostream>
#include <map>
#include <err.h>
#include <expected.h>
#include <guard.h>

/**
 * TODO: Consider  RAII.  tradeoffs (flexibility vs explicit cleanup)
 * TODO: Seprate output functions
 * TODO: Debug output support
 * 
 * TODO: Timing and scalabilty checks
 *       - naive, 10,000 files per directory = ~50files/s    100 files per directory = ~1000/s
 *       - index,
 *       - local caching until commit. 
 * TODO: Allow to chain createBranch-> addFile (where the file is added to the new Branch's context)
 * TODO: Convert Error from string -> (errString, errNo, origin)
 *
 * TODO: Support for sharding. 2 years later, what did I mean, what for?
 * TODO: Missing interface
 *         merge
 *         tag
 *
 * TODO: Implement rollback
 * TODO: lift & chain implementation
 * TODO: Tags support
 * TODO: Metadata support (notes)
 * 
 * TODO: Testing, testing and more testing 
 **/
namespace gd
{
  struct context;
  class Object {
    git_oid oid_;
    git_filemode_t mod_;
    std::string name_;
    
    Object(std::string&& name) 
    :name_(name) {}

    static Object create(const std::filesystem::path& fullpath) {
      return Object(fullpath.filename());
    }

    public:
      git_oid const * oid() const noexcept { return &oid_; }
      git_filemode_t mod() const noexcept { return mod_; }
      const std::string&  name() const noexcept { return name_; }

      /** 
       * Creats a blob from content and return an Object representation 
       */
      static Result<Object> 
      createBlob(gd::context& ctx,const std::filesystem::path& fullpath, const std::string& content) noexcept; 

      static Result<Object> 
      createDir(const std::filesystem::path& fullpath, treebuilder_t& bld) noexcept;
  };

  std::ostream& operator<<(std::ostream& os, Object const& ) noexcept;

  struct LongerPathFirst {
    bool operator()(const std::filesystem::path& a, const std::filesystem::path& b) const {
      auto aLen = std::distance(a.begin(), a.end());
      auto bLen = std::distance(b.begin(), b.end());
      return aLen > bLen || a > b;
    }
  };

  class TreeBuilder {
    using Directory = std::filesystem::path;

    using ObjectList = std::vector<Object>;
    using DirectoryObjects = std::pair<Directory, ObjectList>;

    std::map<Directory, ObjectList, LongerPathFirst> dirObjs_;

    /// @brief Inserts a file at a directory.
    /// @param fullpath  Directory owning the object. Example: from/root
    /// @param obj and Object representing a directory or file
    /// Collects objects per dir 
    void insert(const std::filesystem::path& dir, const Object&& obj) noexcept;

    public:
    Result<void> 
    insertFile(gd::context& ctx, const std::filesystem::path& fullpath, const std::string& content) noexcept;

    void del() noexcept {
      /// TODO:
    }

    void mv() noexcept {
      /// TODO:
    }

    Result<gd::tree_t> 
    commit(gd::context& ctx) noexcept;

    void clean() noexcept {
      dirObjs_.clear();
    }

    bool empty() const noexcept {
      return dirObjs_.empty();
    }

    void print(std::ostream&) noexcept;
  };

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
      static Result<gd::context> init(gd::context&& ctx) noexcept;

      
      /**
       * @brief Updates the tip to given commit by commitId
       * @param ctx The old context, with valid repository/reference 
       **/ 
      Result<void> update(gd::context&& ctx, git_oid* commitId) noexcept;

      /// @brief The latest commit of the context 
      commit_t commit_;

      /// @brief Root tree of context 
      tree_t root_;
    };
  };

  /**
   * A context tracking struct, for call chaining 
   * Updates are collected and dumped on commit, 
   * This aggregates all directory updates into one git write.
   **/
  struct context
  {
    context(git_repository* repo,
            const std::string& branch = defaultRef ) noexcept
    : repo_(repo), ref_(branch) {}

    context(context&& other ) noexcept  = default;
    context& operator=(context&& other) noexcept  = default;

    void setRepo(git_repository* repo) noexcept;
    void setBranch(const std::string& fullPathRef) noexcept;

    Result<gd::context> updateCommitTree(const git_oid& treeOid) noexcept;

    git_repository* repo_;     /*  git repository             */
    std::string ref_;          /*  git reference (branch tip) */
    TreeBuilder updates_;      /*  Collects updates           */

    internal::Node tip_;       /* Internal call chaining information */
  };

  namespace ni
  {
    Result<context> selectBranch(context&& ctx, const std::string& name) noexcept;
    Result<context> add(context&& ctx, const std::filesystem::path& fullpath, const std::string& content) noexcept;
    Result<context> del(context&& ctx, const std::string& fullpath) noexcept;
    Result<context> mv(context&& ctx, const std::string& fullpath, const std::string& toFullpath) noexcept;
    Result<context> createBranch(context&& ctx, const git_oid* commitId, const std::string& name) noexcept;
    Result<context> createBranch(context&& ctx, const std::string& name) noexcept;
    Result<context> commit(context&& ctx, const std::string& author, const std::string& email, const std::string& message) noexcept;
    Result<context> rollback(context&& ctx) noexcept;

    Result<std::string> read(context&& ctx, const std::string& fullpath) noexcept;
  }

  bool cleanRepo(const std::string& repoFullPath) noexcept;

 /**
  * selecting a repository for chained calls
  *
  * @param repo[Repository] : The repository to work with
  **/
  Result<context>
  selectRepository(const std::string& fullpath, const std::string& name = "") noexcept;

 /**
  * selecting a branch for chained calles
  *
  * @param name[std::string] : The branch's name
  **/
  inline auto selectBranch(const std::string& name) noexcept
  {
    return [&name](context&& ctx) -> Result<context> {
      return ni::selectBranch(std::move(ctx), name);
    };
  }

 /**
  * Adds a file/revision in the current context (repo, branch)
  *
  * @param fullpath[std::string] : File name full path (relative to root)
  * @param content[std::string]  : File contents blob
  **/
  inline auto add(const std::string& fullpath, const std::string& content) noexcept
  {
    return [&fullpath, &content](context&& ctx) -> Result<context> {
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
    return [&fullpath](context&& ctx) -> Result<context> {
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
    return [&fullpath, &toFullpath](context&& ctx) -> Result<context> {
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
    return [&filesAndContents](context&& ctx) -> Result<context> {
      Result<context>  foldingCtx(std::move(ctx));

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
    return [&author, &email, &message](context&& ctx) -> Result<context>  {
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
    return [](context&& ctx) -> Result<context>  {
       return ni::rollback(std::move(ctx));
    };
  }
  /**
   * Creates a branch in a given repository, from a given commitId 
   *
   * @param commitId[git_oid] : The originating commit for the branch
   * @param name[std::string] : Branch name
   **/
  inline auto createBranch(const git_oid* commitId, const std::string& name) noexcept
  {
     return [&commitId, &name](context&& ctx) -> Result<context> {
       return ni::createBranch(std::move(ctx), commitId, name);
     };
  }

  /**
   * Creates a new branch in a given repository, from the context's tip 
   *
   * @param name[std::string] : Branch name
   **/
  inline auto createBranch(const std::string& name) noexcept
  {
     return [&name](context&& ctx) -> Result<context> {
       return ni::createBranch(std::move(ctx), name);
     };
  }

  /**
   * Reads file content in a given context
   * @param fullpath[string] : Full path of file
   **/
  inline auto read(const std::string& fullpath) noexcept
  {
    return [&fullpath](context&& ctx) -> Result<std::string> {
      return ni::read(std::move(ctx), fullpath);
    };
  }

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

    Result<context> getThreadContext() noexcept;

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