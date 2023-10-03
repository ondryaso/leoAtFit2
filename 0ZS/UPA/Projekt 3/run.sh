#!/bin/sh
source venv/bin/activate
python3.8 scraper/get_products.py > urls_run.txt
head -n 20 urls_run.txt | python3.8 scraper/get_prices.py > data_run.tsv
