
This repository is based on https://github.com/duckdb/extension-template, check it out if you want to build and ship your own DuckDB extension.

---
# DuckDB Wireduck Extension
#### Dissection is the first step to analysis.  ><(((('>

![Description](./docs/wireduck.jpg)


## What is this tool good for ?
This extension, Wireduck, allow reading PCAP files using duckdb.

[Wireshark](https://www.wireshark.org/) is the leading open source tool for network traffic analysis, [tshark](https://www.wireshark.org/docs/man-pages/tshark.html), Wireshark's CLI, allows filterting and fetching netework data.
Howerver, when analytics, aggregation, joining and other data wrangleing tasks are in order things gets a little more complex. This is where this extension can help by harnessing the argonomity of duckdb and SQL.

In addiiton, while duckdb supports leading data format (parquet ,json, delta, etc) wireshark supports over 3000 protocols. from IoT to Telcom to financial protocols.
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
read_pcap reads the passed pcap file and extract the default fields supported by tshark.
```
select * from read_pcap('~/wireduck/fix.pcap')   limit 10;
┌─────────────────────┬──────────────┬───────────┬──────────────────────────┬─────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│  frame.time_epoch   │ frame.number │ frame.len │     frame.protocols      │                                              _ws.col.info                                               │
│      timestamp      │    int64     │   int64   │         varchar          │                                                 varchar                                                 │
├─────────────────────┼──────────────┼───────────┼──────────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ 2015-11-28 17:59:35 │            1 │        74 │ eth:ethertype:ip:tcp     │ 53867 → 11001 [SYN] Seq=0 Win=43690 Len=0 MSS=65495 SACK_PERM TSval=734420 TSecr=0 WS=128               │
│ 2015-11-28 17:59:35 │            2 │        74 │ eth:ethertype:ip:tcp     │ 11001 → 53867 [SYN, ACK] Seq=0 Ack=1 Win=65535 Len=0 MSS=65495 SACK_PERM TSval=734420 TSecr=734420 WS=4 │
│ 2015-11-28 17:59:35 │            3 │        66 │ eth:ethertype:ip:tcp     │ 53867 → 11001 [ACK] Seq=1 Ack=1 Win=43776 Len=0 TSval=734420 TSecr=734420                               │
│ 2015-11-28 17:59:35 │            4 │       166 │ eth:ethertype:ip:tcp:fix │ Logon                                                                                                   │
│ 2015-11-28 17:59:35 │            5 │        66 │ eth:ethertype:ip:tcp     │ 11001 → 53867 [ACK] Seq=1 Ack=101 Win=130968 Len=0 TSval=734420 TSecr=734420                            │
│ 2015-11-28 17:59:35 │            6 │       166 │ eth:ethertype:ip:tcp:fix │ Logon                                                                                                   │
│ 2015-11-28 17:59:35 │            7 │        66 │ eth:ethertype:ip:tcp     │ 53867 → 11001 [ACK] Seq=101 Ack=101 Win=43776 Len=0 TSval=734420 TSecr=734420                           │
│ 2015-11-28 17:59:45 │            8 │       147 │ eth:ethertype:ip:tcp:fix │ Heartbeat                                                                                               │
│ 2015-11-28 17:59:45 │            9 │       147 │ eth:ethertype:ip:tcp:fix │ Heartbeat                                                                                               │
│ 2015-11-28 17:59:45 │           10 │        66 │ eth:ethertype:ip:tcp     │ 53867 → 11001 [ACK] Seq=182 Ack=182 Win=43776 Len=0 TSval=736934 TSecr=736934                           │
├─────────────────────┴──────────────┴───────────┴──────────────────────────┴─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ 10 rows                                                                                                                                                                   5 columns │
└─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```
Additional parameter supported is *climit* - that corresponds to tshark "-c" option to limit the number of captured packets read.
(this is more efficient from using SQL limit)
```
select * from read_pcap('~/wireduck/fix.pcap', climit:=4)   ;
┌─────────────────────┬──────────────┬───────────┬──────────────────────────┬─────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│  frame.time_epoch   │ frame.number │ frame.len │     frame.protocols      │                                              _ws.col.info                                               │
│      timestamp      │    int64     │   int64   │         varchar          │                                                 varchar                                                 │
├─────────────────────┼──────────────┼───────────┼──────────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ 2015-11-28 17:59:35 │            1 │        74 │ eth:ethertype:ip:tcp     │ 53867 → 11001 [SYN] Seq=0 Win=43690 Len=0 MSS=65495 SACK_PERM TSval=734420 TSecr=0 WS=128               │
│ 2015-11-28 17:59:35 │            2 │        74 │ eth:ethertype:ip:tcp     │ 11001 → 53867 [SYN, ACK] Seq=0 Ack=1 Win=65535 Len=0 MSS=65495 SACK_PERM TSval=734420 TSecr=734420 WS=4 │
│ 2015-11-28 17:59:35 │            3 │        66 │ eth:ethertype:ip:tcp     │ 53867 → 11001 [ACK] Seq=1 Ack=1 Win=43776 Len=0 TSval=734420 TSecr=734420                               │
│ 2015-11-28 17:59:35 │            4 │       166 │ eth:ethertype:ip:tcp:fix │ Logon                                                                                                   │
└─────────────────────┴──────────────┴───────────┴──────────────────────────┴─────────────────────────────────────────────────────────────────────────────────────────────────────────┘
 
```

### the `glossary` tables
Glossary tables are the data dictionary of all protocols and fields supported by wireshark.
it allows wireduck to dynamically build the schema according to the fetached protocol.

* glossary_protocols - contains the supported protocol (same as "tshark -G protocol").
* glossary_fields - contains supported fields per protocol (same as "tshark -G fields").

glossary tables are loaded once during the extension load.
```
D load wireduck;
[WireDuck] TShark detected. Loading extension...
[WireDuck] initializing glossary tables, may take a few minutes ..
[WireDuck] glossary initialized.
D .tables
glossary_fields     glossary_protocols
```
using the glossary tables wireduck can deduce the returned schema once specific protocol[s] are selected using the *protocols* parameter.

for example:
```
select * from read_pcap('~/wireduck/fix.pcap', protocols:=['udp'] ,climit:=4)   ;
┌─────────────────────┬──────────────┬───────────┬──────────────────────┬───┬──────────────────────┬───────────────────┬──────────────────────┬─────────────────────┬──────────────────────┐
│  frame.time_epoch   │ frame.number │ frame.len │   frame.protocols    │ … │ udplite.checksum_c…  │ udp.checksum.zero │ udp.checksum.partial │ udp.length.bad_zero │     _ws.col.info     │
│      timestamp      │    int64     │   int64   │       varchar        │   │       varchar        │      varchar      │       varchar        │       varchar       │       varchar        │
├─────────────────────┼──────────────┼───────────┼──────────────────────┼───┼──────────────────────┼───────────────────┼──────────────────────┼─────────────────────┼──────────────────────┤
│ 2015-11-28 17:59:35 │            1 │        74 │ eth:ethertype:ip:tcp │ … │ NULL                 │ NULL              │ NULL                 │ NULL                │ 53867 → 11001 [SYN…  │
│ 2015-11-28 17:59:35 │            2 │        74 │ eth:ethertype:ip:tcp │ … │ NULL                 │ NULL              │ NULL                 │ NULL                │ 11001 → 53867 [SYN…  │
│ 2015-11-28 17:59:35 │            3 │        66 │ eth:ethertype:ip:tcp │ … │ NULL                 │ NULL              │ NULL                 │ NULL                │ 53867 → 11001 [ACK…  │
│ 2015-11-28 17:59:35 │            4 │       166 │ eth:ethertype:ip:t…  │ … │ NULL                 │ NULL              │ NULL                 │ NULL                │ Logon                │
├─────────────────────┴──────────────┴───────────┴──────────────────────┴───┴──────────────────────┴───────────────────┴──────────────────────┴─────────────────────┴──────────────────────┤
│ 4 rows                                                                                                                                                              32 columns (9 shown) │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```
note that 32 colums are selected. this is becasue tshark suports 32 fields for the udp protocol.

```
desc select * from read_pcap('~/wireduck/fix.pcap', protocols:=['udp'] ,climit:=4)   ;
┌───────────────────────────────┬─────────────┬─────────┬─────────┬─────────┬─────────┐
│          column_name          │ column_type │  null   │   key   │ default │  extra  │
│            varchar            │   varchar   │ varchar │ varchar │ varchar │ varchar │
├───────────────────────────────┼─────────────┼─────────┼─────────┼─────────┼─────────┤
│ frame.time_epoch              │ TIMESTAMP   │ YES     │ NULL    │ NULL    │ NULL    │
│ frame.number                  │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ frame.len                     │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ frame.protocols               │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.srcport                   │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.dstport                   │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.port                      │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.stream                    │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.length                    │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.checksum                  │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.checksum_calculated       │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.checksum.status           │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.proc.srcuid               │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.proc.srcpid               │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.proc.srcuname             │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.proc.srccmd               │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.proc.dstuid               │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.proc.dstpid               │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.proc.dstuname             │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.proc.dstcmd               │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.pdu.size                  │ BIGINT      │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.time_relative             │ TIMESTAMP   │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.time_delta                │ TIMESTAMP   │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.payload                   │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.possible_traceroute       │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.length.bad                │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udplite.checksum_coverage.bad │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.checksum.zero             │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.checksum.partial          │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.checksum.bad              │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ udp.length.bad_zero           │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
│ _ws.col.info                  │ VARCHAR     │ YES     │ NULL    │ NULL    │ NULL    │
├───────────────────────────────┴─────────────┴─────────┴─────────┴─────────┴─────────┤
│ 32 rows                                                                   6 columns │
└─────────────────────────────────────────────────────────────────────────────────────┘
```
once protocol fields are deduced from the protocol parameter -the fields can be used in a query.

```
D select count(*) , sum ("tcp.len") , "tcp.srcport" ,"tcp.dstport"   from read_pcap('~/wireduck/fix.pcap', protocols:=['ip','tcp'])  group by  "tcp.srcport" ,"tcp.dstport" ;
┌──────────────┬────────────────┬─────────────┬─────────────┐
│ count_star() │ sum("tcp.len") │ tcp.srcport │ tcp.dstport │
│    int64     │     int128     │    int64    │    int64    │
├──────────────┼────────────────┼─────────────┼─────────────┤
│          429 │         259678 │       11001 │       53867 │
│           56 │          19702 │       53867 │       11001 │
└──────────────┴────────────────┴─────────────┴─────────────┘
```


### File IO Limitations

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


