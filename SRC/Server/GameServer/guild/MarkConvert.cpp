// ===========================================================================
// [SERVER] MarkConvert.cpp
// Guild Mark Format Converter - Server Side Only
// Converts old guild_mark.idx/tga format to new block-based format
// ===========================================================================
#include "stdafx.h"
#include <Core/Logging.hpp>
#include "MarkManager.h"

#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define OLD_MARK_INDEX_FILENAME "guild_mark.idx"
#define OLD_MARK_DATA_FILENAME  "guild_mark.tga"

static constexpr uint32_t OLD_IMAGE_WIDTH = 512;
static constexpr uint32_t OLD_IMAGE_HEIGHT = 512;
static constexpr size_t OLD_IMAGE_DATA_SIZE = OLD_IMAGE_WIDTH * OLD_IMAGE_HEIGHT * sizeof(Pixel);

static constexpr uint32_t OLD_MAX_MARK_ROWS = 42;
static constexpr uint32_t OLD_MARKS_PER_ROW = 32;

// ---------------------------------------------------------------------------
// [SERVER] Load legacy raw pixel file (guild_mark.tga is raw BGRA, not TGA)
// ---------------------------------------------------------------------------
static std::vector<Pixel> LoadOldGuildMarkImageFile()
{
	std::ifstream file(OLD_MARK_DATA_FILENAME, std::ios::binary | std::ios::ate);

	if (!file)
	{
		LOG_ERROR("GuildMarkConvert: cannot open {}", OLD_MARK_DATA_FILENAME);
		return {};
	}

	const std::streamoff fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	if (fileSize < 0 || static_cast<size_t>(fileSize) < OLD_IMAGE_DATA_SIZE)
	{
		LOG_ERROR("GuildMarkConvert: {} is too small ({} bytes, expected {})", OLD_MARK_DATA_FILENAME, fileSize, OLD_IMAGE_DATA_SIZE);
		return {};
	}

	std::vector<Pixel> data(OLD_IMAGE_WIDTH * OLD_IMAGE_HEIGHT);

	if (!file.read(reinterpret_cast<char*>(data.data()), OLD_IMAGE_DATA_SIZE))
	{
		LOG_ERROR("GuildMarkConvert: failed to read {}", OLD_MARK_DATA_FILENAME);
		return {};
	}

	return data;
}

// ---------------------------------------------------------------------------
// [SERVER] GuildMarkConvert - migrate old format to new block-based format
// ---------------------------------------------------------------------------
bool GuildMarkConvert(const std::vector<uint32_t>& vecGuildID)
{
	std::error_code ec;
	std::filesystem::create_directory("mark", ec);

	if (!std::filesystem::exists(OLD_MARK_INDEX_FILENAME, ec))
		return true;

	std::ifstream file(OLD_MARK_INDEX_FILENAME);

	if (!file)
		return false;

	std::vector<Pixel> oldImage = LoadOldGuildMarkImageFile();
	if (oldImage.empty())
		return false;

	const std::unordered_set<uint32_t> validGuilds(vecGuildID.begin(), vecGuildID.end());

	LOG_INFO("GuildMarkConvert: starting conversion ({} valid guilds)", validGuilds.size());

	Pixel mark[SGuildMark::SIZE];
	std::string line;

	while (std::getline(file, line))
	{
		std::istringstream ins(line);

		uint32_t guild_id = 0, mark_id = 0;

		if (!(ins >> guild_id >> mark_id))
			continue;

		if (validGuilds.find(guild_id) == validGuilds.end())
		{
			LOG_INFO("GuildMarkConvert: skipping guild {} (not in valid set)", guild_id);
			continue;
		}

		const uint32_t row = mark_id / OLD_MARKS_PER_ROW;
		const uint32_t col = mark_id % OLD_MARKS_PER_ROW;

		if (row >= OLD_MAX_MARK_ROWS)
		{
			LOG_ERROR("GuildMarkConvert: invalid mark_id {} (row {} >= {})", mark_id, row, OLD_MAX_MARK_ROWS);
			continue;
		}

		const uint32_t sx = col * SGuildMark::WIDTH;
		const uint32_t sy = row * SGuildMark::HEIGHT;

		if (sx + SGuildMark::WIDTH > OLD_IMAGE_WIDTH || sy + SGuildMark::HEIGHT > OLD_IMAGE_HEIGHT)
		{
			LOG_ERROR("GuildMarkConvert: mark {} out of bounds (sx={}, sy={})", mark_id, sx, sy);
			continue;
		}

		const Pixel* src = oldImage.data() + sy * OLD_IMAGE_WIDTH + sx;
		Pixel* dst = mark;

		for (uint32_t y = 0; y < SGuildMark::HEIGHT; ++y)
		{
			std::memcpy(dst, src, SGuildMark::WIDTH * sizeof(Pixel));
			dst += SGuildMark::WIDTH;
			src += OLD_IMAGE_WIDTH;
		}

		CGuildMarkManager::instance().SaveMark(guild_id, reinterpret_cast<uint8_t*>(mark));
	}

	file.close();

	std::filesystem::rename(OLD_MARK_INDEX_FILENAME, OLD_MARK_INDEX_FILENAME ".removable", ec);
	std::filesystem::rename(OLD_MARK_DATA_FILENAME, OLD_MARK_DATA_FILENAME ".removable", ec);

	LOG_INFO("GuildMarkConvert: conversion complete");
	return true;
}
