/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "OutlineNavigator.h"

OutlineNavigator::OutlineNavigator(const BMessage& outline)
    : fOutline(outline)
{
}

BString OutlineNavigator::GetCurrentContext(int32 cursorOffset)
{
    std::vector<BString> crumbs = GetBreadcrumbs(cursorOffset);

    BString context;
    for (size_t i = 0; i < crumbs.size(); i++) {
        if (i > 0) context << " > ";
        context << crumbs[i];
    }
    return context;
}

std::vector<BString> OutlineNavigator::GetBreadcrumbs(int32 offset)
{
    std::vector<BString> crumbs;
    BuildBreadcrumbs(&fOutline, offset, crumbs);
    return crumbs;
}

void OutlineNavigator::BuildBreadcrumbs(BMessage* msg, int32 targetOffset, std::vector<BString>& crumbs)
{
    if (!msg) return;

    int32 index = 0;
    BMessage child;

    while (msg->FindMessage("children", index, &child) == B_OK) {
        int32 offset;
        BString text;

        if (child.FindInt32("offset", &offset) != B_OK) {
            index++;
            continue;
        }

        child.FindString("text", &text);

        // Check if target is within or after this heading
        if (offset <= targetOffset) {
            crumbs.push_back(text);

            // Recursively search children
            BuildBreadcrumbs(&child, targetOffset, crumbs);
            return;
        }

        index++;
    }
}

BMessage* OutlineNavigator::FindHeadingAtOffset(int32 offset, int32 level)
{
    return FindHeadingAtOffsetRecursive(&fOutline, offset, level);
}

BMessage* OutlineNavigator::FindHeadingAtOffsetRecursive(BMessage* msg, int32 targetOffset, int32 targetLevel)
{
    if (!msg) return nullptr;

    int32 index = 0;
    BMessage child;

    while (msg->FindMessage("children", index, &child) == B_OK) {
        int32 offset, level;

        if (child.FindInt32("offset", &offset) == B_OK &&
            child.FindInt32("level", &level) == B_OK) {

            // Check if this is the heading we're looking for
            if (offset == targetOffset && (targetLevel == -1 || level == targetLevel)) {
                return new BMessage(child);
            }

            // Search children
            BMessage* found = FindHeadingAtOffsetRecursive(&child, targetOffset, targetLevel);
            if (found) return found;
        }

        index++;
    }

    return nullptr;
}

BMessage* OutlineNavigator::FindParentHeading(BMessage* root, BMessage* child, int32 parentLevel)
{
    // This is complex - need to track parent chain while traversing
    // For now, simplified version
    return nullptr;
}

BMessage* OutlineNavigator::FindNextSibling(BMessage* parent, int32 currentOffset, int32 level)
{
    if (!parent) return nullptr;

    int32 index = 0;
    BMessage child;
    bool foundCurrent = false;

    while (parent->FindMessage("children", index, &child) == B_OK) {
        int32 offset, nodeLevel;

        if (child.FindInt32("offset", &offset) == B_OK &&
            child.FindInt32("level", &nodeLevel) == B_OK) {

            if (foundCurrent && nodeLevel == level) {
                return new BMessage(child);
            }

            if (offset == currentOffset) {
                foundCurrent = true;
            }
        }

        index++;
    }

    return nullptr;
}

int32 OutlineNavigator::NextSectionInChapter(int32 currentOffset)
{
    BMessage* current = FindHeadingAtOffset(currentOffset, 2);
    if (!current) return -1;

    BMessage* chapter = FindParentHeading(&fOutline, current, 1);
    if (!chapter) return -1;

    BMessage* next = FindNextSibling(chapter, currentOffset, 2);
    if (!next) return -1;

    int32 nextOffset;
    if (next->FindInt32("offset", &nextOffset) == B_OK) {
        return nextOffset;
    }

    return -1;
}

int32 OutlineNavigator::PreviousSectionInChapter(int32 currentOffset)
{
    BMessage* current = FindHeadingAtOffset(currentOffset, 2);
    if (!current) return -1;

    BMessage* chapter = FindParentHeading(&fOutline, current, 1);
    if (!chapter) return -1;

    // Find previous H2 sibling
    int32 index = 0;
    BMessage child;
    BMessage* previous = nullptr;

    while (chapter->FindMessage("children", index, &child) == B_OK) {
        int32 offset, level;

        if (child.FindInt32("offset", &offset) == B_OK &&
            child.FindInt32("level", &level) == B_OK) {

            if (offset == currentOffset) {
                // Found current, return previous
                if (previous) {
                    int32 prevOffset;
                    if (previous->FindInt32("offset", &prevOffset) == B_OK) {
                        return prevOffset;
                    }
                }
                return -1;
            }

            if (level == 2) {
                previous = &child;
            }
        }

        index++;
    }

    return -1;
}

int32 OutlineNavigator::NextHeading(int32 currentOffset)
{
    // Build flat list and find next
    OutlineIndex index;
    index.BuildIndex(fOutline);

    const auto& headings = index.GetAll();

    for (size_t i = 0; i < headings.size(); i++) {
        if (headings[i].offset > currentOffset) {
            return headings[i].offset;
        }
    }

    return -1;
}

int32 OutlineNavigator::PreviousHeading(int32 currentOffset)
{
    // Build flat list and find previous
    OutlineIndex index;
    index.BuildIndex(fOutline);

    const auto& headings = index.GetAll();

    int32 prevOffset = -1;
    for (size_t i = 0; i < headings.size(); i++) {
        if (headings[i].offset >= currentOffset) {
            return prevOffset;
        }
        prevOffset = headings[i].offset;
    }

    return prevOffset;
}

// OutlineIndex implementation

void OutlineIndex::BuildIndex(const BMessage& outline)
{
    fFlatIndex.clear();
    BuildIndexRecursive(&outline);
}

void OutlineIndex::BuildIndexRecursive(const BMessage* msg)
{
    if (!msg) return;

    int32 index = 0;
    BMessage child;

    while (msg->FindMessage("children", index, &child) == B_OK) {
        HeadingInfo info;

        child.FindString("text", &info.text);
        child.FindInt32("level", &info.level);
        child.FindInt32("offset", &info.offset);
        child.FindInt32("line", &info.line);

        fFlatIndex.push_back(info);

        // Recurse for nested headings
        BuildIndexRecursive(&child);
        index++;
    }
}

const OutlineIndex::HeadingInfo* OutlineIndex::FindByOffset(int32 offset) const
{
    for (const auto& info : fFlatIndex) {
        if (info.offset == offset) return &info;
    }
    return nullptr;
}

std::vector<OutlineIndex::HeadingInfo> OutlineIndex::GetLevel(int32 level) const
{
    std::vector<HeadingInfo> result;
    for (const auto& info : fFlatIndex) {
        if (info.level == level) {
            result.push_back(info);
        }
    }
    return result;
}
