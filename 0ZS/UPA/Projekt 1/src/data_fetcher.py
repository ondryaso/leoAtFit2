# data_fetcher.py
# Downloading data from the timetable CIS.
# Author: František Nečas (xnecas27@stud.fit.vut.cz)

import os
import logging
from pathlib import Path

import requests
from bs4 import BeautifulSoup

from util import get_database
from settings import Settings

from db_importer import import_files, refresh_locations_view

DOWNLOAD_DIRECTORY = "downloaded"
BASE_URL = "https://portal.cisjr.cz"
DOWNLOAD_ROOT_URL = f"{BASE_URL}/pub/draha/celostatni/szdc/"
PARENT_BUTTON = "[To Parent Directory]"


logger = logging.getLogger(__name__)


def update_data():
    """Downloads and imports data into the database"""
    db = get_database()
    downloaded = download_data(db)
    import_files(downloaded, db)
    refresh_locations_view(db)


def get_links_to_download(url):
    response = requests.get(url)
    if not response.ok:
        # TODO: throw an exception and catch somewhere?
        return {}

    bs = BeautifulSoup(response.text, "html.parser")
    links = {}
    for sample in bs.find_all("a", href=True):
        links[sample.text] = sample["href"]
    return links


def download_file(url, download_as):
    logger.info(f"Downloading {url} into {download_as}")
    response = requests.get(url, stream=True)
    if not response.ok:
        # TODO: throw an exception?
        return

    with open(download_as, "wb") as outfile:
        for chunk in response.iter_content(chunk_size=4196, decode_unicode=True):
            outfile.write(chunk)


def download_data_rec(url, collection, path=''):
    downloaded = {}
    target_dir = Path(DOWNLOAD_DIRECTORY) / path
    if not target_dir.exists():
        target_dir.mkdir()
    for file, link in get_links_to_download(url).items():
        if file == PARENT_BUTTON:
            continue
        full_url = f"{BASE_URL}{link}"
        if link.endswith("/"):
            # Recursively download subdirectory
            downloaded.update(download_data_rec(full_url, collection, Path(path) / file))
        # Only zips are relevant to us
        if file.endswith(".zip"):
            if collection.count_documents({"_id": link}) == 0:
                target_file = target_dir / file
                download_file(full_url, target_file)
                downloaded[target_file] = link

    return downloaded


def download_data(db):
    """Downloads the new data and returns the list of downloaded files."""
    logger.info("Scanning for new files")
    collection = db[Settings.IMPORTED_FILES_COLLECTION]
    return download_data_rec(DOWNLOAD_ROOT_URL, collection)
