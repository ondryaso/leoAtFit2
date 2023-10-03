# lookup.py
# Train stations and connection querying.
# Author: Ondřej Ondryáš (xondry02@stud.fit.vut.cz)

import datetime
import logging
import typing
from typing import Tuple, Dict, Any, List
from util import get_database
from settings import Settings
from datetime import datetime, date, time

TRAINS_COLLECTION = Settings.TRAINS_COLLECTION
LOCATIONS_COLLECTION = Settings.LOCATIONS_COLLECTION
MAX_S = Settings.MAX_STATIONS_PER_QUERY

db = get_database()
logger = logging.getLogger(__name__)


class LookupModel(typing.NamedTuple):
    from_id: int
    to_id: int
    on_date: date
    departure_after: time | None = None
    departure_before: time | None = None
    arrival_after: time | None = None
    arrival_before: time | None = None


def find_station(name: str, page: int = 0) -> Tuple[Dict[int, str], bool]:
    logger.debug(f"Finding station '{name}', page {page}.")
    locations = db.get_collection(LOCATIONS_COLLECTION)
    res = locations.find(filter={"$text": {"$search": f"\"{name}\""}}, projection={"_id": 1, "name": 1},
                         skip=page * MAX_S, limit=MAX_S + 1)
    res_dict = {}
    got = 0
    for loc in res:
        got += 1
        if got == (MAX_S + 1):
            break
        res_dict[loc["_id"]] = loc["name"]

    logger.debug(f"Got {got} results.")
    return res_dict, got == (MAX_S + 1)


def find_all_stations(name: str, regex: bool) -> List[str]:
    logger.debug(f"Finding stations '{name}', regex mode: {regex}.")
    locations = db.get_collection(LOCATIONS_COLLECTION)
    if regex:
        res = locations.find(filter={"name": {"$regex": name}}, projection={"_id": 0, "name": 1})
    else:
        res = locations.find(filter={"$text": {"$search": f"\"{name}\""}}, projection={"_id": 0, "name": 1})
    return [x["name"] for x in res]


def find_connections(find: LookupModel) -> List:
    logger.debug(f"Finding connections {find.from_id}->{find.to_id} on {find.on_date}.")
    on_datetime = datetime.combine(find.on_date, time(0, 0))

    # A cursor over
    lookup = _location_lookup(find.from_id, find.to_id, on_datetime, on_datetime)
    # The station doesn't exist or no applicable connections found
    if lookup is None:
        return []
    # Array if objects { _id (train ID), on_datetime, valid_to, valid_bitmap, to: [ IDs of following stations ] }
    connections = lookup["connections"]
    if connections is None or len(connections) == 0:
        return []
    # Find applicable connections based on bitmap
    connections = [x["_id"] for x in connections if _goes_on_day(x, find.on_date)]

    # Find trains (their paths are already limited to those available on the given date)
    trains_cursor = _train_lookup_cursor(connections, find)
    result = []

    for possible_train in trains_cursor:
        # Calculate the actual path (array of locations)
        path = _calculate_path(possible_train["paths"])
        # Check if the path matches the time constraints from the query
        if len(path) != 0 and _check_path(path, find):
            result.append(path)

    return result


def get_departure_time(location) -> time | None:
    return _get_time_value("ALD", location)


def get_arrival_time(location) -> time | None:
    return _get_time_value("ALA", location)


def _calculate_path(paths: List) -> List:
    paths.sort(key=lambda x: x["CZPTTCreation"])
    mappings = {}
    paths_dict = {}

    for path in paths:
        key = frozenset(path["path_id"].items())
        mappings[key] = key
        paths_dict[key] = path

    for path in reversed(paths):
        if "related_path_id" not in path:
            pass
        else:
            related_id = path["related_path_id"]
            key = frozenset(related_id.items())
            if mappings[key] == key:
                path_key = frozenset(path["path_id"].items())
                mappings[key] = mappings[path_key]

    locations = []
    unique_vals = set(mappings.values())

    for path_id in unique_vals:
        path = paths_dict[path_id]
        for location in path["Locations"]:
            train_activities = location.get("TrainActivity", [])
            for activity in train_activities:
                if activity["TrainActivityType"] == "0001":
                    locations.append(location)
                    break

    return locations


def _get_time_value(element_name: str, location) -> time | None:
    time_el = location.get(element_name, None)
    time_val: str = time_el.get("Time", None) if time_el is not None else None
    if time_val is None:
        return None
    return time_val.time()


def _check_time_value(time_val: time | None, before: time | None, after: time | None) -> bool:
    if time_val is None:
        return False
    # Check departures
    if before is not None and time_val > before:
        return False
    if after is not None and time_val < after:
        return False

    return True


def _check_path(path: List, find: LookupModel) -> bool:
    dep_val = None
    arr_val = None
    dep_loc_idx = 0
    arr_loc_idx = 0
    cnt = 0

    for location in path:
        loc_code = location["LocationPrimaryCode"]
        if loc_code == find.from_id:
            dep_val = _get_time_value("ALD", location)
            dep_loc_idx = cnt
            if not _check_time_value(dep_val, find.departure_before, find.departure_after):
                logger.warning(f"Invalid location data: train {location.get('OperationalTrainNumber')} departs from "
                               f"{location.get('PrimaryLocationName')} but has no departure time.")
                return False

        if loc_code == find.to_id:
            arr_val = _get_time_value("ALA", location)
            arr_loc_idx = cnt
            if not _check_time_value(arr_val, find.arrival_before, find.arrival_after):
                logger.warning(f"Invalid location data: train {location['OperationalTrainNumber']} arrives to "
                               f"{location['PrimaryLocationName']} but has no arrival time.")
                return False

        cnt += 1

    if dep_val is None or arr_val is None:
        logger.warning("Invalid path: origin or destination station not found in locations.")
        return False

    path.append({"origin_loc_idx": dep_loc_idx, "origin_departure": dep_val,
                 "destination_loc_idx": arr_loc_idx, "destination_arrival": arr_val})
    return True


def _goes_on_day(connection: Dict, on_date: date) -> bool:
    """Calculates whether the train goes on the given date based on its validity range and bitmap"""
    bitmap: str = connection["valid_bitmap"]
    valid_from: datetime = connection["valid_from"].date()
    date_diff = on_date - valid_from

    if date_diff.days < 0:
        return False
    if date_diff.days >= len(bitmap):
        return False

    return bitmap[date_diff.days] == '1'


def _location_lookup(from_id: int, to_id: int, valid_from: datetime, valid_to: datetime):
    locations = db.get_collection(LOCATIONS_COLLECTION)
    cursor = locations.aggregate([
        {
            "$match": {
                "_id": from_id,
                "connections.to": to_id,
                # TODO: Filter out more documents in this query
            }
        }, {
            "$set": {
                "connections": {
                    "$filter": {
                        "input": "$connections",
                        "as": "c",
                        "cond": {
                            "$and": [
                                {"$in": [to_id, "$$c.to"]},
                                {"$lte": ["$$c.valid_from", valid_from]},
                                {"$gte": ["$$c.valid_to", valid_to]}
                            ]
                        }
                    }
                }
            }
        }
    ])

    try:
        return next(cursor)
    except StopIteration:
        return None


def _train_lookup_cursor(train_ids: List[Any], find: LookupModel):
    on_datetime = datetime.combine(find.on_date, time(0, 0))
    pipeline = [
        # Filter out trains that we know they go between our stations; and they contain a path with our date
        {"$match":
            {
                "_id": {"$in": train_ids},
                # I am *almost* certain this isn't needed because we've checked these bounds in the
                # locations/connections query
                # "paths": {"$elemMatch": {
                #     "PlannedCalendar.StartDateTime": {
                #         "$lte": on_datetime
                #     },
                #     "PlannedCalendar.EndDateTime": {
                #         "$gte": on_datetime
                #     }
                # }}}
            }},
        # Filter out paths with the date
        {"$project": {
            "paths": {"$filter": {
                "input": "$paths",
                "as": "p",
                "cond": {"$and": [
                    {"$lte": ["$$p.PlannedCalendar.StartDateTime", on_datetime]},
                    {"$gte": ["$$p.PlannedCalendar.EndDateTime", on_datetime]},
                    {"$eq": ["1", {"$substrBytes": [
                        "$$p.PlannedCalendar.BitmapDays",
                        {
                            "$dateDiff": {
                                "startDate": "$$p.PlannedCalendar.StartDateTime",
                                "endDate": on_datetime,
                                "unit": "day"
                            }
                        },
                        1.0
                    ]}]},
                    # Also remove paths we know for sure they have been explicitly cancelled by a message
                    # (check their Cancellations array and perform the same comparison as above)
                    {"$eq": [0.0, {"$size": {
                        "$filter": {
                            "input": "$$p.Cancellations",
                            "as": "c",
                            "cond": {"$and": [
                                {"$lte": ["$$c.PlannedCalendar.StartDateTime", on_datetime]},
                                {"$gte": ["$$c.PlannedCalendar.EndDateTime", on_datetime]},
                                {"$eq": ["1", {"$substrBytes": [
                                    "$$c.PlannedCalendar.BitmapDays",
                                    {
                                        "$dateDiff": {
                                            "startDate": "$$c.PlannedCalendar.StartDateTime",
                                            "endDate": on_datetime,
                                            "unit": "day"
                                        }
                                    },
                                    1.0
                                ]}]}
                            ]}}}
                    }]}]
                }
            }}
        }},
        # Only return trains with non-empty paths
        {"$match": {"paths": {"$ne": []}}}
    ]

    trains_collection = db.get_collection(TRAINS_COLLECTION)
    return trains_collection.aggregate(pipeline)
