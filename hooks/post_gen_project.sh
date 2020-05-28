#!/bin/bash -e

if [ ! -d .git ]; then
    git init
    git add .
    git commit -a -m "Initial Cookiecutter Commit from github.com:BrianPugh/cookiecutter-esp32-webserver.git ( 0.3.0 )."

    # Ignore error for rapid cookiecutter development
    git remote add origin {{cookiecutter.git_repo}}
fi

