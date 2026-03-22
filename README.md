<h1 align=center><code>C⊕rdoba</code></h1>
<div align=center>

[![Version](<https://img.shields.io/github/v/tag/perplexedpigmy/cordoba?label=Version&pattern=v(.+)>)](https://github.com/perplexedpigmy/cordoba/releases)
[![Build](https://github.com/perplexedpigmy/cordoba/actions/workflows/build.yaml/badge.svg)](https://github.com/perplexedpigmy/cordoba/actions)
[![License](https://img.shields.io/badge/license-CC0-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B23)
[![libgit2](https://img.shields.io/badge/libgit2-1.4-blue.svg)](https://libgit2.org/)
[![Contributor](https://img.shields.io/github/contributors/perplexedpigmy/cordoba)](https://github.com/perplexedpigmy/cordoba/graphs/contributors)

</div>

<p float="left">
  <img src="img/logo.svg" width="150" height="150" align="left" style="margin-right: 20px;"/>
  <strong>⊕rdoba</strong> is a lightweight C++ library for managing versioned NoSQL
  documents. Like the Spanish city famous for its libraries, C⊕rdoba can store, update
  and read data, better known in the Andalusian dialect as
  <a href="https://en.wikipedia.org/wiki/Create,_read,_update_and_delete">CRUD</a>. Unlike
  the city, and thanks it being powered by <a href="https://github.com/libgit2/libgit2"><strong>libgit2</strong></a>,
  it supports common versioned data operations (create, read, update, delete, branching,
  commits, history). For operations not yet directly supported by C⊕rdoba, the underlying
  git repository can be manipulated using standard git CLI tools (e.g., <code>git merge</code>,
  <code>git tag</code>, <code>git notes</code>).
</p>

Transactions are supported via commits, providing **Atomicity**, **Durability**,
and **Isolation** (via branch isolation and optimistic locking for same-branch
conflicts). **Consistency** (schema/constraints/validations) was not part of the
original requirements, but can be added as a future extension.

The library was introduced as a proof of concept for a commercial enterprise
solution that required to capture and address non-structured versioned data,
with fast reads, decent store time and the ability to manage and access
different versions. Schema support (constraints, validations), fast queries,
roles & security were not part of the original requirements, but are possible
future extensions. There is not much extra work foreseen in the short term on
this library; it can be used as is, or as a window to git's impressive
performance as a DB backend.

---

## Project Structure

```
cordoba/
├── CMakeLists.txt          # Root CMake with gd_BUILD_APPS option
├── CMakePresets.json       # CMake presets (Debug, Release, etc.)
├── VERSION                 # Version file (0.1.0)
├── cmake/                  # CMake helpers
│   ├── CPM.cmake           # CPM package manager
│   └── CordobaConfig*.cmake.in  # Package config templates
├── gd/                     # Library source
│   ├── inc/gd/             # Public headers
│   ├── src/                # Implementation
│   └── test/               # Unit tests (crud.cpp, greens.cpp)
├── examples/               # Example executables (speed.cpp, example.cpp)
└── .devbox/                # Devbox configuration
```

---

## Build Dependencies

C⊕rdoba requires a modern C++ toolchain with C++20/23 support:

| Dependency           | Version | Description                             |
| -------------------- | ------- | --------------------------------------- |
| gcc                  | 14+     | C++ compiler with C++20/23 support      |
| cmake                | 3.14+   | Build system                            |
| ninja                | latest  | Build tool (optional, faster than make) |
| git                  | latest  | Version control                         |
| libssl-dev           | system  | SSL/TLS development files               |
| libcurl4-openssl-dev | system  | HTTP client library development files   |
| just                 | latest  | Task runner (optional)                  |
| valgrind             | latest  | Memory checker (optional)               |

### Debian/Ubuntu

```bash
apt install gcc-14 cmake ninja-build git libssl-dev libcurl4-openssl-dev just valgrind
```

### Using Devbox

If you use [Devbox](https://www.jetify.com/devbox/), add this to your project's
`devbox.json`:

```json
{
  "packages": ["gcc@14", "cmake", "ninja", "git", "openssl", "curl"]
}
```

Then run `devbox shell` before building.

### Note for Library Consumers

Unlike package managers such as npm or cargo which install build tools
automatically, C++ package managers (CPM, Conan, vcpkg) only fetch library
source code. When including C⊕rdoba in your project, ensure your build
environment has the toolchain listed above installed.

---

## Installation

### Using CPM (CMake Package Manager)

[CPM](https://github.com/cpm-cmake/CPM.cmake) is a lightweight CMake package
manager that fetches and builds dependencies. Add this to your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.14)
project(your_project)

# Option 1: Include CPM.cmake directly in your project
include(cmake/CPM.cmake)

# Option 2: Let CPM download CPM.cmake automatically
# CPMAddPackage("gh:cpm-cmake/CPM.cmake@0.42.1")

# Optionally set gd_BUILD_APPS ON to build tests and examples
set(gd_BUILD_APPS ON)

# Fetch C⊕rdoba from GitHub
CPMAddPackage("gh:perplexedpigmy/cordoba@0.2.0")

# Your executable links against the gd library
add_executable(your_app src/your_app.cpp)
target_link_libraries(your_app PRIVATE gd::gd)

# C++23 is required
target_compile_features(your_app PRIVATE cxx_std_23)
```

#### CPM Options

| Option          | Default | Description                                                         |
| --------------- | ------- | ------------------------------------------------------------------- |
| `gd_BUILD_APPS` | `OFF`   | Set to `ON` to build tests/examples (not recommended for consumers) |

#### What CPM Fetches

When you add C⊕rdoba via CPM, it automatically fetches:

- **libgit2 v1.4.3** - Git library used by C⊕rdoba
- **spdlog** - Logging library
- **Catch2** - Testing framework (only if `gd_BUILD_APPS=ON`)
- **CLI11** - Command-line parsing (only if `gd_BUILD_APPS=ON`)

#### Consumer Projects

For a complete example of a CPM-based consumer project, see
[`/tmp/test/consumer/CMakeLists.txt`](/tmp/test/consumer/CMakeLists.txt).

### Using cmake installers (with CPM.cmake in your project)

If your project already has `cmake/CPM.cmake`:

```cmake
# In your cmake/CPM.cmake or top-level CMakeLists.txt
set(gd_BUILD_APPS OFF)  # Don't build tests/examples
CPMAddPackage("gh:perplexedpigmy/cordoba")

target_link_libraries(your_app PRIVATE gd::gd)
```

### Building from source

**Prerequisites:** GCC 14+, CMake 3.14+, Ninja or Make

```bash
git clone https://github.com/perplexedpigmy/cordoba
cd cordoba

# Configure with CMake (or use presets)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -Dgd_BUILD_APPS=ON

# Build
cmake --build build
```

### Using Devbox (Recommended)

If you have [Devbox](https://www.jetify.com/devbox/) installed, the project
includes a `devbox.json` with all build dependencies. Run:

```bash
devbox install
devbox run cmake --preset Release
devbox run cmake --build build/release
```

### Other Package Managers

> **Note:** The following package managers are not yet implemented.
> Contributions welcome!

#### Conan

Conan 2.x support is available. Add this to your `conanfile.txt`:

```ini
[requires]
cordoba/0.2.0

[generators]
CMakeToolchain
CMakeDeps

[options]
cordoba/*:build_apps=False
```

Or in your `conanfile.py`:

```python
from conan import ConanFile

class MyProject(ConanFile):
    requires = "cordoba/0.2.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("cordoba/0.2.0")
```

Then in your `CMakeLists.txt`:

```cmake
find_package(cordoba REQUIRED)
target_link_libraries(your_app PRIVATE cordoba::gd)
```

**Note:** Requires GCC 14+ with C++23 support.

---

## Development

### CMake Presets

The project uses CMake presets for consistent builds across environments.

| Preset            | Description                         |
| ----------------- | ----------------------------------- |
| `Debug`           | Debug build with symbols            |
| `DebugSanitize`   | Debug build with sanitizers enabled |
| `Release`         | Optimized release build             |
| `ReleaseSanitize` | Release build with sanitizers       |

**List available presets:**

```bash
cmake --list-presets
```

### Building

**Using just (recommended):**

```bash
devbox run just build Release
```

**Using just with other presets:**

```bash
devbox run just build Debug
devbox run just build DebugSanitize
devbox run just build ReleaseSanitize
```

**Building manually:**

```bash
cmake --preset Release
cmake --build build/release
```

### Testing

**Run all just commands:**

```bash
devbox run just test     # Run unit tests (crud)
devbox run just stress   # Run stress tests (greens)
devbox run just bench    # Run performance benchmark (speed)
```

**Run tests manually:**

```bash
./build/release/gd/crud            # Unit tests
./build/release/gd/greens -g 5 -b 10 -c 30 -o 50  # Stress test
./build/release/examples/speed     # Performance benchmark
./build/release/examples/example   # Example application
```

### Memory Checking

Run valgrind memcheck on any executable:

```bash
devbox run valgrind --leak-check=full ./build/release/gd/crud
devbox run valgrind --leak-check=full ./build/release/gd/greens
devbox run valgrind --leak-check=full ./build/release/examples/speed
devbox run valgrind --leak-check=full ./build/release/examples/example
```

All executables pass valgrind with **0 errors** and **0 leaks**.

---

## Usage

Assuming some package manager is used, and the linkage with the library is
spoken for. All one needs is to include the header files and use the relevant
namespace.

```c++
#include <gd/gd.h>
using namespace gd; // All of C⊕rdoba's functionality
using namespace gd::shorthand; // add >>, || chaining, optional and recommended

```

### Why use `gd::shorthand`?

The library's interface have foregone exceptions. It's using `expected`, also
know as (Result in rust, Either in scala/haskell) a return value that can return
either a response or an error. While not native to C++ and missing some
constructs that will help bring it to its utmost glory, It's still an
interesting concept to use.

It allows chaining commands. Here is how one could open a database, add a file
to it and commit it, in just one line. Being assured that if an error happens in
any of the calls, no harm is done, and `ctx` will hold that error, and skip any
commands that happen thereafter.

```c++
  auto ctx = selectRepository(repoPath)
    .and_then( add("README", "not\n") )
    .and_then( commit("test", "test@here.org", "feat: add README") );
```

One can even do better, and log the error, and rollback our change:

```c++
  auto ctx = selectRepository(repoPath)
    .and_then( add("README", "not\n") )
    .and_then( commit("test", "test@here.org", "feat: add README") )
    .or_else( [](const auto& err) {
      log(err);
      rollback()
    });
```

Alas the `and_then` and `or_else` can be tedious to type and a bit of clutter on
the eyes. This is where the shorthand can come in handy.

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

Any type of command can be chained as many times as we like as long as there was
no error. In the below example 3 branches are forked from the default context.
The default context when just opening a repository is the last commit of the
`main` branch. Creating a branch does not change the context to that branch. One
will have to use `selectBranch` to achieve that.

_note:_ repoPath is a path to the git repository. If the repository doesn't
exist, it will be created.

```c++
  auto ctx = selectRepository(repoPath)
    >> createBranch("KenAndRitchie")
    >> createBranch("StevenPinker")
    >> createBranch("AIReboot")
    || [](const auto& err) { cout << "Failed to create branches: " << err << endl; };
```

The context can be reused as long as it is not containing an Error.

```c++
  auto ctx = selectRepository(repoPath)
    >> createBranch("KenAndRitchie");

  ctx >> createBranch("2nd branch");

```

But it can be also trivially recreated using `selectRepository`, once a
repository is created, it's cached making it fast & trivial re`select` it.
However, it's more typing, and less concise.

```c++
  selectRepository(repoPath)
    >> createBranch("KenAndRitchie");

  selectRepository(repoPath)
    >> createBranch("2nd branch");
```

Okay, so we have a repository and we kinda understand branches, what's next?
While we saw similar example above, this one is a more complete use case.
Introducing first commit with an typo, to have it be corrected in the
consecutive commit, and capturing any potential error that could have happened
anywhere in that chain, all in one chain of commands.

```c++
  auto ctx = selectRepository(repoPath)
    >> selectBranch("StevenPinker")
    >> add("the/blank/slate", "If you think the nature-nurture debate has been resolved, you are wrong ... this book is required reading")
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

Failures can be caught with the `or_else` clause as shown above, but contexts
can also be explicitly tested for errors, allowing the introduction of more
granular logic in between calls to the database.

```c++
  auto ctx = selectRepository(repoPath)
    >> selectBranch("KenAndRitchie");

  if (!ctx)
    cerr << "Oops: " << ctx.error() << endl;
  else
    cout << "Successful switch to KenAndRitchie branch" << endl;
```

The context or `expected` has `operator bool()` defined, returning false if it
has an error.

There are 2 ways to access read file content.

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

If you need to trace the library's internals you can use
[spdlog](https://github.com/gabime/spdlog). You can configure it as you need and
use `setLogger` to tell the library to use it to log its internal logging. An
example for that can be seen in the `greens` utility, where the logger is used
for both the library and the using app.

```c++
  #include <spdlog/sinks/basic_file_sink.h>

  auto logger = spdlog::basic_logger_mt("examples", "/tmp/test/logs/examples.log");
  logger->set_level(spdlog::level::debug);
  setLogger(logger);
```

That's about it, lightweight, simple and easy to use.

## Performance

There are two aspects for performance that I thought were interesting to
measure.

### Commit size impact on performance (timing.cpp)

The first being the impact of commit size on commit time. This is interesting
because git is known to have a performance degradation with larger directories
(bigger trees, longer look up times) but also the way libgit2 is implemented,
the tree is constructed from the tip, meaning a naive implementation having
multiple files introduced to the same directory will have constructed the
directory structure once for each of the introduced files (or file version),
which will cause an exponential growth.

In fact such naive implementation was introduced to gaze at its performance or
lack thereof, and identify potential improvements.

The tests were executed using file sizes of 100 random characters on my not so
powerful laptop:

- model name: 12th Gen Intel(R) Core(TM) i7-1260P
- cpu MHz: 2241.490
- cache size: 18432 KB
- cpu cores: 12
- with a low end SSD drive

Each commit has N files added to each of 13 directories in one commit, and the
entire execution time was measured.

_note_ why 13 directories? This was a magic number in the corporation whose
needs initiated this endeavor.

#### Naive implementation

| Num files | num dirs | Total files | Total Time(s) | Files/sec |
| :-------: | :------: | :---------: | :-----------: | :-------: |
|    10     |    13    |     130     |    0.0909     |  140.15   |
|    100    |    13    |    1,300    |     1.11      |  90.144   |
|   1,000   |    13    |   13,000    |     37.94     |  26.358   |
|  10,000   |    13    |   130,000   |    3259.63    |  3.6794   |

And indeed the naive implementation scaled poorly with growing number of files.

### C⊕rdoba's Implementation

| Num files | num dirs | Total files | Total Time(s) | Files/sec |
| :-------: | :------: | :---------: | :-----------: | :-------: |
|    10     |    13    |     130     |    0.0227     |  440.15   |
|    100    |    13    |    1,300    |    0.1463     |  683.61   |
|   1,000   |    13    |   13,000    |    1.3015     |  768.31   |
|  10,000   |    13    |   130,000   |    10.392     |  962.24   |

With 10,000 files per directory we still didn't reach the point of degradation
and in fact the commit was performing better per second with growing number of
files per directory per commit.

### Concurrent performance

The other concern was that, git, never has been designed to have concurrent
clients working on it in parallel, and definitely not in different contexts
(Branches, commits, etc), so synchronization primitives had to be introduced and
the question that some may ask, and justly so, is, for what price?

To measure that a utility was introduced i.e. 'greens' that launches `G` agents
each of which will introduce `C` commits, each of which is composed of `A`
number of CRUD Operations. So in essence `G x C x A` operations are happening in
`G` concurrent contexts. The utility took some liberties to avoid complex
merging scenarios. Merging scenarios may occur if multiple agents try to add to
the same branch at the same time; C⊕rdoba won't allow that. Only the first agent
wins, the others will be notified that their context is stale, leaving them the
choice of merging, or rolling back and trying again. 2nd approach was selected
for simplicity.

#### IMPORTANT

1. The size and content of the files are also random, so some time variation
   beyond scaling should be expected, but we assume they are negligent (far
   lower than linear growth).

2. The utility also validates the expected DAG against the git repository, so
   not all the time consumed is related to what we are interested in measuring.
   Thus, the timings are to be read worse case scenario and then some. But this
   doesn't bother our benchmarking.

3. Tested on a laptop, with Windows manager and plenty of apps running,
   variation can come from anywhere, but again, we consider that negligent, as
   we only use it for benchmarking, and as long as there was no unexplained
   spike it is perfect for our needs.

Great, let's have some fun and see whether we get any scalability issues. This
is how we execute this commandline utility (It has help if you care to use it
yourself):

```sh
# 5 concurrent agents x 30 Commits x 50 operations = 7,500 Operations = 3,676.47 per second
devbox run just stress
```

Let's draw a table with out results when num agents is our variable.

| Num agents | Num Ops |  real   |  user   |   sys   | Ops/sec  |
| :--------: | :-----: | :-----: | :-----: | :-----: | :------: |
|     5      |  7,500  | 2.046s  | 1.631s  | 1.534s  | 3,676.47 |
|     10     | 15,000  | 5.637s  | 5.077s  | 4.572s  | 2,660.98 |
|     20     | 30,000  | 16.479s | 15.927  | 12.750s | 1,820.50 |
|     25     | 37,500  | 28.062s | 25.179s | 22.382s | 1,336.32 |

Interesting, roughly speaking, doubling the agents (which is also doubling the
number of ops), is tripling the time, which is mostly due to rollbacks/retries
when there is a contention. One can argue whether this is a reasonable use case,
but even if it is, the linear scaling is somewhat acceptable.

But let's dig deeper, what if we doubled the agents but kept the same number of
ops? We can do it by pairing 80 ops for 5 agents, 40 ops for 10 agents and 20
ops for 20 agents.

| Num agents | Num Ops |  real  |  user  |  sys   | Ops/sec  |
| :--------: | :-----: | :----: | :----: | :----: | :------: |
|     5      | 12,000  | 3.694s | 2.067s | 3.213s | 3,252.03 |
|     10     | 12,000  | 3.670s | 3.308s | 2.912s | 3,269.03 |
|     20     | 12,000  | 5.971s | 5.500s | 4.395s | 2,009.71 |

This indeed enforces the hypothesis that the contention may be indeed the
culprit for the degradation above. Because we don't see much effect when we
doubled the agents the first time, only the 2nd time.

Let's dig dipper, what if we kept the number of agents static, and played with
ops?

| Num agents | Ops/Commit | Num Commits | Total Ops |  real  |  user  |  sys   | Ops/sec  |
| :--------: | :--------: | :---------: | :-------: | :----: | :----: | :----: | :------: |
|     10     |     20     |     30      |   6,000   | 2.071s | 1.595s | 1.618s | 2,897.15 |
|     10     |     40     |     30      |  12,000   | 4.200s | 3.758s | 3.451s | 2,857.14 |
|     10     |     80     |     30      |  24,000   | 7.353s | 6.791s | 5.861s | 3,263.97 |

Nice, we don't see any scaling problem when using constant number of agents as
their number of ops grows. Again we seem to have come to the same conclusion
that the library scales well, the degradation is our unrealistic approach of
rollback and retrying when there is contention on branches. In real life, this
is a merge and that doesn't happen in real time, unless we have some solid logic
behind it.

This final benchmark can put our minds to rest. We have the means to minimize
contention on branches: each agent is choosing a branch for each commit
randomly. If we increase the branch pool the likelihood of contention is
decreased, and we can do that via the `-b` switch. Let's do that while keeping
constant number of ops. Let's also choose 20 agents, the number where we saw the
problem start to bud. And to boot, we actually can avoid doing the validations
(-n) and display the retries percentage.

Total Ops = 20 agents x 30 commits x 50 Ops

| Num agents | Total Ops | Num branches |  real   |  user   |   sys   | Ops/sec  | Retries(%) |
| :--------: | :-------: | :----------: | :-----: | :-----: | :-----: | :------: | :--------: |
|     20     |  30,000   |      10      | 12.186s | 12.398s | 13.186s | 2,461.84 |     3%     |
|     20     |  30,000   |      20      | 10.357s | 8.669s  | 10.708s | 2,896.59 |     1%     |
|     20     |  30,000   |      40      | 8.718s  | 6.980s  | 8.791s  | 3,441.15 |    <1%     |

We have seen this >=3000 ops per second before, with lower number of ops and
agents. This seems to support our above assumption that
contention/rollback/retries are responsible for the alleged degradation. So does
the diminishing retries percentage. The validation itself against the git
repository, at least for these numbers, doesn't yield much runtime advantage.

#### Summary

The `greens` utility helped us verify that most importantly, the result of
competing agents introducing data to the repository is correct and valid, but it
also shows that, merging and conflicts notwithstanding, it scales predictably
and well. Also >3000 ops/s on a medium tier laptop is more than what I expected.

## Future development

C⊕rdoba may be enough for the original requirements, but it's not nearly as
feature full like Dolt. Some features may boost the use cases of the library:

- Credentials, roles and security
- Indexing and queries
- Schema, constraints & validations
- Enhanced merging (as function of schema)
- Data tagging — supported via git CLI (`git tag`)
- Data merging — supported via git CLI (`git merge`)
- Adding notes — supported via git CLI (`git notes`)
- Thread context, that allows continuation without the need of propagating the
  context down the stack call (Experimental, not fully tested)

---

## Contributing

C⊕rdoba welcomes your contributions! It is released under the maximally
permissive
[CC0](https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt) public
domain dedication and fallback license, so your changes must also be released
under this license.

### Getting started

C⊕rdoba is written in modern C++ and requires gcc-14 or later. All new features
must be covered by unit or integration tests. Run tests with:

```bash
devbox run just test    # Unit tests (74 assertions)
devbox run just stress  # Stress tests
```

All tests should be executed and pass before submitting a pull request.

---

## License

C⊕rdoba is released under the
[CC0](https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt) public
domain dedication. See the LICENSE file for details.

---

## Contact

For questions or feedback, please open an issue on GitHub or contact C⊕rdoba
[here](cordoba@xmousse.com).
