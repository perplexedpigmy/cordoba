#include <gd/gd.h>

#include <filesystem>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <ios>

#include <openssl/sha.h>
#include "generator.h"

using namespace gd;
using namespace gd::shorthand; // Support chaining via >> and ||
using namespace std;

inline void assertError(char const * msg, const string& err)
{
  throw runtime_error(""s + msg + "\n   " +  err );
}


void sanityCheck()
{
  cout << "Basic interface tests " << endl;
  const string repoPath = "/tmp/test/sanityCheck";
  std::filesystem::remove_all(repoPath);

  try {
    auto commitId = selectRepository(repoPath)
      >> add("README", "bla bla\n")
      >> commit("mno", "mno@xmousse.com.org", "...\n")
      || [](const auto& err) { assertError("Failed to create first commit", err); };

    auto res = db
      >> createBranch(*commitId, "KenAndRitchie")
      >> createBranch(*commitId, "StevenPinker")
      >> createBranch(*commitId, "AIReboot")
      || [](const auto& err) { assertError("Create branches", err); };

    commitId = db   // Implicitly master branch (last context)
      >> add("src/content/new.txt", "contentssssss")
      >> add("src/content/sub/more.txt", "check check")
      >> commit("mno", "mno@xmousse.com.org", "comment is long")
      ||  [](const auto& err) { assertError("Updating master", err); };

    commitId = db
      >> selectBranch("KenAndRitchie")
      >> add("src/dev/c/hello.c", "#include <hello.h>")
      >> add("src/dev/include/hello.h", "#pragma once\n")
      >> commit("mno", "mno@xmousse.com.org", "Ken and Ritchie's inventory")
      || [](const auto& err) { assertError("Updating branch Ken&Ritichie", err); };

    commitId  = db
      >> selectBranch("StevenPinker")
      >> add("the/blank/slate", "If you think the nature-nurture debate has been resolved, you are wrong ... this book is required reading ― Literary Review")
      >> add("the/staff/of/thought", "Powerful and gripping")
      >> add("Enlightement now", "THE TOP TEN SUNDAY TIMES BESTSELLER")
      >> commit("mno", "mno@xmousse.com.org", "comment is long")
      || [](const auto& err) { assertError("Updating branch StevenPinker", err); };

    auto content = db // Implicitly use previous used branch 'two'
      >> read("the/blank/slate")
      || [](const auto& err) { assertError("Reading file", err); };

    db >> selectBranch("KenAndRitchie")
      >> del("src/dev/include/hello.h")
      >> mv("src/dev/c/hello.c", "src/dev/cpp/hello.cpp")
      >> commit("mno", "mno@xmousse.com.org", "remove header file")
      || [](const auto& err) { assertError("Unable to remove or move files", err); };

    std::cout << "The blank Slate review: " << *content << std::endl;
  } catch(const runtime_error& e)  { cout << "Error:" << e.what() << endl; }
}

// Output
// Commiting :     1 Files for 13 domains ::   0.00869548s      115.002 files/s
// Commiting :    10 Files for 13 domains ::    0.0909334s      109.971 files/s
// Commiting :   100 Files for 13 domains ::      1.10934s      90.1436 files/s
// Commiting :  1000 Files for 13 domains ::       37.939s      26.3581 files/s
// Commiting : 10000 Files for 13 domains ::      3259.63s      3.06784 files/s
//
void speedTest()
{
  cout << "\n\nWrite speed test" << endl;
  constexpr size_t numFilesPerDomain(100);
  constexpr size_t maxFileSize(1000);

  const string repoPath = "/tmp/test/speedTest";
  std::filesystem::remove_all(repoPath);

  //  Add 12 (domains) x numFilesPerDomain files in one commit
  //  Each domain resides in its own directory.
  //  Naieve implementation slows down exponantionally with growth of number of files !!!
  const std::vector<std::string> domains{ "AB", "AS", "UT", "AC", "RT", "TZ", "AD", "AZ", "PT", "RS", "PT", "TV", "VZ"};

  auto dbx = selectRepository(repoPath);
  for (size_t numFiles = 1; numFiles <= 1'000; numFiles *= 10)
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

    cout << "Commiting : " << right << setw(5) << numFiles  << " Files for " <<  domains.size() << " domains :: "
         <<  setw(12) <<  durationS << "s   "
         <<  setw(10) << (numFiles / durationS) << " files/s" << endl;

    dbx >> selectBranch("master"); // Reset the tip to HEAD
  }
}

int main() {

  sanityCheck();
  speedTest();
  return 0;
}
