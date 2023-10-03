#!/bin/sh

if [ ! -f "package.json" ]; then
    echo "package.json not found. The working directory must contain the project sources."
fi

npm install
npm test
node performance_tests.js
