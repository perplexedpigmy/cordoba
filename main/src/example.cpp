#include <gd/gd.h>

using namespace gd;

int main() {

  Repository repo("/home/pigmy/dev/git/gitdb/test/orig", "test db");

  // Initial commit. is performed on the main branch, but it cannnot reference it yet.
  // As a branch is reference to a commit. when there are not yet any.
  auto commitId = selectRepository(repo)
    .and_then(addFile("README", "bla bla\n"))
    .and_then(commit("mno", "mno@nowhere.org", "...\n"));

  if (!commitId)
    std::cout <<  "Failed to create first commit" << commitId.error() << std::endl;

  auto res =selectRepository(repo)
    .and_then(fork(*commitId, "one"))
    .and_then(fork(*commitId, "two"))
    .and_then(fork(*commitId, "three"));

  if (!res)
    std::cout << "Unable to create branches: " << res.error() << std::endl;

  commitId = selectRepository(repo)
             .and_then(selectBranch("master"))
             .and_then(addFile("src/content/new.txt", "contentssssss"))
             .and_then(addFile("src/content/sub/more.txt", "check check"))
             .and_then(commit("mno", "mno@nowhere.org", "comment is long"));

  if (!res)
    std::cout << res.error() << std::endl;

  commitId = selectRepository(repo)
             .and_then(selectBranch("one"))
             .and_then(addFile("src/dev/c++/hello.cpp", "#include <hello.h>"))
             .and_then(addFile("src/dev/inc/hello.h", "#pragma once\n"))
             .and_then(commit("mno", "mno@nowhere.org", "comment is long"));

  if (!res)
    std::cout << res.error() << std::endl;

  commitId  = selectRepository(repo)
              .and_then(selectBranch("two"))
              .and_then(addFile("dictionary/music/a/abba", "mama mia"))
              .and_then(addFile("dictionary/music/p/petshop", "boys"))
              .and_then(addFile("dictionary/music/s/sandra", "dreams\nare dreams"))
              .and_then(commit("mno", "mno@nowhere.org", "comment is long"));

  if (!res)
    std::cout << res.error() << std::endl;

  auto content = selectRepository(repo)
                .and_then(selectBranch("two"))
                .and_then(read("dictionary/music/s/sandra"));

  if (!content)
    std::cout << "Unable to read file: " << content.error() << std::endl;

  std::cout << "Content: " << *content << std::endl;

  return 0;
}
