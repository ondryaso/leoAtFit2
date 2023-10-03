import gzip
import logging
from pathlib import Path, PosixPath
from typing import Mapping, Any
import re
from zipfile import ZipFile, is_zipfile

import pymongo
import xmltodict
from pymongo.collection import Collection
from pymongo.database import Database
import datetime

from settings import Settings

LOCATIONS_COLLECTION_VIEW = Settings.LOCATIONS_COLLECTION
TRAINS_COLLECTION = Settings.TRAINS_COLLECTION
IMPORTED_FILES_COLLECTION = Settings.IMPORTED_FILES_COLLECTION

from util import get_database

logger = logging.getLogger(__name__)

related_planned_transport_identifiers_str = "RelatedPlannedTransportIdentifiers"
related_path_id_str = "related_path_id"
calendar_start_date_time_str = "StartDateTime"
calendar_end_date_time_str = "EndDateTime"
cancellation_date_str = "CZPTTCancelation"
train_str = "TR"
path_str = "PA"
id_str = "_id"
path_id_str = "path_id"
identifiers_str = "Identifiers"
information_str = "CZPTTInformation"
paths_str = "paths"
planned_calendar_str = "PlannedCalendar"
company_str = "Company"
core_str = "Core"
variant_str = "Variant"
timetable_year_str = "TimetableYear"
start_date_str = "StartDate"
czptt_location_str = "CZPTTLocation"
location_str = "Location"
location_primary_code_str = "LocationPrimaryCode"
location_primary_name_str = "PrimaryLocationName"
location_country_iso_str = "CountryCodeISO"
location_timing_str = "TimingAtLocation"
location_qualifier_code_str = "@TimingQualifierCode"
timing_time_str = "Time"
timing_offset_str = "Offset"
timing_arrival_str = "ALA"
timing_departure_str = "ALD"
timing_str = "Timing"
calendar_validity_period_str = "ValidityPeriod"
calendar_start_datetime = "StartDateTime"
calendar_end_datetime = "EndDateTime"
calendar_bitmap_days = "BitmapDays"
locations_str = "Locations"
cancellations_str = "Cancellations"
train_path_str = "train_path"
cpzttcis_message = "CZPTTCISMessage"
train_activity_str = "TrainActivity"
train_activity_type_str = "TrainActivityType"
train_stops = "0001"
valid_from_str = "valid_from"
valid_to_str = "valid_to"
valid_bitmap_str = "valid_bitmap"
destinations_str = "to"
location_name_str = "name"
connections_str = "connections"


def is_tar_gz(file):
    # Check magic bytes to see if it is gzip
    with open(file, 'rb') as archive:
        return archive.read(2) == b"\x1f\x8b"


def extract(file):
    if is_tar_gz(file):
        with gzip.GzipFile(file, "r") as file_data:
            yield file_data.read()
    elif is_zipfile(file):
        with ZipFile(file, mode="r") as archive:
            for path_file_name in archive.filelist:
                yield archive.read(path_file_name)


def import_files(downloaded: dict[Path, str], db: Database[Mapping[str, Any]]):
    imported_files_collection = db[IMPORTED_FILES_COLLECTION]
    trains_collection = db[TRAINS_COLLECTION]

    ignored_fields = ["@xmlns:xsd", "@xmlns:xsi"]

    main_file_regex = re.compile(r".*GVD\d{4}\.zip")
    main_files_combined_regex = re.compile(r".*GVD\d{4}(.*)\.zip")
    keys = [str(key) for key in downloaded.keys()]
    main_files = list(filter(main_file_regex.match, keys))
    cancellation_files = [str(key) for key in downloaded.keys() if key.stem.startswith("cancel_")]
    update_files = [key for key in keys if key not in main_files and key not in cancellation_files]

    # Import the main files.
    for main_file in main_files:
        main_file_link = downloaded[PosixPath(main_file)]
        logger.info(f"Importing {main_file} into database.")

        for path_file_data in extract(main_file):
            path_file_data_xml = xmltodict.parse(path_file_data)[cpzttcis_message]
            identifier_keys = {}
            start_dates = {}
            train_data = {}
            path_data = {}

            for element_name, element_data in path_file_data_xml.items():
                if element_name in ignored_fields:
                    continue
                elif element_name == identifiers_str:
                    for identifier_key in \
                            element_data["PlannedTransportIdentifiers"]:
                        object_type: str = identifier_key["ObjectType"]

                        identifier_keys[object_type] = {company_str: int(identifier_key[company_str]),
                                                        core_str: identifier_key[core_str],
                                                        variant_str: identifier_key[variant_str],
                                                        timetable_year_str: int(identifier_key[timetable_year_str])}
                        if start_date_str in identifier_key:
                            start_dates[object_type] = datetime.datetime(identifier_key[start_date_str])

                    train_data[id_str] = identifier_keys[train_str]
                    if train_str in start_dates:
                        train_data[start_date_str] = start_dates[start_date_str]
                elif element_name == information_str:
                    train_data[paths_str] = []
                    path_data[path_id_str] = identifier_keys[path_str]

                    for information_element_name, information_element_data in element_data.items():
                        if information_element_name == czptt_location_str:
                            locations_data = []
                            for location in element_data[czptt_location_str]:
                                location_data = {}
                                for location_element_name, location_element_data in location.items():
                                    if location_element_name == location_str:
                                        location_data[location_country_iso_str] = location_element_data[location_country_iso_str]
                                        location_data[location_primary_name_str] = location_element_data[location_primary_name_str]
                                        location_data[location_primary_code_str] = int(location_element_data[location_primary_code_str])
                                    elif location_element_name == train_activity_str:
                                        train_activity = location_element_data
                                        if isinstance(train_activity, dict):
                                            train_activity = [train_activity]
                                        location_data[location_element_name] = train_activity
                                    elif location_element_name == location_timing_str:

                                        timing_data = location_element_data[timing_str]
                                        if not isinstance(timing_data, list):
                                            timing_data = [timing_data]

                                        for item in timing_data:
                                            date_clean: str = item[timing_time_str]
                                            if date_clean[-3] == ':':
                                                date_clean = date_clean[:-3] + date_clean[-2:]
                                            location_data[item[location_qualifier_code_str]] = {
                                                timing_time_str: datetime.datetime.strptime(date_clean, "%H:%M:%S.%f0%z"),
                                                timing_offset_str: item[timing_offset_str],
                                            }
                                    else:
                                        location_data[location_element_name] = location_element_data
                                locations_data.append(location_data)
                            path_data[locations_str] = locations_data
                            path_data[cancellations_str] = []
                        elif information_element_name == planned_calendar_str:
                            calendar_data = {}
                            calendar_data[calendar_start_date_time_str] = datetime.datetime.strptime(
                                information_element_data[calendar_validity_period_str][calendar_start_date_time_str],
                                "%Y-%m-%dT%H:%M:%S")
                            calendar_data[calendar_end_date_time_str] = datetime.datetime.strptime(
                                information_element_data[calendar_validity_period_str][calendar_end_date_time_str],
                                "%Y-%m-%dT%H:%M:%S")
                            calendar_data[calendar_bitmap_days] = information_element_data[calendar_bitmap_days]
                            path_data[planned_calendar_str] = calendar_data
                else:
                    path_data[element_name] = element_data

            train_data[paths_str].append(path_data)

            if len(list(trains_collection.find({id_str: train_data[id_str]}))) == 0:
                trains_collection.insert_one(train_data)

        # After a successful import of the main file, update collection of imported files.
        update_imported_files_collection(imported_files_collection, main_file_link)

    trains_collection.create_index(f"{paths_str}.{path_id_str}")
    trains_collection.create_index(f"{locations_str}.{location_primary_code_str}")
    trains_collection.create_index([(id_str, pymongo.ASCENDING),
                                    (f"{paths_str}.{path_id_str}", pymongo.ASCENDING)])

    for file in update_files:
        link = downloaded[PosixPath(file)]
        logger.info(f"Importing {file} into database.")

        # Skip main files (already handled or purposefully ignored).
        match = main_files_combined_regex.match(str(file))
        if match:
            # Add it to the collection just to avoid downloading again (the main zip has been added)
            if match.group(1):
                update_imported_files_collection(imported_files_collection, link)
            continue

        # Handle update files.
        for file_data_xml in extract(file):
            file_data = xmltodict.parse(file_data_xml)[cpzttcis_message]
            identifier_keys = {}
            start_dates = {}
            train_data = {}
            related_planned_transport_identifiers = {}
            path_data = {}

            for element_name, element_data in file_data.items():
                if element_name in ignored_fields:
                    continue
                elif element_name == identifiers_str:
                    for identifier_key in \
                            element_data["PlannedTransportIdentifiers"]:
                        object_type: str = identifier_key["ObjectType"]

                        identifier_keys[object_type] = {company_str: int(identifier_key[company_str]),
                                                        core_str: identifier_key[core_str],
                                                        variant_str: identifier_key[variant_str],
                                                        timetable_year_str: int(identifier_key[timetable_year_str])}
                        if start_date_str in identifier_key:
                            start_dates[object_type] = datetime.datetime(identifier_key[start_date_str])

                    train_data[id_str] = identifier_keys[train_str]

                    if related_planned_transport_identifiers_str in element_data:
                        related = element_data[related_planned_transport_identifiers_str]
                        object_type: str = related["ObjectType"]

                        related_planned_transport_identifiers[object_type] = {
                            company_str: int(related[company_str]),
                            core_str: related[core_str],
                            variant_str: related[variant_str],
                            timetable_year_str: int(related[timetable_year_str])}

                        path_data[related_path_id_str] = related_planned_transport_identifiers[object_type]
                    if train_str in start_dates:
                        train_data[start_date_str] = start_dates[start_date_str]
                elif element_name == information_str:
                    train_data[paths_str] = []
                    path_data[path_id_str] = identifier_keys[path_str]

                    for information_element_name, information_element_data in element_data.items():
                        if information_element_name == czptt_location_str:
                            locations_data = []
                            for location in element_data[czptt_location_str]:
                                location_data = {}
                                for location_element_name, location_element_data in location.items():
                                    if location_element_name == location_str:
                                        location_data[location_country_iso_str] = location_element_data[location_country_iso_str]
                                        location_data[location_primary_name_str] = location_element_data[location_primary_name_str]
                                        location_data[location_primary_code_str] = int(location_element_data[location_primary_code_str])
                                    elif location_element_name == train_activity_str:
                                        train_activity = location_element_data
                                        if isinstance(train_activity, dict):
                                            train_activity = [train_activity]
                                        location_data[location_element_name] = train_activity
                                    elif location_element_name == location_timing_str:

                                        timing_data = location_element_data[timing_str]
                                        if not isinstance(timing_data, list):
                                            timing_data = [timing_data]

                                        for item in timing_data:
                                            date_clean: str = item[timing_time_str]
                                            if date_clean[-3] == ':':
                                                date_clean = date_clean[:-3] + date_clean[-2:]
                                            location_data[item[location_qualifier_code_str]] = {
                                                timing_time_str: datetime.datetime.strptime(date_clean, "%H:%M:%S.%f0%z"),
                                                timing_offset_str: item[timing_offset_str],
                                            }
                                    else:
                                        location_data[location_element_name] = location_element_data
                                locations_data.append(location_data)
                            path_data[locations_str] = locations_data
                            path_data[cancellations_str] = []
                        elif information_element_name == planned_calendar_str:
                            calendar_data = {}
                            calendar_data[calendar_start_date_time_str] = datetime.datetime.strptime(
                                information_element_data[calendar_validity_period_str][calendar_start_date_time_str],
                                "%Y-%m-%dT%H:%M:%S")
                            calendar_data[calendar_end_date_time_str] = datetime.datetime.strptime(
                                information_element_data[calendar_validity_period_str][calendar_end_date_time_str],
                                "%Y-%m-%dT%H:%M:%S")
                            calendar_data[calendar_bitmap_days] = information_element_data[calendar_bitmap_days]
                            path_data[planned_calendar_str] = calendar_data
                else:
                    path_data[element_name] = element_data

            if path_data:
                train_data[paths_str].append(path_data)

            found_train = len(list(trains_collection.find({id_str: train_data[id_str]})))
            found_path = len(list(trains_collection.find({id_str: train_data[id_str], f"{paths_str}.{path_id_str}": identifier_keys[path_str]})))
            if found_train:
                # Train record already exists.
                if found_path:
                    pass
                else:  # The current path does not exist yet. Insert it.
                    trains_collection.update_one({id_str: train_data[id_str]}, {"$push":
                                                                                    {paths_str: train_data[paths_str][0]}})
            else:
                trains_collection.insert_one(train_data)

            # After a successful import of the file, update collection of imported files.
            update_imported_files_collection(imported_files_collection, link)

    for file in cancellation_files:
        link = downloaded[PosixPath(file)]
        logger.info(f"Importing {file} into database.")

        # Handle cancellation files.
        for file_data_xml in extract(file):
            file_data = xmltodict.parse(file_data_xml)["CZCanceledPTTMessage"]

            identifier_keys = {}
            start_dates = {}
            cancellation_data = {}

            for element_name, element_data in file_data.items():
                if element_name in ignored_fields:
                    continue
                elif element_name == "PlannedTransportIdentifiers":
                    for identifier_key in \
                            element_data:
                        object_type: str = identifier_key["ObjectType"]

                        identifier_keys[object_type] = {company_str: int(identifier_key[company_str]),
                                                        core_str: identifier_key[core_str],
                                                        variant_str: identifier_key[variant_str],
                                                        timetable_year_str: int(identifier_key[timetable_year_str])}
                elif element_name == planned_calendar_str:
                    calendar_data = {}
                    calendar_data[calendar_start_date_time_str] = datetime.datetime.strptime(
                        element_data[calendar_validity_period_str][calendar_start_date_time_str],
                        "%Y-%m-%dT%H:%M:%S")
                    calendar_data[calendar_end_date_time_str] = datetime.datetime.strptime(
                        element_data[calendar_validity_period_str][calendar_end_date_time_str],
                        "%Y-%m-%dT%H:%M:%S")
                    calendar_data[calendar_bitmap_days] = element_data[calendar_bitmap_days]
                    cancellation_data[planned_calendar_str] = calendar_data
                else:
                    cancellation_data[element_name] = element_data

            train_data = {}
            train_data[id_str] = identifier_keys[train_str]
            train_data[paths_str] = []
            path_data = {path_id_str: identifier_keys[path_str]}
            train_data[paths_str].append(path_data)

            path_data = {path_id_str: identifier_keys[path_str]}
            found_train = len(list(trains_collection.find({id_str: train_data[id_str]})))
            found_path = len(list(trains_collection.find({id_str: train_data[id_str], f"{paths_str}.{path_id_str}": identifier_keys[path_str]})))
            if found_path == 0:
                train_data = {}
                train_data[id_str] = identifier_keys[train_str]
                train_data[paths_str] = []
                path_data[cancellations_str] = []
                path_data[cancellations_str].append(cancellation_data)
                train_data[paths_str].append(path_data)
                if found_train:
                    trains_collection.update_one({id_str: train_data[id_str]}, {"$push":
                        {paths_str: {path_id_str: identifier_keys[path_str], cancellations_str: [cancellation_data]}}
                    })
                else:
                    trains_collection.insert_one(train_data)
            else:  # Path found.
                trains_collection.update_one({id_str: train_data[id_str], f"{paths_str}.{path_id_str}": identifier_keys[path_str]}, {"$push":
                    {f"{paths_str}.$.{cancellations_str}": cancellation_data}})

            # After a successful import of the file, update collection of imported files.
            update_imported_files_collection(imported_files_collection, link)


def get_train_path(train_collection, train_id):
    """Gets path for given train on a specified day."""
    train_path = train_collection.aggregate([
        {"$match": {id_str: train_id}},
        {"$project": {
            id_str: 0,
            "Train": f"${id_str}",
            paths_str: {
                locations_str: {
                    location_primary_code_str: 1,
                    location_primary_name_str: 1,
                    # TODO: Change names to "Arrival" and "Departure".
                    timing_arrival_str: {timing_time_str: 1},
                    timing_departure_str: {timing_time_str: 1},
                }
            },
        }}
    ])

    return train_path


def update_imported_files_collection(imported_files_collection: Collection[Mapping[str, Any]], imported_file_link: str):
    """Inserts a link of the successfully imported file to the IMPORTED_FILES collection."""
    try:
        imported_files_collection.insert_one({"_id": imported_file_link})
    except:
        pass


def refresh_locations_view(db: Database):
    """Creates or updates materialized view of the current data."""
    trains_collection = db[TRAINS_COLLECTION]
    locations_collection = db.get_collection(LOCATIONS_COLLECTION_VIEW)

    # Prepare indexes
    if len(locations_collection.index_information()) == 0:
        logger.info("Preparing location view indexes")
        locations_collection.create_index([(location_name_str, pymongo.TEXT)], default_language="none")
        locations_collection.create_index([(f"{connections_str}.{valid_from_str}", pymongo.ASCENDING),
                                           (f"{connections_str}.{valid_to_str}", pymongo.ASCENDING)])
        locations_collection.create_index(f"{connections_str}.{destinations_str}")

    logger.info("Populating location view")
    trains_collection.aggregate([
        # Get to the locations
        {"$unwind": f"${paths_str}"},
        {"$unwind": f"${paths_str}.{locations_str}"},
        # Keep only those where the train stops
        {
            "$match":
                {
                    f"{paths_str}.{locations_str}.{train_activity_str}.{train_activity_type_str}": {
                        "$in": [train_stops]}
                }
        },
        # Reshape input data to their output structure
        {
            "$project":
                {
                    "_id": "$paths.Locations.LocationPrimaryCode",
                    location_name_str: "$paths.Locations.PrimaryLocationName",
                    "path_id": "$paths.path_id",
                    "train_id": "$_id"
                }
        },
        # Lookup destination locations from the same collection
        {
            "$lookup": {
                "from": TRAINS_COLLECTION,
                "let": {
                    "location_code": "$_id",
                    "train_id": "$train_id",
                    "path_id": "$path_id",
                },
                "pipeline": [
                    # Get the correct train + path
                    {"$match": {"$expr": {"$eq": ["$$train_id", "$_id"]}}},
                    {"$unwind": f"${paths_str}"},
                    {"$match": {"$expr": {"$eq": ["$$path_id", f"${paths_str}.{path_id_str}"]}}},
                    # Bring out the important fields
                    {"$project": {
                        valid_from_str: f"${paths_str}.{planned_calendar_str}.{calendar_start_datetime}",
                        valid_to_str: f"${paths_str}.{planned_calendar_str}.{calendar_end_datetime}",
                        valid_bitmap_str: f"${paths_str}.{planned_calendar_str}.{calendar_bitmap_days}",
                        locations_str: f"${paths_str}.{locations_str}"
                    }},
                    # Keep only stations where the train stops
                    {"$set": {
                        locations_str: {
                            "$filter": {
                                "input": f"${locations_str}",
                                "as": "station",
                                "cond": {
                                    "$cond": {
                                        "if": {"$eq": [{"$type": f"$$station.{train_activity_str}"}, "array"]},
                                        "then": {
                                            "$in": [
                                                train_stops,
                                                f"$$station.{train_activity_str}.{train_activity_type_str}"
                                            ]
                                        },
                                        "else": False
                                    }
                                }
                            }
                        }
                    }},
                    # Keep only stations which come after the currently matched one
                    {"$set": {
                        locations_str: {
                            "$let": {
                                "vars": {
                                    "n": {
                                        "$subtract": [
                                            {"$subtract": [
                                                {"$size": f"${locations_str}"},
                                                {
                                                    "$indexOfArray": [
                                                        {
                                                            "$map": {
                                                                "input": f"${locations_str}",
                                                                "in": {
                                                                    "$eq": [
                                                                        "$$location_code",
                                                                        f"$$this.{location_primary_code_str}"
                                                                    ]
                                                                }
                                                            }
                                                        },
                                                        True
                                                    ]
                                                }
                                            ]},
                                            # Exclude the starting station
                                            1
                                        ]
                                    }
                                },
                                "in": {
                                    "$cond": {
                                        "if": {"$eq": ["$$n", 0]},
                                        "then": [],
                                        "else": {
                                            "$lastN": {
                                                "n": "$$n",
                                                "input": f"${locations_str}",
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }},
                    # Remove paths with no locations
                    {
                        "$match":
                            {
                                locations_str: {
                                    "$ne": []
                                }
                            }
                    },
                    # Keep only location IDs, flatten the objects
                    {"$project": {
                        valid_from_str: {"$toDate": f"${valid_from_str}"},
                        valid_to_str: {"$toDate": f"${valid_to_str}"},
                        valid_bitmap_str: 1,
                        destinations_str: {
                            "$map": {
                                "input": f"${locations_str}",
                                "in": f"$$this.{location_primary_code_str}"
                            }
                        }
                    }}
                ],
                "as": connections_str
            }
        },
        # If there are multiple trains from the station, we want to join the connections into one array
        {
            "$group": {
                "_id": "$_id",
                location_name_str: {"$first": f"${location_name_str}"},
                connections_str: {"$push": f"${connections_str}"},
            }
        },
        # Flatten the array
        {
            "$addFields": {
                connections_str: {
                    "$reduce": {
                        "input": "$connections",
                        "initialValue": [],
                        "in": {"$concatArrays": ["$$value", "$$this"]},
                    }
                }
            }
        },
        # Merge into the on-demand stored view
        {
            "$merge": {
                "into": LOCATIONS_COLLECTION_VIEW,
                "whenMatched": "replace",
                "whenNotMatched": "insert"
            }
        }
    ])


