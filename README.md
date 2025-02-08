<h1 align=center><code>C⊕rdoba</code></h1>
<div align=center>
  <img src=https://img.shields.io/github/v/tag/perplexedpigmy/cordoba?label=Version&pattern=v(.+) alt=version>
  <a href=https://github.com/perplexedpigmy/cordoba/actions/workflows/build.yaml>
    <img src=https://github.com/perplexedpigmy/cordoba/actions/workflows/build.yaml/badge.svg alt=ci>
  </a>
  <img src=https://img.shields.io/github/contributors/perplexedpigmy/cordoba alt=contributors>
</div>


![C⊕rdoba Logo](img/logo.svg)

C⊕rdoba is a lightweight C++ library for managing versioned NoSQL documents. Like the Spanish city famous for its libraries, C⊕rdoba can store, update and read data, better known in the Andalusian dialect as [CRUD](https://en.wikipedia.org/wiki/Create,_read,_update_and_delete). Unlike the city, and thanks it being powered by [**libgit2**](https://github.com/libgit2/libgit2) it can practically do anything that git can, either directly via C⊕rdoba or if not yet supported by using the git command line, git daemon etc. i.e. Cloning, branching, remote connection, merging, history view etc.

Transactions are also a thing, Implicitly supported via commits, they provide Atomicity and Durability. Yet achieving full [ACID](https://en.wikipedia.org/wiki/ACID), Consistency and Isolation need to be introduced.

The library was introduced as a proof of concept for a commercial enterprise solution that required to captured and address non-structured versioned data, with fast reads, decent store time and the ability to manage and access different versions. Schema was not required, nor were fast queries, roles & security, features that can be introduced, but are not part of current codebase. There is not much extra work foreseen in the short term on this library, it can be used as is, or as a window to git's impressive performance as DB backend.

----

## Installation

### Using cmake installers

Lightweight cmake package managers like [CPM](https://github.com/cpm-cmake/CPM.cmake) can be used to build and use the library as a dependency

### Building from source

If you use devcontainer, there is a `.devcontainer` directory with all needed to start building the library.

Otherwise, it's as simple as cloning the project and running a cmake >= 3.14

```bash
git clone https://github.com/perplexedpigmy/cordoba
cd cordoba
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build/release
cmake --build build/release
```

----

## Usage

Assuming some package manager is used, and the linkage with the library is spoken for.
All one needs is to include the header files and use the relevant namespace.

```c++
#include <gd/gd.h>
using namespace gd; // All of C⊕rdoba's functionality 
using namespace gd::shorthand; // add >>, || chaining, optional and recommended

```

----
**why use `gd::shorthand`?**

The library's interface have foregone exceptions, it's using `expected`, also know as (Result in rust, Either in scala/haskell) a return value that can return either a response or an error, While not native to c++ and missing some constructs that will help bring it to its utmost glory, It's still an interesting concept to use.

It allows chaining commands. Here is how one could open a database add a file to it and commit it,
in just one line. Being assured that if an error happens in any of the calls, no harm is done,
and `ctx` will hold that error, and skip any commands that happen thereafter.

```c++
  auto ctx = selectRepository(repoPath)
    .and_then( add("README", "not\n") )
    .and_then( commit("test", "test@here.org", "feat: add README") );
```

One can even do better, and log the error, and rollback our change.

```c++
  auto ctx = selectRepository(repoPath)
    .and_then( add("README", "not\n") )
    .and_then( commit("test", "test@here.org", "feat: add README") )
    .or_else( [](const auto& err) { 
      log(err); 
      rollback() 
    });
```

Alas the `and_then` and `or_else` can be tedious to type and a bit of clutter on the eyes.
This is where the shorthand can come handy

```c++
using namespace gd::shorthand; 

  auto ctx = selectRepository(repoPath)
    >> add("README", "not\n") 
    >> commit("test", "test@here.org", "feat: add README")
    || [](const auto& err) { 
        log(err); 
        rollback() 
      };
```

So from now on only the shorthand will be used.

### Creating a branch

Any type of command can be chained as many times as we like as long as there was no error, In the below example 3 branches are forked from the default context. The default context when just opening a repository is the last commit of the `main` branch.
Creating a branch does not change the context to that branch. One will have to use `selectBranch` to achieve that.

*note:*  repoPath is a path to the git repository, if the repository doesn't exist, it will be created.

```c++
  auto ctx = selectRepository(repoPath)
    >> createBranch("KenAndRitchie")
    >> createBranch("StevenPinker")
    >> createBranch("AIReboot")
    || [](const auto& err) { cout << "Failed to create branches: " << err << std::endl; };
```

The context can be reused as long as it is not containing an Error.

```c++
  auto ctx = selectRepository(repoPath)
    >> createBranch("KenAndRitchie");

  ctx >> createBranch("2nd branch");

```

But it can be also trivially recreated using `selectRepository`, once a repository is created, it's cached
making it fast & trivial re`select` it. However,  it's more typing, and less concise.

```c++
  selectRepository(repoPath)
    >> createBranch("KenAndRitchie");

  selectRepository(repoPath)
    >> createBranch("2nd branch");
```

Okay, so we have a repository and we kinda understand branches, what's next?
While we saw similar example above, this one is a more complete use case. introducing first commit with an typo, to have it be corrected in the consecutive commit, and capturing any potential error that could have happened anywhere in that chain, all in one chain of commands.

```c++
  auto ctx = selectRepository(repoPath)
    >> selectBranch("StevenPinker")
    >> add("the/blank/slate", "If you think the nature-nurture debate has been resolved, you are wrong ... this book is required reading ― Literary Review")
    >> add("the/staff/of/thought", "Powerful and gripping")
    >> add("Enlightenment now", "THE TOP FIVE SUNDAY TIMES BESTSELLER")
    >> commit("test", "test@here.org", "add reviews")

    >> add("Enlightenment now", "THE TOP **TEN** SUNDAY TIMES BESTSELLER")
    >> commit("test", "test@here.org", "correct review")
    || [](const auto& err) { 
      spd::error("Failed introducing data {}", static_cast<string>(err) );
      rollback();
    };
```

Failures can be caught with the `or_else` clause as shown above, but contexts can also be explicitly tested for errors, allowing the introduction of more granular logic in between calls to the database.

```c++
  auto ctx = selectRepository(repoPath) 
    >> selectBranch("KenAndRitchie");

  if (!ctx) 
    cerr << "Oops: " << ctx.error() << std::endl;
  else
    cout << "Successful switch to KenAndRitchie branch" << std::endl;
```

The context or `expected` has `operator bool()` defined, returning false if it has an error.

There are 2 way to access read file content

```c++
  auto ctx = selectRepository(repoPath)
    >> selectBranch("StevenPinker")
    >> read("the/blank/slate");

    // Explicitly get from read context.
    if (!!ctx) 
      cout << "The blank Slate: " << ctx->content() << endl;

    // In chain process by a lambda or a function 
    ctx 
      >> read("Enlightenment NOW")
      >> processContent([](auto content) { 
        cout << "Enlighten NOW: " << content << endl;
      })
      >> add("BookIRead.txt", "Enlightenment NOW")
      >> commit("test", "test@here.org", "correct review")
      || [](const auto& err) { cout << "Oops: " <<  err << endl; };
```

If you need to trace the library's internals you can use [spdlog](https://github.com/gabime/spdlog),
You can configure it as you need and use `setLogger` to tell the library to use it to log its internal logging.
An example for that can be seen in the `greens` utility, where the logger is used for both the library and the using app.

```c++
  #include <spdlog/sinks/basic_file_sink.h>

  auto logger = spdlog::basic_logger_mt("examples", "/tmp/test/logs/examples.log");
  logger->set_level(spdlog::level::debug); 
  setLogger(logger);
```

That's about it, lightweight, simple and easy to use.

## Performance

There are two aspects for performance that I thought were interesting to measure,

### Commit size impact on performance (timing.cpp)

The first being the impact of commit size on commit time. This is interesting because git is known to have a performance degradation with larger directories (bigger trees, longer look up times) but also the way libgit2 is implemented, the tree is constructed from the tip, meaning a naive implementation having multiple files introduced to the same directory will have constructed the directory structure once for each of the introduced files (or file version), which will cause an exponential growth.

In fact such naive implementation was introduced to gaze at its performance or lack thereof, and identify potential improvements.

The tests were executed using file sizes of 100 random characters on my not so powerful laptop,
  model name      : 12th Gen Intel(R) Core(TM) i7-1260P
  cpu MHz         : 2241.490
  cache size      : 18432 KB
  cpu cores       : 12

with a low end SSD drive

Each commit has N files added to each of 13 directories in one commit, and the entire execution time was measured.

*note* why 13 directories? This was a magic number in the corporation whose needs initiated this endeavor.

#### Naive implementation

| Num files | num dirs | Total files | Total Time(s) | Files/sec |
|:---------:|:--------:|:-----------:|:------------:|:----------:|
| 10        | 13       |  130        | 0.0909       |   140.15   |  
| 100       | 13       |  1,300      | 1.11         |   90.144   |
| 1,000     | 13       |  13,000     | 37.94        |   26.358   |
| 10,000    | 13       |  130,000    | 3259.63      |   3.6794   |

And indeed the naive implementation scaled poorly with growing number of files.

### C⊕rdoba's Implementation

| Num files | num dirs | Total files | Total Time(s)| Files/sec |
|:---------:|:--------:|:-----------:|:------------:|:----------:|
| 10        | 13       |  130        | 0.0227       |   440.15   |  
| 100       | 13       |  1,300      | 0.1463       |   683.61   |
| 1,000     | 13       |  13,000     | 1.3015       |   768.31   |
| 10,000    | 13       |  130,000    | 10.392       |   962.24   |

With 10,000 files per directory we still didn't reach the point of degradation and in fact the commit was performing better per second with growing number of files per directory per commit.

### Concurrent performance

The other concern was that, git, never has been designed to have concurrent clients working on it in parallel, and definitely not in different contexts (Branches, commits, etc), so synchronization primitives had to be introduced and the question that some may ask, and justly so, is, for what price?

To measure that a utility was introduced i.e. 'greens' that launches `G` agents each of which will introduce `C` commits, each of which is composed of `A` number of CRUD Operations. so in essence `G x C x A` operations are happening in `G` concurrent contexts. The utility took some liberties to avoid complex merging scenarios, merging scenarios may occur if multiple agents try to add to the same branch at the same time, C⊕rdoba won't allow that. Only the first agent wins, the others will be notified that their context is stale, leaving them the choice of merging, or rolling back and trying again. 2nd approach was selected for simplicity.

#### IMPORTANT

1. The size and content of the files are also random, so some time variation beyond scaling should be expected, but we assume they are negligent (far lower than linear growth).

2. The utility also validates the expected DAG against the git repository, so not all the time consumed is related to what we are interested in measuring, Thus, the timings are to be read worse case scenario and then some. But this doesn't bother our benchmarking.

3. Tested on a laptop, with Windows manager and plenty of apps running, variation can come from anywhere, But again, we consider that negligent, as we only use it for benchmarking, and as long as there was no unexplained spike it is perfect for our needs.

Great, let's have some fun. and see whether we get any scalability issues.
This is how we execute this commandline utility (It has help if you care to use it yourself)

```sh
# 5 concurrent agents x 30 Commits x 50 operations = 7,500 Operations = 3,676.47 per second
time ./build/release/main/gd/greens -g 5 -b 10 -c 30 -o 50
Success

real    0m2.046s
user    0m1.473s
sys     0m1.637s
```

Let's draw a table with out results when num agents is our variable.

| Num agents |   Num Ops   |  real   |  user   |   sys   |   Ops/sec   |
|:----------:|:-----------:|:-------:|:-------:|:-------:|:-----------:|
|    5       |   7,500     | 2.046s  |  1.631s |  1.534s |   3,676.47  |
|    10      |  15,000     | 5.637s  |  5.077s |  4.572s |   2,660.98  |
|    20      |  30,000     | 16.479s | 15.927  | 12.750s |   1,820.50  |
|    25      |  37,500     | 28.062s | 25.179s | 22.382s |   1,336.32  |

Interesting, roughly speaking, doubling the agents(Which is also doubling the number of ops), is tripling the time, which is mostly due to rollbacks/retries when there is a contention. One can argue whether this is a reasonable use case, but even if it is, the linear scaling is somewhat acceptable.

But let's dig deeper, what if we doubled the agents but kept the same number of ops?
we can do it by pairing 80 ops for 5 agents, 40 ops for 10 agents and 20 ops for 20 agents

| Num agents |   Num Ops   |  real   |  user   |   sys   |   Ops/sec |
|:----------:|:-----------:|:-------:|:-------:|:-------:|:-----------:|
|    5       |  12,000     |  3.694s |  2.067s |  3.213s |   3,252.03  |
|    10      |  12,000     |  3.670s |  3.308s |  2.912s |   3,269.03  |
|    20      |  12,000     |  5.971s |  5.500s |  4.395s |   2,009.71  |

This indeed enforces the hypothesis that the contention may be indeed the culprit for the degradation above.
because we don't see much effect when we doubled the agents the first time, only the 2nd time.

lets dig dipper, what if we kept the number of agents static, and played with ops?

| Num agents |     Ops/Commit |  Num Commits | Total Ops     |   real  |  user   |  sys   |   Ops/sec   |
|:----------:|:--------------:|:------------:|:-------------:|:-------:|:-------:|:------:|:-----------:|
|    10      |  20            |  30          |   6,000       |  2.071s | 1.595s  | 1.618s |  2,897.15   |
|    10      |  40            |  30          |  12,000       |  4.200s | 3.758s  | 3.451s |  2,857.14   |
|    10      |  80            |  30          |  24,000       |  7.353s | 6.791s  | 5.861s |  3,263.97   |

Nice, we don't see any scaling problem when using constant number of agents as their number of ops grows.
Again we seem to have come to the same conclusion that the library scales well, the degradation is our unrealistic approach of rollback and retrying when there is contention on branches, in real life. this is a merge and that doesn't happen in real time, unless we have some solid logic behind it, which we currently do not.

This final benchmark can put our minds to rest, we have the means to minimize contention on branches,
each agent is choosing a branch for each commit randomly, if we increase the branch pool the likelihood of contention is decreased, and we can do that via the `-b` switch, let's do that while keeping constant number of ops.
Let's also choose 20 agents, the number where we saw the problem start to bud.
And to boot, we actually can avoid doing the validations(-n) and display the retries percentage.
Total Ops = 20 agents x 30 commits x 50 Ops

| Num agents | Total Ops     |  Num branches|   real  |  user   |  sys   |   Ops/sec   | Retries(%) |
|:----------:|:-------------:|:------------:|:-------:|:-------:|:------:|:-----------:|:----------:|
|    20      |  30,000       |  10          | 12.186s | 12.398s | 13.186s|  2,461.84   |     3%     |
|    20      |  30,000       |  20          | 10.357s |  8.669s | 10.708s|  2,896.59   |     1%     |
|    20      |  30,000       |  40          |  8.718s |  6.980s | 8.791s |  3,441.15   |    <1%     |

We have seen this >=3000 ops per second before, with lower number of ops and agents, This seems to support out above assumption that contention/rollback/retries are responsible for the alleged degradation. So does the diminishing retries percentage.
The validation itself against the git repository, at least for these numbers, doesn't yield much runtime advantage.

#### Summary

The `greens` utility helped us verify that most importantly, the result of competing agents introducing data to the repository is correct and valid, but it also shows that, merging and conflicts notwithstanding, it scales predictably and well. also >3000 ops/s on a medium tier laptop is more than what I expected, to be honest.

## Future development

C⊕rdoba may be enough for the original requirements, but it's not nearly as feature full like Dolt, some of the features are trivial to implement with a git backend, others, like queries and schema support, may have to perform some serious tradeoff juggling, but these features may boost the use cases of the library.

* Credentials, roles and security
* Indexing and queries
* Schema, constraints  & Validations
* Enhanced merging (As function of schema)
* Data tagging (Trivial, wasn't part of the discovery effort, can be performed via git cli)
* Data merging (Trivial, wasn't part of the discovery effort, can be performed via git cli)
* Adding notes (Trivial, wasn't part of the discovery effort, can be performed via git cli)
* Thread context, that allows continuation without the need of propagating the context down the stack call (Experimental, not fully tested)

----

## Contributing

C⊕rdoba welcomes your contributions! just is released under the maximally permissive [CC0](https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt) public domain dedication and fallback license, so your changes must also be released under this license.

### Getting started

C⊕rdoba is written in modern C++ and requires g++13 or later. All new features must be covered by unit or integration tests all tests can be found at main/gd/test, and should be executed prior to the pull request.

----

## License

 is licensed under the MIT License. See the LICENSE file for details.

----

## Contact

For questions or feedback, please open an issue on GitHub or contact C⊕rdoba [here](cordoba@xmousse.com).
