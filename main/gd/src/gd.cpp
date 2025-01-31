#include <gd/gd.h>
#include <pathTraverse.h>
#include <ranges>

#include <mutex>
#include <iostream>
#include <shared_mutex>
#include <algorithm>
#include <expected.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>
#include <out.h>


using namespace std::ranges;

std::ostream& operator<<(std::ostream& os, const gd::Error& e) noexcept {
  return os << (std::string)e;
}

/// @brief An anonymous namespace to keep some implementation details, locally
namespace {
  static char const * const sNoRepositoryError{"No Repository selected"};

  /**
   * Git accesssor abstraction
   * - Initializes git2 library on startup, and release it on shutdown
   * - Caches open repositories, for the entire duration of the application
   * - Cache entries are serialized, multiple threads can use the same cached repositories simultanously.
   * - On shutdown open repositories are released.
   * - [Experimental] Support for thread connection. Once a thread connectes to a repository, there is no need to pass the 
   *   connection as a argument to lower levels of the application, it's possible to use thread local storage
   *   to access the thread context 
   *   IMPORTANT: thread local storage is experimental, some of the semantics may change 
   *   It was introduced to allow disjoint access to a context within the same thread, without the need for sharing a connection
   *   but the required semantics are not yet thought through, and emerging semantics 
   *   a. Should the user allow to close it
   *   b. What should happen on failure, failed context is a dead-end, so it should be reset, by user? automatically?
   *   c. How should it work with same task on multiple threads 
   *   ...
   *   In short it's namely experimental, to identify emerging semantics.
   * 
   *  NOTE: While only one instance is in use, It is not a singlton by design.
   *  In most applications the singlton is superflous, and in many applications it's limiting, or even hazardous (= global static)
   *  While code should be as explicit as possible. The cardinality requirements were not yet fully flushed out.
   **/
  class GitAccess
  {
    public:
    GitAccess() { git_libgit2_init(); }

    ~GitAccess() {
      std::lock_guard<std::shared_mutex> lock(cacheAccess_);
      repoCache_.clear();
      git_libgit2_shutdown();
    }

    /// @brief Cache a repository for the duration of the application
    /// @param ...args path, repositry_t pair to be kept in a hash table
    /// @return A pointer to repository_t, the pointer is guaranted to be safe to use as repositories are open for the entire life time of the application.  
    template<typename... Ts>
    gd::repository_t* cacheRepo(Ts&&... args)
    {
      std::lock_guard<std::shared_mutex> lock(cacheAccess_);
      auto [itr, _] = repoCache_.emplace(std::forward<Ts>(args)...);
      ctx_.setRepo(&itr->second);
      return &itr->second;
    }

    /// @brief Returns a pointer to cached open repository
    /// @param repoFullPath  full path to the repository on the file system
    /// @return a pointer to an already opend repository, or an error, when such repository wasn't opened/created yet.
    Result<gd::repository_t*>
    getRepo(const std::string& repoFullPath)
    {
      std::shared_lock<std::shared_mutex> guard(cacheAccess_);
      if (auto itr{repoCache_.find(repoFullPath)}; itr != repoCache_.end() )
      {
        ctx_.setRepo(&itr->second);
        return &itr->second;
      }

      return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);
    }

    /// @brief Removes directory 'repoFullPath' if exists
    /// @param repoFullPath Path to the Repository
    /// @return True if the repository existed, otherwise false
    bool
    cleanRepo(const std::filesystem::path& repoFullPath) noexcept
    {
      bool removed = false;
      std::shared_lock<std::shared_mutex> guard(cacheAccess_);
      if (auto itr{repoCache_.find(repoFullPath)}; itr != repoCache_.end() ) {
        repoCache_.erase(repoFullPath);
        removed = true;
      }
      std::filesystem::remove_all(repoFullPath);;
      return removed;
    }

    /// @brief Used to retrieve thread Context anywhere in the application 
    /// @return The context of the thread, or an Error if the context failed anywhere in the previous calls
    Result<gd::Context> threadContext() const noexcept
    {
      if (not ctx_.repo_) 
        return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

      return gd::internal::Node::init(gd::Context(ctx_.repo_, ctx_.ref_));
    }

    /// @brief Switches the context to a new reference
    /// @param fullPathRef The full path reference that can be found in the `refs` directory
    void setThreadBranch(const std::string& fullPathRef) noexcept
    {
      ctx_.setBranch(fullPathRef);
    }

    private:
    std::shared_mutex cacheAccess_;
    std::unordered_map<std::filesystem::path, gd::repository_t>  repoCache_;
    static thread_local gd::Context ctx_;
  };

  static GitAccess sGit;                                // Git representation
  static std::string sBranchRefRoot{ "refs/heads/" };   // Relative location to git references
  static std::string sHead{ gd::defaultRef };           // Default reference when not indicated by user
  thread_local gd::Context GitAccess::ctx_{nullptr};    // Implicit context per thread

  static std::shared_ptr<spdlog::logger> sLogger{ spdlog::null_logger_mt("No Logger") };


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
  Result<gd::Context>
  createRepo(const std::string& fullpath, const std::string& name) noexcept
  {
    auto repo = createRepository(fullpath, name);
    if (!repo)
      return unexpected_err(repo.error());

    auto pRepo = sGit.cacheRepo(fullpath, std::move(*repo));

    sLogger->debug("Created repository {} with creator '{}'", fullpath, name);
    return gd::Context{pRepo, sHead};;
  }

  /**
   * Returns true if a repository already exists, otherwise false
   **/
  /// @brief tests for repository existance on file system 
  /// @param repoPath full path of a repository on a files system
  /// @return True if a git repository exists in `repoPath` location otherwise, false.
  bool repoExists(const std::filesystem::path& repoPath) noexcept
  {
    return git_repository_open_ext(nullptr, repoPath.c_str(), GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr) == 0;
  }

  /// @brief Opens an existing directory for use by the application 
  /// @param fullpath Full path to the filesystem repository location
  /// @return A context that can be used to access the repository if successful, otherwise an Error
  Result<gd::Context>
  connectToRepo(const std::filesystem::path& fullpath)
  {
    auto repo = openRepository(fullpath);
    if (!repo)
      return unexpected_err(repo.error());

    auto pRepo = sGit.cacheRepo(fullpath, std::move(*repo));

    sLogger->debug("Connected to repository {}", fullpath);
    auto ctx = gd::Context{pRepo, sHead};
    return ctx;
  }
}

/*******************************************************************************
 *                             internal::Object
 *            Each update is composed of a specific Object
 *******************************************************************************/
Result<gd::ObjectUpdate> 
gd::ObjectUpdate::createBlob(gd::Context& ctx,const std::filesystem::path& fullpath, const std::string& content) noexcept {
  ObjectUpdate blob { create(fullpath, GIT_FILEMODE_BLOB,  &gd::ObjectUpdate::insert) };
  if (git_blob_create_from_buffer(&blob.oid_, *ctx.repo_, content.c_str(), content.size()) != 0)
    return unexpected_git;

  sLogger->debug("Blob created {}: {}", fullpath, blob.oid_);
  return std::move(blob);
}

Result<gd::ObjectUpdate> 
gd::ObjectUpdate::fromEntry(gd::Context& ctx,const std::filesystem::path& fullpath, const git_tree_entry* entry) noexcept {
  auto oid = git_tree_entry_id(entry);
  auto mod = git_tree_entry_filemode(entry);

  ObjectUpdate elem { create(fullpath, mod,  &gd::ObjectUpdate::insert) };
  if (git_oid_cpy(&elem.oid_, oid) !=  0)
    return unexpected_git;

  sLogger->debug("Created {} from {} with {}", fullpath, elem.oid_, mod);
  return std::move(elem);
}

Result<gd::ObjectUpdate> 
gd::ObjectUpdate::remove(const std::filesystem::path& fullpath) noexcept {
  ObjectUpdate removed { create(fullpath, GIT_FILEMODE_UNREADABLE /* Ingored */, &gd::ObjectUpdate::remove ) };

  sLogger->debug("Removed {}", fullpath);
  return std::move(removed);
}

Result<void>
gd::ObjectUpdate::gitIt(git_treebuilder* bld) const noexcept {
  if (auto res = (this->*action_)(bld); !res)
    return unexpected_err(res.error());
  
  return Result<void>();
}

/// @brief creates a directory (Gitspeak for a Tree) 
/// @param fullpath full path of the directory in the repository include its name
/// @param bld A Builder for the Tree (=dir) the new directory is to be added to 
/// @return On success an Object representation of the git Tree, otherwise an Error
Result<gd::ObjectUpdate> 
gd::ObjectUpdate::createDir(const std::filesystem::path& fullpath, treebuilder_t& bld) noexcept {
  ObjectUpdate dir { create(fullpath, GIT_FILEMODE_TREE, &gd::ObjectUpdate::insert) };
  if(git_treebuilder_write(&dir.oid_, bld) != 0)
    return unexpected_git;

  sLogger->debug("Create directoy '/{}'", fullpath);
  return std::move(dir);
}

/*******************************************************************************
 *                             internal::TreeBuilder
 *                  Collect updates per directory to be written on commit
 *******************************************************************************/
void 
gd::TreeCollector::insert(const std::filesystem::path& fullpath, const ObjectUpdate&& obj) noexcept {
  if (auto itr = dirObjs_.find(fullpath); itr != dirObjs_.end()) {
    itr->second.emplace_back(std::move(obj));
    sLogger->debug("TreeCollector: '{}' update added to directory /{}", obj.name(), fullpath) ;
  } else {
    dirObjs_.emplace( fullpath, ObjectList(1, obj) );
    sLogger->debug("TreeCollector: '{}' update added to new directory /{}" , obj.name(), fullpath);
  }
}

Result<void> 
gd::TreeCollector::insertFile(gd::Context& ctx, const std::filesystem::path& fullpath, const std::string& content) noexcept {
  auto blobResult = ObjectUpdate::createBlob(ctx, fullpath, content);
  if (!blobResult) 
    return unexpected_git;

  insert( fullpath.parent_path().relative_path(), std::move(*blobResult) );
  return Result<void>();
}

Result<void> 
gd::TreeCollector::insertEntry(gd::Context& ctx, const std::filesystem::path& fullpath, const git_tree_entry* entry ) noexcept {
  auto blobResult = ObjectUpdate::fromEntry(ctx, fullpath, entry);

  insert( fullpath.parent_path().relative_path(), std::move(*blobResult) );
  return Result<void>();
}

Result<void>
gd::TreeCollector::removeFile(gd::Context& ctx, const std::filesystem::path& fullpath) noexcept {
  auto removed = ObjectUpdate::remove(fullpath);

  insert( fullpath.parent_path().relative_path(), std::move(*removed) );
  return Result<void>();
}

/// @brief Writes all the collected updates to git
/// @param ctx the context used to access the repository
/// @return On success returns RAII flavoured git_tree which is the new root tree containing updates, otherwise an Error.
Result<gd::tree_t> 
gd::TreeCollector::apply(gd::Context& ctx) noexcept {
  git_oid const * treeOid = nullptr;

  for (const auto& [dir, objs]: dirObjs_) {
    bool isRootDir = dir.empty();

    sLogger->debug("Apply: Processing directory '/{}' ({} elements)", dir, objs.size());
    auto tree = getTreeRelativeToRoot(*ctx.repo_, ctx.tip_.root_, dir);
    if (!tree)
      return unexpected_err(tree.error());

    auto bld = getTreeBuilder(*ctx.repo_, isRootDir ? ctx.tip_.root_ : *tree);
    if (!bld)
      return unexpected_err(bld.error());

    for ( auto& obj : objs )  {
      if (auto res = obj.gitIt(*bld); !res)
        return unexpected_err(res.error());
    }

    if (auto parentDir = ObjectUpdate::createDir(dir, *bld); !parentDir) {
      return unexpected_err(parentDir.error()); 
    } else {
      treeOid = parentDir->oid();
      
      if (!isRootDir) { 
        insert(dir.parent_path(), std::move(*parentDir));
      }
    }
  }

  if (treeOid == nullptr) 
    return unexpected(gd::ErrorType::EmptyCommit, "No updates made");

  dirObjs_.clear(); // Clear updates only on success 
  return getTree(*ctx.repo_, treeOid);
}

// TODO: If the order of application of dirObjs_ is irrelevant its type can be replace to a hash or a map
//       In which case only the last action of an element will be collected and retrieval will be O(1) or O(log n)
//       as opposed to the now O(n) retrival that can affect very big transactions in the same directory
//       Even if the order is significant, an addtional hash could be introduced for the same effect.
Result<git_blob*> 
gd::TreeCollector::getBlobByPath(const gd::Context& ctx, const std::filesystem::path& fullpath) const noexcept  {
  
  auto dir = fullpath.parent_path();
  auto name = fullpath.filename();

  auto dirObjsIdx = dirObjs_.find(dir);
  if (dirObjsIdx == dirObjs_.end()) 
    return unexpected(gd::ErrorType::BadDir, "not found in current context");

  const auto& [_, objList] = *dirObjsIdx;
  for(int i=objList.size() - 1; i >=0; --i ) {
    const auto obj = objList[i];
    if (obj.name() == name)  {
      if (obj.isDelete())
        return unexpected(gd::ErrorType::Deleted, "File deleted in uncommitted context") 
      else 
        return getBlobById(*ctx.repo_, obj.oid());
    }
  }

  return unexpected(gd::ErrorType::NotFound, "No update found in uncommitted context");
}

/*******************************************************************************
 *                             internal::context
 *           Context for chaining calls, namely repository and branch
 *******************************************************************************/
void gd::Context::setRepo(gd::repository_t* repo) noexcept
{
  repo_ = repo;
  ref_ = sHead;
}

void gd::Context::setBranch(const std::string& fullPathRef) noexcept
{
  /// TODO: Does `tip` need an update
  ref_ = fullPathRef;
}

git_oid const *
gd::Context::getCommitId() const noexcept {
  return tip_.commitId_;
}

/*******************************************************************************
 *                             internal::Node
 * A Node is specific location on the DAG and a reference to the root tree
 *******************************************************************************/
gd::internal::Node::Node(Node&& other) noexcept
: commit_(std::move(other.commit_)), 
  root_(std::move(other.root_)),
  commitId_(std::move(other.commitId_))
{ 
  other.commitId_ = nullptr;
}

/// @brief Move assignement operator
/// @param other Moved Node
/// @return a reference to Node for chained assignements
gd::internal::Node& gd::internal::Node::operator=(Node&& other) noexcept
{
  if (this != &other) {
    commitId_ = std::move(other.commitId_);
    commit_ = std::move(other.commit_);
    root_ = std::move(other.root_);
  }
  return *this;
}

/// @brief Initialize 
/// @param ctx the context used to access the repository
/// @return On sucess a context for chaining, otherwise an Error
Result<gd::Context>
gd::internal::Node::init(gd::Context&& ctx) noexcept
{
  if (auto commit = getCommitByRef(*ctx.repo_, ctx.ref_); !commit) {
    return unexpected_err(commit.error());
  } else {
    ctx.tip_.commit_ = std::move(*commit);
  }

  ctx.tip_.commitId_ = git_commit_id(ctx.tip_.commit_);
    
  if (auto tree = getTreeOfCommit(*ctx.repo_, ctx.tip_.commit_); !tree) {
    return unexpected_err(tree.error());
  } else {
    ctx.tip_.root_ = std::move(*tree);
  }
  return std::move(ctx);
}

/// @brief Syncs the node with a `git_oid` of type git_commit 
/// @param ctx the context used to access the repository
/// @param commitId The commit id to sync with.
/// @return nothing on success, otherwise an Error
/// @todo Maybe short circuiting the call if commitId == to current (This may require adding git_oid* to state)
///
/// Prerequisites: the git_oid is of type git_commit for an existing commit, or an error will be returned
Result<void>
gd::internal::Node::update(const gd::Context& ctx, git_oid const* commitId) noexcept {

  auto commit = getCommitById(*ctx.repo_, commitId);
  if (!commit) 
    return unexpected_err(commit.error());

  auto tree = getTreeOfCommit(*ctx.repo_, *commit);
  if (!tree)
    return unexpected_err(tree.error());
    
  commitId_ = git_commit_id(*commit);
  commit_ = std::move(*commit);
  root_ = std::move(*tree);

  sLogger->debug("Tip of '{}' updated to {}", ctx.ref_, *commitId);
  return Result<void>();
}

Result<void>
gd::internal::Node::rebase(const gd::Context& ctx) noexcept {

  auto commitId = referenceCommit(*ctx.repo_, ctx.ref_);
  if (!commitId)
    return unexpected_err(commitId.error());

  return update(ctx, *commitId);
}

Result<git_oid const*>
gd::internal::Node::tip(const gd::Context& ctx) noexcept {
  auto commitId = referenceCommit(*ctx.repo_, ctx.ref_);
  if (!commitId)
    return unexpected_err(commitId.error());

  return *commitId;
}

Result<bool>
gd::internal::Node::isTip(const gd::Context& ctx) noexcept {
  auto commitId = referenceCommit(*ctx.repo_, ctx.ref_);
  if (!commitId)
    return unexpected_err(commitId.error());

  return commitId_ != nullptr && git_oid_cmp(*commitId, commitId_) == 0;
}

/*******************************************************************************
 *                            Interface implementation
 *******************************************************************************/

std::shared_ptr<spdlog::logger>
gd::setLogger(std::shared_ptr<spdlog::logger> newLogger) noexcept {
  sLogger.swap(newLogger);
  return newLogger; 
}

bool 
gd::cleanRepo(const std::filesystem::path& repoFullPath) noexcept {
  return sGit.cleanRepo(repoFullPath);
}

Result<gd::Context>
gd::selectRepository(const std::filesystem::path& fullpath, const std::string& name) noexcept
{
  if (auto repo = sGit.getRepo(fullpath); !!repo) {
    return gd::Context(*repo, sHead);
  }

  if (repoExists(fullpath)) 
    return connectToRepo(fullpath);

  return createRepo(fullpath, name);
}

/// @brief Changes the context branch
/// @param ctx The context used to access the repository
/// @param name Branch name
/// @return On success the context for continued chaining, otherwise an Error
Result<gd::Context>
gd::ni::selectBranch(gd::Context&& ctx, const std::string& name) noexcept
{
  if (not ctx.repo_) return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

  std::string ref = sBranchRefRoot + name;

  sGit.setThreadBranch(ref);
  ctx.ref_ =  std::move(ref);

  sLogger->debug("Switched branch to {}", name);
  return internal::Node::init(std::move(ctx));
}

/// @brief adds a file(Blob) at `fullpath` with `content`
/// @param ctx The context used to access the repository
/// @param fullpath Full path (including filename) of the introduced file
/// @param content Full content 
/// @return On success, the context, otherwise false
Result<gd::Context>
gd::ni::add(gd::Context&& ctx, const std::filesystem::path& fullpath, const std::string& content) noexcept
{
  if (not ctx.repo_)
    return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

  auto res = ctx.updates_.insertFile(ctx, fullpath, content);
  if (!res) 
    return unexpected_git;

  sLogger->debug("Add Blob '{}'", fullpath);
  return std::move(ctx);
}

/// @brief Deletes a file(Blob)
/// @param ctx The context used to access the repository
/// @param fullpath Fullpath to the Blob to remove
/// @return On success, the context for continued repository access, otherwise an Errory
Result<gd::Context>
gd::ni::rm(gd::Context&& ctx, const std::string& fullpath) noexcept
{
  if (not ctx.repo_)
    return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

  auto res = ctx.updates_.removeFile(ctx, fullpath);
  if (!res) 
    return unexpected_git;

  sLogger->debug("Remove file", fullpath);
  return std::move(ctx);
}

/// @brief Moves a file from on `fullpath` to `toFullPath`
/// @param ctx The context used to access the repository
/// @param fullpath Orignal path of the moved file, including filename.
/// @param toFullPath Destination path of the moved file, including filename.
/// @return On success, the context for continued repository access, otherwise an Errory
Result<gd::Context>
gd::ni::mv(gd::Context&& ctx, const std::string& fullpath, const std::string& toFullPath) noexcept
{
  if (not ctx.repo_)
    return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

  auto entry = getTreeEntry(ctx.tip_.root_, fullpath);
  if (!entry) 
    return unexpected_err(entry.error());

  auto res = ctx.updates_.insertEntry(ctx, toFullPath, *entry);
  if (!res) 
    return unexpected_git;

  if( auto res = ctx.updates_.removeFile(ctx, fullpath); !res)
    return unexpected_git;

  sLogger->debug("Move {} to {}", fullpath, toFullPath);
  return std::move(ctx);
}

/// @brief Commits collected updates  
/// @param ctx The context used to access the repository
/// @param message The commmit message
/// @return  On success propagates the context, otherwise an Error
Result<gd::Context>
gd::ni::commit(gd::Context&& ctx, const std::string& author, const std::string& email, const std::string& message) noexcept
{
  if (ctx.updates_.empty()) 
    return unexpected(gd::ErrorType::EmptyCommit, "Nothing to commit");

  auto newRoot = ctx.updates_.apply(ctx);
  if (!newRoot)
    return unexpected_err(newRoot.error());

  auto commiter = getSignature(author, email);
  if (!commiter)
    return unexpected_err(commiter.error());

  git_commit const *parents[1]{  ctx.tip_.commit_ };

  git_oid commitId;
  { 
    static std::mutex commitAccess;

    /// The only contention point in need of serialization is updates to the DAG. i.e. commits
    std::scoped_lock serialize(commitAccess);
    int result = git_commit_create(
        &commitId,
        *ctx.repo_,
        ctx.ref_.c_str(), /* name of ref      */
        *commiter,        /* author           */
        *commiter,        /* committer        */
        "UTF-8",          /* message encoding */
        message.c_str(),  /* message          */
        *newRoot,         /* root tree        */
        1,                /* parent count     */
        parents);         /* parents          */

    if (result != 0)
      return unexpected_git;
  }

  if (auto res = ctx.update(&commitId); !res) 
    return unexpected_err(res.error());

  sLogger->debug("Committed on ref {} {}({}): {}", ctx.ref_, commitId, author, message);
  return std::move(ctx);
}


/// @brief Undoes, all the uncomitted updates.
/// @param ctx The context used to access the repository
/// @return On success the context for continued chaining, otherwise an error.
/// TODO: Test 
Result<gd::Context>
gd::ni::rollback(gd::Context&& ctx) noexcept
{
  ctx.updates_.clean();
  return std::move(ctx);
}

/// @brief Branch from a commitId
/// @param ctx The context used to access the repository
/// @param commitId The commit from which to branch
/// @param name The nam of hte new branch 
/// @return On success, the context to continue the call chain, otherwise an Error
Result<gd::Context>
gd::ni::createBranch(gd::Context&& ctx, const git_oid* commitId, const std::string& name) noexcept
{
  auto commit = getCommitById(*ctx.repo_, commitId );
  if (*commit)
    return unexpected_err(commit.error());

  auto branchRef = createBranch(*ctx.repo_, name, ctx.tip_.commit_);
  if (!branchRef)
    return unexpected_err(branchRef.error());

  /// TODO: How about adding commitId to the logs
  sLogger->debug("Branch '{}' created", name);
  return std::move(ctx);
}

/// @brief Creates a branch from current context
/// @param ctx The context used to access the repository
/// @param name The new branch's name
/// @return On success, the context to continue the call chain, otherwise an Error
Result<gd::Context>
gd::ni::createBranch(gd::Context&& ctx, const std::string& name) noexcept
{
  auto branchRef = createBranch(*ctx.repo_, name, ctx.tip_.commit_);
  if (!branchRef)
    return unexpected_err(branchRef.error());

  /// TODO: How about adding commitId to the logs
  sLogger->debug("Branch '{}' created", name);
  return std::move(ctx);
}

/// @brief Reads content of blob(gitspeak for file), first from uncommitted context, otherwise from repository
/// @param ctx The context used to access the repository
/// @param fullpath The fullpath of the blob
/// @return A string representation of the file's content.
Result<gd::ReadContext>
gd::ni::read(gd::Context&& ctx, const std::filesystem::path& fullpath) noexcept
{
  // Search content in context, Error shortcut if deleted
  auto contextBlob = ctx.updates_.getBlobByPath(ctx, fullpath);
  if (!contextBlob && contextBlob.error()._type == gd::ErrorType::Deleted )
    return unexpected_err(contextBlob.error());
  
  if (!!contextBlob)
    return readblob(std::move(ctx), *contextBlob, fullpath);

  auto blob = getBlobFromTreeByPath(ctx.tip_.root_, fullpath);
  if (!blob)
    return unexpected_err(blob.error());
    
  return readblob(std::move(ctx), *blob, fullpath);
}


Result<gd::ReadContext>
gd::ni::readblob(gd::Context&& ctx, git_blob * blob, const std::filesystem::path& fullpath) noexcept {
  git_blob_filter_options opts = GIT_BLOB_FILTER_OPTIONS_INIT;
  git_buf buffer = GIT_BUF_INIT_CONST("", 0);
  if (git_blob_filter(&buffer, blob, fullpath.c_str(), &opts) != 0)
    return unexpected_git;

  std::string content(buffer.ptr, buffer.size);

  git_buf_dispose(&buffer);
  git_buf_free(&buffer);

  return ReadContext(std::move(ctx), std::move(content));
}

/// @brief Gets thread content, support for thread_local context call chaining 
/// @return The thread_local context
Result<gd::Context>
gd::shorthand::getThreadContext() noexcept
{
  return sGit.threadContext();
}
