#!/bin/bash -e

# Skip post-hook if invoked my cookiecutter makefile
[ -d "build" ] && exit 0

git init
git add .
( rmdir components/esp_littlefs && git submodule add https://github.com/joltwallet/esp_littlefs.git components/esp_littlefs && git submodule update --init --recursive ) || true
git commit -a -m "Initial Cookiecutter Commit from github.com:BrianPugh/cookiecutter-esp32-webserver.git ( 0.3.1 )."

git remote add origin {{cookiecutter.git_repo}}
