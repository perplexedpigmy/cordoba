#define CATCH_CONFIG_MAIN
#define DEBUG_OUTPUT
#include <catch2/catch.hpp>
#include <string>
#include <vector>
#include <set>
#include <utility>
#include <algorithm>
#include <iostream>
#include <gd/gd.h>
#include <tuple>
#include <filesystem>

using namespace std;
using namespace std::filesystem;
using namespace gd;
using namespace gd::shorthand; // add >>, || chaining 
using namespace Catch;


/***************************************************************************
 *
 *                               Test cases
 *
 * *************************************************************************/

TEST_CASE("Simple CRUD no commit", "[crud] [nocommit]") {
  const static string testRepoPath{"/tmp/test/unit"};
  const string initialFile{"README"};
  const string initialContent{"test text"};
  cleanRepo(testRepoPath);

  SECTION("create") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent));
  
      REQUIRE(!result == false);
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent);
    
      REQUIRE(!result == false);
    }
  }

  SECTION("read") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(read(initialFile));
  
      REQUIRE(!result == false);
      REQUIRE(initialContent == result->content());
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> read(initialFile);
    
      REQUIRE(!result == false);
      REQUIRE(initialContent == result->content());
    }
  }

  SECTION("update") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(add(initialFile, initialContent + initialContent));
  
      REQUIRE(!result == false);
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> add(initialFile, initialContent + initialContent);
    
      REQUIRE(!result == false);
    }
  }

  SECTION("verified update") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(add(initialFile, initialContent + initialContent))
      .and_then(read(initialFile));
  
      REQUIRE(!result == false);
      REQUIRE(initialContent + initialContent == result->content());
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> add(initialFile, initialContent + initialContent)
      >> read(initialFile);
    
      REQUIRE(!result == false);
      REQUIRE(initialContent + initialContent == result->content());
    }
  }

  SECTION("delete") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(del(initialFile));
  
      REQUIRE(!result == false);
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> del(initialFile);
    
      REQUIRE(!result == false);
    }
  }

  SECTION("verified delete") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(del(initialFile))
      .and_then(read(initialFile));
  
      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "File deleted in uncommitted context");
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> del(initialFile)
      >> read(initialFile);
    
      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "File deleted in uncommitted context");
    }
  }
}


TEST_CASE("Simple CRUD one commit", "[crud] [commit]") {
  const static string testRepoPath{"/tmp/test/unit"};
  const string initialFile{"README"};
  const string initialContent{"test text"};
  cleanRepo(testRepoPath);

  SECTION("create") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message"));
  
      REQUIRE(!result == false);
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message");
    
      REQUIRE(!result == false);
    }
  }

  SECTION("read") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message"))
      .and_then(read(initialFile));
  
      REQUIRE(!result == false);
      REQUIRE(initialContent == result->content());
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message")
      >> read(initialFile);
    
      REQUIRE(!result == false);
      REQUIRE(initialContent == result->content());
    }
  }

  SECTION("update") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message"))
      .and_then(add(initialFile, initialContent + initialContent));
  
      REQUIRE(!result == false);
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message")
      >> add(initialFile, initialContent + initialContent);
    
      REQUIRE(!result == false);
    }
  }

  SECTION("verified update") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message"))
      .and_then(add(initialFile, initialContent + initialContent))
      .and_then(read(initialFile));
  
      REQUIRE(!result == false);
      REQUIRE(initialContent + initialContent == result->content());
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message")
      >> add(initialFile, initialContent + initialContent)
      >> read(initialFile);
    
      REQUIRE(!result == false);
      REQUIRE(initialContent + initialContent == result->content());
    }
  }

  SECTION("delete") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message"))
      .and_then(del(initialFile));
  
      REQUIRE(!result == false);
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message")
      >> del(initialFile);
    
      REQUIRE(!result == false);
    }
  }

  SECTION("verified delete") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message"))
      .and_then(del(initialFile))
      .and_then(read(initialFile));
  
      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "File deleted in uncommitted context");
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message")
      >> del(initialFile)
      >> read(initialFile);
    
      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "File deleted in uncommitted context");
    }
  }
}

TEST_CASE("Simple CRUD one commit + commited update", "[crud] [commit]") {
  const static string testRepoPath{"/tmp/test/unit"};
  const string initialFile{"README"};
  const string initialContent{"test text"};
  cleanRepo(testRepoPath);

  SECTION("update") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message 1"))
      .and_then(add(initialFile, initialContent + initialContent))
      .and_then(commit("test", "test@test.com", "commit message 2"));
  
      REQUIRE(!result == false);
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> add(initialFile, initialContent + initialContent)
      >> commit("test", "test@test.com", "commit message 2");
    
      REQUIRE(!result == false);
    }
  }

  SECTION("verified update") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message 1"))
      .and_then(add(initialFile, initialContent + initialContent))
      .and_then(commit("test", "test@test.com", "commit message 2"))
      .and_then(read(initialFile));
  
      REQUIRE(!result == false);
      REQUIRE(initialContent + initialContent == result->content());
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> add(initialFile, initialContent + initialContent)
      >> commit("test", "test@test.com", "commit message 2")
      >> read(initialFile);
    
      REQUIRE(!result == false);
      REQUIRE(initialContent + initialContent == result->content());
    }
  }

  SECTION("delete") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message 1"))
      .and_then(del(initialFile))
      .and_then(commit("test", "test@test.com", "commit message 2"));
  
      REQUIRE(!result == false);
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> del(initialFile)
      >> commit("test", "test@test.com", "commit message 2");
    
      REQUIRE(!result == false);
    }
  }

  SECTION("verified delete") {
    SECTION("and_then") {
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message 1"))
      .and_then(del(initialFile))
      .and_then(commit("test", "test@test.com", "commit message 2"))
      .and_then(read(initialFile));
  
      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "the path 'README' does not exist in the given tree");
    }

    SECTION("shorthand") {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> del(initialFile)
      >> commit("test", "test@test.com", "commit message 2")
      >> read(initialFile);
    
      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "the path 'README' does not exist in the given tree");
    }
  }
}


TEST_CASE("branch", "[crud] [branch]") {
  const static string testRepoPath{"/tmp/test/unit"};
  const string main = "main";
  const string other = "other";
  const string initialFile{"README"};
  const string initialContent{"test text"};
  const string otherFile("notimportant");
  const string otherFileContent("Boring");
  cleanRepo(testRepoPath);

  SECTION("Read file interduced in parent commit")  {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> createBranch(other)
      >> selectBranch(other)
      >> add(otherFile, otherFileContent)
      >> commit("test", "test@test.com", "commit message 2")
      >> read(initialFile);

      REQUIRE(!result == false);
      REQUIRE(initialContent == result->content());
  }

  SECTION("Read file commited in current branch")  {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> createBranch(other)
      >> selectBranch(other)
      >> add(otherFile, otherFileContent)
      >> commit("test", "test@test.com", "commit message 2")
      >> read(otherFile);

      REQUIRE(!result == false);
      REQUIRE(otherFileContent == result->content());
  }

  SECTION("Read file updated in current branch")  {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> createBranch(other)
      >> selectBranch(other)
      >> add(initialFile, otherFileContent )
      >> commit("test", "test@test.com", "commit message 2")
      >> read(initialFile);

      REQUIRE(!result == false);
      REQUIRE(otherFileContent == result->content());
  }

  SECTION("Switch back to main branch and read a file")  {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> createBranch(other)
      >> selectBranch(other)
      >> add(initialFile, otherFileContent )
      >> commit("test", "test@test.com", "commit message 2")
      >> selectBranch(main)
      >> read(initialFile);

      REQUIRE(!result == false);
      REQUIRE(initialContent == result->content());
  }

  SECTION("Switch back to main branch and try to read file introduced in other branch")  {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> createBranch(other)
      >> selectBranch(other)
      >> add(otherFile, otherFileContent )
      >> commit("test", "test@test.com", "commit message 2")
      >> selectBranch(main)
      >> read(otherFile);

      REQUIRE(!result == true);
      REQUIRE("the path 'notimportant' does not exist in the given tree" == result.error()._msg);
  }
}

TEST_CASE("rollback", "[crud] [rollback]") {
  const static string testRepoPath{"/tmp/test/unit"};
  const string initialFile{"README"};
  const string initialContent{"test text"};
  cleanRepo(testRepoPath);

  SECTION("rollback empties collected files")  {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> rollback()
      >> commit("test", "test@test.com", "commit message 1");

      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "Nothing to commit");
  }
}

TEST_CASE("Errors", "[crud] [error]") {
  const string other = "other";
  const static string testRepoPath{"/tmp/test/unit"};
  const string initialFile{"README"};
  const string initialContent{"test text"};
  cleanRepo(testRepoPath);

  SECTION("selecting unknown branch")  {
      auto result = selectRepository(testRepoPath)
      >> selectBranch(other);

      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "revspec 'refs/heads/other' not found");
  }

  // In Git the first commit is bit special because when there is no commit there are no references
  SECTION("empty first commit")  {
      auto result = selectRepository(testRepoPath)
      >> commit("test", "test@test.com", "commit message");

      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "Nothing to commit");
  }

  // Any consecutive commit is the same, testing for 2nd is enough 
  SECTION("empty second commit")  {
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> commit("test", "test@test.com", "commit message 2");

      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "Nothing to commit");
  }

  SECTION("empty second commit with Using or_else")  {
      std::string errorMsg;
      auto result = selectRepository(testRepoPath)
      .and_then(add(initialFile, initialContent))
      .and_then(commit("test", "test@test.com", "commit message 1"))
      .and_then(commit("test", "test@test.com", "commit message 2"))
      .or_else([&](const auto& err) { errorMsg = err._msg; });

      REQUIRE(!result == true);
      REQUIRE(errorMsg == "Nothing to commit");
  }

  SECTION("empty second commit with Using or_else shorthand")  {
      std::string errorMsg;
      auto result = selectRepository(testRepoPath)
      >> add(initialFile, initialContent)
      >> commit("test", "test@test.com", "commit message 1")
      >> commit("test", "test@test.com", "commit message 2")
      || [&](const auto& err) { errorMsg = err._msg; };

      REQUIRE(!result == true);
      REQUIRE(errorMsg == "Nothing to commit");
  }

  // Branchihg requires a commit to branch from.
  SECTION("Impposible to create branch before first commit")  {
      auto result = selectRepository(testRepoPath)
      >> createBranch("First");

      REQUIRE(!result == true);
      REQUIRE(result.error()._msg == "invalid argument: 'commit'");
  }

}