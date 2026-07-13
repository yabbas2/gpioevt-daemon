#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

struct struct_data {
    int64_t timestamp;
    unsigned char pin;
    unsigned char level;
} __attribute__((packed));

#define DATA_ENTRY_SIZE sizeof(struct struct_data)

typedef union data_entry {
    struct struct_data deserialized;
    char serialized[DATA_ENTRY_SIZE];
} data_entry_t;

#endif
