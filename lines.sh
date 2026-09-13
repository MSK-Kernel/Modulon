#!/bin/bash

echo "Counting lines per file:"
git ls-files | grep '\.c' | xargs wc -l
