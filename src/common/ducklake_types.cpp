#include "common/ducklake_types.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/to_string.hpp"
#include "duckdb/common/array.hpp"
#include "duckdb/common/optional_idx.hpp"
#include "duckdb/common/type_visitor.hpp"

namespace duckdb {

struct DefaultType {
	const char *name;
	LogicalTypeId id;
};

using ducklake_type_array = std::array<DefaultType, 27>;

static constexpr const ducklake_type_array DUCKLAKE_TYPES {{{"boolean", LogicalTypeId::BOOLEAN},
                                                            {"int8", LogicalTypeId::TINYINT},
                                                            {"int16", LogicalTypeId::SMALLINT},
                                                            {"int32", LogicalTypeId::INTEGER},
                                                            {"int64", LogicalTypeId::BIGINT},
                                                            {"int128", LogicalTypeId::HUGEINT},
                                                            {"uint8", LogicalTypeId::UTINYINT},
                                                            {"uint16", LogicalTypeId::USMALLINT},
                                                            {"uint32", LogicalTypeId::UINTEGER},
                                                            {"uint64", LogicalTypeId::UBIGINT},
                                                            {"uint128", LogicalTypeId::UHUGEINT},
                                                            {"float32", LogicalTypeId::FLOAT},
                                                            {"float64", LogicalTypeId::DOUBLE},
                                                            {"decimal", LogicalTypeId::DECIMAL},
                                                            {"time", LogicalTypeId::TIME},
                                                            {"date", LogicalTypeId::DATE},
                                                            {"timestamp", LogicalTypeId::TIMESTAMP},
                                                            {"timestamp_us", LogicalTypeId::TIMESTAMP},
                                                            {"timestamp_ms", LogicalTypeId::TIMESTAMP_MS},
                                                            {"timestamp_ns", LogicalTypeId::TIMESTAMP_NS},
                                                            {"timestamp_s", LogicalTypeId::TIMESTAMP_SEC},
                                                            {"timestamptz", LogicalTypeId::TIMESTAMP_TZ},
                                                            {"timetz", LogicalTypeId::TIME_TZ},
                                                            {"interval", LogicalTypeId::INTERVAL},
                                                            {"varchar", LogicalTypeId::VARCHAR},
                                                            {"blob", LogicalTypeId::BLOB},
                                                            {"uuid", LogicalTypeId::UUID}}};

static LogicalType ParseBaseType(const string &str) {
	for (auto &ducklake_type : DUCKLAKE_TYPES) {
		if (StringUtil::CIEquals(str, ducklake_type.name)) {
			return ducklake_type.id;
		}
	}

	if (StringUtil::CIEquals(str, "json")) {
		return LogicalType::JSON();
	}

	if (StringUtil::CIEquals(str, "geometry")) {
		LogicalType geo_type(LogicalTypeId::BLOB);
		geo_type.SetAlias("GEOMETRY");
		return geo_type;
	}

	throw InvalidInputException("Failed to parse DuckLake type - unsupported type '%s'", str);
}

static string ToStringBaseType(const LogicalType &type) {
	for (auto &ducklake_type : DUCKLAKE_TYPES) {
		if (type.id() == ducklake_type.id) {
			return ducklake_type.name;
		}
	}
	throw InvalidInputException("Failed to convert DuckDB type to DuckLake - unsupported type %s", type);
}

LogicalType DuckLakeTypes::FromString(const string &type) {
	if (StringUtil::StartsWith(type, "decimal(") && StringUtil::EndsWith(type, ")")) {
		// decimal - parse width/scale
		string decimal_members_str = type.substr(8, type.size() - 9);
		vector<string> decimal_members_vect = StringUtil::SplitWithParentheses(decimal_members_str);
		if (decimal_members_vect.size() != 2) {
			throw NotImplementedException("Invalid DECIMAL type - expected width and scale");
		}
		auto width = std::stoull(decimal_members_vect[0]);
		auto scale = std::stoull(decimal_members_vect[1]);
		return LogicalType::DECIMAL(width, scale);
	}
	return ParseBaseType(type);
}

string DuckLakeTypes::ToString(const LogicalType &type) {
	if (type.HasAlias()) {
		if (type.IsJSONType()) {
			return "json";
		}
		if (type.GetAlias() == "GEOMETRY" && type.id() == LogicalTypeId::BLOB) {
			return "geometry";
		}
		throw InvalidInputException("Unsupported user-defined type");
	}
	switch (type.id()) {
	case LogicalTypeId::STRUCT:
		return "struct";
	case LogicalTypeId::LIST:
		return "list";
	case LogicalTypeId::MAP:
		return "map";
	case LogicalTypeId::DECIMAL:
		return "decimal(" + to_string(DecimalType::GetWidth(type)) + "," + to_string(DecimalType::GetScale(type)) + ")";
	case LogicalTypeId::VARCHAR:
		if (!StringType::GetCollation(type).empty()) {
			throw InvalidInputException("Collations are not supported in DuckLake storage");
		}
		return ToStringBaseType(type);
	default:
		return ToStringBaseType(type);
	}
}

void DuckLakeTypes::CheckSupportedType(const LogicalType &type) {
	TypeVisitor::VisitReplace(type, [](const LogicalType &type) {
		DuckLakeTypes::ToString(type);
		return type;
	});
}

} // namespace duckdb
