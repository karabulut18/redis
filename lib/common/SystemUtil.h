#pragma once

#include <cstddef>
#include <sys/sysctl.h>
#include <unistd.h>

/**
 * SystemUtil provides helpers to retrieve hardware-specific constants at runtime.
 */
class SystemUtil
{
public:
    /**
     * Returns the physical memory page size (e.g., 4096 on Intel, 16384 on Apple Silicon).
     */
    static size_t GetPageSize()
    {
        static size_t pageSize = sysconf(_SC_PAGESIZE);
        return pageSize;
    }

    /**
     * Returns the CPU cache line size (e.g., 64 or 128 bytes).
     */
    static size_t GetCacheLineSize()
    {
        static size_t cacheLineSize = 0;
        if (cacheLineSize == 0)
        {
            size_t size = sizeof(cacheLineSize);
            if (sysctlbyname("hw.cachelinesize", &cacheLineSize, &size, NULL, 0) != 0)
            {
                cacheLineSize = 64; // Fallback
            }
        }
        return cacheLineSize;
    }
};
