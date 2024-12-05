#pragma once

#include <string>
#include <utility>
#include <stdexcept>

/**
 * Class to add an iterative ability on a path like string
 * The path always contains a filename (right most entry)
 *
 * The class is constructed for the benefit of its dbg application usage.
 * The filenme is identified and the directory structure can be traversed in reverse order.
 * From path containing the leaf(file) to the outermost directory.
 * This is due to the libgit2 needs of tree construction from leaf(file) all the way through
 * the containing directory stucture up to the tree associated with the commit.
 *
 * The implmenentation doesn't conform to the iterator std requirements beyound its
 * ability to support range loops, it also takes ownership of the 'fullpath' provided
 * and may update it. Once the string is handed to PathTraverse it shouldn't be used again.
 *
 **/
class PathTraverse
{
   private:
     size_t pos_;
     mutable std::string fullpath_;

   public:
     PathTraverse(std::string&& fullpath)
     :pos_(fullpath.find_last_of('/')), fullpath_(std::move(fullpath)) {
       if (fullpath_.size() == 0)
         throw std::runtime_error("PathTraverse ctor: Empty path");
     }

     char const * filename() const { return &fullpath_[pos_ + 1]; }

     class Iterator
     {
        private:
          size_t pos_;
          mutable std::string * refFullPath;
          size_t start_;
        public:

          using iterator_category = std::input_iterator_tag;
          using difference_type   = std::ptrdiff_t;
          using value_type        = std::pair<char const *, char const *>;
          using pointer           = value_type*;
          using reference         = value_type&;


          Iterator(size_t pos = std::string::npos, std::string *refDirs = nullptr )
          :pos_(pos), refFullPath(refDirs), start_(refDirs and (*refDirs)[0] == '/' ? 1 : 0)
          {
            if (refDirs)
              operator++();

          }

          value_type operator*() const { return std::make_pair(&(*refFullPath)[start_],  &(*refFullPath)[pos_]);  }

          /**
           * Destructive iterator in reverse through directory constituting the path
           * supports paths with root (postfixed with a slash) and without
           * Each term(directory) munched off of the left of the string is forcibly null-terminated
           * This allows to both avoid string creation as well as provide libgit2 directly its expected type (char*)
           *
           * The last term(directory) may or may not be prefixed with a slash. the algorithm is to
           * search for slashes backward when not found. pos_ is set to 0, having two effects
           * - The operator* call will take the term starting at character 0
           * - Next call to this ++ function will set it to string::npos. effectively ending the iteration.
           *
           * When the string is preceded by a slash. pos_ is set to 1
           * - operation* will take the term starting at 1, avoiding the first slash
           * - Next call to ++ will set pos_ to string::npos. effectively ending the iteration.
           **/
          Iterator& operator++() {
            if (pos_ > 0 and not (pos_ == 1 and refFullPath->operator[](0) == '/' ) )
            {
              refFullPath->operator[]( pos_ - 1) = '\0';

              if (pos_ = refFullPath->find_last_of('/', pos_); pos_ == std::string::npos or pos_ == 0)
                pos_ = pos_ == 0 ? 1 : 0 ; // On pos_ == 0 there is '/' @[0] avoid it, otherwise no preceding slash
              else
                ++pos_;   // Avoid the slash
            }
            else
            {
               pos_ = std::string::npos;
               refFullPath = nullptr;
            }
            return *this;
          }
          friend bool operator!= (const Iterator& a, const Iterator&);
     };

     Iterator begin() const { return Iterator(pos_+1, &fullpath_); }
     Iterator end() const   { return Iterator(); }
};

inline bool operator!= (const PathTraverse::Iterator& lhs, const PathTraverse::Iterator& rhs)
{
  return lhs.pos_        !=  rhs.pos_        and
         lhs.refFullPath !=  rhs.refFullPath;
};
