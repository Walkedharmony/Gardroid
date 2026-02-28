#pragma once
#include <string>

class AstPacker {
public:
    /**
     * Melakukan repack file .ast dengan terjemahan dari file .txt.
     * * @param astPath Path file .ast asli.
     * @param txtPath Path file .txt yang berisi baris terjemahan.
     * @param outPath Path file .ast baru yang akan disimpan.
     * @param targetLang Node bahasa target (misal: "cn", "ja", "en").
     * @return 1 (Sukses), 0 (Mismatch baris), -1 (File error), -2 (Regex error), -3 (Fatal error).
     */
    static int repack(const std::string& astPath, const std::string& txtPath, const std::string& outPath, const std::string& targetLang);
};