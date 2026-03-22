
REPO := `find /tmp/test/ -maxdepth 1 -type d -printf "%T+ %p\n" | sort -r | head -1 | cut -f2 -d" "`

# Build using CMake preset. Usage: just build Release
build preset:
  #!/usr/bin/bash
  cmake --preset {{preset}}
  case "{{preset}}" in
    Debug) cmake --build build/debug ;;
    Release) cmake --build build/release ;;
    DebugSanitize) cmake --build build/s.debug ;;
    ReleaseSanitize) cmake --build build/s.release ;;
    *) cmake --build build/{{preset}} ;;
  esac

# Run unit tests
test:
  ./build/release/gd/crud

# Run tests with CTest
ctest:
  cd build/release && ctest --output-on-failure -R "^(crud|greens)$"

# Run stress test
stress:
  ./build/release/gd/greens -g 5 -b 10 -c 30 -o 50

# Run performance benchmark
bench:
  ./build/release/examples/speed

[private]
alias b := branch
[private]
alias l := log
[private]
alias o := objects
[private]
alias g := git

# Lists all existing tasks
[private]
default:
  @just --list

[private]
pre: 
  @echo {{REPO}}:
  @echo

# Output logs with names and status. Alias 'l'
log: pre
  git --work-tree={{REPO}} --git-dir={{REPO}} log --name-status

# Display existing branches or changes a branch. Alias 'b'
branch id="": pre
  #!/usr/bin/bash
  if [[ "{{id}}" == ""  ]]; then 
    git --work-tree={{REPO}} --git-dir={{REPO}} branch
  else 
    git --work-tree={{REPO}} --git-dir={{REPO}} checkout {{id}}
  fi

# Display all objects in Repo. alias `o`
objects: pre
  git --git-dir={{REPO}} rev-list --objects --all

# Display files introduced in branch
files branch: pre
  git --work-tree={{REPO}} --git-dir={{REPO}} ls-tree -r --name-only {{branch}} 

follow file: pre
  git --work-tree={{REPO}} --git-dir={{REPO}} log --graph --follow --oneline --stat --all -- po.rs

# Runs any git command against the latest test. Alias 'r'
git +args:
  git --work-tree={{REPO}} --git-dir={{REPO}} {{args}}
