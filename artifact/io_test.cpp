#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <cstring>
#include <stdexcept>

static constexpr size_t MB = 1024ULL * 1024ULL;
static constexpr const char *FILENAME = "data.dat";

constexpr int TEST_ITERATIONS = 3;

auto write_random(size_t size_mb)
{

    const size_t total_bytes = size_mb * MB;

    constexpr size_t CHUNK = 4 * MB;
    std::vector<char> buf(std::min(CHUNK, total_bytes));

    for (int i = 0; i < std::min(CHUNK, total_bytes); i++)
    {
        buf[i] = 32 + (i % (127 - 32));
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::ofstream ofs(FILENAME, std::ios::binary | std::ios::trunc);
    if (!ofs)
        throw std::runtime_error("Cannot open file for writing");

    size_t written = 0;

    while (written < total_bytes)
    {
        size_t n = std::min(buf.size(), total_bytes - written);

        ofs.write(reinterpret_cast<char *>(buf.data()), n);
        if (!ofs)
            throw std::runtime_error("Write error");
        written += n;
    }

    auto end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

auto read_file()
{
    auto start = std::chrono::high_resolution_clock::now();

    std::ifstream ifs(FILENAME, std::ios::binary | std::ios::ate);
    if (!ifs)
        throw std::runtime_error("Cannot open file for reading");

    ifs.seekg(0);

    constexpr size_t CHUNK = 4 * MB;
    std::vector<uint8_t> buf(CHUNK);

    size_t total_read = 0;

    while (ifs)
    {
        ifs.read(reinterpret_cast<char *>(buf.data()), buf.size());
        total_read += ifs.gcount();
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " write <size_mb>\n"
                  << "  " << argv[0] << " read\n";
        return 1;
    }

    const std::string cmd = argv[1];
    long unsigned int duration = 0;

    try
    {
        if (cmd == "write")
        {
            if (argc < 3)
            {
                std::cerr << "Missing size argument\n";
                return 1;
            }
            size_t mb = std::stoull(argv[2]);
            if (mb == 0)
            {
                std::cerr << "Size must be > 0\n";
                return 1;
            }
            
            write_random(mb);
            for (auto i = 0; i < TEST_ITERATIONS; i++)
            {
                duration += write_random(mb);
            }

            duration /= TEST_ITERATIONS;

            std::cout << "WRITE : " << duration << " µs\n";
        }
        else if (cmd == "read")
        {
            read_file();
            for (auto i = 0; i < TEST_ITERATIONS; i++)
            {
                duration += read_file();
            }
            duration /= TEST_ITERATIONS;
            std::cout << "READ : " << duration << " µs\n";
        }
        else
        {
            std::cerr << "Unknown command: " << cmd << "\n";
            return 1;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
