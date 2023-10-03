#!/usr/bin/env python3
# This program accepts a list of product URLs on bike-discount.de and obtains their price

import sys
import time
import locale
from typing import Tuple

import requests
from bs4 import BeautifulSoup

from common import ScrapingException, BASE_URL, BURST_WAIT, REQUEST_BURSTS


def get_name_and_price(url: str) -> Tuple[str, str]:
    """Returns product name and its price from the given URL."""
    response = requests.get(url)
    if not response.ok:
        raise ScrapingException(f"Failed to fetch {url}, reason: {response.reason}")
    soup = BeautifulSoup(response.text, "html.parser")
    name = soup.find("h1", {"class": "product--title"})
    if not name:
        raise ScrapingException(f"Failed to parse name from {url}")
    price = soup.find("span", {"id": "netz-price"})
    if not price:
        raise ScrapingException(f"Failed to parse price from {url}")
    price = price.text.strip().replace("€", "").replace(",", "")
    try:
        locale.atof(price)
    except ValueError:
        raise ScrapingException(f"Omitting item with invalid (non-numeric/malformed) price from {url}")
    return name.text.strip(), price


def output_name_and_price(url: str, name: str, price: str) -> None:
    """Outputs name and price of a product."""
    print(f"{url}\t{name}\t{price}")


def main() -> None:
    request_index = 0
    locale.setlocale(locale.LC_ALL, 'en_US.UTF-8')
    for line in sys.stdin:
        url = line.rstrip("\n")
        if not url.startswith(BASE_URL):
            sys.stderr.write(f"URL {url} does not start with {BASE_URL}, skipping.\n")
            continue

        request_index = (request_index + 1) % REQUEST_BURSTS
        if request_index == 0:
            sys.stderr.write(f"Waiting for {BURST_WAIT} seconds before sending another request.\n")
            time.sleep(BURST_WAIT)

        try:
            output_name_and_price(url, *get_name_and_price(url))
        except ScrapingException as ex:
            sys.stderr.write(f"An error occurred while scraping:\n{str(ex)}\n")


if __name__ == "__main__":
    main()
