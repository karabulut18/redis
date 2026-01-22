#pragma once

#include <stddef.h>

/**
 * container_of - Cast a member of a structure out to the containing structure.
 *
 * @ptr:    The pointer to the member.
 * @type:   The type of the container struct this is embedded in.
 * @member: The name of the member within the struct.
 *
 * This macro is a cornerstone of intrusive data structures (as famously seen in the
 * Linux kernel). It allows you to obtain a pointer to a parent structure when you
 * only have a pointer to one of its fields.
 *
 * Mechanism:
 * 1. It uses offsetof() to determine the byte-offset of 'member' within 'type'.
 * 2. It subtracts this offset from the provided 'ptr' to find the base address
 *    of the containing 'type' instance.
 *
 * Example:
 *     struct MyEntry {
 *         std::string key;
 *         HNode node;
 *     };
 *
 *     void process(HNode* n) {
 *         struct MyEntry* entry = container_of(n, struct MyEntry, node);
 *         // 'entry' now points to the MyEntry instance containing 'n'.
 *     }
 */
#if defined(__GNUC__) || defined(__clang__)
#define container_of(ptr, type, member)                                                                                \
    ({                                                                                                                 \
        const typeof(((type*)0)->member)* __mptr = (ptr);                                                              \
        (type*)((char*)__mptr - offsetof(type, member));                                                               \
    })
#else
#define container_of(ptr, type, member) ((type*)((char*)(ptr)-offsetof(type, member)))
#endif
