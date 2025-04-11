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

    struct InitializeGlossaryData : FunctionData {
        string status_message;
        bool finished = false;  // Track if Glossary finished init
        bool Equals(const FunctionData &other_p) const override {
          throw NotImplementedException("InitializeGlossaryData::Equals");
        }
      
        unique_ptr<FunctionData> Copy() const override {
          throw NotImplementedException("InitializeGlossaryData::Copy");
        }
      };

    struct ReadPcapBindData : public TableFunctionData {
        std::string file_path;
        bool finished = false;  // Track if TShark has finished processing
        std::vector<std::string> selected_fields;
        std::vector<duckdb::LogicalType> field_types;
        FILE *pipe = nullptr;
    };

      
static duckdb::LogicalType MapTsharkTypeToDuckDB(const std::string &tshark_type) {
        if (tshark_type.find("UINT") != std::string::npos || tshark_type.find("INT") != std::string::npos) {
            return duckdb::LogicalType::BIGINT;
        }
        if (tshark_type == "FT_FLOAT" || tshark_type == "FT_DOUBLE") {
            return duckdb::LogicalType::DOUBLE;
        }
        if (tshark_type == "FT_BOOLEAN") {
            return duckdb::LogicalType::BOOLEAN;
        }
        if (tshark_type == "FT_ABSOLUTE_TIME" || tshark_type == "FT_RELATIVE_TIME") {
            return duckdb::LogicalType::TIMESTAMP;
        }
        return duckdb::LogicalType::VARCHAR; // Default to VARCHAR
    }

void FetchSelectedFields(ClientContext &context, const std::vector<std::string> &protocols,
        std::vector<std::string> &selected_fields, std::vector<duckdb::LogicalType> &field_types) {
    Connection conn(DatabaseInstance::GetDatabase(context));
    
    std::stringstream sql;
    sql << "SELECT filter_name, field_type FROM glossary_fields WHERE protocol_filter_name IN (''";
    for (size_t i = 0; i < protocols.size(); i++) {
    sql << ", ";
    sql << "'" << protocols[i] << "'";
    }
    sql << ") OR (filter_name in ('frame.number','frame.time_epoch','frame.protocols','frame.len','_ws.col.info') )   ORDER BY CASE protocol_filter_name "; //add default fields
    sql << " WHEN '" << "frame" << "' THEN " << -1;

    for (size_t i = 0; i < protocols.size(); i++) {
     sql << " WHEN '" << protocols[i] << "' THEN " << (i + 1);
    }
    sql << " END;";
    auto result = conn.Query(sql.str());
    if (result->RowCount() > 0) {  //  Ensure result has rows
        for (idx_t row_idx = 0; row_idx < result->RowCount(); row_idx++) {
            selected_fields.push_back(result->GetValue(0, row_idx).ToString());  // ✅ Convert `Value` to string
            field_types.push_back(MapTsharkTypeToDuckDB(result->GetValue(1, row_idx).ToString()));
        }
    }
    
}

static unique_ptr<FunctionData> ReadPcapBind(ClientContext &context, TableFunctionBindInput &input,
    vector<LogicalType> &return_types, vector<string> &names) {
    auto bind_data = make_uniq<ReadPcapBindData>();
    // Extract the file path argument
    auto &fs = FileSystem::GetFileSystem(context);
    string file_path = input.inputs[0].GetValue<string>();

    // **Open remote or local file using DuckDB’s FileSystem API**
    std::unique_ptr<FileHandle> file_handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
    if (!file_handle) {
        throw IOException("Failed to open file: " + file_path);
    }

    bind_data->file_path = file_handle->GetPath();  // Get actual local path if remote
    std::vector<string> protocols;
    // Get named parameters with default fallback
        
    if (input.named_parameters.count("protocols")) {
        // Extract the list of protocols argument
        const auto &list_val = input.named_parameters["protocols"];
        const auto &list = duckdb::ListValue::GetChildren(list_val);
        for (const auto &val : list) {
            protocols.push_back(val.ToString());  // Convert each element to string
        }
    }
    // Fetch the selected fields and corresponding DuckDB types
    FetchSelectedFields(context, protocols, bind_data->selected_fields, bind_data->field_types);
    // Construct return columns based on fetched fields, and tshark command
    std::string command;
    command = "tshark -r " + bind_data->file_path + " -T fields  ";
    for (size_t i = 0; i < bind_data->selected_fields.size(); i++) {

        names.push_back(bind_data->selected_fields[i]);
        return_types.push_back(bind_data->field_types[i]);
        command.append (" -e " + bind_data->selected_fields[i] );
    }

    if (input.named_parameters.count("climit")) {
        auto limit = input.named_parameters["climit"].GetValue<int64_t>();
        command.append( " -c " + std::to_string (limit) );
    }

    if (input.named_parameters.count("cfilter")) {
        auto filter = input.named_parameters["cfilter"].GetValue<string>();
        command.append( " -Y " +filter );
    }
    bind_data->pipe = popen(command.c_str(), "r");  // Now open TShark for reading
    if (!bind_data->pipe) {
        throw std::runtime_error("Failed to open TShark output.");
    }
    return std::move(bind_data);
}


std::string Trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Parses a TShark output line into a vector of clean strings
inline std::vector<std::string> ParseTsharkLine(char* buffer) {
    std::vector<std::string> fields;

    // strtok modifies the buffer, so we operate on it directly
    char* token = std::strtok(buffer, "\t");
    while (token != nullptr) {
        std::string value(token);

        // Strip trailing newline and carriage return
        while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
            value.pop_back();
        }

        fields.push_back(value);
        token = std::strtok(nullptr, "\t");
    }

    return fields;
}
static void ReadPcapFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
    auto &bind_data = data.bind_data->CastNoConst<ReadPcapBindData>();
    auto &fs = FileSystem::GetFileSystem(context);
    
    // If already finished, signal end of data
    if (bind_data.finished) {
        output.SetCardinality(0);
        return;
    }

    size_t num_columns = bind_data.selected_fields.size();
    size_t max_field_len = 256;  // adjust if fields are longer on average
    size_t buffer_size = num_columns * max_field_len + 1;
    std::vector<char> buffer(buffer_size);  // dynamically sized buffer
    
    idx_t row_idx = 0;
    idx_t max_rows = STANDARD_VECTOR_SIZE;  // Default DuckDB chunk size
    

    // Read from TShark process in chunks
    while (fgets(buffer.data(), buffer.size(), bind_data.pipe) != nullptr) {
        if (row_idx >= max_rows) {
            break;
        }

        std::stringstream ss(buffer.data());
        std::vector<std::string> row;
        std::string field;
        // std::vector<std::string> row = ParseTsharkLine(buffer.data());  TODO use this 
        
        while (std::getline(ss, field, '\t')) {
            row.push_back(Trim(field));
        }
        if (row.size() < 1) continue;  // Skip malformed lines  , 5 is the minimum default feidls we add
        // Store in DuckDB output
        for (size_t col_idx = 0; col_idx < bind_data.selected_fields.size(); col_idx++) {
            try {
                std::string field_value = row[col_idx];  // Extract raw value  
                LogicalType field_type = bind_data.field_types[col_idx];      // Get expected type
                std::string field_name = bind_data.selected_fields[col_idx];  // Get field name

                if (field_value.empty()) {
                    //  If field is blank, set NULL instead of throwing an exception
                    output.SetValue(col_idx, row_idx, Value());
                    continue;
                }
                switch (bind_data.field_types[col_idx].id()) {
                    case LogicalTypeId::BIGINT:
                        output.SetValue(col_idx, row_idx, Value::BIGINT(std::stoll(row[col_idx])));
                        break;
                    case LogicalTypeId::DOUBLE:
                        output.SetValue(col_idx, row_idx, Value::DOUBLE(std::stod(row[col_idx])));
                        break;
                    case LogicalTypeId::BOOLEAN:
                        output.SetValue(col_idx, row_idx, Value::BOOLEAN(row[col_idx] == "1"));
                        break;
                    case LogicalTypeId::TIMESTAMP:
                        output.SetValue(col_idx, row_idx, Value::TIMESTAMP(Timestamp::FromEpochSeconds(std::stod(row[col_idx]))));
                        break;
                    default:
                        output.SetValue(col_idx, row_idx, Value(row[col_idx]));  // Default to TEXT
                        break;
                }
            }
            catch (const std::exception &e) {
                std::cerr << "⚠️ Error extracting field: "
                          << bind_data.selected_fields[col_idx]  // Field name
                          << " | row_idx: " << row_idx  // row_idx  
                          << " | col_idx: " << col_idx  // Raw value
                          << " | Value: " << row[col_idx]  // Raw value
                          << " | Expected Type: " << bind_data.field_types[col_idx].ToString()  // Type
                          << " | Error: " << e.what()  // Error message
                          << std::endl;

                output.SetValue(col_idx, row_idx, Value());  // Set NULL on error
                col_idx++;
                break;
            }
        }
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
// **Function to check if TShark is installed**
static bool CheckTSharkAvailable() {
    int exit_code = std::system("tshark -v > /dev/null 2>&1");
    return exit_code == 0;
}
// Scalar function: returns BOOLEAN
static void CheckTsharkFunction(DataChunk &input, ExpressionState &state, Vector &result) {
    result.SetValue(0, Value::BOOLEAN(CheckTSharkAvailable()));
}

// **Procedure to Initialize the WireDuck Glossary**
static unique_ptr<FunctionData> InitializeGlossaryBind(ClientContext &context, TableFunctionBindInput &input,
                                                       vector<LogicalType> &return_types, vector<string> &names) {

	// Returning a single string column
	return_types.push_back(LogicalType::VARCHAR);
	names.push_back("status_message");
	return make_uniq<InitializeGlossaryData>();
	;
}

// **Initialize and Populate glossary_protocols**
static void InitializeGlossaryProtocols(Connection &conn) {
	// **Create the table**
	conn.Query("BEGIN TRANSACTION");
	conn.Query("CREATE TABLE IF NOT EXISTS glossary_protocols ("
	           "full_name TEXT NOT NULL, "
	           "short_name TEXT, "
	           "filter_name TEXT NOT NULL , "
	           "can_enable BOOLEAN NOT NULL, "
	           "is_displayed BOOLEAN NOT NULL, "
	           "is_filterable BOOLEAN NOT NULL);");
	conn.Query("COMMIT");

	// **Run TShark -G protocols**
	FILE *pipe = popen("tshark -G protocols", "r");
	if (!pipe) {
		throw std::runtime_error("[WireDuck] ERROR: Failed to execute 'tshark -G protocols'");
	}
    constexpr idx_t CHUNK_SIZE = 1024;
    // Prepare the appender
    Appender appender(conn, "glossary_protocols");
    char buffer[1024];

	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		std::stringstream ss(buffer);
		std::string full_name, short_name, filter_name, can_enable, is_displayed, is_filterable;

		// Read all six columns (handling missing values)
		if (!std::getline(ss, full_name, '\t') || !std::getline(ss, short_name, '\t') ||
		    !std::getline(ss, filter_name, '\t') || !std::getline(ss, can_enable, '\t') ||
		    !std::getline(ss, is_displayed, '\t') || !std::getline(ss, is_filterable, '\t')) {
			std::cerr << "[WireDuck] Warning: Skipping malformed line: " << buffer << std::endl;
			continue;
		}

		// Trim whitespace
		auto trim = [](std::string &s) {
			s.erase(0, s.find_first_not_of(" \t\r\n"));
			s.erase(s.find_last_not_of(" \t\r\n") + 1);
		};
		trim(full_name);
		trim(short_name);
		trim(filter_name);

		// Escape single quotes for SQL safety
		auto escape_quotes = [](std::string &s) {
			size_t pos = 0;
			while ((pos = s.find("'", pos)) != std::string::npos) {
				s.replace(pos, 1, "''");
				pos += 2;
			}
		};
		escape_quotes(full_name);
		escape_quotes(short_name);
		
        // Append row to buffer
        appender.BeginRow();
        appender.Append(full_name.c_str());
        appender.Append(short_name.c_str());
        appender.Append(filter_name.c_str());
        appender.Append(can_enable == "T");
        appender.Append(is_displayed == "T");
        appender.Append(is_filterable == "T");
        appender.EndRow();
	}
    appender.Close();
	pclose(pipe);
}

static void InitializeGlossaryFields(Connection &conn) {
	// **Create the glossary_fields table**
	conn.Query("BEGIN TRANSACTION");
	conn.Query("CREATE TABLE IF NOT EXISTS glossary_fields ("
	           "field_name TEXT NOT NULL , "
	           "filter_name TEXT NOT NULL UNIQUE, "
	           "field_type TEXT NOT NULL, "
	           "protocol_filter_name TEXT NOT NULL, "
	           "encoding TEXT, "
	           "bitmask TEXT, "
	           "description TEXT);");

	conn.Query("COMMIT");
	// **Run TShark -G fields**
	FILE *pipe = popen("tshark -G fields", "r");
	if (!pipe) {
		throw std::runtime_error("[WireDuck] ERROR: Failed to execute 'tshark -G fields'");
	}
    Appender appender(conn, "glossary_fields");
	char buffer[2048];
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		std::stringstream ss(buffer);
		std::string row_type, field_name, filter_name, field_type, protocol_filter_name, encoding, bitmask, description;

		// Read the row type first (P or F)
		if (!std::getline(ss, row_type, '\t')) {
			std::cerr << "[WireDuck] Warning: Skipping malformed line: " << buffer << std::endl;
			continue;
		}

		if (row_type != "F") {
			continue; // Skip protocol (P) rows, we only care about fields
		}

		// Read all expected columns, handling missing ones
		std::getline(ss, field_name, '\t');
		std::getline(ss, filter_name, '\t');
		std::getline(ss, field_type, '\t');
		std::getline(ss, protocol_filter_name, '\t'); // ✅ Renamed from "protocol"
		std::getline(ss, encoding, '\t');
		std::getline(ss, bitmask, '\t');
		std::getline(ss, description, '\t');

		// Trim whitespace
		auto trim = [](std::string &s) {
			s.erase(0, s.find_first_not_of(" \t\r\n"));
			s.erase(s.find_last_not_of(" \t\r\n") + 1);
		};
		trim(field_name);
		trim(filter_name);
		trim(field_type);
		trim(protocol_filter_name);
		trim(encoding);
		trim(bitmask);
		trim(description);

		// Escape single quotes for SQL safety
		auto escape_quotes = [](std::string &s) {
			size_t pos = 0;
			while ((pos = s.find("'", pos)) != std::string::npos) {
				s.replace(pos, 1, "''");
				pos += 2;
			}
		};
		escape_quotes(field_name);
		escape_quotes(filter_name);
		escape_quotes(field_type);
		escape_quotes(protocol_filter_name);
		escape_quotes(description);

		  // Append row to buffer
        appender.BeginRow();
        appender.Append(field_name.c_str());
        appender.Append(filter_name.c_str());
        appender.Append(field_type.c_str());

        appender.Append(protocol_filter_name.c_str());
        appender.Append(encoding.c_str());
        appender.Append(bitmask.c_str());
        appender.Append(description.c_str());
        appender.EndRow();
	
	}
    appender.Close();
	pclose(pipe);
}

static void InitializeGlossaryProcedure(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
	auto &bind_data = data.bind_data->CastNoConst<InitializeGlossaryData>();

	// If already finished, signal end of data
	if (bind_data.finished) {
		output.SetCardinality(0);
		return;
	}
	DatabaseInstance &db = DatabaseInstance::GetDatabase(context); // Ensure function uses the same DB instance
	Connection conn(db);                                           // Pass the correct connection
	idx_t row_idx = 0;
	std::cout << "[WireDuck] initializing glossary tables, may take a few minutes .." << std::endl;
	InitializeGlossaryProtocols(conn);
	output.SetValue(0, row_idx++, Value("glossary_protocols initialized"));
	InitializeGlossaryFields(conn);
	output.SetValue(0, row_idx++, Value("glossary_fields initialized"));

	// Output a success message as a single row
	output.SetCardinality(row_idx);
	bind_data.finished = true;
}

static void LoadInternal(DatabaseInstance &instance) {

    // scalar function to validate tshark
    auto check_tshark_func=ScalarFunction (
        "check_tshark_installed",                             // SQL name
        {},                                                   // no parameters
        LogicalType::BOOLEAN,                                 // return type
        CheckTsharkFunction                                   // function pointer
    );
    ExtensionUtil::RegisterFunction(instance, check_tshark_func);


    auto read_pcap_selected = TableFunction("read_pcap",
        {LogicalType::VARCHAR, },  ReadPcapFunction,ReadPcapBind);
        read_pcap_selected.named_parameters["protocols"] =LogicalType::LIST(LogicalType::VARCHAR);
        read_pcap_selected.named_parameters["climit"] = LogicalType::BIGINT;
        read_pcap_selected.named_parameters["cfilter"] = LogicalType::VARCHAR;

    ExtensionUtil::RegisterFunction(instance, read_pcap_selected);
    
    auto initialize_glossary =TableFunction ("initialize_glossary", {}, InitializeGlossaryProcedure, InitializeGlossaryBind);
    ExtensionUtil::RegisterFunction(instance, initialize_glossary);

}
void WireduckExtension::Load(DuckDB &db) {
    if (!CheckTSharkAvailable()) {
        throw InvalidInputException("[WireDuck] ERROR: TShark is not installed or not accesible. Please install TShark before using this extension.");
       
    }
    std::cout << "[WireDuck] TShark detected. Loading extension..." << std::endl;

    auto &catalog = Catalog::GetSystemCatalog(*db.instance);
    Connection conn(*db.instance);

    // Check if glossary tables exist
    auto protocol_check = conn.Query("SELECT name FROM sqlite_master WHERE type='table' AND name='glossary_protocols'");
    auto field_check = conn.Query("SELECT name FROM sqlite_master WHERE type='table' AND name='glossary_fields'");

    if (protocol_check->RowCount() == 0 || field_check->RowCount() == 0) {
        std::cerr << "[WireDuck] initializing glossary tables, may take a few minutes .." << std::endl;
        InitializeGlossaryProtocols(conn);
        InitializeGlossaryFields(conn);
        std::cerr << "[WireDuck] glossary initialized." << std::endl;
    } else {
        std::cerr << "[WireDuck] glossary already initialized." << std::endl;
    }
   
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
