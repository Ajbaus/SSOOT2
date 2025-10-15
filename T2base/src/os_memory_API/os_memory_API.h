#pragma once
#include <stdint.h>
#include <stddef.h>   // necesario para size_t

/* =================== Layout fijo del archivo =================== */
#define PCBS_SIZE       ((size_t)8192)     /* 32 PCBs x 256B */
#define IPT_SIZE        ((size_t)196608)   /* 65536 entradas x 3B */
#define BITMAP_SIZE     ((size_t)8192)     /* 65536 bits = 8KiB */
#define FRAME_SIZE      ((size_t)32768)    /* 32 KiB = 2^15 */
#define TOTAL_FRAMES    ((size_t)65536)    /* 2 GiB / 32 KiB */
#define DATA_OFFSET     (PCBS_SIZE + IPT_SIZE + BITMAP_SIZE)

/* =================== Parámetros de tablas =================== */
#define PCB_COUNT       32
#define PCB_SIZE        256
#define PROC_NAME_LEN   14
#define FILES_PER_PROC  10
#define FILE_ENTRY_SIZE 24
#define FILE_NAME_LEN   14

/* =================== Estructura de descriptor =================== */
typedef struct {
    int  process_id;
    char file_name[15];   /* 14 + '\0' para uso interno */
    char mode;            /* 'r' o 'w' */
    uint64_t size;        /* informativo (se lee de la tabla) */
    uint32_t vaddr;       /* vaddr inicial (se lee de la tabla) */
} osmFile;

/* =================== API general =================== */
void mount_memory(char* memory_path);
int  format_memory(char* memory_path);  /* Bonus del enunciado */
void list_processes(void);
int  processes_slots(void);
void list_files(int process_id);
void frame_bitmap_status(void);

/* =================== API procesos =================== */
int  start_process(int process_id, char* process_name);
int  finish_process(int process_id);
int  clear_all_processes(void);
int  file_table_slots(int process_id);

/* =================== API archivos =================== */
osmFile* open_file(int process_id, char* file_name, char mode);
int      write_file(osmFile* file_desc, char* src_path);
int      read_file(osmFile* file_desc, char* dest_path);
void     delete_file(int process_id, char* file_name);
void     close_file(osmFile* file_desc);

/* =================== Helpers direcciones =================== */
static inline uint16_t vpn_of(uint32_t vaddr)   { return (uint16_t)((vaddr >> 15) & 0x0FFF); }
static inline uint16_t off_of(uint32_t vaddr)   { return (uint16_t)(vaddr & 0x7FFF); }
static inline uint64_t paddr_rel(uint16_t pfn, uint16_t off) { return (((uint64_t)pfn)<<15) | off; }
static inline uint64_t paddr_abs(uint16_t pfn, uint16_t off) { return (uint64_t)DATA_OFFSET + paddr_rel(pfn, off); }
