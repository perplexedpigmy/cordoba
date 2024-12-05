#define DEBUG_OUTPUT
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
#include <filesystem>

using namespace std;
using namespace std::filesystem;
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

  // auto commitId = selectRepository(testRepoPath)
  //   .and_then(add(initialFile, initialContent))
  //   .and_then(commit("mno", "mno@nowhere.org", "...\n"));

  // SECTION("initial Content") {
  //   auto content = selectRepository(testRepoPath)
  //     .and_then(selectBranch("master"))
  //     .and_then(read(initialFile))
  //     .or_else( [](const auto& err) { std::cout << err << std::endl;});

  //   REQUIRE(!content == false);
  //   REQUIRE(*content == initialContent);
  // }

  // SECTION("1 Branch") {
  //   const string branchName{"one"};
  //   // const string readMeContent{"In branch one"};
  //   const set<pair<string, string>> fileAndContents = {{initialFile, "In branch one"},
  //                                                      {"a", "contents of A"},
  //                                                      {"b", "contents of B"},
  //                                                      {"dir/a", "contents of dir/a"},
  //                                                      {"dar/a", "contents of dar/a"},
  //                                                      {"dir/aa/f", "contents of dir/aa/f"}};

  //   auto res = selectRepository(testRepoPath)
  //     .and_then(createBranch(*commitId, branchName))
  //     .or_else( [](const auto& err) { std::cout << err << std::endl;});

  //   auto branchContext = selectRepository(testRepoPath)
  //     .and_then(selectBranch(branchName))
  //     .and_then(add(fileAndContents))
  //     .and_then(commit("mno", "mno@xmousse.com.org", "comment is long"))
  //     .or_else( [](const auto& err) { std::cout << err << std::endl;});

  //   for (const auto& [file, contents] : fileAndContents)
  //   {
  //     auto branchContent = selectRepository(testRepoPath)
  //     .and_then(selectBranch(branchName))
  //     .and_then(read(file))
  //     .or_else( [](const auto& err) { std::cout << err << std::endl;});

  //     auto abc = *branchContent;
  //     REQUIRE(!branchContent == false);
  //     REQUIRE(*branchContent == contents);
  //   }

  // }

  SECTION("Test implementation") {
    const string branchName{"one"};
    // auto res = selectRepository(testRepoPath, "testuser", false /* not bare */)
    //   .and_then(createBranch(*res, branchName))
    //   .or_else( [](const auto& err) { std::cout << err << std::endl;});

    auto branchContext = selectRepository(testRepoPath, "testuser")
      .and_then(add("README", "no"))
      .and_then(add("1/2/3/a", "1/2/3/a"))
      .and_then(add("1/2/3/b", "1/2/3/b"))
      // .and_then(add("1/2/c", "1/2/c"))
      // .and_then(add("README", "README"))
      // .and_then(add("1/2/3/4/d", "1/2/3/4/d"))
      // .and_then(add("1/2/e", "1/2/e"))
      // .and_then(add("1/f", "1/f"))
      // .and_then(add("1/g", "1/g"))
      .and_then(commit("mno", "mno@xmousse.com.org", "comment is long"));

  }

  SECTION("") {
  }

}
