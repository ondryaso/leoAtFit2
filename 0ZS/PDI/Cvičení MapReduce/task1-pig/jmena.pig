-- Load the data in comma-separated-values format with a given schema
nameCounts =
	LOAD '$INPUT1'
	USING org.apache.pig.piggybank.storage.CSVExcelStorage
		(',', 'NO_MULTILINE', 'NOCHANGE', 'SKIP_INPUT_HEADER')
	AS (
		name : chararray,
		year1900 : int, year1901 : int, year1902 : int, year1903 : int, year1904 : int, year1905 : int,
		year1906 : int, year1907 : int, year1908 : int, year1909 : int, year1910 : int,
		year1911 : int, year1912 : int, year1913 : int, year1914 : int, year1915 : int,
		year1916 : int, year1917 : int, year1918 : int, year1919 : int, year1920 : int,
		year1921 : int, year1922 : int, year1923 : int, year1924 : int, year1925 : int,
		year1926 : int, year1927 : int, year1928 : int, year1929 : int, year1930 : int,
		year1931 : int, year1932 : int, year1933 : int, year1934 : int, year1935 : int,
		year1936 : int, year1937 : int, year1938 : int, year1939 : int, year1940 : int,
		year1941 : int, year1942 : int, year1943 : int, year1944 : int, year1945 : int,
		year1946 : int, year1947 : int, year1948 : int, year1949 : int, year1950 : int,
		year1951 : int, year1952 : int, year1953 : int, year1954 : int, year1955 : int,
		year1956 : int, year1957 : int, year1958 : int, year1959 : int, year1960 : int,
		year1961 : int, year1962 : int, year1963 : int, year1964 : int, year1965 : int,
		year1966 : int, year1967 : int, year1968 : int, year1969 : int, year1970 : int,
		year1971 : int, year1972 : int, year1973 : int, year1974 : int, year1975 : int,
		year1976 : int, year1977 : int, year1978 : int, year1979 : int, year1980 : int,
		year1981 : int, year1982 : int, year1983 : int, year1984 : int, year1985 : int,
		year1986 : int, year1987 : int, year1988 : int, year1989 : int, year1990 : int,
		year1991 : int, year1992 : int, year1993 : int, year1994 : int, year1995 : int,
		year1996 : int, year1997 : int, year1998 : int, year1999 : int, year2000 : int,
		year2001 : int, year2002 : int, year2003 : int, year2004 : int, year2005 : int,
		year2006 : int, year2007 : int, year2008 : int, year2009 : int, year2010 : int,
		year2011 : int, year2012 : int, year2013 : int, year2014 : int, year2015 : int, year2016 : int
	);

-- Extract names
names = FOREACH nameCounts GENERATE name;

-- If there were duplicates, this should get rid of them
-- namesGroups = GROUP names BY name;
-- namesClean = FOREACH namesGroups GENERATE group;

-- Store output
STORE names INTO 'jmena';

-- Merge output using:
-- hadoop fs -cat jmena/* | hadoop fs -put - jmena.txt