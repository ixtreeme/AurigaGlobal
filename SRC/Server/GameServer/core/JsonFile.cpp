#include "stdafx.h"
#include "JsonFile.h"

#include <Core/Logging.hpp>

#include <fstream>

namespace JsonFile
{
	bool Load(const std::string& path, nlohmann::json& doc)
	{
		std::ifstream in(path);

		if (!in)
		{
			LOG_ERROR("JSON: cannot open '{}'", path);
			return false;
		}

		try
		{
			doc = nlohmann::json::parse(in);
		}
		catch (const std::exception& ex)
		{
			LOG_ERROR("JSON: invalid file '{}': {}", path, ex.what());
			return false;
		}

		return true;
	}

	std::string ToToken(const nlohmann::json& value)
	{
		if (value.is_string())
			return value.get<std::string>();

		return value.dump();
	}
}
