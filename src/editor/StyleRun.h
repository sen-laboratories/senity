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
