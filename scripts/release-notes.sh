#!/bin/bash

cat <<- EOH
# Automatically generated Algomorph release

This release is automatically generated every time I push to Algomorph Beta-3.
As such it is the latest version of the code and may be unstable, unusable,
unsuitable for human consumption, and so on.

These assets were built against
https://vcvrack.com/downloads/Rack-SDK-1.0.0.zip

The build date and most recent commits are:

EOH
date
echo ""
echo "Most recent commits:"
echo ""
git log --pretty=oneline | head -5
