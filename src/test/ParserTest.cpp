// src/test/ParserTest.cpp
#include <Application.h>
#include <stdio.h>
#include "../MarkdownParser.h"

class TestApp : public BApplication {
public:
    TestApp() : BApplication("application/x-vnd.senity-test") {}
    
    void ReadyToRun() override {
        RunTests();
        PostMessage(B_QUIT_REQUESTED);
    }
    
    void RunTests() {
        MarkdownParser parser;
        
        const char* markdown = 
            "# Hello World\n"
            "\n"
            "This is **bold** and *italic*.\n"
            "\n"
            "```cpp\n"
            "int main() { return 0; }\n"
            "```\n";
        
        printf("Testing MarkdownParser...\n");
        
        if (parser.Parse(markdown)) {
            printf("✓ Parse succeeded\n");
            
            const auto& runs = parser.GetStyleRuns();
            printf("✓ Found %zu style runs\n", runs.size());
            
            for (size_t i = 0; i < runs.size(); i++) {
                printf("  Run %zu: offset=%d, length=%d, type=%d\n",
                       i, runs[i].offset, runs[i].length, runs[i].type);
            }
            
            const BMessage& outline = parser.GetOutline();
            printf("✓ Outline created\n");
            outline.PrintToStream();
            
            printf("\n✓ All tests passed!\n");
        } else {
            printf("✗ Parse failed\n");
        }
    }
};

int main() {
    TestApp app;
    app.Run();
    return 0;
}
