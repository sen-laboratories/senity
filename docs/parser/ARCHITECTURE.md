# MarkdownParser Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                          MarkdownParser                              │
│                                                                      │
│  ┌────────────────┐                                                 │
│  │ Configuration  │                                                 │
│  ├────────────────┤                                                 │
│  │ - Debug On/Off │                                                 │
│  │ - Unicode On/Off│                                                │
│  │ - Fonts        │                                                 │
│  │ - Colors       │                                                 │
│  └────────────────┘                                                 │
│                                                                      │
│  ┌────────────────┐          ┌─────────────────┐                   │
│  │ tree-sitter    │          │  Source Text    │                   │
│  ├────────────────┤          ├─────────────────┤                   │
│  │ TSParser       │───────>  │  char* copy     │                   │
│  │ TSTree         │          │  (persistent)   │                   │
│  └────────────────┘          └─────────────────┘                   │
│         │                                                            │
│         │ parse                                                     │
│         ▼                                                            │
│  ┌────────────────────────────────────────────┐                    │
│  │            Parse Tree (TSTree)              │                    │
│  │                                            │                    │
│  │    document                                │                    │
│  │    ├── atx_heading                        │                    │
│  │    │   ├── atx_h1_marker: "#"            │                    │
│  │    │   └── heading_content: "Title"       │                    │
│  │    ├── paragraph                          │                    │
│  │    │   ├── text: "Hello "                 │                    │
│  │    │   ├── strong_emphasis                │                    │
│  │    │   │   └── text: "world"              │                    │
│  │    │   └── text: "!"                      │                    │
│  │    └── list                               │                    │
│  │        ├── list_item                      │                    │
│  │        │   ├── list_marker_minus: "-"     │                    │
│  │        │   └── text: "Item"               │                    │
│  │        └── list_item                      │                    │
│  │            ├── task_list_marker_unchecked │                    │
│  │            └── text: "Task"               │                    │
│  └────────────────────────────────────────────┘                    │
│         │                                                            │
│         │ process                                                   │
│         ▼                                                            │
│  ┌─────────────────────────────────────┐                           │
│  │          Output Products             │                           │
│  │                                      │                           │
│  │  ┌──────────────────────────────┐   │                           │
│  │  │      Style Runs              │   │                           │
│  │  ├──────────────────────────────┤   │                           │
│  │  │ [0, 7)   HEADING_1           │   │                           │
│  │  │ [10, 15) NORMAL              │   │                           │
│  │  │ [15, 20) STRONG              │   │                           │
│  │  │ [42, 43) LIST_BULLET  "•"    │   │                           │
│  │  │ [50, 53) TASK_UNCHECKED "☐"  │   │                           │
│  │  └──────────────────────────────┘   │                           │
│  │                                      │                           │
│  │  ┌──────────────────────────────┐   │                           │
│  │  │        Outline               │   │                           │
│  │  ├──────────────────────────────┤   │                           │
│  │  │ L1: "Title"        [0]       │   │                           │
│  │  │ L2: "Section"      [100]     │   │                           │
│  │  │ L3: "Subsection"   [200]     │   │                           │
│  │  └──────────────────────────────┘   │                           │
│  └─────────────────────────────────────┘                           │
└─────────────────────────────────────────────────────────────────────┘
```

## Data Flow

### Initial Parse

```
User Input
    │
    ▼
┌──────────────────────┐
│ Parse(text)          │
└──────────────────────┘
    │
    ├─> Store source copy
    │
    ├─> ts_parser_parse_string()
    │       │
    │       ▼
    │   ┌─────────────┐
    │   │   TSTree    │ ◄─── Persistent!
    │   └─────────────┘
    │
    ├─> ProcessNode(root)
    │       │
    │       ├─> Detect node types
    │       ├─> Extract metadata
    │       ├─> Create style runs
    │       │       │
    │       │       └─> Add Unicode symbols
    │       │
    │       └─> Recursive children
    │
    └─> BuildOutline()
            │
            └─> Cursor traversal
                    │
                    └─> Find all headings
```

### Incremental Parse

```
User Edit (offset, oldLen, newLen)
    │
    ▼
┌──────────────────────────────┐
│ ParseIncremental(...)        │
└──────────────────────────────┘
    │
    ├─> Create TSInputEdit
    │       │
    │       └─> {start, old_end, new_end, points}
    │
    ├─> ts_tree_edit(tree, &edit)  ◄── Tell tree-sitter
    │
    ├─> Update source copy
    │
    ├─> ts_parser_parse_string(parser, OLD_TREE, text, len)
    │       │
    │       └─> Tree-sitter reuses unchanged nodes! ⚡
    │
    ├─> Get changed ranges  ◄── Debug info
    │
    ├─> Re-process tree (fast!)
    │
    └─> Rebuild outputs
```

## Component Interaction

```
┌─────────────────────────────────────────────────────────────┐
│                         Application                          │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ uses
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      MarkdownParser                          │
│                                                              │
│  Public API:                                                 │
│  • Parse(text) / ParseIncremental(...)                      │
│  • GetStyleRuns() → vector<StyleRun>                        │
│  • GetOutline() → BMessage                                  │
│  • GetNodeAtOffset(pos) → TSNode                           │
│  • SetDebugEnabled(bool)                                    │
│  • SetUseUnicodeSymbols(bool)                              │
└─────────────────────────────────────────────────────────────┘
            │                           │
            │ depends on               │ depends on
            ▼                           ▼
┌──────────────────────┐    ┌──────────────────────┐
│   tree-sitter API    │    │    Haiku API Kit     │
├──────────────────────┤    ├──────────────────────┤
│ • ts_parser_*        │    │ • BString            │
│ • ts_tree_*          │    │ • BMessage           │
│ • ts_node_*          │    │ • BFont              │
│ • ts_tree_cursor_*   │    │ • rgb_color          │
└──────────────────────┘    └──────────────────────┘
```

## Style Run Generation

```
ProcessNode(TSNode)
    │
    ├─> Get node type string
    │
    ├─> Is it a special node?
    │   │
    │   ├─> task_list_marker_unchecked
    │   │   └─> CreateStyleRun(..., TASK_UNCHECKED, text="☐")
    │   │
    │   ├─> task_list_marker_checked
    │   │   └─> CreateStyleRun(..., TASK_CHECKED, text="☑")
    │   │
    │   ├─> list_marker_*
    │   │   └─> CreateStyleRun(..., LIST_BULLET, text="•")
    │   │
    │   └─> Other types (headings, code, emphasis...)
    │       └─> CreateStyleRun(..., TYPE)
    │
    └─> Recurse to children
```

## Outline Building

```
BuildOutline()
    │
    ├─> Create TSTreeCursor
    │
    └─> Traverse tree:
        │
        ├─> Get current node
        │
        ├─> Is it "atx_heading"?
        │   │
        │   ├─> Extract level from marker
        │   ├─> Extract text from heading_content
        │   ├─> Create BMessage entry
        │   │   ├─> "text": heading text
        │   │   ├─> "level": 1-6
        │   │   ├─> "offset": byte position
        │   │   └─> "length": byte length
        │   │
        │   └─> Add to outline
        │
        └─> Move cursor (sibling or first child)
```

## Incremental Parse Example

```
Before:
   "Hello **world**!"
    0     7     14  16

Edit: Insert "beautiful " at position 9
   "Hello **beautiful world**!"
    0     7  9        19     25 27

TSInputEdit:
    start_byte = 9
    old_end_byte = 9      (insertion)
    new_end_byte = 19     (+10 chars)

tree-sitter:
    ├─> Identifies unchanged region [0, 9)
    ├─> Identifies unchanged region [9, 16) → now [19, 26)
    ├─> Only re-parses the changed token "**beautiful world**"
    └─> Reuses all other parsed nodes!

Result: ~100x faster than re-parsing entire document
```

## Memory Layout

```
MarkdownParser Object:
┌────────────────────────────────────┐
│ TSParser* fParser                  │ ──> tree-sitter parser (reused)
│ TSTree* fTree                      │ ──> current parse tree
│ const char* fSourceText            │ ──> points to fSourceCopy
│ char* fSourceCopy                  │ ──> owned copy of source
│                                    │
│ vector<StyleRun> fStyleRuns        │ ──> generated output
│ BMessage fOutline                  │ ──> generated output
│                                    │
│ map<Type, BFont> fFonts            │ ──> style config
│ map<Type, rgb_color> fForeground   │ ──> style config
│ map<Type, rgb_color> fBackground   │ ──> style config
│                                    │
│ SyntaxHighlighter* fSyntaxHighlighter │ ──> not owned
│                                    │
│ bool fDebugEnabled                 │ ──> option
│ bool fUseUnicodeSymbols            │ ──> option
└────────────────────────────────────┘

StyleRun Object:
┌────────────────────────────────────┐
│ int32 offset                       │ ──> byte offset in source
│ int32 length                       │ ──> byte length
│ Type type                          │ ──> style type enum
│ rgb_color foreground               │ ──> text color
│ rgb_color background               │ ──> background color
│ BFont font                         │ ──> font settings
│ BString language                   │ ──> for code blocks
│ BString url                        │ ──> for links
│ BString text                       │ ──> Unicode replacement
└────────────────────────────────────┘
```

## Debug Output Flow

```
With SetDebugEnabled(true):

Parse(text)
    │
    ├─> Print "=== Parsing Markdown (N bytes) ==="
    │
    ├─> DumpTree()
    │   └─> Print S-expression of entire tree
    │
    ├─> ProcessNode(root)
    │   └─> DebugPrintNode() for each node
    │       └─> Print type, range, content with indentation
    │
    ├─> DumpStyleRuns()
    │   └─> Print each style run with details
    │
    └─> DumpOutline()
        └─> Print heading hierarchy

Output:
    === Parsing Markdown (342 bytes) ===
    
    === Parse Tree ===
    (document ...)
    
    === Processing Parse Tree ===
    document [0, 342) named
      atx_heading [0, 15) named
        atx_h1_marker [0, 1) named "#"
        ...
    
    === Style Runs (12) ===
    [0] offset=0, len=15, type=HEADING_1 "# Main Heading"
    ...
    
    === Building Outline ===
    Found heading L1 at 0: 'Main Heading'
    ...
    
    === Outline ===
    Found 4 headings:
    L1 [0]: Main Heading
      L2 [71]: Subheading
    ...
```

## Node Type Examples

```
Markdown:              tree-sitter Node Type:
─────────────────────  ────────────────────────────────
# Heading              atx_heading
                         ├── atx_h1_marker
                         └── heading_content

**bold**               strong_emphasis
                         └── text

*italic*               emphasis
                         └── text

`code`                 code_span

```cpp                 fenced_code_block
code                     ├── fenced_code_block_delimiter
```                      ├── info_string
                         ├── code_fence_content
                         └── fenced_code_block_delimiter

[link](url)            inline_link
                         ├── link_text
                         └── link_destination

- [ ] Task             list_item
                         ├── task_list_marker_unchecked
                         └── paragraph

- [X] Done             list_item
                         ├── task_list_marker_checked
                         └── paragraph

- Bullet               list_item
                         ├── list_marker_minus
                         └── paragraph
```

This architecture provides a solid foundation for modern Markdown editing!
