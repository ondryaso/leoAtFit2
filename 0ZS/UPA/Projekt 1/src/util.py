import pymongo

from settings import Settings


def get_database():
    return pymongo.MongoClient(Settings.CONNECTION_STRING)[Settings.DATABASE_NAME]
