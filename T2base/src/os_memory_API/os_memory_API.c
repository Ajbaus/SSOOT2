#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "os_memory_API.h"
#include "../osm_File/osm_File.h"

// =============================================================
//  CONSTANTES Y VARIABLES GLOBALES
// =============================================================
#define PCBS_SIZE       (size_t)8192
#define IPT_SIZE        (size_t)196608
#define BITMAP_SIZE     (size_t)8192
#define FRAME_SIZE      (size_t)32768
#define TOTAL_FRAMES    (size_t)65536
#define DATA_SIZE       (FRAME_SIZE * TOTAL_FRAMES)
#define TOTAL_SIZE      (PCBS_SIZE + IPT_SIZE + BITMAP_SIZE + DATA_SIZE)

char* path = NULL;

// =============================================================
//  FUNCIONES INTERNAS DE APOYO
// =============================================================

// Offset dentro del archivo donde comienza el PCB i-ésimo
static long pcb_offset(int index) { return index * 256L; } // Cada entrada mide 256 bytes

// Offset inicial del bitmap
static long bitmap_offset(void) { return (long)(PCBS_SIZE + IPT_SIZE); }

// Busca un proceso por ID. Retorna índice o -1 si no existe
static int find_process_by_id(FILE* f, int process_id) {
    for (int i = 0; i < 32; i++) {
        unsigned char state;
        fseek(f, pcb_offset(i), SEEK_SET);
        fread(&state, 1, 1, f);

        if (state == 0x01) {
            fseek(f, pcb_offset(i) + 15, SEEK_SET); // 1 byte estado + 14 nombre
            unsigned char pid;
            fread(&pid, 1, 1, f);
            if (pid == (unsigned char)process_id)
                return i;
        }
    }
    return -1;
}

// Busca el primer PCB libre
static int find_free_pcb_slot(FILE* f) {
    for (int i = 0; i < 32; i++) {
        unsigned char state;
        fseek(f, pcb_offset(i), SEEK_SET);
        fread(&state, 1, 1, f);
        if (state == 0x00) return i;
    }
    return -1;
}

// Convierte entero de 40 bits LE a u64
static uint64_t u40_le_to_u64(const unsigned char b[5]) {
    return  ((uint64_t)b[0])
          | ((uint64_t)b[1] << 8)
          | ((uint64_t)b[2] << 16)
          | ((uint64_t)b[3] << 24)
          | ((uint64_t)b[4] << 32);
}

// =============================================================
//  BONUS: FORMATEAR MEMORIA
// =============================================================
int format_memory(char* memory_path) {
    FILE* f = fopen(memory_path, "wb");
    if (!f) {
        perror("Error creando archivo de memoria");
        return -1;
    }

    void* empty = calloc(1, TOTAL_SIZE);
    fwrite(empty, 1, TOTAL_SIZE, f);
    free(empty);
    fclose(f);

    printf("Memoria formateada correctamente: %s (%.2f GB)\n",
           memory_path, TOTAL_SIZE / 1024.0 / 1024.0 / 1024.0);
    return 0;
}

// =============================================================
//  FUNCIONES GENERALES
// =============================================================
void mount_memory(char* memory_path) {
    if (path) free(path);
    path = strdup(memory_path);
    printf("Memoria montada en: %s\n", path);
}

int processes_slots() {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Error al abrir memoria.\n");
        return -1;
    }

    int count = 0;
    for (int i = 0; i < 32; i++) {
        unsigned char state;
        fseek(f, pcb_offset(i), SEEK_SET);
        fread(&state, 1, 1, f);
        if (state == 0x00) count++;
    }

    fclose(f);
    return count;
}

void list_processes() {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Error al abrir memoria.\n");
        return;
    }

    printf("\nProcesos activos:\n");
    for (int i = 0; i < 32; i++) {
        unsigned char state;
        fseek(f, pcb_offset(i), SEEK_SET);
        fread(&state, 1, 1, f);

        if (state == 0x01) {
            char name[15] = {0};
            fseek(f, pcb_offset(i) + 1, SEEK_SET);
            fread(name, 1, 14, f);
            unsigned char pid;
            fread(&pid, 1, 1, f);
            printf("PID: %-3d | Nombre: %-14s | Index: %d\n", pid, name, i);
        }
    }
    fclose(f);
}

// =============================================================
//  FUNCIONES PARA PROCESOS
// =============================================================

int start_process(int process_id, char* process_name) {
    FILE* f = fopen(path, "r+b");
    if (!f) {
        printf("Error al abrir memoria para escritura.\n");
        return -1;
    }

    if (find_process_by_id(f, process_id) != -1) {
        printf("Error: ya existe un proceso con ID %d.\n", process_id);
        fclose(f);
        return -1;
    }

    int slot = find_free_pcb_slot(f);
    if (slot == -1) {
        printf("Error: no hay espacio en la tabla de PCBs.\n");
        fclose(f);
        return -1;
    }

    fseek(f, pcb_offset(slot), SEEK_SET);
    unsigned char state = 0x01;
    fwrite(&state, 1, 1, f);

    char name14[14] = {0};
    strncpy(name14, process_name, 14);
    fwrite(name14, 1, 14, f);

    unsigned char pid = (unsigned char)process_id;
    fwrite(&pid, 1, 1, f);

    unsigned char zeros[240] = {0};
    fwrite(zeros, 1, 240, f);

    fclose(f);
    printf("Proceso creado correctamente: PID=%d, Nombre=%s, Slot=%d\n",
           process_id, process_name, slot);
    return 0;
}

int finish_process(int process_id) {
    FILE* f = fopen(path, "r+b");
    if (!f) {
        printf("Error al abrir memoria.\n");
        return -1;
    }

    int idx = find_process_by_id(f, process_id);
    if (idx == -1) {
        printf("No existe proceso con ID %d.\n", process_id);
        fclose(f);
        return -1;
    }

    fseek(f, pcb_offset(idx), SEEK_SET);
    unsigned char zero[256] = {0};
    fwrite(zero, 1, 256, f);
    fclose(f);

    printf("Proceso %d eliminado del slot %d.\n", process_id, idx);
    return 0;
}

int clear_all_processes() {
    FILE* f = fopen(path, "r+b");
    if (!f) {
        printf("Error al abrir memoria.\n");
        return -1;
    }

    int terminated = 0;
    for (int i = 0; i < 32; i++) {
        unsigned char state;
        fseek(f, pcb_offset(i), SEEK_SET);
        fread(&state, 1, 1, f);

        if (state == 0x01) {
            fseek(f, pcb_offset(i), SEEK_SET);
            unsigned char zero[256] = {0};
            fwrite(zero, 1, 256, f);
            terminated++;
        }
    }

    fclose(f);
    printf("clear_all_processes(): %d procesos eliminados.\n", terminated);
    return terminated;
}

int file_table_slots(int process_id) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Error al abrir memoria.\n");
        return -1;
    }

    int idx = find_process_by_id(f, process_id);
    if (idx == -1) {
        fclose(f);
        return -1;
    }

    int libres = 0;
    fseek(f, pcb_offset(idx) + 16, SEEK_SET); // Estado + nombre + id = 16 bytes

    for (int i = 0; i < 10; i++) {
        unsigned char valid;
        fread(&valid, 1, 1, f);
        fseek(f, 23, SEEK_CUR); // Saltar al siguiente (24 bytes por archivo)
        if (valid == 0x00) libres++;
    }

    fclose(f);
    return libres;
}

// =============================================================
//  LISTADO DE ARCHIVOS & BITMAP
// =============================================================
void list_files(int process_id) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("Error al abrir memoria.\n"); return; }

    int pcb_index = find_process_by_id(f, process_id);
    if (pcb_index == -1) {
        printf("Proceso %d no encontrado.\n", process_id);
        fclose(f);
        return;
    }

    long tabla_offset = pcb_offset(pcb_index) + 16; // 1 estado +14 nombre +1 id

    for (int i = 0; i < 10; i++) {
        long entry = tabla_offset + i * 24;

        unsigned char valid = 0;
        fseek(f, entry, SEEK_SET);
        fread(&valid, 1, 1, f);
        if (valid != 0x01) continue;

        char name[15] = {0};
        fread(name, 1, 14, f);

        unsigned char bsize[5] = {0};
        fread(bsize, 1, 5, f);
        uint64_t size = u40_le_to_u64(bsize);

        uint32_t vaddr = 0;
        fread(&vaddr, 1, 4, f);

        uint16_t vpn = (uint16_t)((vaddr >> 15) & 0x0FFF);

        // Formato: <VPN HEX> <FILE SIZE DEC> <DIRECCION VIRTUAL HEX> <FILE NAME>
        printf("0x%X %llu 0x%08X %s\n",
               vpn,
               (unsigned long long)size,
               vaddr,
               name);
    }

    fclose(f);
}

void frame_bitmap_status(void) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("Error al abrir memoria.\n"); return; }

    unsigned char buf[BITMAP_SIZE];
    fseek(f, bitmap_offset(), SEEK_SET);
    size_t n = fread(buf, 1, BITMAP_SIZE, f);
    fclose(f);
    if (n != BITMAP_SIZE) {
        printf("Error leyendo bitmap.\n");
        return;
    }

    unsigned used = 0;
    for (size_t bit = 0; bit < TOTAL_FRAMES; ++bit) {
        size_t byte_i = bit >> 3;
        unsigned bit_i = bit & 7;
        unsigned is_set = (buf[byte_i] >> bit_i) & 1u;
        used += is_set;
    }
    unsigned freec = (unsigned)TOTAL_FRAMES - used;

    printf("USADOS: %u LIBRES: %u\n", used, freec);
}

// =============================================================
//  FUNCIONES PARA ARCHIVOS (simples)
//  (Estas no mapean páginas; solo demuestran flujo I/O.)
// =============================================================

osmFile* open_file(int process_id, char* file_name, char mode) {
    FILE* f = fopen(path, "r+b");
    if (!f) {
        printf("Error al abrir memoria.\n");
        return NULL;
    }

    // Buscar proceso
    int pcb_index = -1;
    for (int i = 0; i < 32; i++) {
        unsigned char state;
        fseek(f, pcb_offset(i), SEEK_SET);
        fread(&state, 1, 1, f);
        if (state == 0x01) {
            fseek(f, pcb_offset(i) + 15, SEEK_SET);
            unsigned char pid;
            fread(&pid, 1, 1, f);
            if (pid == (unsigned char)process_id) {
                pcb_index = i;
                break;
            }
        }
    }
    if (pcb_index == -1) {
        printf("Error: proceso %d no encontrado.\n", process_id);
        fclose(f);
        return NULL;
    }

    long tabla_offset = pcb_offset(pcb_index) + 16; // Estado+nombre+id

    // Buscar archivo existente
    for (int i = 0; i < 10; i++) {
        long entry = tabla_offset + i * 24;
        unsigned char valid;
        fseek(f, entry, SEEK_SET);
        fread(&valid, 1, 1, f);
        if (valid == 0x01) {
            char name[15] = {0};
            fread(name, 1, 14, f);
            if (strcmp(name, file_name) == 0) {
                if (mode == 'r') {
                    fclose(f);
                    osmFile* desc = (osmFile*)malloc(sizeof(osmFile));
                    desc->process_id = process_id;
                    strncpy(desc->file_name, file_name, 14);
                    desc->file_name[14] = '\0';
                    desc->mode = mode;
                    desc->size = 0;
                    desc->virtual_address = 0;
                    printf("Archivo '%s' abierto en modo lectura.\n", file_name);
                    return desc;
                } else {
                    printf("Error: archivo '%s' ya existe.\n", file_name);
                    fclose(f);
                    return NULL;
                }
            }
        }
    }

    // Si modo escritura: crear nuevo archivo
    if (mode == 'w') {
        for (int i = 0; i < 10; i++) {
            long entry = tabla_offset + i * 24;
            unsigned char valid;
            fseek(f, entry, SEEK_SET);
            fread(&valid, 1, 1, f);
            if (valid == 0x00) {
                fseek(f, entry, SEEK_SET);
                unsigned char valid_bit = 0x01;
                fwrite(&valid_bit, 1, 1, f);
                char name14[14] = {0};
                strncpy(name14, file_name, 14);
                fwrite(name14, 1, 14, f);
                unsigned char size40[5] = {0};
                fwrite(size40, 1, 5, f); // tamaño inicial 0
                unsigned char addr[4] = {0};
                fwrite(addr, 1, 4, f);
                fclose(f);

                osmFile* desc = (osmFile*)malloc(sizeof(osmFile));
                desc->process_id = process_id;
                strncpy(desc->file_name, file_name, 14);
                desc->file_name[14] = '\0';
                desc->mode = mode;
                desc->size = 0;
                desc->virtual_address = 0;
                printf("Archivo '%s' creado para proceso %d.\n", file_name, process_id);
                return desc;
            }
        }
        printf("Error: tabla de archivos llena.\n");
    }

    fclose(f);
    return NULL;
}

int write_file(osmFile* file_desc, char* src) {
    if (!file_desc || file_desc->mode != 'w') {
        printf("Error: descriptor inválido o modo incorrecto.\n");
        return -1;
    }

    FILE* f = fopen(path, "r+b");
    FILE* local = fopen(src, "rb");
    if (!f || !local) {
        printf("Error al abrir archivos.\n");
        if (f) fclose(f);
        if (local) fclose(local);
        return -1;
    }

    size_t offset_data = PCBS_SIZE + IPT_SIZE + BITMAP_SIZE;
    fseek(f, offset_data, SEEK_SET);

    char buffer[4096];
    size_t total = 0, n;
    while ((n = fread(buffer, 1, sizeof(buffer), local)) > 0) {
        fwrite(buffer, 1, n, f);
        total += n;
    }

    fclose(f);
    fclose(local);

    printf("Archivo '%s' escrito en memoria (%zu bytes).\n", file_desc->file_name, total);
    return (int)total;
}

int read_file(osmFile* file_desc, char* dest) {
    if (!file_desc || file_desc->mode != 'r') {
        printf("Error: descriptor inválido o modo incorrecto.\n");
        return -1;
    }

    FILE* f = fopen(path, "rb");
    FILE* local = fopen(dest, "wb");
    if (!f || !local) {
        printf("Error al abrir archivos.\n");
        if (f) fclose(f);
        if (local) fclose(local);
        return -1;
    }

    size_t offset_data = PCBS_SIZE + IPT_SIZE + BITMAP_SIZE;
    fseek(f, offset_data, SEEK_SET);

    char buffer[4096];
    size_t total = 0, n;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0 && total < 4096 * 10) {
        fwrite(buffer, 1, n, local);
        total += n;
    }

    fclose(f);
    fclose(local);

    printf("Archivo '%s' leído (%zu bytes copiados a %s).\n", file_desc->file_name, total, dest);
    return (int)total;
}

void delete_file(int process_id, char* file_name) {
    FILE* f = fopen(path, "r+b");
    if (!f) {
        printf("Error al abrir memoria.\n");
        return;
    }

    int pcb_index = -1;
    for (int i = 0; i < 32; i++) {
        unsigned char state;
        fseek(f, pcb_offset(i), SEEK_SET);
        fread(&state, 1, 1, f);
        if (state == 0x01) {
            fseek(f, pcb_offset(i) + 15, SEEK_SET);
            unsigned char pid;
            fread(&pid, 1, 1, f);
            if (pid == (unsigned char)process_id) { pcb_index = i; break; }
        }
    }
    if (pcb_index == -1) { fclose(f); printf("Proceso %d no encontrado.\n", process_id); return; }

    long tabla_offset = pcb_offset(pcb_index) + 16;
    for (int i = 0; i < 10; i++) {
        long entry = tabla_offset + i * 24;
        unsigned char valid;
        fseek(f, entry, SEEK_SET);
        fread(&valid, 1, 1, f);
        if (valid == 0x01) {
            char name[15] = {0};
            fread(name, 1, 14, f);
            if (strcmp(name, file_name) == 0) {
                fseek(f, entry, SEEK_SET);
                unsigned char zero[24] = {0};
                fwrite(zero, 1, 24, f);
                fclose(f);
                printf("Archivo '%s' eliminado del proceso %d.\n", file_name, process_id);
                return;
            }
        }
    }

    fclose(f);
    printf("Archivo '%s' no encontrado en proceso %d.\n", file_name, process_id);
}

void close_file(osmFile* file_desc) {
    if (file_desc) {
        printf("Archivo '%s' cerrado.\n", file_desc->file_name);
        free(file_desc);
    }
}
