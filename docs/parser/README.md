# MarkdownParser - Tree-sitter Enhanced Version

This version of MarkdownParser fully embraces the tree-sitter API for robust Markdown parsing with debugging capabilities and Unicode symbol replacements.

## Key Enhancements

### 1. Debugging Support

The parser now includes comprehensive debugging facilities to help understand the parsing process:

#### Debug Functions

- **`SetDebugEnabled(bool enabled)`** - Enable/disable debug output
- **`DumpTree()`** - Print the complete parse tree in S-expression format
- **`DumpStyleRuns()`** - Print all generated style runs with details
- **`DumpOutline()`** - Print the document outline hierarchy

#### What Gets Logged

When debug mode is enabled, the parser outputs:

1. **Parse tree structure** - Shows the complete AST in tree-sitter's S-expression format
2. **Node traversal** - Prints each node as it's processed with indentation showing depth
3. **Style runs** - Details of every styling region created
4. **Outline** - Hierarchical heading structure
5. **Incremental parse info** - Which byte ranges changed during incremental updates

Example debug output:
```
=== Parsing Markdown (342 bytes) ===

=== Parse Tree ===
(document [0, 342)
  (atx_heading [0, 15)
    (atx_h1_marker [0, 1))
    (heading_content [2, 15)))
  (paragraph [17, 69))
  ...)

=== Processing Parse Tree ===
document [0, 342) named
  atx_heading [0, 15) named
    atx_h1_marker [0, 1) named "#"
    heading_content [2, 15) named "Main Heading"
  ...

=== Style Runs (12) ===
  [0] offset=0, len=15, type=HEADING_1 "# Main Heading"
  [1] offset=38, len=4, type=STRONG "bold"
  ...

=== Building Outline ===
Found heading L1 at 0: 'Main Heading'
Found heading L2 at 71: 'Subheading with a Task List'
...

=== Outline ===
Found 4 headings:
L1 [0]: Main Heading
  L2 [71]: Subheading with a Task List
    L3 [156]: Code Example
  L2 [267]: Another Section
```

### 2. Fixed Outline Generation

The outline system has been completely rewritten to properly traverse the tree-sitter AST:

#### Previous Issues
- Used recursive function that didn't properly traverse siblings
- Missed headings that weren't direct children

#### New Implementation
- Uses `TSTreeCursor` for proper tree traversal
- Visits ALL nodes in the tree, not just direct children
- Correctly identifies all headings at any nesting level
- Extracts heading text from `heading_content` child nodes
- Stores both offset and length for each heading

#### Outline Structure

```cpp
const BMessage& outline = parser.GetOutline();

// Iterate through headings
int32 count = 0;
outline.GetInfo("heading", NULL, &count);

for (int32 i = 0; i < count; i++) {
    BMessage heading;
    outline.FindMessage("heading", i, &heading);
    
    BString text;
    int32 level;    // 1-6
    int32 offset;   // Byte offset in source
    int32 length;   // Length in bytes
    
    heading.FindString("text", &text);
    heading.FindInt32("level", &level);
    heading.FindInt32("offset", &offset);
    heading.FindInt32("length", &length);
}
```

### 3. Unicode Symbol Replacements

The parser now replaces ASCII markers with proper Unicode symbols for better visual presentation:

#### Replacements

| Original | Unicode | Character | Code Point |
|----------|---------|-----------|------------|
| `*`, `-`, `+` (bullets) | `•` | Bullet | U+2022 |
| `[ ]` (unchecked) | `☐` | Ballot Box | U+2610 |
| `[X]` (checked) | `☑` | Ballot Box with Check | U+2611 |

#### Control

- **`SetUseUnicodeSymbols(bool use)`** - Enable/disable Unicode replacements (enabled by default)
- **`GetUseUnicodeSymbols()`** - Check current setting

#### Implementation Details

When Unicode symbols are enabled:
1. Parser detects task markers and list bullets
2. Creates style runs with `StyleRun::text` field containing the Unicode replacement
3. Renderer can choose to:
   - Display the Unicode symbol from `StyleRun::text`
   - Or display the original text from source
4. This allows copy-paste to preserve original ASCII markers while displaying prettier symbols

#### New Style Types

```cpp
StyleRun::TASK_MARKER_UNCHECKED  // For [ ] checkboxes
StyleRun::TASK_MARKER_CHECKED    // For [X] checkboxes
```

### 4. Tree-sitter API Best Practices

The implementation now fully embraces tree-sitter patterns:

#### Proper Tree Traversal

```cpp
// Using TSTreeCursor for efficient traversal
TSTreeCursor cursor = ts_tree_cursor_new(root);
do {
    TSNode node = ts_tree_cursor_current_node(&cursor);
    // Process node...
} while (ts_tree_cursor_goto_next_sibling(&cursor) || 
         ts_tree_cursor_goto_first_child(&cursor));
ts_tree_cursor_delete(&cursor);
```

#### Node Queries

```cpp
// Get specific child by field name
TSNode infoNode = ts_node_child_by_field_name(node, "info_string", 11);

// Get node at specific position
TSNode node = ts_node_descendant_for_byte_range(root, offset, offset);
```

#### Incremental Parsing

```cpp
// Create edit descriptor
TSInputEdit edit;
edit.start_byte = offset;
edit.old_end_byte = offset + oldLength;
edit.new_end_byte = offset + newLength;
edit.start_point = {line, column};
edit.old_end_point = {old_line, old_column};
edit.new_end_point = {new_line, new_column};

// Tell tree-sitter what changed
ts_tree_edit(fTree, &edit);

// Re-parse (tree-sitter reuses unchanged nodes)
TSTree* newTree = ts_parser_parse_string(fParser, fTree, newText, len);

// See what changed
TSRange* ranges;
uint32_t range_count;
ranges = ts_tree_get_changed_ranges(oldTree, newTree, &range_count);
```

### 5. Enhanced StyleRun Structure

```cpp
struct StyleRun {
    int32 offset;
    int32 length;
    Type type;
    
    rgb_color foreground;
    rgb_color background;
    BFont font;
    
    BString language;  // For code blocks (e.g., "cpp", "python")
    BString url;       // For links
    BString text;      // For Unicode symbol replacements
};
```

The `text` field enables:
- Unicode symbol display while preserving ASCII in source
- Custom text replacements for any style type
- Renderer flexibility in choosing display vs. source

## Usage Examples

### Basic Parsing with Debug

```cpp
MarkdownParser parser;
parser.SetDebugEnabled(true);

if (parser.Parse(markdownText)) {
    // Debug output automatically printed
    // Access results...
}
```

### Accessing the Outline

```cpp
const BMessage& outline = parser.GetOutline();
int32 count = 0;
outline.GetInfo("heading", NULL, &count);

for (int32 i = 0; i < count; i++) {
    BMessage heading;
    outline.FindMessage("heading", i, &heading);
    
    BString text;
    int32 level, offset;
    heading.FindString("text", &text);
    heading.FindInt32("level", &level);
    heading.FindInt32("offset", &offset);
    
    printf("Level %d: %s (at byte %d)\n", level, text.String(), offset);
}
```

### Rendering with Unicode Symbols

```cpp
const std::vector<StyleRun>& runs = parser.GetStyleRuns();

for (const StyleRun& run : runs) {
    if (!run.text.IsEmpty()) {
        // Display Unicode replacement
        view->DrawString(run.text.String());
    } else {
        // Display original text from source
        BString original(sourceText + run.offset, run.length);
        view->DrawString(original.String());
    }
    
    // Apply styling
    view->SetFont(&run.font);
    view->SetHighColor(run.foreground);
    // etc...
}
```

### Incremental Updates

```cpp
// User types at position 100, inserting "hello" (5 chars)
int32 editOffset = 100;
int32 oldLength = 0;  // Insertion
int32 newLength = 5;

parser.ParseIncremental(newText, editOffset, oldLength, newLength);

// tree-sitter only re-parses what changed!
```

### Querying Nodes

```cpp
// Get the node at cursor position
int32 cursorOffset = GetCursorPosition();
TSNode node = parser.GetNodeAtOffset(cursorOffset);

if (!ts_node_is_null(node)) {
    const char* nodeType = ts_node_type(node);
    
    if (strcmp(nodeType, "inline_link") == 0) {
        // User clicked on a link, extract URL
        TSNode dest = ts_node_child_by_field_name(node, "link_destination", 16);
        // ...
    }
}
```

## Building

Requires:
- Tree-sitter library (`tree-sitter-0.dll` or `libtree-sitter.so`)
- Tree-sitter Markdown grammar (`tree-sitter-markdown`)
- Haiku API Kit

```bash
g++ -o test_parser test_parser.cpp MarkdownParser.cpp \
    -ltree-sitter -ltree-sitter-markdown \
    -lbe -lstdc++
```

## Testing

Run the included test program:

```bash
./test_parser
```

This will:
1. Parse a sample document with various Markdown features
2. Show debug output for tree structure, style runs, and outline
3. Test incremental parsing
4. Demonstrate outline and style run access
5. Test node queries

## Node Types Reference

Common tree-sitter-markdown node types:

- `document` - Root node
- `atx_heading` - # headings (with children: marker, content)
- `heading_content` - Text content of heading
- `paragraph` - Paragraph
- `emphasis` - *italic*
- `strong_emphasis` - **bold**
- `code_span` - `inline code`
- `fenced_code_block` - ```code block```
- `info_string` - Language identifier in fenced code block
- `inline_link` - [text](url)
- `link_destination` - URL in link
- `list_item` - List item
- `list_marker_minus`, `list_marker_star`, etc. - Bullet types
- `task_list_marker_checked` - [X]
- `task_list_marker_unchecked` - [ ]
- `block_quote` - > quote

## Future Enhancements

Potential additions:
- Full syntax highlighting integration
- Table support (tree-sitter-markdown has table nodes)
- Image handling
- Reference-style links
- Definition lists
- Performance profiling with large documents
- Custom node visitors pattern
- Export to HTML using tree structure

## License

MIT License (matching tree-sitter licensing)
