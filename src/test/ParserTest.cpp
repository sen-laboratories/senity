#include "../MarkdownParser.h"

#include "ParserTest.h"
#include <stdio.h>

const char* kApplicationSignature = "application/x-vnd.senlabs-senity";

const char* TEST_MARKDOWN = R"(# Main Heading

This is a paragraph with **bold** and *italic* text.

## Subheading with a Task List

- [X] Completed task
- [ ] Uncompleted task
- Regular bullet point

### Code Example

Here's some inline `code` and a block:

```cpp
#include <stdio.h>

int main() {
    printf("Hello, World!\n");
    return 0;
}
```

## Another Section

[Link to Haiku](https://www.haiku-os.org)

> This is a blockquote
> with multiple lines

1. Numbered item one
2. Numbered item two
)";


int main(int32 argc, char **argv)
{
	BApplication* app = new ParserTest();
	app->Run();

	delete app;
	return 0;
}


ParserTest::~ParserTest()
{
}

ParserTest::ParserTest()
	:
	BApplication(kApplicationSignature)
{
    MarkdownParser parser;

    // Enable debugging
    parser.SetDebugEnabled(true);
    parser.SetUseUnicodeSymbols(true);

    PrintSeparator("PARSING MARKDOWN");

    // Parse the test markdown
    if (!parser.Parse(TEST_MARKDOWN)) {
        printf("ERROR: Failed to parse markdown\n");
        return;
    }

    // The debug output will show:
    // 1. Parse tree structure
    // 2. Processing trace
    // 3. Style runs created
    // 4. Outline with headings

    PrintSeparator("TESTING INCREMENTAL PARSE");

    // Test incremental parsing - change "bold" to "very bold"
    const char* MODIFIED = R"(# Main Heading

This is a paragraph with **very bold** and *italic* text.

## Subheading with a Task List

- [X] Completed task
- [ ] Uncompleted task
- Regular bullet point

### Code Example

Here's some inline `code` and a block:

```cpp
#include <stdio.h>

int main() {
    printf("Hello, World!\n");
    return 0;
}
```

## Another Section

[Link to Haiku](https://www.haiku-os.org)

> This is a blockquote
> with multiple lines

1. Numbered item one
2. Numbered item two
)";

    // The edit is at position 38 (after "**"), removed 4 chars "bold", added 9 chars "very bold"
    int32 editOffset = 38;
    int32 oldLength = 4;
    int32 newLength = 9;

    if (!parser.ParseIncremental(MODIFIED, editOffset, oldLength, newLength)) {
        printf("ERROR: Failed to incrementally parse\n");
        return;
    }

    PrintSeparator("ACCESSING OUTLINE PROGRAMMATICALLY");

    const BMessage* outline = parser.GetOutline();
    int32 headingCount = 0;
    outline->GetInfo("heading", NULL, &headingCount);

    printf("\nOutline has %d headings:\n", headingCount);
    for (int32 i = 0; i < headingCount; i++) {
        BMessage heading;
        if (outline->FindMessage("heading", i, &heading) == B_OK) {
            BString text;
            int32 level, offset;
            heading.FindString("text", &text);
            heading.FindInt32("level", &level);
            heading.FindInt32("offset", &offset);

            printf("  %d. Level %d at byte %d: \"%s\"\n",
                   i + 1, level, offset, text.String());
        }
    }

    PrintSeparator("ACCESSING STYLE RUNS");

    const std::vector<StyleRun>& runs = parser.GetStyleRuns();
    printf("\nFound %zu style runs\n", runs.size());
    printf("\nUnicode replacements:\n");

    for (size_t i = 0; i < runs.size(); i++) {
        const StyleRun& run = runs[i];
        if (!run.text.IsEmpty()) {
            printf("  At offset %d: Replace with '%s'\n",
                   run.offset, run.text.String());
        }
    }

    PrintSeparator("TESTING NODE QUERIES");

    // Test getting node at specific offset
    int32 testOffset = 50; // Should be in the paragraph
    TSNode node = parser.GetNodeAtOffset(testOffset);

    if (!ts_node_is_null(node)) {
        printf("\nNode at offset %d:\n", testOffset);
        printf("  Type: %s\n", ts_node_type(node));
        printf("  Range: [%u, %u)\n",
               ts_node_start_byte(node), ts_node_end_byte(node));
        printf("  Line: %d\n", parser.GetLineForOffset(testOffset));
    }

    PrintSeparator("TEST COMPLETE");
}

void ParserTest::PrintSeparator(const char* title)
{
    printf("\n");
    printf("================================================================================\n");
    printf("  %s\n", title);
    printf("================================================================================\n");
}
