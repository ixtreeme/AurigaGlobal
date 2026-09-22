#include "stdafx.h"

static LPBUFFER normalized_buffer_pool[32] = {};

#define DEFAULT_POOL_SIZE 8192

// internal function forward
void buffer_realloc(LPBUFFER& buffer, int length);

static int buffer_get_pool_index(int size) {
	int i;
	for (i = 0; i < 32; ++i) {
		if ((1 << i) >= size) {
			return i;
		}
	}
	return -1; // too big... not pooled
}
static int buffer_get_exac_pool_index(int size) {
	int i;
	for (i = 0; i < 32; ++i) {
		if ((1 << i) == size) {
			return i;
		}
	}
	return -1; // too big... not pooled
}
static void buffer_pool_free ()
{
	for (int i = 31; i >= 0; i--)
	{
		if (normalized_buffer_pool[i] != nullptr)
		{
			LPBUFFER next;
			for (LPBUFFER p = normalized_buffer_pool[i]; p != nullptr; p = next)
			{
				next = p->next;
				free(p->mem_data);
				free(p);
			}
			normalized_buffer_pool[i] = nullptr;
		}
	}
}
static bool buffer_larger_pool_free (int n)
{
	for (int i = 31; i > n; i--)
	{
		if (normalized_buffer_pool[i] != nullptr)
		{
			LPBUFFER buffer = normalized_buffer_pool[i];
			LPBUFFER next = buffer->next;
			free(buffer->mem_data);
			free(buffer);
			normalized_buffer_pool[i] = next;
			return true;
		}
	}
	return false;
}
bool safe_create(char** pdata, int number)
{
	if (!((*pdata) = (char *) calloc (number, sizeof(char)))) 
	{ 
		LOG_ERROR("calloc failed [{}] {}", errno, strerror(errno)); 
		return false; 
	}	
	else 
	{
		return true;
	}
}

LPBUFFER buffer_new(int size)
{
	if (size < 0) {
		return nullptr;
	}

	LPBUFFER buffer = nullptr;
	int pool_index = buffer_get_pool_index(size);
	if (pool_index >= 0) {
		BUFFER** buffer_pool = normalized_buffer_pool + pool_index;
		size = 1 << pool_index;

		if (*buffer_pool) {
			buffer = *buffer_pool;
			*buffer_pool = buffer->next;
		}
	}

	if (buffer == nullptr) 
	{
		CREATE(buffer, BUFFER, 1);
		buffer->mem_size = size;
		if (!safe_create(&buffer->mem_data, size))
		{
			if (!buffer_larger_pool_free(pool_index))
				buffer_pool_free();
			CREATE(buffer->mem_data, char, size);
			LOG_ERROR("buffer pool free success.");
		}
	}
	assert(buffer != NULL);
	assert(buffer->mem_size == size);
	assert(buffer->mem_data != NULL);

	buffer_reset(buffer);

	return buffer;
}

void buffer_delete(LPBUFFER buffer)
{
	if (buffer == nullptr) {
		return;
	}
	buffer_reset(buffer);

	int size = buffer->mem_size;

	int pool_index = buffer_get_exac_pool_index(size);
	if (pool_index >= 0) {
		BUFFER** buffer_pool = normalized_buffer_pool + pool_index;
		buffer->next = *buffer_pool;
		*buffer_pool = buffer;
	}
	else {
		free(buffer->mem_data);
		free(buffer);
	}
}

uint32_t buffer_size(LPBUFFER buffer)
{
	return (buffer->length);
}

void buffer_reset(LPBUFFER buffer)
{
	buffer->read_point = buffer->mem_data;
	buffer->write_point = buffer->mem_data;
	buffer->write_point_pos = 0;
	buffer->length = 0;
	buffer->next = nullptr;
	buffer->flag = 0;
}

void buffer_write(LPBUFFER& buffer, const void *src, int length)
{
	if (buffer->write_point_pos + length >= buffer->mem_size)
		buffer_realloc(buffer, buffer->mem_size + length + MIN(10240, length));

	memcpy(buffer->write_point, src, length);
	buffer_write_proceed(buffer, length);
}

void buffer_read(LPBUFFER buffer, void * buf, int bytes)
{
	memcpy(buf, buffer->read_point, bytes);
	buffer_read_proceed(buffer, bytes);
}

uint8_t buffer_byte(LPBUFFER buffer)
{
	uint8_t val = *(uint8_t *) buffer->read_point;
	buffer_read_proceed(buffer, sizeof(uint8_t));
	return val;
}

uint16_t buffer_word(LPBUFFER buffer)
{
	uint16_t val = *(uint16_t *) buffer->read_point;
	buffer_read_proceed(buffer, sizeof(uint16_t));
	return val;
}

uint32_t buffer_dword(LPBUFFER buffer)
{
	uint32_t val = *(uint32_t *) buffer->read_point;
	buffer_read_proceed(buffer, sizeof(uint32_t));
	return val;
}

const void * buffer_read_peek(LPBUFFER buffer)
{
	return (const void *) buffer->read_point;
}

void buffer_read_proceed(LPBUFFER buffer, int length)
{
	if (length == 0)
		return;

	if (length < 0)
		LOG_ERROR("buffer_proceed: length argument lower than zero (length: {})", length);
	else if (length > buffer->length)
	{
		LOG_ERROR("buffer_proceed: length argument bigger than buffer (length: {}, buffer: {})", length, buffer->length);
		length = buffer->length;
	}

	if (length < buffer->length)
	{
		if (buffer->read_point + length - buffer->mem_data > buffer->mem_size)
		{
			LOG_ERROR("buffer_read_proceed: buffer overflow! length {} read_point {}", length, buffer->read_point - buffer->mem_data);
			abort();
		}

		buffer->read_point += length;
		buffer->length -= length;
	}
	else
	{
		buffer_reset(buffer);
	}
}

void * buffer_write_peek(LPBUFFER buffer)
{
	return (buffer->write_point);
}

void buffer_write_proceed(LPBUFFER buffer, int length)
{
	buffer->length += length;
	buffer->write_point	+= length;
	buffer->write_point_pos += length;
}

int buffer_has_space(LPBUFFER buffer)
{
	return (buffer->mem_size - buffer->write_point_pos);
}

void buffer_adjust_size(LPBUFFER& buffer, int add_size)
{
	if (buffer->mem_size >= buffer->write_point_pos + add_size)
		return;

	LOG_INFO("buffer_adjust {} current {}/{}", add_size, buffer->length, buffer->mem_size);
	buffer_realloc(buffer, buffer->mem_size + add_size);
}

void buffer_realloc(LPBUFFER& buffer, int length)
{
	int	i, read_point_pos;
	LPBUFFER temp;

	assert(length >= 0 && "buffer_realloc: length is lower than zero");

	if (buffer->mem_size >= length)
		return;

	i = length - buffer->mem_size;

	if (i <= 0)
		return;

	temp = buffer_new (length);
	LOG_INFO("reallocating buffer to {}, current {}", temp->mem_size, buffer->mem_size);
	memcpy(temp->mem_data, buffer->mem_data, buffer->mem_size);

	read_point_pos = buffer->read_point - buffer->mem_data;

	temp->write_point = temp->mem_data + buffer->write_point_pos;
	temp->write_point_pos = buffer->write_point_pos;
	temp->read_point = temp->mem_data + read_point_pos;
	temp->flag = buffer->flag;
	temp->next = nullptr;
	temp->length = buffer->length;

	buffer_delete(buffer);
	buffer = temp;
}
