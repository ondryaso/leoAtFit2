import os


class SettingsMeta(type):
    def __getattribute__(self, item):
        env_value = os.environ.get(item, None)
        if env_value:
            return env_value
        return super().__getattribute__(item)


class Settings(metaclass=SettingsMeta):
    """Static settings of the application.

    The attributes can be overridden from the environment variables.
    """

    CONNECTION_STRING = "mongodb://localhost:27017"
    DATABASE_NAME = "jizdni_rady"
    TRAINS_COLLECTION = "trains"
    IMPORTED_FILES_COLLECTION = "imported_files"
    LOCATIONS_COLLECTION = "locations"
    MAX_STATIONS_PER_QUERY = 10
