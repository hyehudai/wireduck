#define DUCKDB_EXTENSION_MAIN

#include "wireduck_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/file_system.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
// OpenSSL linked through vcpkg
#include <openssl/opensslv.h>

namespace duckdb {

    struct PcapBindData : FunctionData {
        // shared_ptr<MultiFileList> file_list;
        // unique_ptr<MultiFileReader> multi_file_reader;
        // MultiFileReaderBindData reader_bind;
        
        string file_path;
        FILE *pipe = nullptr;
        bool finished = false;  // Track if TShark has finished processing
        
        bool Equals(const FunctionData &other_p) const override {
          throw NotImplementedException("PcapBindData::Equals");
        }
      
        unique_ptr<FunctionData> Copy() const override {
          throw NotImplementedException("PcapBindData::Copy");
        }
      };

static unique_ptr<FunctionData> DefaultPcapBindFunction(ClientContext &context, TableFunctionBindInput &input,
        vector<LogicalType> &return_types, vector<string> &names) {
    auto result = make_uniq<PcapBindData>();
    auto &fs = FileSystem::GetFileSystem(context);
    string file_path = input.inputs[0].GetValue<string>();

    // **Open remote or local file using DuckDB’s FileSystem API**
    std::unique_ptr<FileHandle> file_handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
    if (!file_handle) {
        throw IOException("Failed to open file: " + file_path);
    }

    
    result->file_path = file_handle->GetPath();  // Get actual local path if remote
   
    return_types.push_back(LogicalType::BIGINT);  // Packet Number
    names.push_back("packet_number");

    return_types.push_back(LogicalType::DOUBLE);  // Epoch Timestamp
    names.push_back("epoch_timestamp");

    return_types.push_back(LogicalType::VARCHAR); // Protocol
    names.push_back("protocol");

    return_types.push_back(LogicalType::BIGINT);  // Packet Length
    names.push_back("length");

    return_types.push_back(LogicalType::VARCHAR); // Info
    names.push_back("info");

    // **Start TShark process in reading from stdin**
    std::string command = "tshark -r \"" + result->file_path + "\" -T fields -e frame.number -e frame.time_epoch -e frame.protocols -e frame.len -e _ws.col.Info";
    
    result->pipe = popen(command.c_str(), "r");  // Now open TShark for reading
    if (!result->pipe) {
        throw std::runtime_error("Failed to open TShark output.");
    }

    // Return Function Bind Data
    return std::move(result);

}


static void PcapTableFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
    auto &bind_data = data.bind_data->CastNoConst<PcapBindData>();
    auto &fs = FileSystem::GetFileSystem(context);
    
    // If already finished, signal end of data
    if (bind_data.finished) {
        output.SetCardinality(0);
        return;
    }

    char buffer[1024];
    idx_t row_idx = 0;
    idx_t max_rows = STANDARD_VECTOR_SIZE;  // Default DuckDB chunk size

    // Read from TShark process in chunks
    while (fgets(buffer, sizeof(buffer), bind_data.pipe) != nullptr) {
        if (row_idx >= max_rows) {
            break;  // Stop when chunk size is reached
        }

        std::stringstream ss(buffer);
        std::vector<std::string> row;
        std::string field;

        while (std::getline(ss, field, '\t')) {
            row.push_back(field);
        }

        if (row.size() < 5) continue;  // Skip malformed lines

        // Store in DuckDB output
        output.SetValue(0, row_idx, Value::BIGINT(std::stoll(row[0])));  // Packet Number
        output.SetValue(1, row_idx, Value::DOUBLE(std::stod(row[1])));   // Epoch Timestamp
        output.SetValue(2, row_idx, Value(row[2]));                      // Protocol
        output.SetValue(3, row_idx, Value::BIGINT(std::stoll(row[3])));  // Packet Length
        output.SetValue(4, row_idx, Value(row[4]));                      // Info

        row_idx++;
    }

    // **If no more lines, signal completion**
    if (row_idx == 0) {
        output.SetCardinality(0);
        bind_data.finished = true;
        pclose(bind_data.pipe);
        bind_data.pipe = nullptr;
    } else {
        output.SetCardinality(row_idx);
    }
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
