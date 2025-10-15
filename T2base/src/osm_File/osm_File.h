#pragma once
#ifndef OSM_FILE_H
#define OSM_FILE_H

typedef struct {
    int  process_id;
    char file_name[15];   // 14 + '\0' para uso local
    char mode;            // 'r' o 'w'
    int  size;            // solo informativo en esta base
    int  virtual_address; // vaddr inicial (por ahora 0 en la base)
} osmFile;

#endif
