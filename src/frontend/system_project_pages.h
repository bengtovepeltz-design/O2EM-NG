#pragma once

#include <vector>

#include "src/database/game_database.h"

// Application-owned project information.
//
// These pages are part of the executable/system resource, not the user's
// o2em-ng.db. Legacy project_pages rows may remain in an old database for
// compatibility, but they never override this list.
std::vector<ProjectPage> BuildSystemProjectPages();
