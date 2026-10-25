#include "../Server/GameServer/mission/MissionPackage.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace
{
using namespace auriga::mission;
std::string ReadSource(const std::filesystem::path &path)
{
    if (std::filesystem::file_size(path) > PackageLimits::Bytes)
        throw std::runtime_error("Source byte limit");
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open source");
    std::string text(PackageLimits::Bytes + 1, '\0');
    file.read(text.data(), std::streamsize(text.size()));
    if (file.bad())
        throw std::runtime_error("Cannot read source");
    text.resize(size_t(file.gcount()));
    return text;
}
void WriteAtomically(const std::filesystem::path &output, std::span<const uint8_t> bytes)
{
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(output)))
        throw std::runtime_error("Refusing symlink output");
    std::random_device random;
    auto temporary = output;
    temporary += "." + std::to_string(random()) + ".tmp";
    bool created = false;
    try
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::out | std::ios::noreplace);
        if (!file)
            throw std::runtime_error("Cannot create exclusive temporary output");
        created = true;
        file.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
        file.flush();
        if (!file)
            throw std::runtime_error("Cannot write package output");
        file.close();
        if (!file)
            throw std::runtime_error("Cannot close package output");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), output.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot atomically replace output, Windows error=" +
                                     std::to_string(GetLastError()));
#else
        std::filesystem::rename(temporary, output);
#endif
    }
    catch (...)
    {
        if (created)
        {
            std::error_code ec;
            std::filesystem::remove(temporary, ec);
        }
        throw;
    }
}
void Errors(const std::vector<Diagnostic> &errors)
{
    for (const auto &error : errors)
        std::cerr << error.path << ": " << error.message << '\n';
}
} // namespace
int main(int argc, char **argv)
{
    using namespace auriga::mission;
    try
    {
        const std::string_view mode = argc > 1 ? argv[1] : "";
        if (mode == "compile" && argc == 4)
        {
            const std::filesystem::path input(argv[2]), output(argv[3]);
            if (output.extension() != ".aurmission")
                throw std::invalid_argument("Output must end in .aurmission");
            if (std::filesystem::weakly_canonical(input) == std::filesystem::weakly_canonical(output))
                throw std::invalid_argument("Source and output must differ");
            const auto source = ParsePackageSource(ReadSource(input));
            if (!source)
            {
                Errors(source.errors);
                return 1;
            }
            const auto compiled = CompilePackage(source.source);
            if (!compiled)
            {
                Errors(compiled.errors);
                return 1;
            }
            WriteAtomically(output, compiled.bytes);
            std::cout << "Compiled " << source.source.manifest.id
                      << " version=" << source.source.manifest.version << " bytes=" << compiled.bytes.size()
                      << " -> " << output.string() << '\n';
            return 0;
        }
        if (mode == "check" && argc == 3)
        {
            const auto loaded = LoadPackageFile(argv[2]);
            if (!loaded)
            {
                Errors(loaded.errors);
                return 1;
            }
            std::cout << "Package valid: " << loaded.package->Manifest().id
                      << " version=" << loaded.package->Manifest().version
                      << " missions=" << loaded.package->Graphs().size()
                      << " (domain validation; gameplay bindings and live persistence follow separately)\n";
            return 0;
        }
        if (mode == "schema" && argc == 3)
        {
            const auto schema = ExportNativeSchemas();
            WriteAtomically(argv[2], {reinterpret_cast<const uint8_t *>(schema.data()), schema.size()});
            std::cout << "Exported native domain schema\n";
            return 0;
        }
        std::cerr << "Usage:\n  MissionCompiler compile <source.json> <package.aurmission>\n"
                     "  MissionCompiler check <package.aurmission>\n  MissionCompiler schema <schema.json>\n";
        return 2;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
