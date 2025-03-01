#include <string>

#include <Common/Arena.h>
#include <Common/CurrentThread.h>
#include <Common/Exception.h>
#include <Common/MemoryTracker.h>
#include <Compression/CompressedReadBuffer.h>
#include <Compression/ICompressionCodec.h>
#include <Compression/LZ4_decompress_faster.h>
#include <IO/BufferWithOwnMemory.h>
#include <IO/ReadBufferFromMemory.h>
#include <Interpreters/Context.h>

using namespace DB;
ContextMutablePtr context;
extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    if (context)
        return true;

    static SharedContextHolder shared_context = Context::createShared();
    context = Context::createGlobal(shared_context.get());
    context->makeGlobalContext();

    MainThreadStatus::getInstance();

    return 0;
}

namespace DB
{
    CompressionCodecPtr getCompressionCodecLZ4(int level);
}

struct AuxiliaryRandomData
{
    size_t level;
    size_t decompressed_size;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    try
    {
        total_memory_tracker.resetCounters();
        total_memory_tracker.setHardLimit(1_GiB);
        CurrentThread::get().memory_tracker.resetCounters();
        CurrentThread::get().memory_tracker.setHardLimit(1_GiB);

        if (size < sizeof(AuxiliaryRandomData) + LZ4::ADDITIONAL_BYTES_AT_END_OF_BUFFER)
            return 0;

        const auto * p = reinterpret_cast<const AuxiliaryRandomData *>(data);
        auto codec = DB::getCompressionCodecLZ4(static_cast<int>(p->level));

        size_t output_buffer_size = p->decompressed_size % 65536;
        size -= sizeof(AuxiliaryRandomData);
        size -= LZ4::ADDITIONAL_BYTES_AT_END_OF_BUFFER;
        data += sizeof(AuxiliaryRandomData) / sizeof(uint8_t);

        // std::string input = std::string(reinterpret_cast<const char*>(data), size);
        // fmt::print(stderr, "Using input {} of size {}, output size is {}. \n", input, size, output_buffer_size);

        DB::Memory<> memory;
        memory.resize(output_buffer_size + LZ4::ADDITIONAL_BYTES_AT_END_OF_BUFFER);

        codec->doDecompressData(reinterpret_cast<const char *>(data), static_cast<UInt32>(size), memory.data(), static_cast<UInt32>(output_buffer_size));
    }
    catch (...)
    {
    }

    return 0;
}
