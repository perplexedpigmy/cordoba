#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <pathTraverse.h>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <iostream>
#include <tuple>

using namespace std;

/**
 *  Constructs a path from dirs concatenated in reverse order + a file name
 *  Given dirs = { d1 d2 d3 } and file = file
 *  Yields  d3/d2/d1/file
 **/
string createPath(const vector<string>& dirs, const string& file)
{
  string path;
  for (const string& dir : dirs)
  {
     path = dir + '/' + path;
  }
  return path + file;
}


/**
 * Auxilary function to create the compare vectors
 * The first(expected) is created from the original vector that generated the path, but reversed
 * The second(result) is created by iterating the PathTraverse and collecting the directories, and than reversing it
 * The thrid(expected full path) created by iterating the PathTravers and collecting the paths
 * The fourth(result full path) is created as the first step but collecting the full paths
 *
 * The reversal of both vectors is introduced for easier user readability.
 **/
tuple<vector<string>, vector<string>, vector<string>, vector<string> > constructCmpVectors(vector<string>& expected, PathTraverse& pt)
{
  vector<string> result;
  vector<string> resultPath;
  vector<string> expectedPath;
  for (auto [path, dir] : pt)
  {
    result.emplace_back(dir);
    resultPath.emplace_back(path);
  }

  string path;
  for(auto itr = expected.rbegin(); itr != expected.rend(); ++itr)
  {
    path = (expectedPath.size() ? path + "/" : ""s) + *itr;
    expectedPath.emplace_back(path);
  }

  reverse(result.begin(), result.end());
  reverse(expected.begin(), expected.end());
  reverse(resultPath.begin(), resultPath.end());
  return make_tuple(expected, result, expectedPath, resultPath);
}

/***************************************************************************
 *
 *                               Test cases
 *
 * *************************************************************************/


TEST_CASE("LHS string ", "[pathTraverse]") {
  vector<string> dirs = { "a", "b", "c" };
  string file = "file.txt";

  string lhs = createPath(dirs, file);
  PathTraverse pt{ std::move(lhs) };

  REQUIRE(file == pt.filename());

  auto [expected, result, expectedPath, resultPath] = constructCmpVectors(dirs, pt);
  REQUIRE( expected == result );
  REQUIRE( expectedPath == resultPath);
}

TEST_CASE("0 directories one file. no preceding slash", "[pathTraverse]") {
  vector<string> dirs = {};
  string file = "file.txt";

  PathTraverse pt{createPath(dirs, file)};

  REQUIRE(file == pt.filename());

  auto [expected, result, expectedPath, resultPath] = constructCmpVectors(dirs, pt);
  REQUIRE( expected == result );
  REQUIRE( expectedPath == resultPath);
}

TEST_CASE("1 directories one file. no preceding slash", "[pathTraverse]") {
  vector<string> dirs = { "dir1" };
  string file = "file.txt";

  PathTraverse pt{createPath(dirs, file)};

  REQUIRE(file == pt.filename());

  auto [expected, result, expectedPath, resultPath] = constructCmpVectors(dirs, pt);
  REQUIRE( expected == result );
  REQUIRE( expectedPath == resultPath);
}

TEST_CASE("2 directories one file. no preceding slash", "[pathTraverse]") {
  vector<string> dirs = { "dir2", "dir1" };
  string file = "file.txt";

  PathTraverse pt{createPath(dirs, file)};

  REQUIRE(file == pt.filename());

  auto [expected, result, expectedPath, resultPath] = constructCmpVectors(dirs, pt);
  REQUIRE( expected == result );
  REQUIRE( expectedPath == resultPath);
}

TEST_CASE("3 directories one file. no preceding slash", "[pathTraverse]") {
  vector<string> dirs = { "dir3", "dir2", "dir1" };
  string file = "file.txt";


  PathTraverse pt{createPath(dirs, file)};

  REQUIRE(file == pt.filename());

  auto [expected, result, expectedPath, resultPath] = constructCmpVectors(dirs, pt);
  REQUIRE( expected == result );
  REQUIRE( expectedPath == resultPath);
}


// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// ///


TEST_CASE("0 directories one file. preceding slash", "[pathTraverse]") {
  vector<string> dirs = {};
  string file = "file.txt";

  PathTraverse pt{"/"s + createPath(dirs, file)};

  REQUIRE(file == pt.filename());

  auto [expected, result, expectedPath, resultPath] = constructCmpVectors(dirs, pt);
  REQUIRE( expected == result );
  REQUIRE( expectedPath == resultPath);
}

TEST_CASE("1 directories one file. preceding slash", "[pathTraverse]") {
  vector<string> dirs = { "dir1" };
  string file = "file.txt";

  PathTraverse pt{"/" + createPath(dirs, file)};

  REQUIRE(file == pt.filename());

  auto [expected, result, expectedPath, resultPath] = constructCmpVectors(dirs, pt);
  REQUIRE( expected == result );
  REQUIRE( expectedPath == resultPath);
}

TEST_CASE("2 directories one file. preceding slash", "[pathTraverse]") {
  vector<string> dirs = { "dir2", "dir1" };
  string file = "file.txt";

  PathTraverse pt{"/"s + createPath(dirs, file)};

  REQUIRE(file == pt.filename());

  auto [expected, result, expectedPath, resultPath] = constructCmpVectors(dirs, pt);
  REQUIRE( expected == result );
  REQUIRE( expectedPath == resultPath);
}

TEST_CASE("3 directories one file. preceding slash", "[pathTraverse]") {
  vector<string> dirs = { "dir3", "dir2", "dir1" };
  string file = "file.txt";

  PathTraverse pt{"/"s + createPath(dirs, file)};

  REQUIRE(file == pt.filename());

  auto [expected, result, expectedPath, resultPath] = constructCmpVectors(dirs, pt);
  REQUIRE( expected == result );
  REQUIRE( expectedPath == resultPath);
}
