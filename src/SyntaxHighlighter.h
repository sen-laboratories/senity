#pragma once

#include <String.h>
#include <map>
#include <vector>

extern "C" {
#include <tree_sitter/api.h>
}

// Forward declarations for language parsers
// You'll link these as needed
extern "C" {
    const TSLanguage *tree_sitter_c();
    const TSLanguage *tree_sitter_cpp();
    const TSLanguage *tree_sitter_python();
    const TSLanguage *tree_sitter_javascript();
    const TSLanguage *tree_sitter_rust();
    const TSLanguage *tree_sitter_go();
    // Add more as needed
}

struct SyntaxToken {
    int32 offset;
    int32 length;

    enum Type {
        KEYWORD,
        TYPE,
        FUNCTION,
        VARIABLE,
        STRING,
        NUMBER,
        COMMENT,
        OPERATOR,
        PUNCTUATION,
        NORMAL
    } type;
};

class SyntaxHighlighter {
public:
    SyntaxHighlighter();
    ~SyntaxHighlighter();

    // Highlight code and return colored tokens
    std::vector<SyntaxToken> Tokenize(const char* code, const char* language);

    // Check if language is supported
    bool SupportsLanguage(const char* language) const;

    // Get list of supported languages
    std::vector<BString> GetSupportedLanguages() const;

    // Color configuration
    void SetColorScheme(const std::map<SyntaxToken::Type, rgb_color>& colors);
    rgb_color GetColorForType(SyntaxToken::Type type) const;

private:
    // Language parser cache
    std::map<BString, const TSLanguage*> fLanguages;
    std::map<SyntaxToken::Type, rgb_color> fColorScheme;

    // Initialize language support
    void RegisterLanguage(const char* name, const TSLanguage* language);
    void InitializeLanguages();

    // Tree-sitter processing
    void ProcessNode(TSNode node, const char* source, std::vector<SyntaxToken>& tokens);
    SyntaxToken::Type ClassifyNode(TSNode node, const char* source);

    // Node type string helpers
    bool IsKeyword(const char* type) const;
    bool IsType(const char* type) const;
    bool IsFunction(const char* type) const;
};
