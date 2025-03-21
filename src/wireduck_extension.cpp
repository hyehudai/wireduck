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
        FILE *pipe = nullptr;
        bool finished = false;  // Track if TShark has finished processing
        std::vector<std::string> selected_fields;
        std::vector<duckdb::LogicalType> field_types;
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
    std::string command;
    
    std::vector<string> protocols;
    if  (input.inputs.size() > 1 && !input.inputs[1].IsNull()){
        // Extract the list of protocols argument
        const auto &list_val = ListValue::GetChildren(input.inputs[1]);  //Extract list elements
        for (const auto &val : list_val) {
            protocols.push_back(val.ToString());  // Convert each element to string
        }
    }
    // Fetch the selected fields and corresponding DuckDB types
    FetchSelectedFields(context, protocols, bind_data->selected_fields, bind_data->field_types);
    // Construct return columns based on fetched fields, and tshark command
    command = "tshark -r " + bind_data->file_path + " -T fields ";
    for (size_t i = 0; i < bind_data->selected_fields.size(); i++) {

        names.push_back(bind_data->selected_fields[i]);
        return_types.push_back(bind_data->field_types[i]);
        command.append (" -e " + bind_data->selected_fields[i] );
    }
    
    // **Start TShark process in reading from stdin**
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
static void ReadPcapFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
    auto &bind_data = data.bind_data->CastNoConst<ReadPcapBindData>();
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
            row.push_back(Trim(field));
        }
        if (row.size() < 5) continue;  // Skip malformed lines  , 5 is the minimum default feidls we add
        // Store in DuckDB output
        for (size_t col_idx = 0; col_idx < bind_data.selected_fields.size(); col_idx++) {
            try {
                std::string field_value = row[col_idx];  // Extract raw value
                LogicalType field_type = bind_data.field_types[col_idx];  // Get expected type
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
                          << " | col_idx: " << col_idx  // Raw value
                          << " | Value: " << row[col_idx]  // Raw value
                          << " | Expected Type: " << bind_data.field_types[col_idx].ToString()  // Type
                          << " | Error: " << e.what()  // Error message
                          << std::endl;

                output.SetValue(col_idx, row_idx, Value());  // Set NULL on error
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
static bool IsTSharkAvailable() {
    FILE *pipe = popen("tshark --version", "r");
    if (!pipe) {
        return false; // Unable to execute command
    }

    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        pclose(pipe);
        return true; // TShark is installed
    }

    pclose(pipe);
    return false; // No output means TShark is missing
}

// **Function to Populate Tables Using TShark**
static void PopulateGlossary(Connection &conn) {
    std::cout << "[WireDuck] Populating glossary tables from TShark (this may take a minute for two)..." << std::endl;

    FILE *pipe;

    // **Insert Protocols**
    pipe = popen("tshark -G protocols", "r");
    if (pipe) {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::stringstream ss(buffer);
            std::string id, protocol_name, description;
            std::getline(ss, id, '\t');
            std::getline(ss, protocol_name, '\t');
            std::getline(ss, description, '\t');

            conn.Query("INSERT INTO glossary_protocols VALUES (" + id + ", '" + protocol_name + "', '" + description + "')");
        }
        pclose(pipe);
    }

    // **Insert Fields**
    pipe = popen("tshark -G fields", "r");
    if (pipe) {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::stringstream ss(buffer);
            std::string id, protocol, field_name, field_type, display, filterable, description;
            std::getline(ss, id, '\t');
            std::getline(ss, protocol, '\t');
            std::getline(ss, field_name, '\t');
            std::getline(ss, field_type, '\t');
            std::getline(ss, display, '\t');
            std::getline(ss, filterable, '\t');
            std::getline(ss, description, '\t');

            conn.Query("INSERT INTO glossary_fields VALUES (" + id + ", '" + protocol + "', '" + field_name + "', '" + field_type + "', " + (display == "T" ? "TRUE" : "FALSE") + ", " + (filterable == "T" ? "TRUE" : "FALSE") + ", '" + description + "')");
        }
        pclose(pipe);
    }

    // **Insert Values**
    pipe = popen("tshark -G values", "r");
    if (pipe) {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::stringstream ss(buffer);
            std::string id, field_name, value, meaning;
            std::getline(ss, id, '\t');
            std::getline(ss, field_name, '\t');
            std::getline(ss, value, '\t');
            std::getline(ss, meaning, '\t');

            conn.Query("INSERT INTO glossary_values VALUES (" + id + ", '" + field_name + "', '" + value + "', '" + meaning + "')");
        }
        pclose(pipe);
    }

    std::cout << "[WireDuck] Glossary tables populated." << std::endl;
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
	           "filter_name TEXT NOT NULL UNIQUE, "
	           "can_enable BOOLEAN NOT NULL, "
	           "is_displayed BOOLEAN NOT NULL, "
	           "is_filterable BOOLEAN NOT NULL);");
	conn.Query("COMMIT");

	// **Run TShark -G protocols**
	FILE *pipe = popen("tshark -G protocols", "r");
	if (!pipe) {
		throw std::runtime_error("[WireDuck] ERROR: Failed to execute 'tshark -G protocols'");
	}

	char buffer[1024];
    conn.Query("BEGIN TRANSACTION");
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

		// Convert 'T'/'F' to Boolean (1/0)
		std::string can_enable_bool = (can_enable == "T") ? "1" : "0";
		std::string is_displayed_bool = (is_displayed == "T") ? "1" : "0";
		std::string is_filterable_bool = (is_filterable == "T") ? "1" : "0";

		// Construct and execute the SQL query
		std::string query = "INSERT INTO glossary_protocols (full_name, short_name, filter_name, can_enable, "
		                    "is_displayed, is_filterable) "
		                    "VALUES ('" +
		                    full_name + "', '" + short_name + "', '" + filter_name + "', " + can_enable_bool + ", " +
		                    is_displayed_bool + ", " + is_filterable_bool + ")";

		conn.Query(query);
	}
    conn.Query("COMMIT");
	pclose(pipe);
}

static void InitializeGlossaryFields(Connection &conn) {
	// **Create the glossary_fields table**
	conn.Query("BEGIN TRANSACTION");
	conn.Query("CREATE TABLE IF NOT EXISTS glossary_fields ("
	           "field_name TEXT NOT NULL UNIQUE, "
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

		// Construct and execute the SQL query
		std::string query = "INSERT INTO glossary_fields (field_name, filter_name, field_type, protocol_filter_name, "
		                    "encoding, bitmask, description) "
		                    "VALUES ('" +
		                    field_name + "', '" + filter_name + "', '" + field_type + "', '" + protocol_filter_name +
		                    "', '" + encoding + "', '" + bitmask + "', '" + description + "')";

		conn.Query(query);
	}

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
    // function with selected protocol
    auto read_pcap_default = TableFunction("read_pcap",
        {LogicalType::VARCHAR },  ReadPcapFunction,ReadPcapBind);

    auto read_pcap_selected = TableFunction("read_pcap",
        {LogicalType::VARCHAR, LogicalType::LIST(LogicalType::VARCHAR)},  ReadPcapFunction,ReadPcapBind);
    
    ExtensionUtil::RegisterFunction(instance, read_pcap_default);
    ExtensionUtil::RegisterFunction(instance, read_pcap_selected);
    
    auto initialize_glossary =TableFunction ("initialize_glossary", {}, InitializeGlossaryProcedure, InitializeGlossaryBind);
    ExtensionUtil::RegisterFunction(instance, initialize_glossary);

}
void WireduckExtension::Load(DuckDB &db) {
    if (!IsTSharkAvailable()) {
        throw std::runtime_error(
            "[WireDuck] ERROR: TShark is not installed or not accesible. Please install TShark before using this extension."
        );
    }

    std::cout << "[WireDuck] TShark detected. Loading extension..." << std::endl;
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
