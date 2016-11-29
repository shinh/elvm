#!/bin/sh

set -e

crystal build --no-codegen "$1" 2>&1
