#include <guard.h>

tl::expected<gd::blob_t, gd::Error>
getBlobFromTreeByPath(git_tree const * root, const std::filesystem::path& path) noexcept {
  git_object *object;
  int result = git_object_lookup_bypath(&object, reinterpret_cast<const git_object*>(root), path.c_str(), GIT_OBJECT_BLOB);
  if (result != 0)
    return unexpected_git;

  if (git_object_type(object) == GIT_OBJECT_BLOB) 
    return reinterpret_cast<git_blob*>(object);
    // return make_guard(git_blob_free, reinterpret_cast<git_blob*>(object));

  return unexpected(gd::ErrorType::BadFile, path.string() + " is not a file(blob)");
}

tl::expected<gd::tree_t, gd::Error>
getTree(git_repository* repo, const git_oid* treeOid) noexcept {
  git_tree* tree;
  if(git_tree_lookup(&tree, repo, treeOid) != 0)
    return unexpected_git;

  return tree;
}

tl::expected<gd::tree_t, gd::Error>
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

tl::expected<gd::tree_t, gd::Error>
getTreeOfCommit(git_repository* repo, git_commit* commit) noexcept {
  git_tree* tree;
  if(git_commit_tree(&tree, commit) != 0)
    return unexpected_git;

  return tree;
}

tl::expected<gd::object_t, gd::Error>
getObjectBySpec(git_repository* repo, const std::string& spec) noexcept {
  git_object* obj = nullptr;
  if(git_revparse_single(&obj, repo, spec.c_str()) != 0)
    return unexpected_git;

  return obj;
}

tl::expected<gd::commit_t, gd::Error>
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

tl::expected<gd::treebuilder_t, gd::Error>
getTreeBuilder(git_repository* repo, const git_tree* tree) noexcept {
    git_treebuilder *bld = nullptr;
    if (git_treebuilder_new(&bld, repo, tree) != 0)
      return unexpected_git;

    return bld;
}


tl::expected<gd::signature_t, gd::Error>
getSignature(const std::string& author, const std::string& email) noexcept {
  git_signature *commiter = nullptr;
  if(git_signature_now(&commiter, author.data(), email.data()) != 0 )
    return unexpected_git;

  return commiter;
}