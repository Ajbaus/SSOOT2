#pragma once
#ifndef OSM_FILE_H
#define OSM_FILE_H
#include <stdint.h>

typedef struct {
    int      process_id;
    char     file_name[15];   /* 14 + '\0' */
    char     mode;            /* 'r' o 'w' */
    uint64_t size;            /* tamaño del archivo */
    uint32_t vaddr;           /* vaddr inicial */
} osmFile;

#endif
