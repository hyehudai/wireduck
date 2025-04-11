
This repository is based on https://github.com/duckdb/extension-template, check it out if you want to build and ship your own DuckDB extension.

---
# DuckDB Wireduck Extension
#### Dissection is the first step to analysis.  ><(((('>

![Description](./docs/wireduck.jpg)


## What is this tool good for ?
This extension, Wireduck, allow reading PCAP files using duckdb.

[Wireshark](https://www.wireshark.org/) is the leading open source tool for network traffic analysis, [tshark](https://www.wireshark.org/docs/man-pages/tshark.html), Wireshark's CLI, allows filterting and fetching netework data.
Howerver, when analytics, aggregation, joining and other data wrangleing tasks are in order things gets a little more complex and data wrangling is required. This is where this extension can help by harnessing the argonomity of duckdb and SQL.

In addiiton, while duckdb suports leading data format (parquet ,json, delta, etc) wireshark supports over 3000 protocols. from IoT to Telcom to financial protocols.
So any new dissector developed for wireshark immediatly enables analytics over the data.

## How wireduck works ?
Wireduck runs tshark behind the scenes utilizing wireshark's glossary to be able to parse any packet from any supported protocol to its fields. 


## Installation
### Prerequities
tshark (installed as part of wireshark) is installed.
validate its exists via
```
tshark --version
```
installation is simple through the DuckDB Community Extension repository, just type
```
INSTALL wireduck FROM community
LOAD wireduck
```



## Examples

### the `read_pcap` function
explain how it works behind the scens with tshark and the stdin

### the `glossary` tables
glossary tables are the data dictionary of all protocols and fields supported by wireshark.
it allows wireduck to dynamically build the schema according to the fetached protocol.

* glossary_protocols - contains the supported protocol.
* glossary_fields - contains supported fields per protocol.

### File IO Limitations
The `read_pcap` function is NOT integrated into DuckDB's file system abstraction.
meaning you CANNOT read pcap files directly from e.g. HTTP or S3 sources. For example

Multiple files as a list or via glob are not supported.

fixing this is on the roadmap

## Building
### dependencies
tshark ( installed part of wireshark) is installed.
and validate its exists via
```
tshark --version
```
### Build steps
Now to build the extension, run:
```sh
make
```
The main binaries that will be built are:
```sh
./build/release/duckdb
./build/release/test/unittest
./build/release/extension/wireduck/wireduck.duckdb_extension
```
- `duckdb` is the binary for the duckdb shell with the extension code automatically loaded.
- `unittest` is the test runner of duckdb. Again, the extension is already linked into the binary.
- `wireduck.duckdb_extension` is the loadable binary as it would be distributed.

## Running the extension
To run the extension code, simply start the shell with `./build/release/duckdb`.

Now we can use the features from the extension directly in DuckDB.

## Running the tests
Different tests can be created for DuckDB extensions. The primary way of testing DuckDB extensions should be the SQL tests in `./test/sql`. These SQL tests can be run using:
```sh
make test
```

### Installing the deployed binaries
To install your extension binaries from S3, you will need to do two things. Firstly, DuckDB should be launched with the
`allow_unsigned_extensions` option set to true. How to set this will depend on the client you're using. Some examples:

CLI:
```shell
duckdb -unsigned
```

Python:
```python
con = duckdb.connect(':memory:', config={'allow_unsigned_extensions' : 'true'})
```

NodeJS:
```js
db = new duckdb.Database(':memory:', {"allow_unsigned_extensions": "true"});
```

Secondly, you will need to set the repository endpoint in DuckDB to the HTTP url of your bucket + version of the extension
you want to install. To do this run the following SQL query in DuckDB:
```sql
SET custom_extension_repository='bucket.s3.eu-west-1.amazonaws.com/<your_extension_name>/latest';
```
Note that the `/latest` path will allow you to install the latest extension version available for your current version of
DuckDB. To specify a specific version, you can pass the version instead.

After running these steps, you can install and load your extension using the regular INSTALL/LOAD commands in DuckDB:
```sql
INSTALL wireduck
LOAD wireduck
```


