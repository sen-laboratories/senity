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

    // Tree-sitter already gives us semantic node types - just map them directly
    
    // Comments
    if (strstr(nodeType, "comment")) {
        return SyntaxToken::COMMENT;
    }

    // Strings
    if (strstr(nodeType, "string") || strstr(nodeType, "char_literal") ||
        strcmp(nodeType, "string_literal") == 0 || strcmp(nodeType, "raw_string_literal") == 0) {
        return SyntaxToken::STRING;
    }

    // Numbers
    if (strstr(nodeType, "number") || strstr(nodeType, "integer") ||
        strstr(nodeType, "float") || strstr(nodeType, "decimal") ||
        strcmp(nodeType, "number_literal") == 0) {
        return SyntaxToken::NUMBER;
    }

    // Types - tree-sitter marks these semantically
    if (strstr(nodeType, "type") || strcmp(nodeType, "type_identifier") == 0 ||
        strcmp(nodeType, "primitive_type") == 0) {
        return SyntaxToken::TYPE;
    }

    // Functions
    if (strstr(nodeType, "function") || strstr(nodeType, "method") ||
        strcmp(nodeType, "function_declarator") == 0 || strcmp(nodeType, "call_expression") == 0) {
        return SyntaxToken::FUNCTION;
    }

    // Operators
    if (strstr(nodeType, "operator") || strcmp(nodeType, "binary_expression") == 0 ||
        strcmp(nodeType, "unary_expression") == 0) {
        return SyntaxToken::OPERATOR;
    }

    // Keywords - tree-sitter marks language keywords with their actual keyword as the type
    // Examples: "if", "for", "while", "return", "class", "def", "fn", "let", "const", etc.
    // These are anonymous nodes (not "named" semantic constructs)
    if (!ts_node_is_named(node)) {
        const char* text = nodeType;
        size_t len = strlen(text);
        
        // Keywords are typically short and alphabetic
        if (len >= 2 && len <= 15) {
            bool isKeyword = true;
            for (size_t i = 0; i < len; i++) {
                if (!std::isalpha(text[i]) && text[i] != '_') {
                    isKeyword = false;
                    break;
                }
            }
            if (isKeyword) {
                return SyntaxToken::KEYWORD;
            }
        }
    }

    // Identifiers
    if (strcmp(nodeType, "identifier") == 0 || strcmp(nodeType, "field_identifier") == 0) {
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
