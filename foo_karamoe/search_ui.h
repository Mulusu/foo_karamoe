#pragma once
#include "stdafx.h"



namespace foo_karamoe {

    enum SearchStatus {
        // Numbers are the unicode number of the emoji used to indicate that status
        Idle = 0x1F4A4,         // System is idle
        Waiting = 0x23f3,       // Waiting for typing debounce timer to fire search
        Network = 0x1F4E1,      // HTTP search query underway
        Parsing = 0x1F50D,      // Parsing response of the HTTP search query
        Done = 0x2705,          // Search / Queueing is done
        Filing = 0x1F4DD,       // Preparing files
        Save = 0x1F4BE,         // Saving files to filesystem
        Error = 0x26A0          // Operation failed
    };

    const int DEBOUNCE_WAIT = 750;    // Wait after typing stops before search is fired, ms

    const int SEARCH_BAR_HEIGHT = 30;
    const int SEARCH_STATUS_WIDTH = SEARCH_BAR_HEIGHT;

	interface SearchUI {

	};
}
