#include <Core/buffer.h>
#include <Core/Logging.hpp>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

// The production Core/buffer.cpp under the access patterns of the asio
// sessions: an output buffer that keeps being appended to while the previous
// chunk is in flight (so it is rarely drained to empty), an input buffer left
// with a partial packet after each read, and growth once a prefix has been
// consumed. Every byte is numbered, so any reordering or loss shows.
namespace {
int checks = 0;
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }

struct Stream
{
    uint64_t written { 0 };
    uint64_t read { 0 };
};

char ByteAt(uint64_t index) { return static_cast<char>(index % 251); }

void Append(LPBUFFER& buffer, Stream& stream, size_t count)
{
    std::vector<char> data(count);
    for (char& byte : data)
        byte = ByteAt(stream.written++);
    buffer_write(buffer, data.data(), static_cast<int>(count));
}

void Consume(LPBUFFER buffer, Stream& stream, size_t count)
{
    Check(buffer_size(buffer) >= count, "the bytes to consume are there");
    const char* data = static_cast<const char*>(buffer_read_peek(buffer));
    bool inOrder = true;
    for (size_t i = 0; i < count; ++i)
        inOrder = inOrder && data[i] == ByteAt(stream.read++);
    Check(inOrder, "bytes come out in the order they went in");
    buffer_read_proceed(buffer, static_cast<int>(count));
}

// PeerSession: StartWrite stages everything unsent, the game keeps encoding
// packets while that chunk is in flight, and the completion consumes exactly
// the staged chunk. The encoder (packet_encode) refuses a packet when
// buffer_has_space() is short, which closes the connection.
void NeverDrainedWriterKeepsItsRoom()
{
    LPBUFFER buffer = buffer_new(150 * 1024 * 2);
    const size_t capacity = buffer->mem_size;
    Stream stream;
    size_t inFlight = 0;
    bool encoderRefused = false;

    for (int tick = 0; tick < 20000; ++tick)
    {
        const size_t before = 1200 + tick % 700;
        const size_t after = 800 + tick % 500;

        encoderRefused = encoderRefused || buffer_has_space(buffer) < static_cast<int>(before);
        Append(buffer, stream, before);
        if (inFlight)
            Consume(buffer, stream, inFlight);
        inFlight = buffer_size(buffer);
        encoderRefused = encoderRefused || buffer_has_space(buffer) < static_cast<int>(after);
        Append(buffer, stream, after);
    }

    Check(stream.written > 30u * 1024 * 1024, "the run wrote far more than the capacity");
    Check(!encoderRefused, "the encoder never runs out of room while the backlog is small");
    Check(buffer->mem_size == capacity, "the storage never grows for a small backlog");
    Check(buffer_size(buffer) == stream.written - stream.read, "only the unsent bytes are held");
    buffer_delete(buffer);
}

// DESC/CPeer input: reads arrive in arbitrary chunks, the parser consumes
// whole packets and leaves the partial one for the next read.
void PartialPacketReaderStaysBounded()
{
    LPBUFFER buffer = buffer_new(64 * 1024);
    const size_t capacity = buffer->mem_size;
    Stream stream;
    const size_t packet = 37;

    for (int read = 0; read < 50000; ++read)
    {
        Append(buffer, stream, 1000 + read % 3000);
        const size_t whole = buffer_size(buffer) / packet;
        for (size_t i = 0; i < whole; ++i)
            Consume(buffer, stream, packet);
    }

    Check(buffer->mem_size == capacity, "a reader with a partial packet does not grow the storage");
    Check(buffer_size(buffer) < packet, "only the partial packet is left");
    buffer_delete(buffer);
}

void GrowthKeepsOnlyTheUnreadBytes()
{
    LPBUFFER buffer = buffer_new(1024);
    Stream stream;
    Append(buffer, stream, 1000);
    Consume(buffer, stream, 400);
    Check(buffer->read_point_pos == 0 && buffer->write_point_pos == 600,
        "past the middle of the storage a partial read moves the rest to the front");

    Append(buffer, stream, 5000);
    Check(buffer->read_point_pos == 0 && buffer->write_point_pos == 5600, "growth starts at the unread bytes");
    Check(buffer->mem_size >= 5600 && buffer->mem_size < 16 * 1024, "the new storage is sized for the unread bytes");
    Consume(buffer, stream, 5600);
    Check(buffer_size(buffer) == 0 && buffer->read_point_pos == 0 && buffer->write_point_pos == 0, "a full read resets");
    buffer_delete(buffer);
}

void WriteReusesTheConsumedPrefix()
{
    LPBUFFER buffer = buffer_new(1024);
    Stream stream;
    Append(buffer, stream, 400);
    Consume(buffer, stream, 100);
    Check(buffer->read_point_pos == 100, "a small prefix is left in place");

    const char* storage = buffer->mem_data.get();
    Append(buffer, stream, 700);
    Check(buffer->mem_size == 1024 && buffer->mem_data.get() == storage,
        "the prefix makes room in place: no new storage");
    Check(buffer->read_point_pos == 0 && buffer->write_point_pos == 1000, "the unread bytes moved to the front");
    Consume(buffer, stream, 1000);

    Append(buffer, stream, 300);
    buffer_adjust_size(buffer, 900);
    Check(buffer->mem_size >= 1200 && buffer_has_space(buffer) >= 900, "adjust_size makes the requested room");
    Consume(buffer, stream, 300);
    buffer_delete(buffer);
}

void TempBufferRoundTrip()
{
    TEMP_BUFFER temp(16);
    Stream stream;
    LPBUFFER buffer = temp.getptr();
    Append(buffer, stream, 5000);
    Check(temp.size() == 5000 && temp.getptr() == buffer, "the temp buffer grows in place of its own storage");
    Consume(buffer, stream, 5000);
    temp.reset();
    Check(temp.size() == 0, "reset empties it");
}
}

std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("buffer-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("buffer-test");
    return logger;
}

int main()
{
    try {
        NeverDrainedWriterKeepsItsRoom();
        PartialPacketReaderStaysBounded();
        GrowthKeepsOnlyTheUnreadBytes();
        WriteReusesTheConsumedPrefix();
        TempBufferRoundTrip();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << "\n";
        return 1;
    }
    std::cout << "core buffer: " << checks << " checks passed\n";
    return 0;
}
