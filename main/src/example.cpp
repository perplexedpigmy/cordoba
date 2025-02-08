#include <gd/gd.h>

#include <filesystem>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <ios>

#include <openssl/sha.h>
#include "generator.h"
#include "out.h"

#include <spdlog/sinks/basic_file_sink.h>

using namespace gd;
using namespace gd::shorthand; // add >>, || chaining 
using namespace std;


static const filesystem::path repoPath = "/tmp/test/examples";

/// Uses SPDLogger https://github.com/gabime/spdlog, if not explicitly set, logging is not captured
/// For configuration see SPDLog documentation
void setLogger() {
  auto logger = spdlog::basic_logger_mt("examples", "/tmp/test/logs/examples.log");
  logger->set_level(spdlog::level::trace); 
  setLogger(logger);
}

/// Cleaning the repository is usefull on rare occasions like testing, but is unlikely to be used in any production code
/// Unless one wants to start the repository from scratch on every run that is, so use sagaciously.
void cleanRepository() {
  cleanRepo(repoPath);
}

/// Getting a context requires selecting a repository (Which will create or open a repository in a given path),
/// once getting a context files can be `add`ed and transacted (i.e `commit`ted)
///  operator >> chains different serially consecutive commands, if any command in the chain fails, the context will 
///  contain an error, error can  checked directly 
void addElementToDefaultBranch() {
  auto ctx = selectRepository(repoPath)
    >> add("README", "not\n")
    >> commit("test", "test@here.org", "feat: add README");

    if (!ctx) {
      cerr << "Error: " << ctx.error()._msg << std::endl;
    } else {
      cout << "Introduced first commit" << std::endl;
    }
    // OUTPUT:
    //
    // Introduced first commit
}

/// Errors can be also captured via a dedicated function/lambda
void addAnotherElementToDefaultBranch() {
  auto ctx = selectRepository(repoPath)
    >> commit("test", "test@here.org", "fix: add a license")
    || [](const auto& err) { std::cerr  << "Failed to commit: " <<  err << std::endl; };

    // OUTPUT:
    //
    // Failed to commit: Nothing to commit
}

/// Branch creation doesn't change the current context, for that use `selectBranch`, thusly, it's possible to branch 
/// from current context multiple branches, chaining any other command after branching is also supported.  
void createSomeBranches() {
  auto ctx = selectRepository(repoPath)
    >> createBranch("KenAndRitchie")
    >> createBranch("StevenPinker")
    >> createBranch("AIReboot")
    || [](const auto& err) { cout << "Failed to create branches: " << err << std::endl; };

    // OUTPUT:
    //
}

/// After branch selection, it's possible to chain updating commands, `add`ing is not just a 'create file` but rather a `create revision`
/// that's why it's possible to keep adding the same file with different content, creating another version of it.
void addElementsOnBranch() {
  auto ctx = selectRepository(repoPath)
    >> selectBranch("StevenPinker")
    >> add("the/blank/slate", "If you think the nature-nurture debate has been resolved, you are wrong ... this book is required reading ― Literary Review")
    >> add("the/staff/of/thought", "Powerful and gripping")
    >> add("Enlightenment now", "THE TOP FIVE SUNDAY TIMES BESTSELLER")
    >> commit("test", "test@here.org", "add reviews")

    >> add("Enlightenment now", "THE TOP **TEN** SUNDAY TIMES BESTSELLER")
    >> commit("test", "test@here.org", "correct review")
    || [](const auto& err) { 
      spdlog::error("Failed updating branch StevenPinker: {}", static_cast<string>(err));
      rollback();
    };
}

/// A Commit is naturally equivalent to a DB transaction, and it's counterpart is of course a `rollback`
/// Moreover, the chaining can be spread over multiple statements allowing additional introduction of additional logic.
void rollbackUnwantedChanges() {
  auto ctx = selectRepository(repoPath) 
    >> selectBranch("KenAndRitchie");

  if (!!ctx) 
    cout << "Successful switch to KenAndRitchie" << std::endl;

  ctx 
    >> add("src/dev/c/hello.c", "#include <hello.h>")
    >> add("src/dev/include/hello.h", "#pragma once\n")
    >> rollback()  // Zig is king baby, zig is king
    || [](const auto& err) { cout << "Updating branch Ken&Ritchie: " <<  err << endl; };

    // OUTPUT:
    //
    // Successful switch to KenAndRitchie
}

/// There are multiple ways to read and process the content 
void readingContent() {
  auto ctx = selectRepository(repoPath)
    >> selectBranch("StevenPinker")
    >> read("the/blank/slate");

    // Explicitly use the read content.
    if (!!ctx)
      cout << "The blank Slate: " << ctx->content() << endl;

    // Or via a closure/function
    ctx 
      >> read("Enlightenment now")
      >> processContent([](auto content) { cout << "Enlightenment NOW: " << content << endl;});

    // On failure of course the call chain will be short cut to the error capture.
    ctx 
      >> read("SomethingThatDoesntExist")
      >> processContent([](auto content) { cout << "Bwahahaha: " << content << endl;})
      || [](const auto& err) { cout << "Oops: " <<  err << endl; };

    // OUTPUT:
    //
    // The blank Slate: If you think the nature-nurture debate has been resolved, you are wrong ... this book is required reading ― Literary Review
    // Enlightenment NOW: THE TOP **TEN** SUNDAY TIMES BESTSELLER
    // Oops: the path 'SomethingThatDoesntExist' does not exist in the given tree
}

// Let's create some commits over 2 branches
//
//         C---E---G topic
//        /
//   A---B---D---F   main
//
// Important:
// 1.  The branch is only created after the first commit, In git branching is possible only when the DAG is not empty
// 2.  There are no empty commits, something has to change to introduce a commit
// 3.  `createBranch` is does not switch to the created branch, use selectBrach for that.

void createTree() {
  const string main = "main";   // Default branch is main
  const string topic = "topic";

  auto ctx = selectRepository(repoPath)
    >> add("file", "content")   
    >> commit("test", "test@here.org", "A")
    >> createBranch(topic)     // After creating the branch we are still on `main`, branching is only possible after first commit

    >> del("file")             // Deleting is a sufficient change for a commit
    >> commit("test", "test@here.org", "B")

    >> selectBranch(topic)
    >> add("file", "content")   
    >> commit("test", "test@here.org", "C")
    
    >> selectBranch(main)
    >> add("README", "New update")   
    >> commit("test", "test@here.org", "D")

    >> selectBranch(topic)
    >> add("file", "Some more info")   
    >> commit("test", "test@here.org", "E")

    >> selectBranch(main)
    >> add("new", "It is")   
    >> commit("test", "test@here.org", "F")

    >> selectBranch(topic)
    >> del("file")
    >> commit("test", "test@here.org", "G")
      || [](const auto& err) { cout << "Failed tree creation: " <<  err << endl; };

    // OUTPUT:
    //
}


int main() {

  setLogger();
  cleanRepository();

  addElementToDefaultBranch();
  addAnotherElementToDefaultBranch();
  createSomeBranches();
  addElementsOnBranch();
  rollbackUnwantedChanges();
  readingContent();
  createTree();

  return 0;
}
