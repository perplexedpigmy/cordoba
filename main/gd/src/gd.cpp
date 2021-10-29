#include <gd/gd.h>
#include <pathTraverse.h>
#include <map>
#include <mutex>
  /* std::ostream& operator<<(std::ostream& os, git_tree* tree) */ /* { */
  /*    char buf[10]; */
  /*    return os << "Tree(" << git_oid_tostr(buf, 9, git_tree_id(tree)) <<")" ; */
  /* } */

  /* std::ostream& operator<<(std::ostream& os, git_oid oid) */
  /* { */
  /*    char buf[10]; */
  /*    return os << "Object(" << git_oid_tostr(buf, 9, &oid) <<")" ; */
  /* } */


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
  std::string error(char const * const file, int line) noexcept
  {
    const git_error *err = git_error_last();
    return std::string("[") + std::to_string(err->klass) + std::string("] ") + file + std::string(":") + std::to_string(line) + std::string(": ") + err->message;
  }


#define Error error(__FILE__, __LINE__)
#define Unexpected tl::make_unexpected(error(__FILE__, __LINE__))


  static std::mutex sCacheAccess;
  static std::unordered_map<std::string, gd::context> sRepoCache;
  static bool sInitialized{false};
  static std::string sRefRoot{"refs/heads/"};


  /**
   * Create a repository at a given path with a given name
   **/
  tl::expected<gd::context, std::string>
  createRepo(const std::string& path, const std::string& name)
  {
    git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;

    opts.flags |= GIT_REPOSITORY_INIT_MKPATH; /* mkdir as needed to create repo */
    opts.flags |= GIT_REPOSITORY_INIT_BARE ;  /* Bare repository                */
    opts.description = name.data();       /* User given name                */

    gd::context ctx;
    if(git_repository_init_ext(&ctx.repo_, path.data(), &opts) != 0)
      return Unexpected;

    sRepoCache.emplace(path, ctx);

    return ctx;
  }


  /**
   * Returns true if a repository already exists, otherwise false
   **/
  bool repoExists(const std::string& repoPath) noexcept
  {
    return git_repository_open_ext(nullptr, repoPath.data(), GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) == 0;
  }
}

void gd::cleanup()
{
  std::lock_guard<std::mutex> lock(sCacheAccess);

  for (const auto& [path, ctx] : sRepoCache)
    git_repository_free(ctx.repo_);

  git_libgit2_shutdown();
  sRepoCache.clear();
}


tl::expected<git_tree*, std::string>
gd::getTree(gd::context& ctx, const std::string& path, git_tree const * root) noexcept
{
  git_tree_entry *entry;
  int result = git_tree_entry_bypath(&entry, root, path.data());

  if (result == GIT_ENOTFOUND)
    return nullptr;

  if (result !=  GIT_OK )
    return Unexpected;

  git_tree *tree = nullptr ;
  if (git_tree_entry_type(entry) == GIT_OBJECT_TREE) {
    result = git_tree_lookup(&tree, ctx.repo_, git_tree_entry_id(entry));
    git_tree_entry_free(entry);
    if (result < 0)
      return Unexpected;

    return tree;
  }
  return tl::make_unexpected(path + " is not a direcotry");
}


tl::expected<git_blob*, std::string>
gd::getBlob(gd::context& ctx,  const std::string& path, git_tree const * root) noexcept
{
  git_object *object;
  int result = git_object_lookup_bypath(&object, reinterpret_cast<const git_object*>(root), path.data(), GIT_OBJECT_BLOB);
  if (result != 0)
    return Unexpected;

  if (git_object_type(object) == GIT_OBJECT_BLOB)
    return reinterpret_cast<git_blob*>(object);

  return tl::make_unexpected(path + " is not a file");
}


tl::expected<gd::context, std::string>
gd::selectRepository(const std::string& fullpath, const std::string& name) noexcept
{
  std::lock_guard<std::mutex> lock(sCacheAccess);

  auto itr = sRepoCache.find(fullpath);
  if (itr != sRepoCache.end())
    return itr->second;

  if (!sInitialized)
    sInitialized = git_libgit2_init() > 0;

  gd::context ctx;
  if (repoExists(fullpath))
  {
    if (git_repository_open_ext(&ctx.repo_, fullpath.data(), GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr) != 0)
      return Unexpected;

    return ctx;
  }

  return createRepo(fullpath, name);
}


tl::expected<gd::context, std::string>
gd::ni::selectBranch(gd::context&& ctx, const std::string& name) noexcept
{
  if (not sInitialized)
    return tl::make_unexpected("Git not initalized");

  if (not ctx.repo_)
    return tl::make_unexpected("No Repository was selected");

  ctx.ref_ = sRefRoot + name;

  git_object* commitObj = nullptr;
  if(git_revparse_single(&commitObj, ctx.repo_, name.data()) != 0)
    return Unexpected;

  if (git_object_type(commitObj) != GIT_OBJECT_COMMIT)
    return tl::make_unexpected(name + " doesn't reference a commit");

  git_commit* commit = reinterpret_cast<git_commit*>(commitObj);

  ctx.parents_[0] = commit;
  ctx.numParents_ = 1;

  if(git_commit_tree(&ctx.prevCommitTree_, commit) != 0)
    return Unexpected;

  return std::move(ctx);
}


tl::expected<gd::context, std::string>
gd::ni::addFile(gd::context&& ctx, const std::string& fullpath, const std::string& content) noexcept
{
  if (not sInitialized)
    return tl::make_unexpected("Git not initalized");

  if (not ctx.repo_)
    return tl::make_unexpected("Repository not selected");

  if(ctx.ref_.empty())
    return tl::make_unexpected("Branch not selected");

  git_oid elemOid;
  if (git_blob_create_from_buffer(&elemOid, ctx.repo_, content.data(), content.size()) != 0)
    return Unexpected;

  auto mode = GIT_FILEMODE_BLOB;

  PathTraverse pt(fullpath.data());
  char const * name = pt.filename();

  for (auto [path, dir] : pt)
  {
    auto prev = getTree(ctx, path, ctx.prevCommitTree_);
    if (!prev)
      return Unexpected;

    git_treebuilder *bld = nullptr;
    if (git_treebuilder_new(&bld, ctx.repo_, *prev) != 0)
      return Unexpected;

    if(git_treebuilder_insert(nullptr, bld, name, &elemOid, mode) != 0)
      return Unexpected;

    if(git_treebuilder_write(&elemOid, bld) != 0)
      return Unexpected;

    git_treebuilder_free(bld);
    git_tree_free(*prev);
    mode = GIT_FILEMODE_TREE;

    name = dir;
  }

  git_treebuilder *bld = nullptr;
  if(git_treebuilder_new(&bld, ctx.repo_, ctx.prevCommitTree_) != 0)
    return Unexpected;

  if(git_treebuilder_insert(nullptr, bld, name, &elemOid, mode) != 0)
    return Unexpected;

  if(git_treebuilder_write(&elemOid, bld) != 0)
    return Unexpected;

  git_treebuilder_free(bld);
  git_tree_free(ctx.prevCommitTree_);

  if(git_tree_lookup(&ctx.prevCommitTree_, ctx.repo_, &elemOid) != 0)
    return Unexpected;

  return std::move(ctx);
}


tl::expected<git_oid, std::string>
gd::ni::commit(gd::context&& ctx, const std::string& author, const std::string& email, const std::string& message) noexcept
{
  git_signature *commiter = nullptr;
  if(git_signature_now(&commiter, author.data(), email.data()) != 0 )
    return Unexpected;

  git_oid commit_id;
  int result = git_commit_create(
      &commit_id,
      ctx.repo_,
      ctx.ref_.data(),      /* name of ref to update */
      commiter,             /* author */
      commiter,             /* committer */
      "UTF-8",              /* message encoding */
      message.data(),       /* message */
      ctx.prevCommitTree_,  /* root tree */
      ctx.numParents_,      /* parent count */
      ctx.parents_);        /* parents */

  git_signature_free(commiter);
  git_tree_free(ctx.prevCommitTree_);

  for (size_t i = 0; i < ctx.numParents_; ++i)
    git_commit_free(const_cast<git_commit*>(ctx.parents_[i]));

  ctx.prevCommitTree_ = nullptr;
  ctx.numParents_ = 0;

  if (result != 0)
    return Unexpected;

  return commit_id;
}


tl::expected<gd::context, std::string>
gd::ni::fork(gd::context&& ctx, const git_oid& commitId, const std::string& name) noexcept
{
  git_commit *commit = nullptr;
  int result = git_commit_lookup(&commit, ctx.repo_, &commitId);
  if (result != 0)
    return Unexpected;

  git_reference* branchRef = nullptr;
  result = git_branch_create(&branchRef, ctx.repo_, name.data(), commit, 0 /* no force */);
  if (result != 0)
    return Unexpected;

  git_reference_free(branchRef);
  git_commit_free(commit);

  return std::move(ctx);
}


tl::expected<std::string, std::string>
gd::ni::read(gd::context&& ctx, const std::string& fullpath) noexcept
{
  auto blob = getBlob(ctx, fullpath, ctx.prevCommitTree_);
  if (!blob)
    return tl::make_unexpected(blob.error());

  git_blob_filter_options opts = GIT_BLOB_FILTER_OPTIONS_INIT;

  git_buf buffer = GIT_BUF_INIT_CONST("", 0);
  if (git_blob_filter(&buffer, *blob, fullpath.data(), &opts) != 0)
    return Unexpected;

  std::string content(buffer.ptr, buffer.size);

  git_buf_dispose(&buffer);
  git_buf_free(&buffer);
  git_blob_free(*blob);

  // TODO: Same as in commit, consider adding this as dtor/utilty function or RAII
  git_tree_free(ctx.prevCommitTree_);

  for (size_t i = 0; i < ctx.numParents_; ++i)
    git_commit_free(const_cast<git_commit*>(ctx.parents_[i]));

  return  content;
}
