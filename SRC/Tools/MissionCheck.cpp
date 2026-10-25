#include "../Server/GameServer/mission/MissionValidator.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv)
{
    using namespace auriga::mission;
    if (argc != 2)
    {
        std::cerr << "Usage: MissionCheck <definition.json>\n";
        return 2;
    }
    try
    {
        const std::filesystem::path path(argv[1]);
        if (std::filesystem::file_size(path) > Limits::DefinitionBytes)
        {
            std::cerr << "Definition byte limit exceeded\n";
            return 1;
        }
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot open definition");
        std::string text(Limits::DefinitionBytes + 1, '\0');
        file.read(text.data(), std::streamsize(text.size()));
        if (file.bad())
            throw std::runtime_error("Cannot read definition");
        text.resize(size_t(file.gcount()));
        const auto parsed = ParseDefinition(text);
        if (!parsed)
        {
            for (const auto &error : parsed.errors)
                std::cerr << error.path << ": " << error.message << '\n';
            return 1;
        }
        const auto compiled = CompileDefinition(parsed.value);
        if (!compiled)
        {
            for (const auto &error : compiled.errors)
                std::cerr << error.path << ": " << error.message << '\n';
            return 1;
        }
        std::cout
            << "Mission definition valid: " << parsed.value.id << " version=" << parsed.value.version
            << " nodes=" << compiled.definition->Nodes().size()
            << " (core structure only; game bindings and campaign reachability require later validation)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
