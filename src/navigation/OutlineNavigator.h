/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <Message.h>
#include <String.h>
#include <vector>

// Navigate document outline structure
class OutlineNavigator {
public:
    OutlineNavigator(const BMessage& outline);

    // Get breadcrumb trail at cursor position
    BString GetCurrentContext(int32 cursorOffset);

    // Navigation
    int32 NextSectionInChapter(int32 currentOffset);
    int32 PreviousSectionInChapter(int32 currentOffset);
    int32 NextHeading(int32 currentOffset);
    int32 PreviousHeading(int32 currentOffset);

    // Query
    BMessage* FindHeadingAtOffset(int32 offset, int32 level = -1);
    std::vector<BString> GetBreadcrumbs(int32 offset);

private:
    BMessage fOutline;

    void BuildBreadcrumbs(BMessage* msg, int32 targetOffset, std::vector<BString>& crumbs);
    BMessage* FindHeadingAtOffsetRecursive(BMessage* msg, int32 offset, int32 level);
    BMessage* FindParentHeading(BMessage* root, BMessage* child, int32 parentLevel);
    BMessage* FindNextSibling(BMessage* parent, int32 currentOffset, int32 level);
};

// Flat index for efficient access
class OutlineIndex {
public:
    struct HeadingInfo {
        BString text;
        int32 level;
        int32 offset;
        int32 line;

        HeadingInfo() : level(0), offset(0), line(0) {}
    };

    void BuildIndex(const BMessage& outline);
    const HeadingInfo* FindByOffset(int32 offset) const;
    std::vector<HeadingInfo> GetLevel(int32 level) const;
    const std::vector<HeadingInfo>& GetAll() const { return fFlatIndex; }

private:
    std::vector<HeadingInfo> fFlatIndex;

    void BuildIndexRecursive(const BMessage* msg);
};
