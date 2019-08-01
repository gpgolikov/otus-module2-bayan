# otus-module2-bayan

OTUS modile 2 homework - _nickname_

## installation
```
$ curl -sSL "https://bintray.com/user/downloadSubjectPublicKey?username=bintray" | apt-key add -
$ echo "deb http://dl.bintray.com/gpgolikov/otus-cpp xenial main" | tee -a /etc/apt/sources.list.d/otus.list

$ apt update
$ apt install nickname
```

## installation of libc++1-7
```
$ curl -sSL "https://build.travis-ci.org/files/gpg/llvm-toolchain-xenial-7.asc" | apt-key add -
$ echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-7 main" | tee -a /etc/apt/sources.list >/dev/null

$ apt update
$ apt install libc++1-7 libc++abi-7

```

## UTF-8
_nickname_ tool uses wide char string for internal string storage.
For achievement of success conversion of string _nickname_ uses `mbsrtowcs` function
and current use locale.
