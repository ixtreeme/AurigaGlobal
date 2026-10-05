#include "stdafx.h"

// Grows the backing storage to at least `required` bytes, keeping the
// committed data. The growth is geometric so consecutive appends do not
// reallocate every time.
static void buffer_reserve(LPBUFFER buffer, std::size_t required)
{
	if (buffer->mem_size >= required)
		return;

	std::size_t new_size = buffer->mem_size > 0 ? buffer->mem_size : 1;

	while (new_size < required)
		new_size = new_size + new_size / 2 + 1024;

	std::unique_ptr<char[]> new_data(new char[new_size]);
	memcpy(new_data.get(), buffer->mem_data.get(), buffer->mem_size);

	buffer->mem_data = std::move(new_data);
	buffer->mem_size = new_size;
}

LPBUFFER buffer_new(int size)
{
	if (size < 0)
		return nullptr;

	LPBUFFER buffer = new BUFFER;

	// The legacy pool rounded every request up to the next power of two;
	// keep that so buffer_has_space() reports the same initial capacity.
	std::size_t capacity = size > 0 ? static_cast<std::size_t>(size) : 1;
	std::size_t rounded = 1;

	while (rounded < capacity)
		rounded <<= 1;

	capacity = rounded;

	buffer->mem_data.reset(new char[capacity]);
	buffer->mem_size = capacity;

	return buffer;
}

void buffer_delete(LPBUFFER buffer)
{
	delete buffer;
}

uint32_t buffer_size(LPBUFFER buffer)
{
	return static_cast<uint32_t>(buffer->write_point_pos - buffer->read_point_pos);
}

void buffer_reset(LPBUFFER buffer)
{
	buffer->write_point_pos = 0;
	buffer->read_point_pos = 0;
}

void buffer_write(LPBUFFER& buffer, const void *src, int length)
{
	if (!buffer || length <= 0)
		return;

	buffer_reserve(buffer, buffer->write_point_pos + static_cast<std::size_t>(length));

	memcpy(buffer->mem_data.get() + buffer->write_point_pos, src, static_cast<std::size_t>(length));
	buffer->write_point_pos += static_cast<std::size_t>(length);
}

const void * buffer_read_peek(LPBUFFER buffer)
{
	return buffer->mem_data.get() + buffer->read_point_pos;
}

void buffer_read_proceed(LPBUFFER buffer, int length)
{
	if (length <= 0)
		return;

	const std::size_t available = buffer->write_point_pos - buffer->read_point_pos;
	std::size_t proceed = static_cast<std::size_t>(length);

	if (proceed > available)
	{
		LOG_ERROR("buffer_read_proceed: length argument bigger than buffer (length: {}, buffer: {})", length, available);
		proceed = available;
	}

	if (proceed < available)
		buffer->read_point_pos += proceed;
	else
		buffer_reset(buffer);
}

void * buffer_write_peek(LPBUFFER buffer)
{
	return buffer->mem_data.get() + buffer->write_point_pos;
}

void buffer_write_proceed(LPBUFFER buffer, int length)
{
	if (length <= 0)
		return;

	if (buffer->write_point_pos + static_cast<std::size_t>(length) > buffer->mem_size)
	{
		LOG_ERROR("buffer_write_proceed: buffer overflow (write_pos: {}, length: {}, capacity: {})", buffer->write_point_pos, length, buffer->mem_size);
		buffer_reserve(buffer, buffer->write_point_pos + static_cast<std::size_t>(length));
	}

	buffer->write_point_pos += static_cast<std::size_t>(length);
}

int buffer_has_space(LPBUFFER buffer)
{
	return static_cast<int>(buffer->mem_size - buffer->write_point_pos);
}

void buffer_adjust_size(LPBUFFER& buffer, int add_size)
{
	if (!buffer || add_size <= 0)
		return;

	buffer_reserve(buffer, buffer->write_point_pos + static_cast<std::size_t>(add_size));
}

TEMP_BUFFER::TEMP_BUFFER(int Size, bool ForceDelete) : buf(nullptr)
{
	if (ForceDelete && Size < 1024 * 128)
		Size = 1024 * 128;

	buf = buffer_new(Size);
}

TEMP_BUFFER::~TEMP_BUFFER()
{
	buffer_delete(buf);
}

const void * TEMP_BUFFER::read_peek()
{
	return buffer_read_peek(buf);
}

void TEMP_BUFFER::write(const void * data, int size)
{
	buffer_write(buf, data, size);
}

int TEMP_BUFFER::size()
{
	return static_cast<int>(buffer_size(buf));
}

void TEMP_BUFFER::reset()
{
	buffer_reset(buf);
}
