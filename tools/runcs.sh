#!/bin/sh

set -e

if which dotnet; then
    mkdir -p $1_dir
    cp $1 $1_dir/Program1.cs
    cd $1_dir
    if [ ! -e project.json ]; then
        dotnet new
        dotnet restore
    fi > /dev/null

    mv Program1.cs Program.cs

    dotnet build > /dev/null
    dotnet bin/Debug/*/*.dll
else
    gmcs $1 -out:$1.exe > /dev/null
    mono $1.exe
fi
