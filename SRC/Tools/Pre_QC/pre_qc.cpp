#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

class PreQC {
private:
    std::vector<std::string> questFiles;
    std::unordered_map<std::string, std::string> defines;
    std::string outputDir;
    bool compile;

public:
    PreQC(const std::string& questListFile, const std::string& outputDir, bool compile)
        : outputDir(outputDir), compile(compile) {
        loadQuestList(questListFile);
        prepareOutputDirectory();
    }

    void processQuests() {
        for (const auto& file : questFiles) {
            processQuestFile(file);
        }
        if (compile) {
            compileQuests();
        }
    }

private:
    void loadQuestList(const std::string& questListFile) {
        std::ifstream file(questListFile);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                questFiles.push_back(line);
            }
        }
    }

    void prepareOutputDirectory() {
        if (fs::exists(outputDir)) {
            fs::remove_all(outputDir);
        }
        fs::create_directory(outputDir);
    }

    void processQuestFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file) {
            std::cerr << "Nem sikerült megnyitni: " << filePath << std::endl;
            return;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        extractDefines(content);
        std::string processedContent = replaceDefines(content);

        std::ofstream outFile(outputDir + "/" + fs::path(filePath).filename().string());
        outFile << processedContent;
        outFile.close();
    }

    void extractDefines(const std::string& content) {
        std::regex defineRegex(R"(define\s+(\S+)\s+\"(.*?)\")");
        std::smatch match;
        std::string::const_iterator searchStart(content.cbegin());

        while (std::regex_search(searchStart, content.cend(), match, defineRegex)) {
            defines[match[1].str()] = match[2].str();
            searchStart = match.suffix().first;
        }
    }

    std::string replaceDefines(const std::string& content) {
        std::string result = content;
        for (const auto& pair : defines) {
            std::regex keyRegex("\\b" + pair.first + "\\b");
            result = std::regex_replace(result, keyRegex, pair.second);
        }
        return result;
    }

    void compileQuests() {
        for (const auto& file : questFiles) {
            std::string command = "./qc " + outputDir + "/" + fs::path(file).filename().string();
            system(command.c_str());
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Használat: " << argv[0] << " <quest_list> <output_dir> [-c]" << std::endl;
        return 1;
    }

    std::string questListFile = argv[1];
    std::string outputDir = argv[2];
    bool compile = (argc > 3 && std::string(argv[3]) == "-c");

    PreQC preqc(questListFile, outputDir, compile);
    preqc.processQuests();
    
    return 0;
}
