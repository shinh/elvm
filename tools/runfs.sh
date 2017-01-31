#!/bin/sh

set -e

mkdir -p $1_dir
cp $1 $1_dir/Program1.fs
cd $1_dir
if [ ! -e project.json ]; then
    dotnet new -l fsharp
fi > /dev/null

mv Program1.fs Program.fs

dotnet restore > /dev/null
dotnet build > /dev/null
dotnet bin/Debug/*/*.dll
