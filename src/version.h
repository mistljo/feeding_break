/**
 * @file version.h
 * @brief Central version information for Feeding Break Controller
 * 
 * Version info is automatically extracted from Git during build.
 * Use git tags (e.g., git tag v2.1.0) to set version numbers.
 */

#ifndef VERSION_H
#define VERSION_H

// App name
#define APP_NAME "Feeding Break"

// Fallback values if git info not available
#ifndef BUILD_VERSION
#define BUILD_VERSION "dev"
#endif

#ifndef BUILD_COMMIT
#define BUILD_COMMIT "unknown"
#endif

#ifndef BUILD_BRANCH
#define BUILD_BRANCH "unknown"
#endif

#ifndef BUILD_DATE
#define BUILD_DATE __DATE__ " " __TIME__
#endif

// Helper macros for display
#define VERSION_STRING      APP_NAME " " BUILD_VERSION
#define VERSION_FULL        APP_NAME " " BUILD_VERSION " (" BUILD_COMMIT ")"
#define VERSION_WITH_DATE   APP_NAME " " BUILD_VERSION "\nBuild: " BUILD_DATE

#endif // VERSION_H
