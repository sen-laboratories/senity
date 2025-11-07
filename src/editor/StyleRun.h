/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <Font.h>
#include <String.h>
#include <SupportDefs.h>

/**
 * StyleRun - Represents a styled text segment in the document
 *
 * Each StyleRun corresponds to a semantic element in the Markdown document
 * and includes font, color, and optional metadata (URLs, language, replacement text).
 */
struct StyleRun {
    enum Type {
        NORMAL = 0,

        // Headings
        HEADING_1,
        HEADING_2,
        HEADING_3,
        HEADING_4,
        HEADING_5,
        HEADING_6,

        // Code
        CODE_INLINE,
        CODE_BLOCK,

        // Inline formatting
        EMPHASIS,           // *italic* or _italic_
        STRONG,             // **bold**
        UNDERLINE,          // Markdown extended: ++underline++ (if supported)
        STRIKETHROUGH,      // ~~strikethrough~~

        // Links
        LINK,               // Link text
        LINK_URL,           // Link URL

        // Lists
        LIST_BULLET,        // Bullet point (-, *, +)
        LIST_NUMBER,        // Numbered list item

        // Block elements
        BLOCKQUOTE,

        // Task lists
        TASK_MARKER_UNCHECKED,  // [ ] -> ☐
        TASK_MARKER_CHECKED,    // [x] or [X] -> ✅

        // Tables
        TABLE_HEADER,           // Table header cell (bold)
        TABLE_CELL,             // Table data cell
        TABLE_DELIMITER,        // Table pipe |
        TABLE_ROW_DELIMITER,    // Delimiter row |---|---|

        // Syntax highlighting (for code blocks)
        SYNTAX_KEYWORD,
        SYNTAX_TYPE,
        SYNTAX_FUNCTION,
        SYNTAX_STRING,
        SYNTAX_NUMBER,
        SYNTAX_COMMENT,
        SYNTAX_OPERATOR
    };

    Type type;              // Semantic type of this run
    int32 offset;           // Byte offset in document
    int32 length;           // Length in bytes
    BFont font;             // Font to use
    rgb_color foreground;   // Text color
    rgb_color background;   // Background color (optional)

    // Optional metadata
    BString url;            // For LINK type
    BString language;       // For CODE_BLOCK type
    BString text;           // Replacement text (e.g., Unicode symbols for bullets/checkboxes)

    StyleRun()
        : type(NORMAL)
        , offset(0)
        , length(0)
        , foreground({0, 0, 0, 255})
        , background({255, 255, 255, 255})
    {}
};

// Color mapping for Markdown elements
static const struct {
    StyleRun::Type type;
    rgb_color color;
} COLOR_MAP[] = {
    {StyleRun::Type::NORMAL, {0, 0, 0, 255}},           // black
    {StyleRun::Type::LINK, {0, 102, 204, 255}},         // blue
    {StyleRun::Type::CODE_INLINE, {60, 60, 60, 255}},   // gray
    {StyleRun::Type::CODE_BLOCK, {60, 60, 60, 255}},    // gray
    {StyleRun::Type::LIST_BULLET, {128, 128, 128, 255}}, // gray
    {StyleRun::Type::LIST_NUMBER, {128, 128, 128, 255}}, // gray
    {StyleRun::Type::TASK_MARKER_UNCHECKED, {128, 128, 128, 255}}, // gray
    {StyleRun::Type::TASK_MARKER_CHECKED, {0, 150, 0, 255}},        // green
    {StyleRun::Type::TABLE_HEADER, {0, 0, 0, 255}},                  // black (bold via font)
    {StyleRun::Type::TABLE_CELL, {0, 0, 0, 255}},                    // black
    {StyleRun::Type::TABLE_DELIMITER, {180, 180, 180, 255}},         // light gray (pipes)
    {StyleRun::Type::TABLE_ROW_DELIMITER, {150, 150, 150, 255}},     // medium gray (---|---)
    {StyleRun::Type::HEADING_1, {0, 102, 204, 255}},                 // blue (bold via font)
    {StyleRun::Type::HEADING_2, {0, 102, 204, 255}},    // blue
    {StyleRun::Type::HEADING_3, {0, 102, 204, 255}},    // blue
    {StyleRun::Type::HEADING_4, {0, 102, 204, 255}},    // blue
    {StyleRun::Type::HEADING_5, {0, 102, 204, 255}},    // blue
    {StyleRun::Type::HEADING_6, {0, 102, 204, 255}},    // blue
};
