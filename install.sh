#!/bin/bash

set -e

helpmsg() {
  echo -ne "Please run \"$ kcz tutorial.txt\" to open the text editor's tutorial.\n"
}

cfgfilemsg() {
  echo -ne "[ ] Moving the unabombrc to ~/.config/kaczynski/ (3/3)"
  cp unabombrc ~/.config/kaczynski/
  echo -ne "\r[\033[0;32m+\033[0m] Unabombrc copied successfully.               (3/3)\n"
  helpmsg
}

installmsg() {
  echo -ne "[ ] Moving the Kaczynski binary to /usr/bin/     (2/3)\n"
  sudo cp bin/kcz /usr/bin/kcz
  if [ $? -eq 0 ]; then
    tput cuu 2
    echo -ne "\r[\033[0;32m+\033[0m] Kaczynski-TED installation successful.       (2/3)\n"
    cfgfilemsg
  else
    tput cuu 2
    echo -ne "\r[\033[0;31m-\033[0m] Kaczynski-TED installation failed.            \n"
  fi
}

buildmsg() {
  echo -ne "[ ] Building Kaczynski...                        (1/3)"
  g++ -std=c++23 -lncursesw -o bin/kcz src/main.cpp
  if [ $? -eq 0 ]; then
    echo -ne "\r[\033[0;32m+\033[0m] Kaczynski-TED built successfully.            (1/3)\n"
    installmsg
  else
    echo -ne "\r[\033[0;31m-\033[0m] Kaczynski-TED build failed.                   \n"
  fi
}

buildmsg
