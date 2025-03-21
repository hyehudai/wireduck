
### extension Roadmap
* faile the LOAD if tshark is not accesible
* function run_tshark_report - a debug funtion that allows to call tshark -r  and pas the parameters
* read_pcap with additional option for specific protocols
* push down time filter
* timestamp format - a real timestamp
* push down limit ( use tshark -c)
* use multiple file systems
* add tests tht compare fiels to tshark statistics
* glossary on disl - do not commit every insert
### try with Exchanges pcap files and see what happens


read_pcap('nnn.pcap',protocols=['tcp,fix']) - the bind data will return a schema with all _wl.co , frame and the passed protocols's fields)