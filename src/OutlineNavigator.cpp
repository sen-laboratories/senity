class OutlineNavigator {
public:
    OutlineNavigator(const BMessage& outline) : fOutline(outline) {}

    // Get chapter/section context at cursor
    BString GetCurrentContext(int32 cursorOffset) {
        std::vector<BString> crumbs;
        BuildBreadcrumbs(const_cast<BMessage*>(&fOutline), cursorOffset, crumbs);

        BString context;
        for (size_t i = 0; i < crumbs.size(); i++) {
            if (i > 0) context << " > ";
            context << crumbs[i];
        }
        return context;
    }

    // Jump to next H2 in current H1
    int32 NextSectionInChapter(int32 currentOffset) {
        // Find current H2
        BMessage* current = FindHeadingAtOffset(&fOutline, currentOffset, 2);
        if (!current) return -1;

        // Find parent H1
        BMessage* chapter = FindParentHeading(&fOutline, current, 1);
        if (!chapter) return -1;

        // Get next H2 within this chapter
        BMessage* next = FindNextSibling(chapter, currentOffset, 2);
        if (!next) return -1;

        int32 nextOffset;
        next->FindInt32("offset", &nextOffset);
        return nextOffset;
    }

private:
    BMessage fOutline;

    BMessage* FindHeadingAtOffset(BMessage* msg, int32 offset, int32 level) {
        // Implementation similar to examples above
        return nullptr;
    }

    BMessage* FindParentHeading(BMessage* root, BMessage* child, int32 parentLevel) {
        // Walk tree to find parent at specified level
        return nullptr;
    }
};

// Efficient access: Store flat index alongside hierarchy
class OutlineIndex {
public:
    void BuildIndex(const BMessage& outline) {
        fFlatIndex.clear();
        BuildIndexRecursive(&outline, 0);
    }

    struct HeadingInfo {
        BString text;
        int32 level;
        int32 offset;
        int32 line;
        BMessage* node;
    };

    const HeadingInfo* FindByOffset(int32 offset) const {
        for (const auto& info : fFlatIndex) {
            if (info.offset == offset) return &info;
        }
        return nullptr;
    }

    std::vector<HeadingInfo> GetLevel(int32 level) const {
        std::vector<HeadingInfo> result;
        for (const auto& info : fFlatIndex) {
            if (info.level == level) result.push_back(info);
        }
        return result;
    }

private:
    std::vector<HeadingInfo> fFlatIndex;

    void BuildIndexRecursive(const BMessage* msg, int depth) {
        int32 index = 0;
        BMessage child;

        while (msg->FindMessage("children", index, &child) == B_OK) {
            HeadingInfo info;
            child.FindString("text", &info.text);
            child.FindInt32("level", &info.level);
            child.FindInt32("offset", &info.offset);
            child.FindInt32("line", &info.line);
            info.node = &child;

            fFlatIndex.push_back(info);

            // Recurse for nested headings
            BuildIndexRecursive(&child, depth + 1);
            index++;
        }
    }
};
