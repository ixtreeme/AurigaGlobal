#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace JsonFile
{
	// Reads and parses a JSON file; logs the reason on failure.
	bool Load(const std::string& path, nlohmann::json& doc);
}
