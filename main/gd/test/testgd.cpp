#include <catch2/catch.hpp>
#include <pathTraverse.h>
#include <string>
#include <vector>
#include <set>
#include <utility>
#include <algorithm>
#include <iostream>
#include <gd/gd.h>
#include <tuple>
#include <experimental/filesystem>

using namespace std;
using namespace std::experimental::filesystem;
using namespace gd;
using namespace Catch;



/***************************************************************************
 *
 *                               Test cases
 *
 * *************************************************************************/


TEST_CASE("stores versions appropriately", "[pathTraverse]") {
  const static string testRepoPath{"/tmp/dgTestRepo"};
  const string initialFile{"README"};
  const string initialContent{"test text"};

  remove_all(testRepoPath);

  auto commitId = selectRepository(testRepoPath)
    .and_then(addFile(initialFile, initialContent))
    .and_then(commit("mno", "mno@nowhere.org", "...\n"));


  SECTION("initial Content") {
    REQUIRE(1 == 1);
    auto content = selectRepository(testRepoPath)
      .and_then(selectBranch("master"))
      .and_then(read(initialFile));

    REQUIRE(!content == false);
    REQUIRE(*content == initialContent);
  }

  SECTION("1 Branch") {
    const string readMeContent{"In branch one"};
    const set<pair<string, string>> fileAndContents = { { initialFile, "In branch one" },
                                                           { "a", "contents of A" },
                                                           { "b", "contents of B" },
                                                           { "dir/a", "contents of dir/a" },
                                                           { "dar/a", "contents of dar/a" },
                                                           { "dir/aa/f", "contents of dir/aa/f" } };

    auto res =selectRepository(testRepoPath)
      .and_then(fork(*commitId, "one"));

    for (const auto& [file, contents] : fileAndContents)
    {
      auto branchContent = selectRepository(testRepoPath)
        .and_then(selectBranch("one"))
        .and_then(addFiles(fileAndContents))
        .and_then(read(file));

      REQUIRE(!branchContent == false);
      REQUIRE(*branchContent == contents);
    }

  }
  SECTION("") {
  }

  SECTION("") {
  }

  gd::cleanup();
}
