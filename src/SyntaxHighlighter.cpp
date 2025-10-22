/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "SyntaxHighlighter.h"
#include <cctype>
#include <cstring>
#include <algorithm>

SyntaxHighlighter::SyntaxHighlighter()
{
    InitializeLanguages();

    // Default color scheme
    fColorScheme[SyntaxToken::KEYWORD] = {0, 0, 255, 255};       // Blue
    fColorScheme[SyntaxToken::TYPE] = {0, 128, 128, 255};        // Teal
    fColorScheme[SyntaxToken::FUNCTION] = {128, 0, 128, 255};    // Purple
    fColorScheme[SyntaxToken::VARIABLE] = {0, 0, 0, 255};        // Black
    fColorScheme[SyntaxToken::STRING] = {0, 128, 0, 255};        // Green
    fColorScheme[SyntaxToken::NUMBER] = {255, 102, 0, 255};      // Orange
    fColorScheme[SyntaxToken::COMMENT] = {128, 128, 128, 255};   // Gray
    fColorScheme[SyntaxToken::OPERATOR] = {0, 0, 0, 255};        // Black
    fColorScheme[SyntaxToken::PUNCTUATION] = {0, 0, 0, 255};     // Black
    fColorScheme[SyntaxToken::NORMAL] = {0, 0, 0, 255};          // Black
}

SyntaxHighlighter::~SyntaxHighlighter()
{
    // Tree-sitter language objects are static, no cleanup needed
}

void SyntaxHighlighter::InitializeLanguages()
{
    // Register all compiled-in languages
    RegisterLanguage("c", tree_sitter_c());
    RegisterLanguage("cpp", tree_sitter_cpp());
    RegisterLanguage("c++", tree_sitter_cpp());
    RegisterLanguage("cxx", tree_sitter_cpp());
    RegisterLanguage("python", tree_sitter_python());
    RegisterLanguage("py", tree_sitter_python());
    RegisterLanguage("javascript", tree_sitter_javascript());
    RegisterLanguage("js", tree_sitter_javascript());
    RegisterLanguage("rust", tree_sitter_rust());
    RegisterLanguage("rs", tree_sitter_rust());
    RegisterLanguage("go", tree_sitter_go());
    RegisterLanguage("golang", tree_sitter_go());
}

void SyntaxHighlighter::RegisterLanguage(const char* name, const TSLanguage* language)
{
    BString key(name);
    key.ToLower();
    fLanguages[key] = language;
}

bool SyntaxHighlighter::SupportsLanguage(const char* language) const
{
    if (!language) return false;

    BString key(language);
    key.ToLower();
    return fLanguages.find(key) != fLanguages.end();
}

std::vector<BString> SyntaxHighlighter::GetSupportedLanguages() const
{
    std::vector<BString> languages;
    for (const auto& pair : fLanguages) {
        languages.push_back(pair.first);
    }
    return languages;
}

std::vector<SyntaxToken> SyntaxHighlighter::Tokenize(const char* code, const char* language)
{
    std::vector<SyntaxToken> tokens;

    if (!code || !language) return tokens;

    // Find language parser
    BString langKey(language);
    langKey.ToLower();

    auto it = fLanguages.find(langKey);
    if (it == fLanguages.end()) {
        // Language not supported
        return tokens;
    }

    const TSLanguage* tsLanguage = it->second;

    // Create parser
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tsLanguage);

    // Parse the code
    TSTree* tree = ts_parser_parse_string(parser, nullptr, code, strlen(code));
    if (!tree) {
        ts_parser_delete(parser);
        return tokens;
    }

    // Get root node and process
    TSNode root = ts_tree_root_node(tree);
    ProcessNode(root, code, tokens);

    // Sort tokens by offset
    std::sort(tokens.begin(), tokens.end(),
              [](const SyntaxToken& a, const SyntaxToken& b) {
                  return a.offset < b.offset;
              });

    // Cleanup
    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return tokens;
}

void SyntaxHighlighter::ProcessNode(TSNode node, const char* source, std::vector<SyntaxToken>& tokens)
{
    // Get node type and position
    const char* nodeType = ts_node_type(node);
    uint32_t startByte = ts_node_start_byte(node);
    uint32_t endByte = ts_node_end_byte(node);

    // Skip unnamed nodes (usually handled as part of parent)
    if (!ts_node_is_named(node)) {
        return;
    }

    // Classify this node
    SyntaxToken::Type tokenType = ClassifyNode(node, source);

    // Only create token if it's not NORMAL (to reduce token count)
    if (tokenType != SyntaxToken::NORMAL) {
        SyntaxToken token;
        token.offset = startByte;
        token.length = endByte - startByte;
        token.type = tokenType;
        tokens.push_back(token);
    }

    // Process children recursively
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        ProcessNode(child, source, tokens);
    }
}

SyntaxToken::Type SyntaxHighlighter::ClassifyNode(TSNode node, const char* source)
{
    const char* nodeType = ts_node_type(node);

    // Tree-sitter grammars already provide semantic node types
    // We just need to recognize the common patterns

    // Comments - very common pattern across all grammars
    if (strstr(nodeType, "comment")) {
        return SyntaxToken::COMMENT;
    }

    // Strings - common patterns
    if (strstr(nodeType, "string") || strstr(nodeType, "char")) {
        return SyntaxToken::STRING;
    }

    // Numbers - common patterns
    if (strstr(nodeType, "number") || strstr(nodeType, "integer") ||
        strstr(nodeType, "float") || strstr(nodeType, "decimal")) {
        return SyntaxToken::NUMBER;
    }

    // Types - grammar provides these
    if (strstr(nodeType, "type")) {
        return SyntaxToken::TYPE;
    }

    // Functions - grammar provides these
    if (strstr(nodeType, "function") || strstr(nodeType, "method")) {
        return SyntaxToken::FUNCTION;
    }

    // Operators - grammar provides these
    if (strstr(nodeType, "operator")) {
        return SyntaxToken::OPERATOR;
    }

    // Keywords - Tree-sitter marks these specifically
    // Each grammar defines its own keywords as node types
    // For example: "if", "for", "while", "return", "class", "def", etc.
    // We detect them by checking if it's a short, known keyword-like node
    TSSymbol symbol = ts_node_symbol(node);
    if (ts_language_symbol_type(ts_node_language(node), symbol) == TSSymbolTypeAnonymous) {
        // Anonymous nodes are usually keywords, operators, punctuation
        // The grammar author marks language keywords as anonymous nodes
        const char* text = ts_node_type(node);
        size_t len = strlen(text);

        // Keywords are typically short (2-15 chars) and alphabetic
        if (len >= 2 && len <= 15) {
            bool allAlpha = true;
            for (size_t i = 0; i < len; i++) {
                if (!std::isalpha(text[i]) && text[i] != '_') {
                    allAlpha = false;
                    break;
                }
            }
            if (allAlpha) {
                return SyntaxToken::KEYWORD;
            }
        }
    }

    // Identifiers
    if (strcmp(nodeType, "identifier") == 0) {
        return SyntaxToken::VARIABLE;
    }

    return SyntaxToken::NORMAL;
}

rgb_color SyntaxHighlighter::GetColorForType(SyntaxToken::Type type) const
{
    auto it = fColorScheme.find(type);
    if (it != fColorScheme.end()) {
        return it->second;
    }
    return fColorScheme.at(SyntaxToken::NORMAL);
}
