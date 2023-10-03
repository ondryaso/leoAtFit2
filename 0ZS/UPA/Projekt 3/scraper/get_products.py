#!/usr/bin/env python3
# This program gets URLs of products from bike-discount.de

import argparse
import sys
import urllib.parse
import time
from typing import List

import requests
from bs4 import BeautifulSoup

from common import ScrapingException, BASE_URL, REQUEST_BURSTS, BURST_WAIT


def get_urls(count: int, category: str) -> List[str]:
    """Gets at least count URLs from the given category (if available)."""
    result: List[str] = []
    current_url = urllib.parse.urljoin(BASE_URL, category)

    request_index = 0
    while current_url and len(result) < count:
        request_index = (request_index + 1) % REQUEST_BURSTS
        if request_index == 0:
            sys.stderr.write(f"Waiting for {BURST_WAIT} seconds before sending another request.\n")
            time.sleep(BURST_WAIT)

        response = requests.get(current_url)
        if not response.ok:
            raise ScrapingException(f"Failed to fetch {current_url}, reason: {response.reason}")

        soup = BeautifulSoup(response.text, "html.parser")
        # Get the links
        for link in soup.find_all("a", {"class": "product--image"}, href=True):
            if len(result) < count:
                result.append(link["href"])
            else:
                break

        # Get the next page if it exists
        current_url = soup.find("a", {"class": "paging--next"}, href=True)
        if current_url:
            # The link is relative
            current_url = urllib.parse.urljoin(BASE_URL, current_url["href"])

    return result


def output_urls(urls: List[str]) -> None:
    for url in urls:
        print(url)


def main() -> None:
    """Main program entrypoint."""
    parser = argparse.ArgumentParser(description="Get URLs of products from bike-discount.de")
    parser.add_argument("--count", "-n", type=int, default=300, help="The number of products to fetch.")
    parser.add_argument("--category", type=str, default="bike",
                        help="The category to fetch the products in. Must be a valid relative URL. Defaults to 'bike'.")
    args = parser.parse_args()
    try:
        urls = get_urls(args.count, args.category)
    except ScrapingException as ex:
        sys.stderr.write(f"An error occurred while scraping:\n{str(ex)}\n")
    else:
        output_urls(urls)


if __name__ == "__main__":
    main()
