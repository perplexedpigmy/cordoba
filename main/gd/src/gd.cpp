#include <gd/gd.h>
#include <pathTraverse.h>

#include <map>
#include <mutex>
#include <iostream>
#include <shared_mutex>
#include <experimental/filesystem>

/* std::ostream& operator<<(std::ostream& os, git_tree* tree) */ /* { */
/*    char buf[10]; */
/*    return os << "Tree(" << git_oid_tostr(buf, 9, git_tree_id(tree)) <<")" ; */
/* } */

std::ostream& operator<<(std::ostream& os, git_oid oid) noexcept
{
   constexpr size_t BufSize = 10;

   std::array<char, BufSize> buf;
   return os << "Object(" << git_oid_tostr(buf.data(), BufSize - 1, &oid) <<")" ;
}

std::ostream& operator<<(std::ostream& os, const gd::Error& e) noexcept 
{
  return  os << (std::string)e;
}

      /* static git_oid oid(char * const sha) */
      /* { */
      /*   git_oid ret; */
      /*   check(git_oid_fromstr(&ret , sha)); */
      /* return ret; */
      /* } */
      /* static std::string sha(git_oid const * oid) */
      /* { */
      /*   char buf[GIT_OID_HEXSZ+1]; */
      /*   return git_oid_tostr(buf, GIT_OID_HEXSZ, oid); */
      /* } */

namespace {
  static char const * const sNoRepositoryError{"No Repository selected"};

  template <typename T, typename F>
  void free_git_resource(T*& resource, F f)
  {
    if(resource)
    {
       f(resource);
       resource = nullptr;
    }
  }

  /**
   * Error handling functions
   **/
  // std::string error(char const * const file, int line) noexcept
  // {
  //   const git_error *err = git_error_last();
  //   return std::string("[") + std::to_string(err->klass) + std::string("] ") + file + std::string(":") + std::to_string(line) + std::string(": ") + err->message;
  // }

  // #define Error error(__FILE__, __LINE__)
  // #define Unexpected tl::make_unexpected(error(__FILE__, __LINE__))

  std::string strigify_git_error() noexcept {
    const git_error *err = git_error_last();
    return std::string("[") + std::to_string(err->klass) + std::string("] ") + err->message;
  }

  #define unexpected(type, msg) tl::make_unexpected( gd::Error(__FILE__, __LINE__, type, msg) );
  #define unexpected_err(err) tl::make_unexpected( err );
  #define unexpected_git tl::make_unexpected( gd::Error(__FILE__, __LINE__, gd::ErrorType::GitError, strigify_git_error()) );

  /**
   * Git accesssor abstraction
   * It's main use is for initialization and RAII handling of cached repos and git
   *
   * Repositories are cached for the duration of the application (344 bytes)
   *
   * NOTE: Only one GitGaurd in the system initialized at static time and destructed
   * on shutdown
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
     * TODO: Update to Result (Expected)
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
    * Retuns true if a repo cleanup occurs when the repo exists
    * If repo doesn't exist returns false
    * Regardless it will remove the directory `repoFullPath`
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
      std::experimental::filesystem::remove_all(repoFullPath);;
      return removed;
    }

    expected<gd::context, gd::Error> threadContext() const noexcept
    {
      if (not ctx_.repo_) return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

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
  expected<gd::context, gd::Error>
  createRepo(const std::string& fullpath, const std::string& name) noexcept
  {
    git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;

    opts.flags |= GIT_REPOSITORY_INIT_MKPATH; /* mkdir as needed to create repo */
    opts.flags |= GIT_REPOSITORY_INIT_BARE ;  /* Bare repository                */
    opts.description = name.data();           /* User given name                */

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

  expected<gd::context, gd::Error>
  connectToRepo(const std::string& fullpath)
  {
    git_repository* repo;
    if (git_repository_open_ext(&repo, fullpath.data(), GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr) != 0)
      return unexpected_git;

    sGit.cacheRepo(fullpath, repo);
    return gd::context{repo, sHead};;
  }


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
  expected<git_tree*, gd::Error>
  getTree(gd::context& ctx, const std::string& path) noexcept
  {
    if (ctx.tip_.tree_ == nullptr)
      return nullptr;

    git_tree_entry *entry;
    int result = git_tree_entry_bypath(&entry, ctx.tip_.tree_, path.data());

    if (result == GIT_ENOTFOUND)
      return nullptr;

    if (result !=  GIT_OK )
      return unexpected_git;

    git_tree *tree = nullptr ;
    if (git_tree_entry_type(entry) == GIT_OBJECT_TREE) {
      result = git_tree_lookup(&tree, ctx.repo_, git_tree_entry_id(entry));
      git_tree_entry_free(entry);
      if (result < 0)
        return unexpected_git;

      return tree;
    }
    return unexpected(gd::ErrorType::BadDir, path + " is not a direcotry");
  }

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
  expected<git_blob*, gd::Error>
  getBlob(gd::context& ctx,  const std::string& path, git_tree const * root) noexcept
  {
    git_object *object;
    int result = git_object_lookup_bypath(&object, reinterpret_cast<const git_object*>(root), path.data(), GIT_OBJECT_BLOB);
    if (result != 0)
      return unexpected_git;

    if (git_object_type(object) == GIT_OBJECT_BLOB)
      return reinterpret_cast<git_blob*>(object);

    return unexpected(gd::ErrorType::BadFile, path + " is not a file");
  }
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


expected<::gd::context, gd::Error>
gd::context::updateCommitTree(const git_oid& newTreeOid) noexcept
{
  free_git_resource(tip_.tree_, git_tree_free);

  if(git_tree_lookup(&tip_.tree_, repo_, &newTreeOid) != 0)
    return unexpected_git;

  return std::move(*this);
}

/*******************************************************************************
 *                             internal::Node
 * A Node is specific location on the DAG and a reference to the tree
 *******************************************************************************/
gd::internal::Node::Node(Node&& other) noexcept
: commit_(other.commit_), tree_(other.tree_) {
  other.commit_ = nullptr;
  other.tree_ = nullptr;
}


gd::internal::Node& gd::internal::Node::operator=(Node&& other) noexcept
{
  if (this == &other) return *this;

  commit_ = other.commit_;
  other.commit_ = nullptr;

  tree_ = other.tree_;
  other.tree_ = nullptr;

  return *this;
}


void gd::internal::Node::reset() noexcept
{
  free_git_resource(commit_, git_commit_free);
  free_git_resource(tree_, git_tree_free);
}

expected<::gd::context, gd::Error>
gd::internal::Node::init(gd::context&& ctx) noexcept
{
  ctx.tip_.reset();
  git_object* commitObj = nullptr;
  if(git_revparse_single(&commitObj, ctx.repo_, ctx.ref_.data()) != 0)
    return unexpected_git;

  if (git_object_type(commitObj) != GIT_OBJECT_COMMIT)
  {
    git_object_free(commitObj);
    return unexpected(gd::ErrorType::EmptyCommit, ctx.ref_ + " doesn't reference a commit");
  }

  ctx.tip_.commit_ = reinterpret_cast<git_commit*>(commitObj);

  if(git_commit_tree(&ctx.tip_.tree_, ctx.tip_.commit_) != 0)
    return unexpected_git;

  return std::move(ctx);
}

/*******************************************************************************
 *                            Interface implementation
 *******************************************************************************/

/**
 * Removes the repo from the cache if it exists
 * cleans up it's git state and removes the directory
 */
 bool 
 gd::cleanRepo(const std::string& repoFullPath) noexcept {
   return sGit.cleanRepo(repoFullPath);
 }

expected<gd::context, gd::Error>
gd::selectRepository(const std::string& fullpath, const std::string& name) noexcept
{
  if (auto [exists, pRepo] =  sGit.getRepo(fullpath); exists)
    return gd::context(pRepo, sHead);

  if (repoExists(fullpath))
    return connectToRepo(fullpath);

  return createRepo(fullpath, name);
}


expected<gd::context, gd::Error>
gd::ni::selectBranch(gd::context&& ctx, const std::string& name) noexcept
{
  if (not ctx.repo_) return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

  std::string ref = sBranchRefRoot + name;

  sGit.setThreadBranch(ref);
  ctx.ref_ =  std::move(ref);

  return internal::Node::init(std::move(ctx));
}


expected<gd::context, gd::Error>
gd::ni::add(gd::context&& ctx, const std::string& fullpath, const std::string& content) noexcept
{
  if (not ctx.repo_) return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

  git_oid elemOid;
  if (git_blob_create_from_buffer(&elemOid, ctx.repo_, content.data(), content.size()) != 0)
    return unexpected_git;

  auto mode = GIT_FILEMODE_BLOB;

  PathTraverse paths(fullpath.data());
  char const * name = paths.filename();

  for (auto [path, dir] : paths)
  {
    auto prev = getTree(ctx, path);
    if (!prev)
      return unexpected_git;

    git_treebuilder *bld = nullptr;
    if (git_treebuilder_new(&bld, ctx.repo_, *prev) != 0)
      return unexpected_git;

    if(git_treebuilder_insert(nullptr, bld, name, &elemOid, mode) != 0)
      return unexpected_git;

    if(git_treebuilder_write(&elemOid, bld) != 0)
      return unexpected_git;

    git_treebuilder_free(bld);
    git_tree_free(*prev);
    mode = GIT_FILEMODE_TREE;

    name = dir;
  }

  git_treebuilder *bld = nullptr;
  if(git_treebuilder_new(&bld, ctx.repo_, ctx.tip_.tree_) != 0)
    return unexpected_git;

  if(git_treebuilder_insert(nullptr, bld, name, &elemOid, mode) != 0)
    return unexpected_git;

  if(git_treebuilder_write(&elemOid, bld) != 0)
    return unexpected_git;

  git_treebuilder_free(bld);

  ctx.dirty_ = true;
  return ctx.updateCommitTree(elemOid);
}


expected<gd::context, gd::Error>
gd::ni::del(gd::context&& ctx, const std::string& fullpath) noexcept
{
  if (not ctx.repo_)    return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

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
}


expected<gd::context, gd::Error>
gd::ni::mv(gd::context&& ctx, const std::string& fullpath, const std::string& toFullPath) noexcept
{
  if (not ctx.repo_)    return unexpected(gd::ErrorType::MissingRepository, sNoRepositoryError);

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
}


expected<git_oid, gd::Error>
gd::ni::commit(gd::context&& ctx, const std::string& author, const std::string& email, const std::string& message) noexcept
{
  if (not ctx.dirty_) return unexpected(gd::ErrorType::EmptyCommit, "Nothing to commit");

  git_signature *commiter = nullptr;
  if(git_signature_now(&commiter, author.data(), email.data()) != 0 )
    return unexpected_git;

  git_commit const *parents[1]{  ctx.tip_.commit_ };

  git_oid commit_id;
  int result = git_commit_create(
      &commit_id,
      ctx.repo_,
      ctx.ref_.data(), /* name of ref      */
      commiter,        /* author           */
      commiter,        /* committer        */
      "UTF-8",         /* message encoding */
      message.data(),  /* message          */
      ctx.tip_.tree_,  /* root tree        */
      1,               /* parent count     */
      parents);        /* parents          */

  git_signature_free(commiter);

  ctx.tip_.reset();

  if (result != 0)
    return unexpected_git;

  ctx.dirty_ = false;
  return commit_id;
}


expected<gd::context, gd::Error>
gd::ni::rollback(gd::context&& ctx) noexcept
{
  ctx.dirty_ = false;
  return std::move(ctx);
}

expected<gd::context, gd::Error>
gd::ni::createBranch(gd::context&& ctx, const git_oid& commitId, const std::string& name) noexcept
{
  git_commit *commit{ nullptr };
  int result = git_commit_lookup(&commit, ctx.repo_, &commitId);
  if (result != 0)
    return unexpected_git;

  git_reference* branchRef{ nullptr };
  result = git_branch_create(&branchRef, ctx.repo_, name.data(), commit, 0 /* no force */);
  if (result != 0)
    return unexpected_git;

  git_reference_free(branchRef);
  git_commit_free(commit);

  return std::move(ctx);
}


expected<std::string, gd::Error>
gd::ni::read(gd::context&& ctx, const std::string& fullpath) noexcept
{
  auto blob = getBlob(ctx, fullpath, ctx.tip_.tree_);
  if (!blob)
    return unexpected_err(blob.error());

  git_blob_filter_options opts = GIT_BLOB_FILTER_OPTIONS_INIT;

  git_buf buffer = GIT_BUF_INIT_CONST("", 0);
  if (git_blob_filter(&buffer, *blob, fullpath.data(), &opts) != 0)
    return unexpected_git;

  std::string content(buffer.ptr, buffer.size);

  git_buf_dispose(&buffer);
  git_buf_free(&buffer);
  git_blob_free(*blob);

  return  content;
}

expected<gd::context, gd::Error>
gd::shorthand::getThreadContext() noexcept
{
  return sGit.threadContext();
}
