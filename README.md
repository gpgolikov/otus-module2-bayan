# otus-module2-bayan

OTUS modile 2 homework - _bayan_

## installation
```
$ curl -sSL "https://bintray.com/user/downloadSubjectPublicKey?username=bintray" | apt-key add -
$ echo "deb http://dl.bintray.com/gpgolikov/otus-cpp xenial main" | tee -a /etc/apt/sources.list.d/otus.list

$ apt update
$ apt install bayan
```

## installation of libc++1-7
```
$ curl -sSL "https://build.travis-ci.org/files/gpg/llvm-toolchain-xenial-7.asc" | apt-key add -
$ echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-7 main" | tee -a /etc/apt/sources.list >/dev/null

$ apt update
$ apt install libc++1-7 libc++abi-7

```

## Unicode
_bayan_ tool uses wide char string for internal string storage. All input strings or file names will be convert to wide string using of current local.

## Usage
_bayan_ scans filesystem for duplicates and prints result to standard output by linebreak separated list where different files are separated by additional linebreak.

```
bayan [options] [<path-to-scan> ...]
```
_\<path-to-scan\>_ - path to be scanned for duplicates. Can be repeated multiple times. If no path is specified then current working directory will be scanned.

_Options:_

* -h [ --help] - prints out brief help message.

* -E [ --exclude-path ] arg - path to be excluded from scanning. It is relative from _path-to-scan_ and can be locate in depth of path. Also path to be excluded can contain sub-directories, for example _build/test_, it excludes all _build/test_ folders in _path-to-scan_ no matter how deep it located. Also it is allowed to repeat this option to specified more paths to be excluded from scanning.

```
            bayan -r -E build/test -E .git .
```

* -P [ --patterns ] arg - patterns of files to be scanned. It is allowed to set list of patterns of filenames to be covered by scanning are separated by _,:;_ symbols. Only POSIX extended syntax is supported. Also patterns are case insensetive.

```
            bayan -P ".*\.(cpp|h)"
```

* -B [ --block-size ] arg (=1024) block size in bytes. File divided by block of _block-size_ and compare with already divided files by applying the hash permutation function. If all blocks of two files are equal then files considered equal. If file size is not multiple by _block-size_ then last block will be padded by _zeros_.

* -S [ --min-size ] arg (=1) - minimum file size to be scanned in bytes. It is additional filter to file selecting procedure. If file size less then _min-size_ then file is ignored.

* -H [ --hash ] arg (=md5) - hash function to be applied on file blocks before performing of comparing. md5 and sha256 values are allowed.

```
            bayan -r -E build/test -P ".*\.txt$" -H sha256 ~/projects
```
The command above scans _~/projects_ directory for duplicates of text files with _txt_ extension except _build/test_ directory in all sub-directories using _SHA-256_ hash function.

* -r [ --recursive ] - scan recursively.