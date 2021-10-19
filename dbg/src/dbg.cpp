#include <git2.h>
#include <tl/expected.hpp>
#include <map>
#include <iostream>

#include "pathTraverse.h"

/**
 * TODO: Consider introducing RAII. Impact on runtime?
 * TODO: Make as a library
 * TODO: Add tests
 * TODO: Timing and scalabilty checks
 **/
namespace dbg
{
  static inline std::string sRefRoot = "refs/heads/";

  std::ostream& operator<<(std::ostream& os, git_tree* tree)
  {
     char buf[10];
     return os << "Tree(" << git_oid_tostr(buf, 9, git_tree_id(tree)) <<")" ;
  }

  std::ostream& operator<<(std::ostream& os, git_oid oid)
  {
     char buf[10];
     return os << "Object(" << git_oid_tostr(buf, 9, &oid) <<")" ;
  }

  std::string
  error(char const * const file, int line)
  {
    const git_error *err = git_error_last();
    return std::string("[") + std::to_string(err->klass) + std::string("] ") + file + std::string(":") + std::to_string(line) + std::string(": ") + err->message;
  }

  #define Error error(__FILE__, __LINE__)
  #define Unexpected tl::make_unexpected(error(__FILE__, __LINE__))


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

      void create(const std::string& path, const std::string& repoName)
      {
        git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;

        opts.flags |= GIT_REPOSITORY_INIT_MKPATH; /* mkdir as needed to create repo */
        opts.flags |= GIT_REPOSITORY_INIT_BARE ;  /* Bare repository                */
        opts.description = repoName.data();       /* User given name                */

        if(git_repository_init_ext(&repo_, path_.data(), &opts) != 0)
          throw std::runtime_error(Error);
      }

    public:
      Repository(const std::string& path, const std::string& name)
      :path_(path), name_(name)
      {
        git_libgit2_init();
        if (exists(path_))
        {
          if (git_repository_open_ext(&repo_, path_.data(), GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) != 0)
            throw std::runtime_error(Error);
        }
        else
        {
           create(path_, name_);
        }
      }

      ~Repository()
      {
        if (repo_) git_repository_free(repo_);
        git_libgit2_shutdown();
      }

      operator git_repository*() const { return repo_; }

      static bool exists(const std::string& repoPath)
      {
        return git_repository_open_ext(nullptr, repoPath.data(), GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) == 0;
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

    Repository& repo_;               /*  git repository             */
    std::string ref_;                     /*  git reference (branch tip) */

    // Internal chaining information
    git_tree* prevCommitTree_;       /*  Commit tree                */
    git_commit const *parents_[2];   /*  Commit parents             */
    size_t  numParents_;             /*  number of parents          */
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
  getTree(Repository& repo,  const std::string& path, git_tree const * root)
  {
    git_tree_entry *entry;
    int result = git_tree_entry_bypath(&entry, root, path.data());

    if (result == GIT_ENOTFOUND)
      return nullptr;

    if (result !=  GIT_OK )
      return Unexpected;

    git_tree *tree = nullptr ;
    if (git_tree_entry_type(entry) == GIT_OBJECT_TREE) {
      result = git_tree_lookup(&tree, repo, git_tree_entry_id(entry));
      git_tree_entry_free(entry);
      if (result < 0)
        return Unexpected;

      return tree;
    }
    return tl::make_unexpected(path + " is not a direcotry");
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
  tl::expected<git_blob*, std::string>
  getBlob(Repository& repo,  const std::string& path, git_tree const * root)
  {
    git_object *object;
    int result = git_object_lookup_bypath(&object, reinterpret_cast<const git_object*>(root), path.data(), GIT_OBJECT_BLOB);
    if (result != 0)
      return Unexpected;

    /* if (result == GIT_ENOTFOUND) */
    /*   return tl::make_unexpected(path + " not found); */

    if (git_object_type(object) == GIT_OBJECT_BLOB)
      return reinterpret_cast<git_blob*>(object);

    return tl::make_unexpected(path + " is not a file");
  }


 /**
  * selecting a repository for chained calls
  *
  * @param repo[Repository] : The repository to work with
  **/
  tl::expected<context, std::string>
  repository(Repository& repo)
  {
    return context(repo, "HEAD");
  }


 /**
  * selecting a branch for chained calles
  *
  * @param name[std::string] : The branch's name
  **/
  auto branch(const std::string& name)
  {
    return [&name](context&& ctx) -> tl::expected<context, std::string> {
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
    };
  }


 /**
  * Adds a file in the current context (repo, branch)
  *
  * @param fullpath[std::string] : File name full path (relative to root)
  * @param content[std::string]  : File contents blob
  **/
  auto addFile(const std::string& fullpath, const std::string& content)
  {
    return [&fullpath, &content](context&& ctx) -> tl::expected<context, std::string> {
      // Blob handling
      git_oid elemOid;
      if (git_blob_create_from_buffer(&elemOid, ctx.repo_, content.data(), content.size()) != 0)
         return Unexpected;

      auto mode = GIT_FILEMODE_BLOB;

      PathTraverse pt(fullpath.data());
      char const * name = pt.filename();

      for (auto [path, dir] : pt)
      {
        auto prev = getTree(ctx.repo_, path, ctx.prevCommitTree_);
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
    };
  }

  /**
   * Commit previous updates to the context(Repository/Branch)
   *
   * @param author[std::string]  :  commiter's name (assuming commiter == author)
   * @param email[std::string]   :  commiter's email
   * @param message[std::string] :  Commit's message
   **/
  auto commit(const std::string& author, const std::string& email, const std::string& message)
  {
    return [&author, &email, &message](context&& ctx) -> tl::expected<git_oid, std::string>  {

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
    };
  }

  /**
   * Creates a branch in a given repository
   *
   * @param commitId[git_oid] : The originating commit for the branch
   * @param name[std::string]      : Branch name
   **/
  auto bifurcate(const git_oid& commitId, const std::string& name)
  {
     return [&commitId, &name](context&& ctx) -> tl::expected<context, std::string> {
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
     };
  }

  /**
   *
   **/
  auto read(const std::string& fullpath)
  {
    return [&fullpath](context&& ctx) -> tl::expected<std::string, std::string> {
      auto blob = getBlob(ctx.repo_, fullpath, ctx.prevCommitTree_);
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
    };
  }

};

using namespace dbg;

int main() {

  Repository repo("/home/pigmy/dev/git/gitdb/test/orig", "test db");

  // Initial commit. is performed on the main branch, but it cannnot reference it yet.
  // As a branch is reference to a commit. when there are not yet any.
  auto commitId = repository(repo)
    .and_then(addFile("README", "bla bla\n"))
    .and_then(commit("mno", "mno@nowhere.org", "...\n"));

  if (!commitId)
    std::cout <<  "Failed to create first commit" << commitId.error() << std::endl;

  auto res =repository(repo)
    .and_then(bifurcate(*commitId, "one"))
    .and_then(bifurcate(*commitId, "two"))
    .and_then(bifurcate(*commitId, "three"));

  if (!res)
    std::cout << "Unable to create branches: " << res.error() << std::endl;

  commitId = repository(repo)
             .and_then(branch("master"))
             .and_then(addFile("src/content/new.txt", "contentssssss"))
             .and_then(addFile("src/content/sub/more.txt", "check check"))
             .and_then(commit("mno", "mno@nowhere.org", "comment is long"));

  if (!res)
    std::cout << res.error() << std::endl;

  commitId = repository(repo)
             .and_then(branch("one"))
             .and_then(addFile("src/dev/c++/hello.cpp", "#include <hello.h>"))
             .and_then(addFile("src/dev/inc/hello.h", "#pragma once\n"))
             .and_then(commit("mno", "mno@nowhere.org", "comment is long"));

  if (!res)
    std::cout << res.error() << std::endl;

  commitId  = repository(repo)
              .and_then(branch("two"))
              .and_then(addFile("dictionary/music/a/abba", "mama mia"))
              .and_then(addFile("dictionary/music/p/petshop", "boys"))
              .and_then(addFile("dictionary/music/s/sandra", "dreams\nare dreams"))
              .and_then(commit("mno", "mno@nowhere.org", "comment is long"));

  if (!res)
    std::cout << res.error() << std::endl;

  auto content = repository(repo)
                .and_then(branch("two"))
                .and_then(read("dictionary/music/s/sandra"));

  if (!content)
    std::cout << "Unable to read file: " << content.error() << std::endl;

  std::cout << "Content: " << *content << std::endl;

  return 0;
}
