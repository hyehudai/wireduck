
This repository is based on https://github.com/duckdb/extension-template, check it out if you want to build and ship your own DuckDB extension.

---
# DuckDB Wireduck Extension
![Description](./docs/wireduck.jpg)

This extension, Wireduck, allow you to read PCAP files using duckdb.
It uses behind the scenes the tshark utility and allows flexible parsing 
and analysis of network captures.

## Installation
### Prerequities
tshark ( installed part of wireshark) is installed.
and validate its exists via
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
### the `glossary` function


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


