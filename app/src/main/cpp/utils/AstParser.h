#pragma once
#include <string>
#include <vector>
#include <set>

class AstParser {
public:

    static std::vector<std::string> detectLanguages(const std::string& filePath);


    static int extractText(const std::string& inputPath, const std::string& outputPath, const std::string& targetLang);

private:
    static std::string readFile(const std::string& path);
    static bool isValidLanguageKey(const std::string& key);
};