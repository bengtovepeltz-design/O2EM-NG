#pragma once

#include <string>

// Opens a native Windows text editor. Returns true when Save was pressed.
// The value is left unchanged when the user cancels.
bool OpenMetadataTextEditor(
    const char* fieldLabel,
    bool multiline,
    std::string& value);
