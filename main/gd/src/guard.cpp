#include <git2.h>
#include <guard.h>
#include <string.h>

Result<gd::blob_t>
getBlobFromTreeByPath(git_tree const * root, const std::filesystem::path& path) noexcept {
  git_object *object;
  int result = git_object_lookup_bypath(&object, reinterpret_cast<const git_object*>(root), path.c_str(), GIT_OBJECT_BLOB);
  if (result != 0)
    return unexpected_git;

  if (git_object_type(object) == GIT_OBJECT_BLOB) 
    return reinterpret_cast<git_blob*>(object);
    /// TODO: What is this ->> return make_guard(git_blob_free, reinterpret_cast<git_blob*>(object));

  return unexpected(gd::ErrorType::BadFile, path.string() + " is not a file(blob)");
}

Result<gd::tree_t>
getTree(git_repository* repo, const git_oid* treeOid) noexcept {
  git_tree* tree;
  if(git_tree_lookup(&tree, repo, treeOid) != 0)
    return unexpected_git;

  return tree;
}

Result<gd::tree_t>
getTreeRelativeToRoot(git_repository* repo, git_tree* root, const std::filesystem::path& path) noexcept {
    git_tree_entry *entry;
    int result = root ? git_tree_entry_bypath(&entry, root, path.c_str()) : GIT_ENOTFOUND;

    if (result == GIT_ENOTFOUND)
      return nullptr;

    if (result != GIT_OK )
      return unexpected_git;

    git_tree *tree = nullptr ;
    if (git_tree_entry_type(entry) == GIT_OBJECT_TREE) {
      result = git_tree_lookup(&tree, repo, git_tree_entry_id(entry));
      git_tree_entry_free(entry);
      if (result < 0)
        return unexpected_git;

      return tree;
    }
    return unexpected(gd::ErrorType::BadDir, path.string() + " is not a direcotry");
}

Result<gd::tree_t>
getTreeOfCommit(git_repository* repo, git_commit* commit) noexcept {
  git_tree* tree;
  if(git_commit_tree(&tree, commit) != 0)
    return unexpected_git;

  return tree;
}

Result<gd::object_t>
getObjectBySpec(git_repository* repo, const std::string& spec) noexcept {
  git_object* obj = nullptr;
  if(auto res = git_revparse_single(&obj, repo, spec.c_str()); res != 0)
    if (auto msg = git_error_last()->message; strcmp("revspec 'HEAD' not found", msg) == 0) {
      return unexpected(gd::ErrorType::InitialContext, "'"s + spec + "' Object retrieve failed: git error '" + msg + "'")
    } else  {
      return unexpected_git;
    }

  return obj;
}

Result<gd::commit_t>
getCommitByRef(git_repository* repo, const std::string& ref ) noexcept {
  auto commitObj = getObjectBySpec(repo, ref);
  if (!commitObj)
    return unexpected_err(commitObj.error());

  if (auto type = git_object_type(*commitObj); type != GIT_OBJECT_COMMIT)
  { /// TODO: Update output -> add decltype in error
    return unexpected(gd::ErrorType::EmptyCommit, ref + " is not a reference to a commit");
  }

  return commitObj->castMove<git_commit, git_commit_free>();
}


Result<gd::commit_t>
getCommitById(git_repository* repo, const git_oid* commitId) noexcept {
  git_commit *commit{ nullptr };
  int result = git_commit_lookup(&commit, repo, commitId);
  if (result != 0)
    return unexpected_git;

  return commit;
}

Result<gd::treebuilder_t>
getTreeBuilder(git_repository* repo, const git_tree* tree) noexcept {
    git_treebuilder *bld = nullptr;
    if (git_treebuilder_new(&bld, repo, tree) != 0)
      return unexpected_git;

    return bld;
}


Result<gd::signature_t>
getSignature(const std::string& author, const std::string& email) noexcept {
  git_signature *commiter = nullptr;
  if(git_signature_now(&commiter, author.data(), email.data()) != 0 )
    return unexpected_git;

  return commiter;
}

Result<gd::reference_t>
createBranch(git_repository* repo, const std::string& name, const git_commit* commit) noexcept {
  git_reference* branchRef{ nullptr };
  int result = git_branch_create(&branchRef, repo, name.c_str(), commit, 0 /* do not force */);
  if (result != 0)
    return unexpected_git;

  return branchRef; 
}

Result<gd::repository_t>
openRepository(const std::filesystem::path& fullpath) noexcept {
  git_repository* repo;
  if (git_repository_open_ext(&repo, fullpath.c_str(), GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr) != 0)
    return unexpected_git;

  return repo;
}

Result<gd::repository_t>
createRepository(const std::string& fullpath, const std::string& name) noexcept {
  git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;

  opts.flags |= GIT_REPOSITORY_INIT_MKPATH; /* mkdir as needed to create repo */
  opts.flags |= GIT_REPOSITORY_INIT_BARE;   /* Bare repository                */
  opts.description = name.c_str();          /* User given name                */
  opts.initial_head = "main";               /* Main branch instead of master  */

  git_repository *repo;
  if (git_repository_init_ext(&repo, fullpath.c_str(), &opts) != 0)
    return unexpected_git;

  return repo;
}

Result<gd::entry_t>
getTreeEntry(const git_tree* root, const std::string& fullpath) {
  git_tree_entry* entry;
  if (git_tree_entry_bypath(&entry, root, fullpath.c_str()) != 0 )
    return unexpected_git;

  return entry;
}