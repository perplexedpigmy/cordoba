#pragma once 
#include <tl/expected.hpp>
#include <expected.h>
#include <git2.h>
#include <err.h>
#include <filesystem>
#include <iostream> // TODO: remove

namespace gd {
  /**
   * Git resource owner abstraction, 
   * Ownership is unique and when the last ower goes out of scope, resource is destructed
   */
  template <typename T, void (*Terminator)(T*)>
  class Guard {
    T* r_; /* pointer to a git resource */
  
    /**
     * Take resource ownership from `other`
     * Set other resource to null to avoid it being freed
     */
    void take(Guard&& other) noexcept {
      if (this == &other) return;
      
      innerDestruct();
      std::swap(r_, other.r_);
      other.r_ = nullptr;
    }

    /**
     * Destruct the resouce if it exists
     */
    void innerDestruct() noexcept {
      if (r_) { 
        // std::cout << this << ": " << "Dtor " <<  r_ << " Type(" << typeid(r_).name() << ")" << std::endl;
        Terminator(r_); 
        r_ = nullptr;
      }
    }

    public:
    Guard(T* resource = nullptr) noexcept
    :r_(resource) {
      // std::cout << this << ": "<< "Ctor " <<  r_ << " Type(" << typeid(r_).name() << ")" << std::endl;
    }
  
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

    Guard(Guard&& other) noexcept { 
      // std::cout << &other << " -> " << this << ": "<< "Move [" <<  other.r_ << "] Type(" << typeid(r_).name() << ")" << std::endl;
      std::swap(r_, other.r_);
      other.r_ = nullptr;
    }

    Guard& operator=(Guard&& other) noexcept {
      // std::cout << &other << " -> " << this << ": "<< "Mov= [" <<  r_  << "] -> [" << other.r_ << "] Type(" << typeid(r_).name() << ")" << std::endl;
      take(std::move(other));
      return *this;
    }

    Guard& operator=(T* newResource) noexcept {
      innerDestruct();
      r_ = newResource;
      return *this;
    }

    operator bool() const noexcept { return r_ != nullptr; }
    operator T * const () const noexcept { return r_; }

    /** 
     * As an abstraction above a C library, upcasting is required
     * The method upcasts and takes the pointer
     */
    template <typename U, void(*F)(U*)>
    Guard<U,F> castMove() noexcept {
      U* resource = reinterpret_cast<U*>(this->r_);
      this->r_ = nullptr;
      return resource;
    }

    ~Guard() noexcept { 
      innerDestruct();
    }
  };

  /** RAII safe git types */
  using repository_t  = Guard<git_repository, git_repository_free>;
  using commit_t      = Guard<git_commit, git_commit_free>;
  using tree_t        = Guard<git_tree, git_tree_free>;
  using object_t      = Guard<git_object, git_object_free>;
  using blob_t        = Guard<git_blob, git_blob_free>;
  using treebuilder_t = Guard<git_treebuilder, git_treebuilder_free>;
  using signature_t   = Guard<git_signature, git_signature_free>;
  using reference_t   = Guard<git_reference, git_reference_free>;
  using entry_t       = Guard<git_tree_entry, git_tree_entry_free>;
}

/// @brief Finds a Blob(File) by its full path 
/// @param root The root tree(directory)
/// @param path full path to file
/// @return On successful retrieval a RAII git_blob pointer, otherwise an Error
Result<gd::blob_t>
getBlobFromTreeByPath(git_tree const * root, const std::filesystem::path& path) noexcept;

/// @brief Finds a Tree(Directory) by a git_oid
/// @param repo a pointer to an open git repository
/// @param treeOid the git_oid of the tree
/// @return On successful retrieval a RAII git_tree pointer, otherwise an Error
Result<gd::tree_t>
getTree(git_repository* repo, const git_oid* treeOid) noexcept;

/// @brief Retrieves a Tree(Directory) by path
/// @param repo a pointer to an open git repository
/// @param root The root tree of a context (tied to a chosen commit)
/// @param path The full path to the Tree(Directory)
/// @return On success a RAII git_tree pointer, otherwise an Error
Result<gd::tree_t>
getTreeRelativeToRoot(git_repository* repo, git_tree* root, const std::filesystem::path& path) noexcept;

/// @brief Retieves a Tree from a Commit
/// @param repo a pointer to an open git repository
/// @param commit The git_commit representing the location on the DAG
/// @return On success a corresponding RAII git_commit, otherwise an Error
Result<gd::tree_t>
getTreeOfCommit(git_repository* repo, git_commit* commit) noexcept;

/// @brief Retieves an object by a given revision string 
/// @param repo A pointer to an open git repository
/// @param spec A revision string. \link https://git-scm.com/docs/git-rev-parse.html#_specifying_revisions See more \endlink
/// @return On success a corresponding RAII git_object, otherwise an Error
Result<gd::object_t>
getObjectBySpec(git_repository* repo, const std::string& spec) noexcept;

/// @brief Retrieves a commit by a given revision string
/// @param repo A pointer to an open git repository
/// @param ref A revision string. \link https://git-scm.com/docs/git-rev-parse.html#_specifying_revisions See more \endlink
/// @return On success a corresponding RAII git_commit, otherwise an Error
Result<gd::commit_t>
getCommitByRef(git_repository* repo, const std::string& ref ) noexcept;

/// @brief Retrieves a commit by a given revision string
/// @param repo A pointer to an open git repository
/// @param commitId The commit's git_oid. Retrieved on `git_commit_create` 
/// @return On success a corresponding RAII git_commit, otherwise an Error
Result<gd::commit_t>
getCommitById(git_repository* repo, const git_oid* commitId) noexcept;

/// @brief Creates a TreeBuilder(Directory builder), to create a single directory
/// @param repo A pointer to an open git repository
/// @param tree A git_tree, as the base of the tree, if `nullptr` new tree is empty
/// @return On success returns a new RAII git_treebuilder, otherwise an Error
Result<gd::treebuilder_t>
getTreeBuilder(git_repository* repo, const git_tree* tree) noexcept;

/// @brief Creates a new signature to be used in a commit
/// @param author The author's name 
/// @param email The authoer's email
/// @return On success the new RAII git_signature, otherwise an Error
Result<gd::signature_t>
getSignature(const std::string& author, const std::string& email) noexcept;

/// @brief Creates a new branch and returns a reference to it
/// @param repo A pointer to an open git repository
/// @param name The new branche's name
/// @param commit The commit on the DAG from which to start the new branch
/// @return On success new RAII git_reference, otherwise an Error
/// 
/// Call for Branch creation doesn't chagne the context's branch
Result<gd::reference_t>
createBranch(git_repository* repo, const std::string& name, const git_commit* commit) noexcept; 

/// @brief Opens an exisiting repository
/// @param fullpath full path to the repository 
/// @return on success a new RIAA git_repository, otherwise an error
Result<gd::repository_t>
openRepository(const std::filesystem::path& fullpath) noexcept;

/// @brief Creates a new repository
/// @param fullpath full path to the new repository
/// @param name creator's name
/// @return On success a new RIAA git_repository, otherwise an error
Result<gd::repository_t>
createRepository(const std::string& fullpath, const std::string& name) noexcept;

/// @brief Find the entry on a provided `root`
/// @param root The root tree to search 
/// @param fullpath Full path of the entry's location on the tree
/// @return On success returns the tree_entry for an element in the tree (dir or blob), Otherwise an Error 
Result<gd::entry_t>
getTreeEntry(const git_tree* root, const std::string& fullpath);