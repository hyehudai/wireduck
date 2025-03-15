#define DUCKDB_EXTENSION_MAIN

#include "wireduck_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

// OpenSSL linked through vcpkg
#include <openssl/opensslv.h>

namespace duckdb {

    struct PcapBindData : FunctionData {
        // shared_ptr<MultiFileList> file_list;
        // unique_ptr<MultiFileReader> multi_file_reader;
        // MultiFileReaderBindData reader_bind;
        vector<string> names;
        vector<LogicalType> types;
      
        bool Equals(const FunctionData &other_p) const override {
          throw NotImplementedException("PcapBindData::Equals");
        }
      
        unique_ptr<FunctionData> Copy() const override {
          throw NotImplementedException("PcapBindData::Copy");
        }
      };

static unique_ptr<FunctionData> DefaultPcapBindFunction(ClientContext &context, TableFunctionBindInput &input,
        vector<LogicalType> &return_types, vector<string> &names) {
    // Define the output schema (Column Names & Types)
    auto result = make_uniq<PcapBindData>();
    result ->types.push_back(LogicalType::BIGINT);  // Packet Number
    result->names.push_back("packet_number");

    result ->types.push_back(LogicalType::DOUBLE);  // Epoch Timestamp
    result->names.push_back("epoch_timestamp");

    result ->types.push_back(LogicalType::VARCHAR); // Protocol
    result->names.push_back("protocol");

    result ->types.push_back(LogicalType::BIGINT);  // Packet Length
    result->names.push_back("length");

    result ->types.push_back(LogicalType::VARCHAR); // Info
    result->names.push_back("info");

    return_types=result ->types;
    names= result->names;
    // Return Function Bind Data
   
    return result;
}


static int was=0;
static void PcapTableFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
   
    vector<vector<string>> tshark_output = {
        {"1", "1678901234.1234", "eth:ip:tcp", "64", "SYN request"},
        {"2", "1678901234.2234", "eth:ip:udp", "128", "DNS Query"},
    };

    idx_t row_idx = 0;
    output.Reset();

    if (was<1)
    {
        for (const auto &row : tshark_output) {
            std::cout << "row_idx " << row_idx << " rows." << std::endl;
            output.SetValue(0, row_idx, Value::BIGINT(std::stoll(row[0])));  // Packet Number
            output.SetValue(1, row_idx, Value::DOUBLE(std::stod(row[1])));   // Epoch Timestamp
            output.SetValue(2, row_idx, Value(row[2]));                      // Protocol
            output.SetValue(3, row_idx, Value::BIGINT(std::stoll(row[3])));  // Packet Length
            output.SetValue(4, row_idx, Value(row[4]));                      // Info

            row_idx++;
        }
        was=1;
    }
    
    std::cout << "out of loop" << std::endl;
    output.SetCardinality(row_idx);
    return;
}
inline void WireduckScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &name_vector = args.data[0];
    UnaryExecutor::Execute<string_t, string_t>(
	    name_vector, result, args.size(),
	    [&](string_t name) {
			return StringVector::AddString(result, "Wireduck "+name.GetString()+" ^^^");
        });
}

inline void WireduckOpenSSLVersionScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &name_vector = args.data[0];
    UnaryExecutor::Execute<string_t, string_t>(
	    name_vector, result, args.size(),
	    [&](string_t name) {
			return StringVector::AddString(result, "Wireduck " + name.GetString() +
                                                     ", my linked OpenSSL version is " +
                                                     OPENSSL_VERSION_TEXT );
        });
}

static void LoadInternal(DatabaseInstance &instance) {
    // Register a scalar function
    auto table_function = TableFunction("read_pcap", {LogicalType::VARCHAR},  PcapTableFunction,DefaultPcapBindFunction);
    // TODO support pojectio and multifile
    // table_function.projection_pushdown = true;
    // MultiFileReader::AddParameters(table_function);
    // ExtensionUtil::RegisterFunction(instance, MultiFileReader::CreateFunctionSet(table_function)); 
    ExtensionUtil::RegisterFunction(instance,table_function);

    auto wireduck_scalar_function = ScalarFunction("wireduck", {LogicalType::VARCHAR}, LogicalType::VARCHAR, WireduckScalarFun);
    ExtensionUtil::RegisterFunction(instance, wireduck_scalar_function);

    // Register another scalar function
    auto wireduck_openssl_version_scalar_function = ScalarFunction("wireduck_openssl_version", {LogicalType::VARCHAR},
                                                LogicalType::VARCHAR, WireduckOpenSSLVersionScalarFun);
    ExtensionUtil::RegisterFunction(instance, wireduck_openssl_version_scalar_function);


}

void WireduckExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}
std::string WireduckExtension::Name() {
	return "wireduck";
}

std::string WireduckExtension::Version() const {
#ifdef EXT_VERSION_WIREDUCK
	return EXT_VERSION_WIREDUCK;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void wireduck_init(duckdb::DatabaseInstance &db) {
    duckdb::DuckDB db_wrapper(db);
    db_wrapper.LoadExtension<duckdb::WireduckExtension>();
}

DUCKDB_EXTENSION_API const char *wireduck_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
