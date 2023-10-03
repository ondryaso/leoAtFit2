#!/bin/sh
# Setup the virtual environment and dependencies
python3.8 -m venv venv
source venv/bin/activate
pip install beautifulsoup4 requests
