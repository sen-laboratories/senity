# MarkdownParser Quick Reference

## Initialization

```cpp
MarkdownParser parser;
parser.SetDebugEnabled(true);          // Optional: see what's happening
parser.SetUseUnicodeSymbols(true);     // Optional: use • ☐ ☑ symbols
```

## Parsing

```cpp
// Initial parse
if (parser.Parse(markdownText)) {
    // Success
}

// Incremental update
parser.ParseIncremental(newText, editOffset, oldLength, newLength);
```

## Getting Results

```cpp
// Style runs (for rendering)
const std::vector<StyleRun>& runs = parser.GetStyleRuns();

// Outline (heading hierarchy)
const BMessage& outline = parser.GetOutline();

// Query node at position
TSNode node = parser.GetNodeAtOffset(cursorPosition);
```

## Style Runs

```cpp
for (const StyleRun& run : parser.GetStyleRuns()) {
    // Position
    int32 offset = run.offset;
    int32 length = run.length;
    
    // Type
    StyleRun::Type type = run.type;  // HEADING_1, STRONG, CODE_BLOCK, etc.
    
    // Styling
    BFont font = run.font;
    rgb_color fg = run.foreground;
    rgb_color bg = run.background;
    
    // Metadata (may be empty)
    BString language = run.language;  // For code blocks
    BString url = run.url;            // For links
    BString text = run.text;          // Unicode replacement symbol
    
    // Render
    if (!run.text.IsEmpty()) {
        DrawString(run.text);         // Draw Unicode symbol
    } else {
        BString original(source + run.offset, run.length);
        DrawString(original);         // Draw original text
    }
}
```

## Outline

```cpp
const BMessage& outline = parser.GetOutline();

int32 count = 0;
outline.GetInfo("heading", NULL, &count);

for (int32 i = 0; i < count; i++) {
    BMessage heading;
    outline.FindMessage("heading", i, &heading);
    
    BString text;
    int32 level;    // 1-6
    int32 offset;   // Byte offset
    int32 length;   // Byte length
    
    heading.FindString("text", &text);
    heading.FindInt32("level", &level);
    heading.FindInt32("offset", &offset);
    heading.FindInt32("length", &length);
    
    printf("%*s%s\n", (level-1)*2, "", text.String());
}
```

## Node Queries

```cpp
// Get node at cursor
TSNode node = parser.GetNodeAtOffset(cursorPos);

if (!ts_node_is_null(node)) {
    // Node type
    const char* type = ts_node_type(node);  // "emphasis", "strong_emphasis", etc.
    
    // Node range
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    
    // Check properties
    bool named = ts_node_is_named(node);
    bool error = ts_node_is_error(node);
    
    // Navigate tree
    TSNode parent = ts_node_parent(node);
    TSNode sibling = ts_node_next_named_sibling(node);
    uint32_t childCount = ts_node_child_count(node);
    TSNode child = ts_node_child(node, 0);
    
    // Get specific child by field
    TSNode info = ts_node_child_by_field_name(node, "info_string", 11);
}
```

## Style Types

```cpp
StyleRun::NORMAL                  // Plain text
StyleRun::HEADING_1               // # heading
StyleRun::HEADING_2               // ## heading
StyleRun::HEADING_3               // ### heading
StyleRun::HEADING_4               // #### heading
StyleRun::HEADING_5               // ##### heading
StyleRun::HEADING_6               // ###### heading
StyleRun::CODE_INLINE             // `code`
StyleRun::CODE_BLOCK              // ```code```
StyleRun::EMPHASIS                // *italic*
StyleRun::STRONG                  // **bold**
StyleRun::LINK                    // [text](url)
StyleRun::LINK_URL                // URL part
StyleRun::LIST_BULLET             // -, *, + (rendered as •)
StyleRun::LIST_NUMBER             // 1. 2. 3.
StyleRun::BLOCKQUOTE              // > quote
StyleRun::TASK_MARKER_UNCHECKED   // [ ] (rendered as ☐)
StyleRun::TASK_MARKER_CHECKED     // [X] (rendered as ☑)
```

## Common Node Types

```cpp
"document"                         // Root
"atx_heading"                      // # heading
"atx_h1_marker" ... "atx_h6_marker" // #, ##, etc.
"heading_content"                  // Heading text
"paragraph"                        // Paragraph
"emphasis"                         // *italic*
"strong_emphasis"                  // **bold**
"code_span"                        // `code`
"fenced_code_block"                // ```code```
"info_string"                      // Language in ```lang
"inline_link"                      // [text](url)
"link_text"                        // [text]
"link_destination"                 // (url)
"list_item"                        // List item
"list_marker_minus"                // -
"list_marker_star"                 // *
"list_marker_plus"                 // +
"task_list_marker_unchecked"       // [ ]
"task_list_marker_checked"         // [X]
"block_quote"                      // > quote
```

## Configuration

```cpp
// Enable debug output
parser.SetDebugEnabled(true);

// Unicode symbols (default: true)
parser.SetUseUnicodeSymbols(true);

// Custom fonts
BFont headingFont(be_bold_font);
headingFont.SetSize(24);
parser.SetFont(StyleRun::HEADING_1, headingFont);

// Custom colors
rgb_color blue = {0, 102, 204, 255};
rgb_color white = {255, 255, 255, 255};
parser.SetColor(StyleRun::LINK, blue, white);
```

## Debug Functions

```cpp
parser.SetDebugEnabled(true);

// Dump entire parse tree
parser.DumpTree();

// Dump all style runs
parser.DumpStyleRuns();

// Dump document outline
parser.DumpOutline();

// These print detailed information to stdout
```

## Incremental Parse Details

```cpp
// User inserts text at position 100
int32 offset = 100;
int32 oldLen = 0;        // Nothing deleted
int32 newLen = 5;        // 5 chars inserted

parser.ParseIncremental(newText, offset, oldLen, newLen);

// User deletes 10 chars at position 50
offset = 50;
oldLen = 10;             // 10 chars deleted
newLen = 0;              // Nothing inserted

parser.ParseIncremental(newText, offset, oldLen, newLen);

// User replaces "old" (3 chars) with "new text" (8 chars) at 200
offset = 200;
oldLen = 3;
newLen = 8;

parser.ParseIncremental(newText, offset, oldLen, newLen);
```

## Error Handling

```cpp
if (!parser.Parse(text)) {
    // Parse failed (rare - tree-sitter is very robust)
    printf("Parse failed\n");
}

// Check for errors in tree
TSNode root = ts_tree_root_node(tree);
if (ts_node_has_error(root)) {
    // Tree contains error nodes
    // Can still use it, errors are localized
}
```

## Memory Management

```cpp
// Parser manages its own memory
MarkdownParser parser;  // Constructor allocates

parser.Parse(text);     // Allocates tree, stores copy of text

parser.Clear();         // Frees tree and text copy

// Destructor cleans up
// ~MarkdownParser() calls Clear()
```

## Performance Tips

1. **Use incremental parsing** for edits
   - 10-100x faster than full reparse
   
2. **Reuse parser object**
   - Don't create new parser for each document
   
3. **Keep tree alive**
   - Don't call Clear() unless necessary
   - Tree enables fast queries
   
4. **Batch small edits**
   - If user types quickly, batch edits
   - Parse once after typing stops

## Example: Complete Editor Integration

```cpp
class MarkdownEditor : public BView {
private:
    MarkdownParser fParser;
    BString fText;
    
public:
    MarkdownEditor() {
        fParser.SetDebugEnabled(false);
        fParser.SetUseUnicodeSymbols(true);
    }
    
    void SetText(const char* text) {
        fText = text;
        fParser.Parse(fText.String());
        Invalidate();
    }
    
    void InsertText(int32 offset, const char* text) {
        int32 len = strlen(text);
        fText.Insert(text, offset);
        fParser.ParseIncremental(fText.String(), offset, 0, len);
        Invalidate();
    }
    
    void DeleteText(int32 offset, int32 length) {
        fText.Remove(offset, length);
        fParser.ParseIncremental(fText.String(), offset, length, 0);
        Invalidate();
    }
    
    void Draw(BRect updateRect) {
        const std::vector<StyleRun>& runs = fParser.GetStyleRuns();
        
        BPoint pen(10, 20);
        
        for (const StyleRun& run : runs) {
            SetFont(&run.font);
            SetHighColor(run.foreground);
            SetLowColor(run.background);
            
            if (!run.text.IsEmpty()) {
                DrawString(run.text.String(), pen);
            } else {
                BString text(fText.String() + run.offset, run.length);
                DrawString(text.String(), pen);
            }
            
            // Update pen position...
        }
    }
    
    void MouseDown(BPoint point) {
        // Convert point to offset
        int32 offset = PointToOffset(point);
        
        TSNode node = fParser.GetNodeAtOffset(offset);
        const char* type = ts_node_type(node);
        
        if (strcmp(type, "inline_link") == 0) {
            // User clicked a link - open it
            TSNode dest = ts_node_child_by_field_name(
                node, "link_destination", 16);
            // ... extract and open URL
        }
    }
};
```

## Unicode Symbols

When `SetUseUnicodeSymbols(true)`:

| ASCII | Unicode | Name | Code Point |
|-------|---------|------|------------|
| -, *, + | • | Bullet | U+2022 |
| [ ] | ☐ | Ballot Box | U+2610 |
| [X] | ☑ | Ballot Box with Check | U+2611 |

Symbols appear in `StyleRun::text` field.

## Build

```bash
make                    # Build test program
make debug              # Build with debug symbols
make test               # Build and run
make clean              # Remove build artifacts
```

## See Also

- **README.md** - Full documentation
- **MIGRATION.md** - Comparison with cmark
- **ARCHITECTURE.md** - System design
- **test_parser.cpp** - Working examples
