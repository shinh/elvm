#!/bin/sh

set -e

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
