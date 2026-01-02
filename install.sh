#!/bin/bash

set -e

buildmsg() {
  echo -ne "[ ] Building Kaczynski-TED...                (1/2)"
  g++ -std=c++23 -lncurses -o bin/kaczynski src/main.cpp
  if [ $? -eq 0 ]; then
    echo -ne "\r[\033[0;32m+\033[0m] Kaczynski-TED built successfully.        (1/2)\n"
  else
    echo -ne "\r[\033[0;31m-\033[0m] Kaczynski-TED build failed.                   \n"
  fi
}

installmsg() {
  echo -ne "[ ] Moving Kaczynski-TED binary to /usr/bin/ (2/2)\n"
  sudo cp bin/kaczynski /usr/bin/kcz
  if [ $? -eq 0 ]; then
    tput cuu 2
    echo -ne "\r[\033[0;32m+\033[0m] Kaczynski-TED installation successful.   (2/2)\n"
  else
    tput cuu 2
    echo -ne "\r[\033[0;31m-\033[0m] Kaczynski-TED installation failed.            \n"
  fi
}

helpmsg() {
  echo -ne "Please run \"$ kcz\" to open the text editor.   \n"
}

buildmsg
installmsg
helpmsg
