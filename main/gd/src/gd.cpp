#include <gd/gd.h>
#include <pathTraverse.h>
#include <ranges>

#include <mutex>
#include <iostream>
#include <shared_mutex>
#include <algorithm>
#include <expected.h>
#ifdef DEBUG_OUTPUT
# include <debug.h>
#endif 

using namespace std::ranges;

namespace gd {
  /// TODO: Output update, re-factor outout operators and update appropriate erros/debug output
  std::ostream& operator<<(std::ostream& os, const Error& e) noexcept {
    return os << (std::string)e;
  }
  
  std::ostream& operator<<(std::ostream& os, Object const& obj) noexcept {
    const static std::unordered_map<git_filemode_t, char const * const>  s = {
      {GIT_FILEMODE_UNREADABLE,       "UNREAD"},
      {GIT_FILEMODE_TREE,             "TREE  "},
      {GIT_FILEMODE_BLOB,             "BLOB  "},
      {GIT_FILEMODE_BLOB_EXECUTABLE,  "EXEC  "},
      {GIT_FILEMODE_LINK,             "LINK  "},
      {GIT_FILEMODE_COMMIT,           "COMMIT"}
    };

    const static std::unordered_map<git_object_t, char const * const> ss = {
      {GIT_OBJECT_ANY,               "ANY    "},
      {GIT_OBJECT_INVALID,           "INVALID"},
      {GIT_OBJECT_COMMIT,            "COMMIT "},
      {GIT_OBJECT_TREE,              "TREE   "},
      {GIT_OBJECT_BLOB,              "BLOB   "},
      {GIT_OBJECT_TAG,               "TAG    "},
      {GIT_OBJECT_OFS_DELTA,         "OFS Δ  "},
      {GIT_OBJECT_REF_DELTA,         "REF Δ  "}
    };

    os << obj.oid() << "  ";
    if (auto i = s.find(obj.mod()); i == s.end()) {
      os << "????";
    } else {
      os << i->second;
    }
    return os << "  " << obj.name();
  }
}

namespace {
  static char const * const sNoRepositoryError{"No Repository selected"};

  /**
   * Git accesssor abstraction
   * It's main use is for initialization and RAII handling of cached repos and git
   *
   * Repositories are cached for the duration of the application (344 bytes)
   *
   * NOTE: Only one GitGaurd in the system initialized at static time and destructed
   * on shutdown
   * TODO: Consolidate with Guard
   **/
  class GitGuard
  {
    public:
    GitGuard()
    {
      git_libgit2_init();
    }

    ~GitGuard() {
      std::lock_guard<std::shared_mutex> lock(cacheAccess_);

      for (auto& [_, repo] : repoCache_)
        git_repository_free(repo);

      git_libgit2_shutdown();
      repoCache_.clear();
    }

    /**
     * Cache git_repository for the duration of the application
     **/
    template<typename... Ts>
    void cacheRepo(Ts&&... args)
    {
      std::lock_guard<std::shared_mutex> lock(cacheAccess_);
      auto [itr, _] = repoCache_.emplace(std::forward<Ts>(args)...);
      ctx_.setRepo(itr->second);
    }

    /**
     * Returns a pair (bool, git_repository*)
     *  bool             : true if the repository exists
     *  git_repository*  : if the repository exists, a Pointer to libgit2 git_repository*, otherwise nullptr
     * TODO: Update to Expected
     **/
    std::pair<bool, git_repository*>
    getRepo(const std::string& repoFullPath)
    {
      std::shared_lock<std::shared_mutex> guard(cacheAccess_);
      if (auto itr{repoCache_.find(repoFullPath)}; itr != repoCache_.end() )
      {
        ctx_.setRepo(itr->second);
        return {true, itr->second};
      }

      return {false, nullptr};
    }

    /**
     * Removes directory 'repoFullPath' if exists
     * 
    * Returns true if repo existed
    * Otherwise false
    */
    bool
    cleanRepo(const std::string& repoFullPath) noexcept
    {
      bool removed = false;
      std::shared_lock<std::shared_mutex> guard(cacheAccess_);
      if (auto itr{repoCache_.find(repoFullPath)}; itr != repoCache_.end() ) {
        git_repository_free(itr->second);
        repoCache_.erase(repoFullPath);
        removed = true;
      }
      std::filesystem::remove_all(repoFullPath);;
      return removed;
    }

    Result<gd::context> threadContext() const noexcept
    {
      if (not ctx_.repo_) 
        return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

      return gd::internal::Node::init(gd::context(ctx_.repo_, ctx_.ref_));
    }

    void setThreadBranch(const std::string& fullPathRef) noexcept
    {
      ctx_.setBranch(fullPathRef);
    }

    private:
    std::shared_mutex cacheAccess_;
    std::unordered_map<std::string, git_repository*>  repoCache_;
    static thread_local gd::context ctx_; // Can be replaced by repo/ref
  };


  static GitGuard sGit;                                 // Git representation
  static std::string sBranchRefRoot{ "refs/heads/" };   // Relative location to git references
  static std::string sHead{ gd::defaultRef };           // Default reference when not indicated by user
  thread_local gd::context GitGuard::ctx_{nullptr};     // Implicit context per thread


  /**
   * Create a repository at a given path with a given name
   * The repository is owned by the cache and as long as the cache is alive (No cleanup was called)
   * It is safe to use the the raw pointer to the git repository.
   *
   * The cache is introduced to avoid paying for git repository opening for every call
   * as well as support a default context, when no user context is defined the commands
   * will be executed in the last valid context.
   *
   * A context is the pair of <repository, branch>
   **/
  Result<gd::context>
  createRepo(const std::string& fullpath, const std::string& name) noexcept
  {
    git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;

    opts.flags |= GIT_REPOSITORY_INIT_MKPATH; /* mkdir as needed to create repo */
    opts.flags |= GIT_REPOSITORY_INIT_BARE ;  /* Bare repository                */
    opts.description = name.data();           /* User given name                */
    opts.initial_head = "main";               /* Main branch istead of master   */

    git_repository* repo;
    if(git_repository_init_ext(&repo, fullpath.data(), &opts) != 0)
      return unexpected_git;

    sGit.cacheRepo(fullpath, repo);
    return gd::context{repo, sHead};;
  }


  /**
   * Returns true if a repository already exists, otherwise false
   **/
  bool repoExists(const std::string& repoPath) noexcept
  {
    return git_repository_open_ext(nullptr, repoPath.data(), GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr) == 0;
  }

  Result<gd::context>
  connectToRepo(const std::string& fullpath)
  {
    git_repository* repo;
    if (git_repository_open_ext(&repo, fullpath.data(), GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr) != 0)
      return unexpected_git;

    sGit.cacheRepo(fullpath, repo);
    return gd::context{repo, sHead};;
  }

}


Result<gd::Object> 
gd::Object::createBlob(gd::context& ctx,const std::filesystem::path& fullpath, const std::string& content) noexcept {
  Object blob { create(fullpath) };
  blob.mod_= GIT_FILEMODE_BLOB;
  if (git_blob_create_from_buffer(&blob.oid_, ctx.repo_, content.data(), content.size()) != 0)
    return unexpected_git;

  return std::move(blob);
}

Result<gd::Object> 
gd::Object::createDir(const std::filesystem::path& fullpath, treebuilder_t& bld) noexcept {
  Object dir { create(fullpath) };
  if(git_treebuilder_write(&dir.oid_, bld) != 0)
    return unexpected_git;

  dir.mod_= GIT_FILEMODE_TREE;

  return std::move(dir);
}

void gd::TreeBuilder::insert(const std::filesystem::path& dir, const Object&& obj) noexcept {
  if (auto itr = dirObjs_.find(dir); itr != dirObjs_.end()) {
    itr->second.emplace_back(std::move(obj));
  } else {
    dirObjs_.emplace( dir, ObjectList(1, obj) );
  }
}


Result<void> 
gd::TreeBuilder::insertFile(gd::context& ctx, const std::filesystem::path& fullpath, const std::string& content) noexcept {
  auto blobResult = Object::createBlob(ctx, fullpath, content);
  if (!blobResult) 
    return unexpected_git;

  insert( fullpath.parent_path().relative_path(), std::move(*blobResult) );
  return Result<void>();
}

Result<gd::tree_t> 
gd::TreeBuilder::commit(gd::context& ctx) noexcept {
  git_oid const * treeOid = nullptr;

  for (const auto& [dir, objs]: dirObjs_) {
    bool isRootDir = dir.empty();

    auto tree = getTreeRelativeToRoot(ctx.repo_, ctx.tip_.root_, dir);
    if (!tree)
      return unexpected_err(tree.error());

    auto bld = getTreeBuilder(ctx.repo_, isRootDir ? ctx.tip_.root_ : *tree);
    if (!bld)
      return unexpected_err(bld.error());

    for ( auto& obj : objs )  {
      if(git_treebuilder_insert(nullptr, *bld, obj.name().data(), obj.oid(), obj.mod()) != 0)
        return unexpected_git;
    }

    if (auto parentDir = Object::createDir(dir, *bld); !parentDir) {
      return unexpected_err(parentDir.error()); 
    } else {
      treeOid = parentDir->oid();
      // Add containing directory to commit
      if (!isRootDir) 
        insert(dir.parent_path(), std::move(*parentDir));
    }
  }

  if (treeOid == nullptr) 
    return unexpected(gd::ErrorType::EmptyCommit, "No updates made");

  dirObjs_.clear(); // The updates are only on success 
  return getTree(ctx.repo_, treeOid);
}

/*******************************************************************************
 *                             internal::context
 * Context for chaining calls, namely repository and branch
 *******************************************************************************/
void gd::context::setRepo(git_repository* repo) noexcept
{
  repo_ = repo;
  ref_ = sHead;
}

void gd::context::setBranch(const std::string& fullPathRef) noexcept
{
  ref_ = fullPathRef;
}

/*******************************************************************************
 *                             internal::Node
 * A Node is specific location on the DAG and a reference to the root tree
 *******************************************************************************/
gd::internal::Node::Node(Node&& other) noexcept
: commit_(std::move(other.commit_)), 
  root_(std::move(other.root_)) 
{ }

gd::internal::Node& gd::internal::Node::operator=(Node&& other) noexcept
{
  if (this != &other) {
    commit_ = std::move(other.commit_);
    root_ = std::move(other.root_);
  }
  return *this;
}

Result<gd::context>
gd::internal::Node::init(gd::context&& ctx) noexcept
{
  if (auto commit = getCommitByRef(ctx.repo_, ctx.ref_); !commit) {
    return unexpected_err(commit.error());
  } else {
    ctx.tip_.commit_ = std::move(*commit);
  }
    
  if (auto tree = getTreeOfCommit(ctx.repo_, ctx.tip_.commit_); !tree) {
    return unexpected_err(tree.error());
  } else {
    ctx.tip_.root_ = std::move(*tree);
  }
  return std::move(ctx);
}

Result<void>
gd::internal::Node::update(gd::context&& ctx, git_oid* commitId) noexcept {

  auto commit = getCommitById(ctx.repo_, commitId);
  if (!commit) 
    return unexpected_err(commit.error());

  auto tree = getTreeOfCommit(ctx.repo_, *commit);
  if (!tree)
    return unexpected_err(tree.error());
    
  commit_ = std::move(*commit);
  root_ = std::move(*tree);

  return Result<void>();
}


/*******************************************************************************
 *                            Interface implementation
 *******************************************************************************/

/**
 * Removes the repo from the cache if it exists
 * cleans up its git state and removes the directory
 */
 bool 
 gd::cleanRepo(const std::string& repoFullPath) noexcept {
   return sGit.cleanRepo(repoFullPath);
 }

Result<gd::context>
gd::selectRepository(const std::string& fullpath, const std::string& name) noexcept
{
  if (auto [exists, pRepo] =  sGit.getRepo(fullpath); exists)
    return gd::context(pRepo, sHead);

  if (repoExists(fullpath))
    return connectToRepo(fullpath);

  return createRepo(fullpath, name);
}


Result<gd::context>
gd::ni::selectBranch(gd::context&& ctx, const std::string& name) noexcept
{
  if (not ctx.repo_) return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

  std::string ref = sBranchRefRoot + name;

  sGit.setThreadBranch(ref);
  ctx.ref_ =  std::move(ref);

  return internal::Node::init(std::move(ctx));
}


Result<gd::context>
gd::ni::add(gd::context&& ctx, const std::filesystem::path& fullpath, const std::string& content) noexcept
{
  if (not ctx.repo_)
    return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

  auto res = ctx.updates_.insertFile(ctx, fullpath, content);
  if (!res) 
    return unexpected_git;

  return std::move(ctx);
}


Result<gd::context>
gd::ni::del(gd::context&& ctx, const std::string& fullpath) noexcept
{
  // TODO!!!!!
  /*
  if (not ctx.repo_)
    return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

  git_tree_update update;
  update.action = GIT_TREE_UPDATE_REMOVE;
  update.filemode = GIT_FILEMODE_BLOB;
  update.path = fullpath.data();

  git_object* obj;
  if( git_object_lookup_bypath(&obj, reinterpret_cast<git_object*>(ctx.tip_.tree_), fullpath.data(), GIT_OBJECT_BLOB) != 0)
    return unexpected_git;

  git_oid_cpy(&update.id,  git_object_id(obj));
  git_object_free(obj);

  git_oid newTree;
  if (git_tree_create_updated(&newTree, ctx.repo_, ctx.tip_.tree_, 1, &update) != 0)
    return unexpected_git;

  ctx.dirty_ = true;
  return ctx.updateCommitTree(newTree);
*/
  return std::move(ctx);
}


Result<gd::context>
gd::ni::mv(gd::context&& ctx, const std::string& fullpath, const std::string& toFullPath) noexcept
{
  // TODO!!!!!
  /*
  if (not ctx.repo_)
    return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

  // Remove File
  git_tree_update update[2];
  update[0].action = GIT_TREE_UPDATE_REMOVE;
  update[0].filemode = GIT_FILEMODE_BLOB;
  update[0].path = fullpath.data();

  update[1].action  = GIT_TREE_UPDATE_UPSERT;
  update[1].filemode = GIT_FILEMODE_BLOB;
  update[1].path = toFullPath.data();

  git_object* obj;
  if( git_object_lookup_bypath(&obj, reinterpret_cast<git_object*>(ctx.tip_.tree_), fullpath.data(), GIT_OBJECT_BLOB) != 0)
    return unexpected_git;

  git_oid_cpy(&update[0].id,  git_object_id(obj));
  git_oid_cpy(&update[1].id,  git_object_id(obj));
  git_object_free(obj);

  git_oid newTree;
  if (git_tree_create_updated(&newTree, ctx.repo_, ctx.tip_.tree_, 2, update) != 0)
    return unexpected_git;

  ctx.dirty_ = true;
  return ctx.updateCommitTree(newTree);
  */
  return std::move(ctx);
}

/// @brief Commits collected updats  
/// @param ctx  
/// @param message 
/// @return  On success propagates the context, otherwise an Error
/// TODO: Commit is now the end of the chain 
//// It doesn't propagate a context and thus it doesn't update the tip
//// The `commitId [:git_oid] is used to create branches
//// But createBranch actually requires git_commit
////
//// If we need to convert git_commit to git_oid
//// git_oid oid;
////    git_commit_id(commit, &oid);
//// TODO: Update commit to continue chaining
////       Including createBranch

Result<gd::context>
gd::ni::commit(gd::context&& ctx, const std::string& author, const std::string& email, const std::string& message) noexcept
{
  /// TODO: Commit is the only location where contention can happen when multiple threads
  ///       Update the git repository. Needs to be serialized !!!!
  if (ctx.updates_.empty()) 
    return unexpected(gd::ErrorType::EmptyCommit, "Nothing to commit");

  auto newRoot = ctx.updates_.commit(ctx);
  if (!newRoot)
    return unexpected_err(newRoot.error());

  auto commiter = getSignature(author, email);
  if (!commiter)
    return unexpected_err(commiter.error());

  git_commit const *parents[1]{  ctx.tip_.commit_ };

  git_oid commitId;
  int result = git_commit_create(
      &commitId,
      ctx.repo_,
      ctx.ref_.data(), /* name of ref      */
      *commiter,       /* author           */
      *commiter,       /* committer        */
      "UTF-8",         /* message encoding */
      message.data(),  /* message          */
      *newRoot,        /* root tree        */
      1,               /* parent count     */
      parents);        /* parents          */

  if (result != 0)
    return unexpected_git;

  if (auto res = ctx.tip_.update(ctx.repo_, &commitId); !res) 
    return unexpected_err(res.error());

  return std::move(ctx);
}


Result<gd::context>
gd::ni::rollback(gd::context&& ctx) noexcept
{
  ctx.updates_.clean();
  return std::move(ctx);
}

/**
 * Creates a new branch from any commitId
 */
Result<gd::context>
gd::ni::createBranch(gd::context&& ctx, const git_oid* commitId, const std::string& name) noexcept
{
  auto commit = getCommitById(ctx.repo_, commitId );
  if (*commit)
    return unexpected_err(commit.error());

  auto branchRef = createBranch(ctx.repo_, name, ctx.tip_.commit_);
  if (!branchRef)
    return unexpected_err(branchRef.error());

  return std::move(ctx);
}

Result<gd::context>
gd::ni::createBranch(gd::context&& ctx, const std::string& name) noexcept
{
  auto branchRef = createBranch(ctx.repo_, name, ctx.tip_.commit_);
  if (!branchRef)
    return unexpected_err(branchRef.error());

  return std::move(ctx);
}

Result<std::string>
gd::ni::read(gd::context&& ctx, const std::string& fullpath) noexcept
{
  auto blob = getBlobFromTreeByPath(ctx.tip_.root_, fullpath);
  if (!blob)
    return unexpected_err(blob.error());

  git_blob_filter_options opts = GIT_BLOB_FILTER_OPTIONS_INIT;

  git_buf buffer = GIT_BUF_INIT_CONST("", 0);
  if (git_blob_filter(&buffer, *blob, fullpath.data(), &opts) != 0)
    return unexpected_git;

  std::string content(buffer.ptr, buffer.size);

  git_buf_dispose(&buffer);
  git_buf_free(&buffer);

  return  content;
}

Result<gd::context>
gd::shorthand::getThreadContext() noexcept
{
  return sGit.threadContext();
}
