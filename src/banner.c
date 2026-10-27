/*
* --------------------------------------------------------------------
* IGNYTION IO
* Copyright (c) 2026 IGNYTION IO. All Rights Reserved.
*
* This software and associated documentation files are proprietary
* and confidential. Unauthorized copying, modification, distribution,
* or use of this file, via any medium, is strictly prohibited.
*
* Website: https://ignytion.io
* ---------------------------------------------------------------------
*/


#include <stdio.h>
#include <sys/utsname.h>

#define SPARK  "\033[38;5;172m"
#define BOLD   "\033[1m"
#define RESET  "\033[0m"

#define IGNYTION_PYTHON   "3.14.0"
#define IGNYTION_TIER     "Pro"
#define CRUCIBLE_ENV      "default"
#define CRUCIBLE_VERSION  "1.0.0"

int main() {
    struct utsname uts;
    uname(&uts);

    char platform[128];
    snprintf(platform, sizeof(platform), "%s-%s", uts.sysname, uts.machine);

    printf("\n");
    printf(SPARK "â•­â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â•®" RESET "\n");
    printf(SPARK "â”‚                                                                        â”‚" RESET "\n");
    printf(SPARK "â”‚" BOLD "   â–ˆâ–ˆâ•— â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— â–ˆâ–ˆâ–ˆâ•—   â–ˆâ–ˆâ•—â–ˆâ–ˆâ•—   â–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ•—   â–ˆâ–ˆâ•—     " RESET SPARK "â”‚" RESET "\n");
    printf(SPARK "â”‚" BOLD "   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â•â•â• â–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ•— â–ˆâ–ˆâ•”â•â•šâ•â•â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•‘ â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•— â–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ•‘     " RESET SPARK "â”‚" RESET "\n");
    printf(SPARK "â”‚" BOLD "   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â–ˆâ–ˆâ•— â–ˆâ–ˆâ•‘ â•šâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•    â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘ â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘ â–ˆâ–ˆâ•”â–ˆâ–ˆâ•— â–ˆâ–ˆâ•‘     " RESET SPARK "â”‚" RESET "\n");
    printf(SPARK "â”‚" BOLD "   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â•šâ–ˆâ–ˆâ•”â•     â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘ â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘ â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     " RESET SPARK "â”‚" RESET "\n");
    printf(SPARK "â”‚" BOLD "   â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘ â•šâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘      â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘ â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â• â–ˆâ–ˆâ•‘ â•šâ–ˆâ–ˆâ–ˆâ–ˆâ•‘     " RESET SPARK "â”‚" RESET "\n");
    printf(SPARK "â”‚" BOLD "   â•šâ•â• â•šâ•â•â•â•â•â• â•šâ•â•  â•šâ•â•â•â•   â•šâ•â•      â•šâ•â•   â•šâ•â•  â•šâ•â•â•â•â•â•  â•šâ•â•  â•šâ•â•â•â•     " RESET SPARK "â”‚" RESET "\n");
    printf(SPARK "â”‚                                                                        â”‚" RESET "\n");
    printf(SPARK "â”‚" BOLD "                       BUILD SILICON, LIKE SOFTWARE                     " RESET SPARK "â”‚" RESET "\n");
    printf(SPARK "â”‚                                                                        â”‚" RESET "\n");
    printf(SPARK "â”‚" BOLD "      CRUCIBLEâ„¢ CORE by Ignytion IO Â· End-to-end chip design platform   " RESET SPARK "â”‚" RESET "\n");
    printf(SPARK "â”‚" BOLD "                         Idea  â†’  RTL  â†’  GDS                           " RESET SPARK "â”‚" RESET "\n");
    printf(SPARK "â”‚                                                                        â”‚" RESET "\n");
    printf(SPARK "â”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¤" RESET "\n");
    printf(SPARK "â”‚                                                                        â”‚" RESET "\n");
    printf(SPARK "â”‚" SPARK "   Platform:    %-54s  " SPARK "â”‚" RESET "\n", platform);
    printf(SPARK "â”‚" SPARK "   Python:      %-54s  " SPARK "â”‚" RESET "\n", IGNYTION_PYTHON);
    printf(SPARK "â”‚" SPARK "   Tier:        %-54s  " SPARK "â”‚" RESET "\n", IGNYTION_TIER);
    printf(SPARK "â”‚" SPARK "   Environment: %-54s  " SPARK "â”‚" RESET "\n", CRUCIBLE_ENV);
    printf(SPARK "â”‚" SPARK "   Version:     %-54s  " SPARK "â”‚" RESET "\n", CRUCIBLE_VERSION);
    printf(SPARK "â”‚                                                                        â”‚" RESET "\n");
    printf(SPARK "â•°â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â•¯" RESET "\n");
    printf("\n");
    printf(SPARK "  Â© 2026 Ignytion IO. All rights reserved. Â· https://ignytion.io" RESET "\n");
    printf("\n");

    return 0;
}
