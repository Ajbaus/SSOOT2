#pragma once
#include "../osm_File/osm_File.h"

/* ====== FUNCIONES GENERALES ====== */

// void mount_memory(char* memory_path);

// void list_processes();

// int processes_slots();

// void list_files(int process_id);

// void frame_bitmap_status();



/* ====== FUNCIONES PARA PROCESOS ====== */

// int start_process(int process_id, char* process_name);

// int finish_process(int process_id);

// int clear_all_processes();

// int file_table_slots(int process_id);


/* ====== FUNCIONES PARA ARCHIVOS ====== */

// osmFile* open_file(int process_id, char* file_name, char mode);

// int read_file(osmFile* file desc, char* dest);

// int write_file(osmFile* file desc, char* src);

// void delete_file(int process id, char* file name);

// void close_file(osmFile* file_desc);


/*====== BONUS =====*/

// int format_memory(char* memory path);
