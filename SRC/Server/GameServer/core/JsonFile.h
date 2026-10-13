#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace JsonFile
{
	// Reads and parses a JSON file; logs the reason on failure.
	bool Load(const std::string& path, nlohmann::json& doc);

	// String value as-is; every other JSON value as its textual form.
	std::string ToToken(const nlohmann::json& value);
}
