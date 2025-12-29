#pragma once

#define container_of(ptr, T, member)                                                                                   \
    ({                                                                                                                 \
        const typeof(((T*)0)->member)* __mptr = (ptr);                                                                 \
        (T*)((char*)__mptr - offsetof(T, member));                                                                     \
    })