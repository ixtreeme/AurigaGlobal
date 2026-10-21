#include "stdafx.h"
#include "JsonFile.h"
#include "locale_service.h"

#include <Core/Logging.hpp>

#include <fstream>
#include <filesystem>

namespace JsonFile
{
	bool Load(const std::string& path, nlohmann::json& doc)
	{
		// Dungeon constants and quest scripts use data/... paths. Resolve these
		// against the configured data root, independent of each core's cwd.
		auto resolved = std::filesystem::path(path);
		if (resolved.is_relative() && !resolved.empty() && *resolved.begin() == "data")
			resolved = std::filesystem::path(LocaleService_GetBasePath()) / resolved.lexically_relative("data");
		std::ifstream in(resolved);

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
