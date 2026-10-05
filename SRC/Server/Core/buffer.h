#ifndef __INC_LIBTHECORE_BUFFER_H__
#define __INC_LIBTHECORE_BUFFER_H__

#include <cstdint>
#include <cstddef>
#include <memory>

#define SAFE_BUFFER_DELETE(buf)		{ if(buf != nullptr) { buffer_delete(buf); buf = nullptr; } }

	typedef struct buffer	BUFFER;
	typedef struct buffer *	LPBUFFER;

	// Contiguous, RAII-managed byte buffer.
	//
	// The legacy pool managed raw malloc'd blocks and could hand out buffers
	// with dangling bookkeeping on realloc. The storage is a unique_ptr<char[]>
	// now; the layout of the public fields is kept so the existing peek/proceed
	// based protocol code stays unchanged.
	struct buffer
	{
		std::unique_ptr<char[]>	mem_data;
		std::size_t		mem_size { 0 };		// allocated capacity
		std::size_t		write_point_pos { 0 };	// committed bytes
		std::size_t		read_point_pos { 0 };	// consumed offset
	};

	extern LPBUFFER	buffer_new(int size);
	extern void		buffer_delete(LPBUFFER buffer);
	extern void		buffer_reset(LPBUFFER buffer);

	extern uint32_t	buffer_size(LPBUFFER buffer);
	extern int		buffer_has_space(LPBUFFER buffer);

	extern void		buffer_write(LPBUFFER& buffer, const void* src, int length);

	extern const void *	buffer_read_peek(LPBUFFER buffer);
	extern void		buffer_read_proceed(LPBUFFER buffer, int length);

	extern void *	buffer_write_peek(LPBUFFER buffer);
	extern void		buffer_write_proceed(LPBUFFER buffer, int length);

	extern void		buffer_adjust_size(LPBUFFER& buffer, int add_size);

	// Small scoped packet building helper (formerly duplicated in the
	// GameServer and Database buffer_manager files).
	class TEMP_BUFFER
	{
		public:
			TEMP_BUFFER(int Size = 8192, bool ForceDelete = false);
			~TEMP_BUFFER();

			const void *	read_peek();
			void		write(const void * data, int size);
			int		size();
			void		reset();

			LPBUFFER	getptr() { return buf; }

		protected:
			LPBUFFER	buf;
	};

#endif
