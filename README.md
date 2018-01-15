# c-git-bruteforce

A tool to bruteforce a git commit hash by adding characters to the commit message.

## Requirements

- libgit2

## Installation

Installs a binary `git-bruteforce` in `/usr/local/bin`

```
$ make
$ make install
```

## Usage

From a git repository, this command will amend the last commit to forge a hash starting with `c0ffee`:

```
$ git-bruteforce c0ffee
```

Obviously, the expected hash must be a string of hexadecimal characters, and longer hashes will take more time to find.

