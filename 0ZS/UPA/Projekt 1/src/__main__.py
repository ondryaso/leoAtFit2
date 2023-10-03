# __main__.py
# The entry file for this UPA project.
# Author: Ondřej Ondryáš (xondry02@stud.fit.vut.cz)
from datetime import datetime, time
from typing import Tuple
import sys
import logging
import argparse

import dateparser
import inquirer
import lookup
from lookup import get_departure_time, get_arrival_time
from termcolor import colored

from data_fetcher import update_data


def main() -> int:
    """Process arguments and stuff"""
    main_parser = argparse.ArgumentParser()

    command_subparsers = main_parser.add_subparsers(title="commands", dest="command", required=True)

    fetch_updates_parser = command_subparsers.add_parser("fetch-updates",
                                                         help="Check, download and apply timetable updates.")
    search_station_parser = command_subparsers.add_parser("search-station", help="Look up a station.")
    search_parser = command_subparsers.add_parser("search", help="Find connections between two stations.")

    # Base data fetching options
    fetch_updates_parser.add_argument("-c", "--clean", action="store_true",
                                      help="Clear all the current data in the database.")

    # Station search options
    search_station_parser.add_argument("name", help="The station name to search.")
    search_station_parser.add_argument("-r", action="store_true",
                                       help="Interpret the given name as a regular expression.")
    search_station_parser.add_argument("-u", "--update-data", action="store_true",
                                       help="Check and download timetable updates before the search.")

    # Search options
    search_parser.add_argument("station_from", help="The origin station.")
    search_parser.add_argument("station_to", help="The destination station.")
    search_parser.add_argument("date", help="The date to search connections on. If it includes a time value, it's used"
                                            "as the --depart-after parameter value.", )
    search_parser.add_argument("-u", "--update-data", action="store_true",
                               help="Check and download timetable updates before the search.")

    search_parser.add_argument("-da", "--depart-after", metavar="HH:mm",
                               help="Only show connections departing from the origin station after the given time.")
    search_parser.add_argument("-aa", "--arrive-after", metavar="HH:mm",
                               help="Only show connections arriving at the origin station after the given time.")
    search_parser.add_argument("-db", "--depart-before", metavar="HH:mm",
                               help="Only show connections departing from the origin station before the given time.")
    search_parser.add_argument("-ab", "--arrive-before", metavar="HH:mm",
                               help="Only show connections arriving at the origin station before the given time.")

    args = main_parser.parse_args()

    logging.basicConfig(encoding="utf-8", level=logging.DEBUG, format="%(levelname)s: %(message)s")
    logging.getLogger("requests").setLevel(logging.WARNING)
    logging.getLogger("urllib3").setLevel(logging.WARNING)

    if args.command == "fetch-updates":
        update_data()
    elif args.command == "search-station":
        search_station(args)
    elif args.command == "search":
        search(args)
    else:
        print("Something's wrong, I can feel it.")

    return 0


def search_station(args):
    res = lookup.find_all_stations(args.name, args.r)
    for s in res:
        print(f"{s}")


def parse_time(time_str: str | None) -> time | None:
    if time_str is None:
        return None

    try:
        return datetime.strptime(time_str, "%H:%M").time()
    except ValueError:
        print(f"Ignoring invalid time value '{time_str}' (the only accepted format is HH:mm).")


def search(args):
    on_date = dateparser.parse(args.date, locales=["cs", "sk", "de", "pl", "en"])
    if on_date is None:
        print("Invalid date/time specified.")
        return

    origin_station = find_select_station(args.station_from, "origin")
    if origin_station is None:
        return

    destination_station = find_select_station(args.station_to, "destination")
    if destination_station is None:
        return

    on_time = on_date.time()
    if on_time == time(0, 0):
        on_time = None

    query = lookup.LookupModel(from_id=origin_station[0], to_id=destination_station[0], on_date=on_date.date(),
                               departure_after=on_time if args.depart_after is None else parse_time(args.depart_after),
                               departure_before=parse_time(args.depart_before),
                               arrival_after=parse_time(args.arrive_after),
                               arrival_before=parse_time(args.arrive_before))

    connections = lookup.find_connections(query)

    if len(connections) == 0:
        print("No connections found.")
    else:
        print(f"Connections between '{origin_station[1]}' and '{destination_station[1]}' on {on_date}:")
        print_connections(connections)


def print_connections(connections):
    connections.sort(key=lambda x: x[-1]["origin_departure"])

    for connection in connections:
        meta = connection[-1]
        origin_loc_idx = meta["origin_loc_idx"]
        origin_loc = connection[origin_loc_idx]
        dest_loc_idx = meta["destination_loc_idx"]
        origin_dep = meta["origin_departure"]
        dest_arr = meta["destination_arrival"]
        origin_train = origin_loc.get("OperationalTrainNumber", "?")

        print(f"Train {colored(origin_train, attrs=['bold'])}, dep. {origin_dep}, arr. {dest_arr}:")
        print("    Station                     \tArrival  \tDeparture")
        cnt = 0
        for location in connection[0:-1]:
            use_bold = cnt == origin_loc_idx or cnt == dest_loc_idx

            name = location['PrimaryLocationName']
            dep = get_departure_time(location) or '-'
            arr = get_arrival_time(location) or '-'

            print("  > " + colored(f"{name:<28}\t{str(arr):<8}\t{dep}", attrs=(["bold"] if use_bold else None),
                                   on_color=("on_cyan" if use_bold else None)))
            cnt += 1

        print()


def find_select_station(station_name: str, station_type: str, page: int = 0) -> Tuple[int, str] | None:
    res = lookup.find_station(station_name, page)

    prev_page_str = "--- Previous page ---"
    next_page_str = "--- Next page ---"

    if len(res[0]) == 0:
        print(f"Station '{station_name}' not found.")
        return None
    if len(res[0]) == 1 and page == 0:
        return next(iter(res[0].items()))

    stations_rev = {v: k for k, v in res[0].items()}
    choices = list(stations_rev.keys())

    if page > 0:
        choices.insert(0, prev_page_str)
    if res[1]:
        choices.append(next_page_str)

    question = [
        inquirer.List("target", message=f"Choose the {station_type} station", choices=choices)
    ]

    answer = inquirer.prompt(question, raise_keyboard_interrupt=False)
    if answer is None:
        return None

    answer = answer["target"]
    if answer == next_page_str:
        return find_select_station(station_name, station_type, page + 1)
    elif answer == prev_page_str:
        return find_select_station(station_name, station_type, page - 1)
    else:
        return stations_rev[answer], answer


if __name__ == '__main__':
    sys.exit(main())
