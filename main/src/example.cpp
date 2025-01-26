#include <gd/gd.h>

#include <filesystem>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <ios>

#include <openssl/sha.h>
#include "generator.h"
#include <spdlog/sinks/basic_file_sink.h>

using namespace gd;
using namespace gd::shorthand; // add >>, || chaining 
using namespace std;

inline void assertError(char const * msg, const string& err)
{
  throw runtime_error(""s + msg + "\n   " +  err );
}

void sanityCheck()
{
  std::cout << "Basic interface tests " << endl;
  const string repoPath = "/tmp/test/sanityCheck";
  cleanRepo(repoPath);

  try {
    auto logger = spdlog::basic_logger_mt("interace", "/tmp/test/logs/sanityCheck.log");
    setLogger(logger);

    auto ctx = selectRepository(repoPath)
      >> add("README", "bla bla\n")
      >> commit("mno", "mno@xmousse.com.org", "...\n")
      || [](const auto& err) { assertError("Failed to create first commit", err); };

    ctx 
      >> createBranch("KenAndRitchie")
      >> createBranch("StevenPinker")
      >> createBranch("AIReboot")
      || [](const auto& err) { assertError("Create branches", err); };

    ctx    // Implicitly master branch (last context)
      >> add("src/content/new.txt", "contentssssss")
      >> add("src/content/sub/more.txt", "check check")
      >> commit("mno", "mno@xmousse.com.org", "comment is long")
      ||  [](const auto& err) { assertError("Updating master", err); };

   ctx 
>> selectBranch("KenAndRitchie")
      >> add("src/dev/c/hello.c", "#include <hello.h>")
      >> add("src/dev/include/hello.h", "#pragma once\n")
      >> commit("mno", "mno@xmousse.com.org", "Ken and Ritchie's inventory")
      || [](const auto& err) { assertError("Updating branch Ken&Ritichie", err); };

   ctx 
      >> selectBranch("StevenPinker")
      >> add("the/blank/slate", "If you think the nature-nurture debate has been resolved, you are wrong ... this book is required reading ― Literary Review")
      >> add("the/staff/of/thought", "Powerful and gripping")
      >> add("Enlightement now", "THE TOP TEN SUNDAY TIMES BESTSELLER")
      >> commit("mno", "mno@xmousse.com.org", "comment is long")
      || [](const auto& err) { assertError("Updating branch StevenPinker", err); };

    ctx // Implicitly use previous used branch 'two'
      >> read("the/blank/slate")
      || [](const auto& err) { assertError("Reading file", err); };

    ctx >> selectBranch("KenAndRitchie")
      >> del("src/dev/include/hello.h")
      >> mv("src/dev/c/hello.c", "src/dev/cpp/hello.cpp")
      >> commit("mno", "mno@xmousse.com.org", "remove header file")
      || [](const auto& err) { assertError("Unable to remove or move files", err); };

    // std::cout << "The blank Slate review: " << *content << std::endl;
  } catch(const runtime_error& e)  { cout << "Error:" << e.what() << endl; }
  std::cout << "The end" << std::endl;
}

// Output
//                                           Naive(1) - Per file write      Collect - per dir write
//                                          -----------------------------   ---------------------------
// Commiting :      1 Files / 13 domains :: 0.00869548s 115.002 files/s :: 0.00413757s 241.688 files/s
// Commiting :     10 Files / 13 domains ::  0.0909334s 109.971 files/s ::  0.0329744s 303.265 files/s
// Commiting :    100 Files / 13 domains ::       1.11s  90.144 files/s ::   0.203213s 492.094 files/s
// Commiting :  1,000 Files / 13 domains ::      37.94s  26.358 files/s ::    1.92068s 520.649 files/s
// Commiting : 10,000 Files / 13 domains ::   3,259.63s   3.679 files/s ::    15.6622s 638.479 files/s
//
// (1) Naively add one file at a time, updating the entire tree up to root
// (2) Collect all elements, on commit, update each directory once
//     Collection is a std::map Path -> Vector<Updates> 
//     std::map collects effected directorys grown is O(logN)
//     std::vector capacity is dynmaically growing, no hint provided, growth is linear O(N)
//     Improvement can be moving to std::unordered_map (Constant time) + sorting O(logN)
void speedTest()
{
  auto logger = spdlog::basic_logger_mt("speed logger", "/tmp/test/logs/speedTest.log");
  setLogger(logger);

  std::cout << "\n\nWrite speed test" << endl;
  constexpr size_t numFilesPerDomain(100);
  constexpr size_t maxFileSize(1000);

  const string repoPath = "/tmp/test/speedTest";
  std::filesystem::remove_all(repoPath);

  //  Add 12 (domains) x numFilesPerDomain files in one commit
  //  Each domain resides in its own directory.
  //  Naieve implementation slows down exponantionally with growth of number of files !!!
  const std::vector<std::string> domains{ "AB", "AS", "UT", "AC", "RT", "TZ", "AD", "AZ", "PT", "RS", "PT", "TV", "VZ"};

  /// TODO: Consolidate context propagation, and tip tracking
  ////      Branch creation can be performed from tip_.commit_t
  auto dbx = selectRepository(repoPath);
  for (size_t numFiles = 1; numFiles <= 10'000; numFiles *= 10)
  {
    auto start = chrono::steady_clock::now();

    for (const auto& domain :  domains)
    {
      for (const auto& [id, content] : hsh::elements(numFiles,  maxFileSize))
      {
        dbx >> add( domain + "/" + id, content);
      }
    }

    dbx >> commit("mno", "mno@xmousse.com.org", "timing commit\n")
        || [](const auto& err) { assertError("Commit failed", err); };

    auto end = chrono::steady_clock::now();
    auto durationMs = chrono::duration <double, micro> (end - start).count();
    auto durationS = chrono::duration <double> (end - start).count();

    std::cout << "Commiting : " << right << setw(5) << numFiles  << " Files for " <<  domains.size() << " domains :: "
      <<  setw(12) <<  durationS << "s   "
      <<  setw(10) << (numFiles / durationS) << " files/s" << endl;

    dbx >> selectBranch("main"); // Reset the tip to HEAD
  }
}

int main() {

  sanityCheck();
  speedTest();
  return 0;
}
