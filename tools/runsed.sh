#!/bin/sh

set -e

(cat ; echo) | sed -n -f $1
