#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "os_memory_API.h"
#include "../osm_File/osm_File.h"

#define PCBS_SIZE       (size_t)8192
#define IPT_SIZE        (size_t)196608
#define BITMAP_SIZE     (size_t)8192
#define FRAME_SIZE      (size_t)32768
#define TOTAL_FRAMES    (size_t)65536
#define DATA_SIZE       (FRAME_SIZE * TOTAL_FRAMES)
#define TOTAL_SIZE      (PCBS_SIZE + IPT_SIZE + BITMAP_SIZE + DATA_SIZE)

char* path = NULL;

static long pcb_offset(int index) {
    return index * 256;
}

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

void mount_memory(char* memory_path) {
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


// Crear un proceso en la tabla de PCBs
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

// Terminar un proceso y liberar su PCB
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

// Retorna los slots libres en la tabla de archivos de un proceso
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

    // Dirección de inicio de la tabla de archivos del proceso
    long tabla_offset = pcb_offset(pcb_index) + 16; // 1 estado +14 nombre +1 id

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
                    osmFile* desc = malloc(sizeof(osmFile));
                    desc->process_id = process_id;
                    strncpy(desc->file_name, file_name, 14);
                    desc->mode = mode;
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

    // Si es modo escritura: crear nuevo archivo
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

                unsigned char size[5] = {0};
                fwrite(size, 1, 5, f); // tamaño inicial 0

                unsigned char addr[4] = {0};
                fwrite(addr, 1, 4, f);
                fclose(f);

                osmFile* desc = malloc(sizeof(osmFile));
                desc->process_id = process_id;
                strncpy(desc->file_name, file_name, 14);
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

    fseek(local, 0, SEEK_END);
    size_t size = ftell(local);
    rewind(local);

    size_t offset_data = PCBS_SIZE + IPT_SIZE + BITMAP_SIZE;
    fseek(f, offset_data, SEEK_SET);

    char buffer[4096];
    size_t total = 0;
    size_t n;
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
    size_t total = 0;
    size_t n;
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
        fclose(f);
        printf("Proceso %d no encontrado.\n", process_id);
        return;
    }

    // Una vez que encuentra el proceso: Buscar archivo y borrarlo
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
