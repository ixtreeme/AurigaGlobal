#pragma once

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

namespace auriga
{
	struct FileCloser
	{
		void operator()(std::FILE* fp) const noexcept
		{
			if (fp)
				std::fclose(fp);
		}
	};

	// RAII wrapper for the C file API (fscanf-style parsing, crash handlers).
	using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

	inline FilePtr OpenFile(const std::string& path, const char* mode)
	{
		return FilePtr(std::fopen(path.c_str(), mode));
	}

	inline bool FileExists(const std::string& path)
	{
		std::error_code ec;
		return std::filesystem::exists(path, ec);
	}
}
