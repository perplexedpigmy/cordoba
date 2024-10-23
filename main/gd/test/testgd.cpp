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

  cleanRepo(testRepoPath);

  auto commitId = selectRepository(testRepoPath)
    .and_then(add(initialFile, initialContent))
    .and_then(commit("mno", "mno@nowhere.org", "...\n"));

  SECTION("initial Content") {
    auto content = selectRepository(testRepoPath)
      .and_then(selectBranch("master"))
      .and_then(read(initialFile))
      .or_else( [](const auto& err) { std::cout << err << std::endl;});

    REQUIRE(!content == false);
    REQUIRE(*content == initialContent);
  }

  SECTION("1 Branch") {
    const string branchName{"one"};
    // const string readMeContent{"In branch one"};
    const set<pair<string, string>> fileAndContents = {{initialFile, "In branch one"},
                                                       {"a", "contents of A"},
                                                       {"b", "contents of B"},
                                                       {"dir/a", "contents of dir/a"},
                                                       {"dar/a", "contents of dar/a"},
                                                       {"dir/aa/f", "contents of dir/aa/f"}};

    auto res = selectRepository(testRepoPath)
      .and_then(createBranch(*commitId, branchName))
      .or_else( [](const auto& err) { std::cout << err << std::endl;});

    auto branchContext = selectRepository(testRepoPath)
      .and_then(selectBranch(branchName))
      .and_then(add(fileAndContents))
      .and_then(commit("mno", "mno@xmousse.com.org", "comment is long"))
      .or_else( [](const auto& err) { std::cout << err << std::endl;});

    for (const auto& [file, contents] : fileAndContents)
    {
      auto branchContent = selectRepository(testRepoPath)
      .and_then(selectBranch(branchName))
      .and_then(read(file))
      .or_else( [](const auto& err) { std::cout << err << std::endl;});

      auto abc = *branchContent;
      REQUIRE(!branchContent == false);
      REQUIRE(*branchContent == contents);
    }

  }

  SECTION("") {
  }

  SECTION("") {
  }

}
