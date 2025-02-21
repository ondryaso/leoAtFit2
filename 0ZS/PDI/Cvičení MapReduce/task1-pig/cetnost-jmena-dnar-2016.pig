-- Load the data in comma-separated-values format with a given schema
cetnostJmenaDNar =
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

-- Generate sums for each item of the data
cetnostJmenaAll =
	FOREACH cetnostJmenaDNar GENERATE name,
	year1900+year1901+year1902+year1903+year1904+year1905+
	year1906+year1907+year1908+year1909+year1910+
	year1911+year1912+year1913+year1914+year1915+
	year1916+year1917+year1918+year1919+year1920+
	year1921+year1922+year1923+year1924+year1925+
	year1926+year1927+year1928+year1929+year1930+
	year1931+year1932+year1933+year1934+year1935+
	year1936+year1937+year1938+year1939+year1940+
	year1941+year1942+year1943+year1944+year1945+
	year1946+year1947+year1948+year1949+year1950+
	year1951+year1952+year1953+year1954+year1955+
	year1956+year1957+year1958+year1959+year1960+
	year1961+year1962+year1963+year1964+year1965+
	year1966+year1967+year1968+year1969+year1970+
	year1971+year1972+year1973+year1974+year1975+
	year1976+year1977+year1978+year1979+year1980+
	year1981+year1982+year1983+year1984+year1985+
	year1986+year1987+year1988+year1989+year1990+
	year1991+year1992+year1993+year1994+year1995+
	year1996+year1997+year1998+year1999+year2000+
	year2001+year2002+year2003+year2004+year2005+
	year2006+year2007+year2008+year2009+year2010+
	year2011+year2012+year2013+year2014+year2015+year2016 AS count;

-- Limit the previously loaded data
cetnostJmenaAll1000ge = FILTER cetnostJmenaAll BY count >= 1000;

-- Execute all the statements, show output, and store it back into HDFS
--DUMP cetnostJmenaAll1000ge;
STORE cetnostJmenaAll1000ge INTO '$OUTPUT'
	USING PigStorage(':');
