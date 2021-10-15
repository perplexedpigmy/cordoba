#include <git2.h>
#include <tl/expected.hpp>
#include <map>
#include <iostream>

#include "pathTraverse.h"

using namespace std;
using namespace tl;

namespace {
  thread_local unordered_map<string, git_tree*> sTreeCache;
}

/**
 * Assert success
 */
void check(int error)
{
  if (error < 0)
  {
    const git_error *err = git_error_last();
    cerr << "Git Error(" << error << "):  " << err->klass << "/" << err->message << endl;
    exit(1);
  }
}

  ostream& operator<<(ostream& os, git_tree* tree)
  {
     char buf[10];
     return os << "Tree(" << git_oid_tostr(buf, 9, git_tree_id(tree)) <<")" ;
  }

  ostream& operator<<(ostream& os, git_oid oid)
  {
     char buf[10];
     return os << "Object(" << git_oid_tostr(buf, 9, &oid) <<")" ;
  }


string error(char const * const file, int line)
{
  const git_error *err = git_error_last();
  return "["s + to_string(err->klass) + "] " + file + ":" + to_string(line) + ": " + err->message;
}
#define Error make_unexpected(error(__FILE__, __LINE__))

bool repositoryExists()
{
  return true;
}

// TODO: Implement
void createRepository()
{
}

void createObject()
{
}

void createTree()
{
}

void commit()
{
}

/**
 * Some documentation
 **/
class Git
{
  private:
    git_repository* repo_ = nullptr;
    const string    repoPath_;

  public:
    Git(const std::string& repoPath)
      :repoPath_(repoPath)
    {
      git_libgit2_init();
    }

    ~Git()
    {

      git_libgit2_shutdown();
    }

    static bool repositoryExists(const std::string& repoPath)
    {
      return git_repository_open_ext(nullptr, repoPath.data(), GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) == 0;
    }


    void createRepo(const std::string& repoName)
    {
      git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;

      opts.flags |= GIT_REPOSITORY_INIT_MKPATH; /* mkdir as needed to create repo */
      opts.flags |=  GIT_REPOSITORY_INIT_BARE ; /* Bare repository */
      opts.description = repoName.data();

      check(git_repository_init_ext(&repo_, "/home/pigmy/dev/git/gitdb/test/orig", &opts));
    }

    static git_oid oid(char * const sha)
    {
      git_oid ret;

      check(git_oid_fromstr(&ret , sha));

      return ret;
    }

    static std::string sha(git_oid const * oid)
    {
      char buf[GIT_OID_HEXSZ+1];

      return git_oid_tostr(buf, GIT_OID_HEXSZ, oid);
    }

    git_oid createBlob(const string& content)
    {
      git_oid oid;
      check(git_blob_create_from_buffer(&oid, repo_, content.data(), content.size()));

      return oid;
    }

    git_oid createTree(const string& filename, const string& content)
		{
			git_treebuilder *bld = nullptr;
			check(git_treebuilder_new(&bld, repo_, nullptr));

			/* Add some entries */
      auto objId = createBlob(content);

			check(git_treebuilder_insert(nullptr, bld,
					filename.data(),     /* filename */
					&objId,              /* OID */
					GIT_FILEMODE_BLOB)); /* mode */

			git_oid oid;
			check(git_treebuilder_write(&oid, bld));
			git_treebuilder_free(bld);

			return oid;

			/* git_treebuilder *bld = nullptr; */
			/* check(git_treebuilder_new(&bld, repo_, nullptr)); */

			/* /1* Add some entries *1/ */
			/* git_object *obj = nullptr; */
			/* check(git_revparse_single(&obj, repo_, "HEAD:README.md")); */
			/* check(git_treebuilder_insert(nullptr, bld, */
			/* 		"README.md",         /1* filename *1/ */
			/* 		git_object_id(obj),  /1* OID *1/ */
			/* 		GIT_FILEMODE_BLOB)); /1* mode *1/ */
			/* git_object_free(obj); */

			/* check(git_revparse_single(&obj, repo_, "v0.1.0:foo/bar/baz.c")); */
			/* check(git_treebuilder_insert(NULL, bld, */
			/* 		"d.c", */
			/* 		git_object_id(obj), */
			/* 		GIT_FILEMODE_BLOB)); */
			/* git_object_free(obj); */

			/* git_oid oid; */

			/* check(git_treebuilder_write(&oid, bld)); */
			/* git_treebuilder_free(bld); */

			/* return oid; */
    }

    git_oid createCommit()
		{
			git_signature *me = nullptr;
			check(git_signature_now(&me, "Me", "me@example.com"));
			git_tree *tree = nullptr;

			git_oid commit_id;
			check(git_commit_create(
					&commit_id,
					repo_,
					"HEAD",                      /* name of ref to update */
					me,                          /* author */
					me,                          /* committer */
					"UTF-8",                     /* message encoding */
					"Flooberhaul the whatnots",  /* message */
					tree,                        /* root tree */
					0,                           /* parent count */
					nullptr));                   /* parents */
			return commit_id;
    }

    git_oid commit(const string &author,
                   const string &email,
                   const string &message,
                   const string &filename,
                   const string &content,
                   size_t nparents,
									 const git_commit *parents)
    {
      // 1. Create BLOB
      git_oid blobOid;
      check(git_blob_create_from_buffer(&blobOid, repo_, content.data(), content.size()));

			// 2. Create tree with only 1 File
			git_treebuilder *bld = nullptr;
			check(git_treebuilder_new(&bld, repo_, nullptr));

			check(git_treebuilder_insert(nullptr, bld,
					filename.data(),     /* filename */
					&blobOid,            /* OID */
					GIT_FILEMODE_BLOB)); /* mode */

			git_oid treeOid;
			check(git_treebuilder_write(&treeOid, bld));
			git_treebuilder_free(bld);

      /****************************************************************************************
       *    for another directory in between
       * **************************************************************************************/
#if 0
      check(git_treebuilder_new(&bld, repo_, nullptr));

			check(git_treebuilder_insert(nullptr, bld,
					"root",              /* filename */
					&treeOid,            /* OID */
					GIT_FILEMODE_TREE)); /* mode */

			check(git_treebuilder_write(&treeOid, bld));
			git_treebuilder_free(bld);
#endif
      /****************************************************************************************
       *
       * **************************************************************************************/
      // 3. Create a commit
			git_signature *commiter = nullptr;
			check(git_signature_now(&commiter, author.data(), email.data()));
			git_tree *tree = nullptr;

      check(git_tree_lookup( &tree, repo_, &treeOid));

			git_oid commit_id;
			check(git_commit_create(
					&commit_id,
					repo_,
					"HEAD",          /* name of ref to update */
					commiter,        /* author */
					commiter,        /* committer */
					"UTF-8",         /* message encoding */
					"root",  /* message */
				  tree,            /* root tree */
					0,               /* parent count */
					nullptr));       /* parents */

			return commit_id;
    }

    struct context
    {
      string ref;
      /* git_treebuilder* builder; */
      git_tree* prevCommitTree;
      git_commit const *  parents[2] = {nullptr, nullptr};
      size_t  numParents = 1;
    };

    expected<context, string> chooseBranch(const string& name)
    {
      context ctx;
      ctx.ref = "refs/heads/"s + name;

      git_object* commitObj = nullptr;

      if(git_revparse_single(&commitObj, repo_, name.data()) != 0)
        return Error;

      if (git_object_type(commitObj) != GIT_OBJECT_COMMIT)
        return make_unexpected(name + " doesn't reference a commit");

      git_commit* commit = reinterpret_cast<git_commit*>(commitObj);

      ctx.parents[0] = commit;

      if(git_commit_tree(&ctx.prevCommitTree, commit) != 0)
        return Error;

      return std::move(ctx);
    }

    expected<git_tree*, string> getTree(const string& path, git_tree const * root)
    {
      git_tree_entry *entry;
      int result = git_tree_entry_bypath(&entry, root, path.data());

      if (result == GIT_ENOTFOUND)
        return nullptr;

      if (result !=  GIT_OK )
        return Error;

      git_tree *tree = nullptr ;
      if (git_tree_entry_type(entry) == GIT_OBJECT_TREE) {
        result = git_tree_lookup(&tree, repo_, git_tree_entry_id(entry));
        git_tree_entry_free(entry);
        if (result < 0)
          return Error;

        return tree;
      }

      return make_unexpected(path + " is not a direcotry");
    }


    auto addFile(const string& fullpath, const string& content)
    {
      return [this, fullpath, content](context&& ctx) -> expected<context, string> {
        // Blob handling
        git_oid elemOid;
        if (git_blob_create_from_buffer(&elemOid, repo_, content.data(), content.size()) != 0)
           return Error;

        auto mode = GIT_FILEMODE_BLOB;

        PathTraverse pt(fullpath.data());
        char const * name = pt.filename();

        for (auto [path, dir] : pt)
        {
          cout << "Handling " << name << endl;

          auto prev = getTree(path, ctx.prevCommitTree);
          if (!prev)
            return Error;

          if (*prev == nullptr)
            cout << "--  Empty " << path << "( adding " << name << ") " << ctx.prevCommitTree << endl;
          else
          {
            cout << "--  Setting " << path << " from " << *prev << endl;
          }

          git_treebuilder *bld = nullptr;
          if (git_treebuilder_new(&bld, repo_, *prev) != 0)
            return Error;

			    if(git_treebuilder_insert(nullptr, bld, name, &elemOid, mode) != 0)
            return Error;

          if(git_treebuilder_write(&elemOid, bld) != 0)
            return Error;

          git_treebuilder_free(bld);
          mode = GIT_FILEMODE_TREE;

          name = dir;
        }

        git_treebuilder *bld = nullptr;
        if(git_treebuilder_new(&bld, repo_, ctx.prevCommitTree) != 0)
          return Error;

        if(git_treebuilder_insert(nullptr, bld, name, &elemOid, mode) != 0)
          return Error;

        if(git_treebuilder_write(&elemOid, bld) != 0)
          return Error;

        git_treebuilder_free(bld);

        if(git_tree_lookup(&ctx.prevCommitTree, repo_, &elemOid) != 0)
          return Error;

        cout << "New tree id: " << ctx.prevCommitTree << endl << endl;

        return std::move(ctx);
      };
    }

    auto commit(const string& author, const string& email, const string& message)
    {
      return [this, author, email, message](context&& ctx) -> expected<context, string>  {

        git_signature *commiter = nullptr;
        if(git_signature_now(&commiter, author.data(), email.data()) != 0 )
          return Error;

        git_oid commit_id;
        int result = git_commit_create(
              &commit_id,
              repo_,
              ctx.ref.data(),  /* name of ref to update */
              commiter,                                  /* author */
              commiter,                                  /* committer */
              "UTF-8",                                   /* message encoding */
              message.data(),                            /* message */
              ctx.prevCommitTree,                        /* root tree */
              ctx.numParents,                            /* parent count */
              ctx.parents);                              /* parents */
        if (result != 0)
          return Error;

        return ctx;
      };
    }

    git_oid commit(const string &branch,
                   const string &author,
                   const string &email,
                   const string &message,
                   const string &filename,
                   const string &content)
    {
      // 0. Identify the HEAD of the Branch
      //git_reference* branchRef = nullptr;

      //check(git_branch_lookup(&branchRef, repo_, branch.data(), GIT_BRANCH_LOCAL));

      git_object* commitObj = nullptr;
      check(git_revparse_single(&commitObj, repo_, branch.data()));

      if (git_object_type(commitObj) != GIT_OBJECT_COMMIT)
      {
        cerr << "Not a commit" << branch << endl;
        exit(1);
      }

      git_commit const * parent[] = { reinterpret_cast<git_commit*>(commitObj) };

      // 1. Create BLOB
      git_oid blobOid;
      check(git_blob_create_from_buffer(&blobOid, repo_, content.data(), content.size()));

			// 2. Create tree with only 1 File
      git_tree* prevTree = nullptr;
      check(git_commit_tree(&prevTree, reinterpret_cast<git_commit*>(commitObj)));
			git_treebuilder *bld = nullptr;

			check(git_treebuilder_new(&bld, repo_, prevTree));

			check(git_treebuilder_insert(nullptr, bld,
					filename.data(),     /* filename */
					&blobOid,            /* OID */
					GIT_FILEMODE_BLOB)); /* mode */

			git_oid treeOid;
			check(git_treebuilder_write(&treeOid, bld));
			git_treebuilder_free(bld);

      // 3. Create a commit
			git_signature *commiter = nullptr;
			check(git_signature_now(&commiter, author.data(), email.data()));
			git_tree *tree = nullptr;

      check(git_tree_lookup( &tree, repo_, &treeOid));

			git_oid commit_id;
			check(git_commit_create(
					&commit_id,
					repo_,
					("refs/heads/"s + branch).data(),         /* name of ref to update */
					commiter,        /* author */
					commiter,        /* committer */
					"UTF-8",         /* message encoding */
					message.data(),  /* message */
					tree,            /* root tree */
					1,               /* parent count */
					parent));       /* parents */

      // Release the tree

			return commit_id;
    }

    void createBranch(const git_oid& commitId, const string& name)
    {
       git_commit *commit = nullptr;
       check(git_commit_lookup(&commit, repo_, &commitId));

       git_reference* branchRef = nullptr;
       check(git_branch_create(&branchRef, repo_, name.data(), commit, 0 /* no force */));

       git_reference_free(branchRef);
       git_commit_free(commit);
    }
};

int main() {

  Git git("/home/pigmy/dev/git/gitdb/test/generated");

  git.createRepo("ENV Test");
  git_oid commitId = git.commit("mno", "mno@nowhere.spec", "init", "README", "ENV disclaimer", 0, nullptr);

  git.createBranch(commitId, "one");
  git.createBranch(commitId, "two");
  git.createBranch(commitId, "three");

  auto res = git.chooseBranch("master")
    .and_then(git.addFile("src/content/new.txt", "contentssssss"))
    .and_then(git.addFile("src/content/sub/more.txt", "check check"))
    .and_then(git.commit("mno", "mno@nowhere.org", "comment is long"));

  if (!res)
    cout << res.error() << endl;

  res = git.chooseBranch("one")
    .and_then(git.addFile("src/dev/c++/hello.cpp", "#include <hello.h>"))
    .and_then(git.addFile("src/dev/inc/hello.h", "#pragma once\n"))
    .and_then(git.commit("mno", "mno@nowhere.org", "comment is long"));

  if (!res)
    cout << res.error() << endl;

  res = git.chooseBranch("two")
    .and_then(git.addFile("dictionary/music/a/abba", "mama mia"))
    .and_then(git.addFile("dictionary/music/p/petshop", "boys"))
    .and_then(git.addFile("dictionary/music/s/sandra", "dreams"))
    .and_then(git.commit("mno", "mno@nowhere.org", "comment is long"));

  if (!res)
    cout << res.error() << endl;


  return 0;
}
