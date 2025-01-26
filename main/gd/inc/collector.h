#pragma once 
#include <expected.h>
#include <guard.h>


#include <map>
#include <filesystem>
#include <cstring>
#include <git2.h>

namespace gd {
  struct Context;

  /// @brief Abstraction describing an Object udpate and an `action` to apply the update to a git repository
  class ObjectUpdate {
    using Action = Result<void>(ObjectUpdate::*)(git_treebuilder*) const noexcept;

    git_oid oid_;
    git_filemode_t mod_;
    std::string name_;
    Action action_;

    /// @brief Inserts an object to a given directory, Object can be a Tree(dir) or a Blob(file)
    /// @param dir The owning directory 
    /// @param obj The Object to be added
    Result<void> insert(git_treebuilder *bld) const noexcept {
      if(git_treebuilder_insert(nullptr, bld, name_.c_str(), &oid_, mod_) != 0)
        return unexpected_git;
      return Result<void>();
    }

    Result<void> remove(git_treebuilder *bld) const noexcept {
      if(git_treebuilder_remove(bld, name_.c_str()))
        return unexpected_git;
      return Result<void>();
    }
    
    ObjectUpdate(std::string&& name, git_filemode_t mod, Action action) 
    :name_(name), mod_(mod), action_(action) {
      std::memset(&oid_, 0, sizeof(git_oid));
    }

    static ObjectUpdate create(const std::filesystem::path& fullpath, git_filemode_t mod, Action action) {
      return ObjectUpdate(fullpath.filename(), mod, action);
    }

    public:
      git_oid const * oid() const noexcept { return &oid_; }
      git_filemode_t mod() const noexcept { return mod_; }
      const std::string&  name() const noexcept { return name_; }

      /// @return  returns True if the ObjectUpdate is delete in uncommited context
      bool isDelete() const noexcept {
        return git_oid_iszero(&oid_);
      }

      /// @brief Creates a blob (gitspeak for a file) on a 'fullpath' location with 'content'
      /// @param ctx The context used to access the repository
      /// @param fullpath Full path of the blob including the actual file name
      /// @param content The blob's full content.
      /// @return On success the Object representation of the Blob in the repository, otherwise an error.
      static Result<ObjectUpdate> 
      createBlob(gd::Context& ctx,const std::filesystem::path& fullpath, const std::string& content) noexcept; 

      /// @brief Creates a blob or tree from a git tree entry
      /// @param ctx The context used to access the repository
      /// @param fullpath Full path of the blob including the actual file name
      /// @param entry The entry to be added
      /// @return On success the Object representation of the added entry, otherwise an error.
      static Result<gd::ObjectUpdate> 
      fromEntry(gd::Context& ctx,const std::filesystem::path& fullpath, const git_tree_entry* entry) noexcept;

      /// @brief Creates a Tree/Directory 
      /// @param fullpath Full path of the directory including the directory name 
      /// @param builder The git treebuilder of a for the given directory
      /// @return On success the Object representation of the added directory, otherwise an error.
      static Result<ObjectUpdate> 
      createDir(const std::filesystem::path& fullpath, treebuilder_t& builder) noexcept;

      /// @brief Removes a Blob or Tree from a directory
      /// @param fullpath The full path including the name of the entry to be removed
      /// @return On success the Object representation of the removal, otherwise an error.
      static Result<ObjectUpdate> 
      remove(const std::filesystem::path& fullpath) noexcept;

      /// @brief Applies the Update into the git repository
      /// @param builder the builder representing the direcotry on the update
      /// @return On success nothing, otherwise an error.
      Result<void>
      gitIt(git_treebuilder* builder) const noexcept;

  };

  std::ostream& operator<<(std::ostream& os, ObjectUpdate const& ) noexcept;

  /// @brief Each file or directory updated requires to construct the entire tree from its location 
  ///        up to the root directory, thus the updates are applied from longenst path to shorter path
  ///        any each such element adds it's parent direcotry if it doesn't exist
  ///        `LongestPathFirst` sorts the Updates in this proper order
  struct LongerPathFirst {
    bool operator()(const std::filesystem::path& a, const std::filesystem::path& b) const {
      auto aLen = std::distance(a.begin(), a.end());
      auto bLen = std::distance(b.begin(), b.end());
      return (aLen != bLen) ? aLen > bLen : a > b;
    }
  };

  /// @brief A Collector of all updates to be commited 
  ///        The abstraction is introduced to have only one update per directory per commit
  class TreeCollector {
    using Directory = std::filesystem::path;
    using ObjectList = std::vector<ObjectUpdate>;
    using DirectoryObjects = std::pair<Directory, ObjectList>;

    std::map<Directory, ObjectList, LongerPathFirst> dirObjs_;

    /// @brief Inserts a file at a directory.
    /// @param fullpath  Directory owning the object. Example: from/root
    /// @param obj and Object representing a directory or file
    /// Collects objects per dir 
    void insert(const std::filesystem::path& dir, const ObjectUpdate&& obj) noexcept;

    public:

    /// @brief inserts a Blob(gitspeak for File) into a directo
    /// @param ctx the context used to access the repository
    /// @param fullpath Full path of a file(or in gitspeak Blob) including filename
    /// @param content the entire file content
    /// @return On success nothing, and Error otherwise.
    Result<void> 
    insertFile(gd::Context& ctx, const std::filesystem::path& fullpath, const std::string& content) noexcept;

    /// @brief Inserts an entry designated by `entry` of type git_tree_entry.
    /// @param ctx the context used to access the repository
    /// @param fullpath Full path of a file(or in gitspeak Blob) including filename
    /// @param entry A git tree entry (could be a file/blob or a directory/tree)
    /// @return On success nothing, and Error otherwise.
    Result<void> 
    insertEntry(gd::Context& ctx, const std::filesystem::path& fullpath, const git_tree_entry* entry) noexcept;

    /// @brief Removes a File (Tree or Blob) 
    /// @param ctx the context used to access the repository
    /// @param fullpath Full path of the File
    /// @return On success nothing, and Error otherwise.
    Result<void>
    removeFile(gd::Context& ctx, const std::filesystem::path& fullpath) noexcept;

    /// @brief Writes all the collected updates to git
    /// @param ctx the context used to access the repository
    /// @return On success returns RAII flavoured git_tree which is the new root tree containing updates, otherwise an Error.
    Result<gd::tree_t> 
    apply(gd::Context& ctx) noexcept;

    /// @brief Clears all the collected updates on all directories
    void clean() noexcept {
      dirObjs_.clear();
    }

    /// @brief Tests whether there are any updates collected
    /// @return True if at least one update was collected, otherwise False 
    bool empty() const noexcept {
      return dirObjs_.empty();
    }

    /// @brief retrieves a not yet committed blob from the collector
    /// @param fullpath The full path of blob to retrieve
    /// @return On success returns a blob, otherwise an Error
    Result<git_blob*> 
    getBlobByPath(const Context& ctx, const std::filesystem::path& fullpath) const noexcept; 
  };
}