#!/bin/bash

helpmsg() {
  echo -ne "Please run \"$ kcz tutorial.txt\" to open the text editor's tutorial.\n"
}

cfgfile() {
  echo -ne "[ ] Moving the unabombrc to ~/.config/kaczynski/ (3/3)"
  mkdir ~/.config/kaczynski/ -p
  cp unabombrc ~/.config/kaczynski/
  echo -ne "\r[\033[0;32m+\033[0m] Unabombrc copied successfully.               (3/3)\n"
  helpmsg
}

install_bin() {
  echo -ne "[ ] Moving the Kaczynski binary to /usr/bin/     (2/3)\n"
  sudo cp bin/kcz /usr/bin/kcz
  if [ $? -eq 0 ]; then
    tput cuu 2
    echo -ne "\r[\033[0;32m+\033[0m] Kaczynski-TED installation successful.       (2/3)\n"
    cfgfile
  else
    tput cuu 2
    echo -ne "\r[\033[0;31m-\033[0m] Kaczynski-TED installation failed.                \n"
    echo -ne "    Please make sure /usr/bin/kcz is not in use and that you have the neccessary permissions.\n"
  fi
}

build() {
  echo -ne "[ ] Building Kaczynski...                        (1/3)"
  g++ src/main.cpp -o bin/kcz -std=c++23 -lncursesw
  if [ $? -eq 0 ]; then
    echo -ne "\r[\033[0;32m+\033[0m] Kaczynski-TED built successfully.            (1/3)\n"
    install_bin
  else
    echo -ne "\r[\033[0;31m-\033[0m] Kaczynski-TED build failed.                       \n"
  fi
}

build
