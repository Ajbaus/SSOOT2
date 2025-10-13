#pragma once
#ifndef OSM_FILE_H
#define OSM_FILE_H

typedef struct {
    int process_id;
    char file_name[15];
    char mode;
    int size;
    int virtual_address;
} osmFile;

#endif
