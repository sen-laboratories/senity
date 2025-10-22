#!/bin/bash
g++ -o test_parser src/test/ParserTest.cpp src/MarkdownParser.cpp src/SyntaxHighlighter.cpp \
-lcmark-gfm -lcmark-gfm-extensions -ltree-sitter -ltree-sitter-c -ltree-sitter-cpp \
-ltree-sitter-go -ltree-sitter-javascript -ltree-sitter-python -ltree-sitter-rust -lbe -lstdc++ \
-I$HOME/config/non-packaged/develop/headers \
-L$HOME/config/non-packaged/lib
