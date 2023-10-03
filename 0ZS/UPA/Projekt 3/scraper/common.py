BASE_URL: str = "https://www.bike-discount.de/en/"

# Take a break for 10 seconds after every 20 requests
REQUEST_BURSTS: int = 20
BURST_WAIT: int = 10


class ScrapingException(Exception):
    """An error occurred during scraping."""
