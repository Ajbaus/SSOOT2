#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>

int main(int argc, char const *argv[]) {
    char *mempath = argc > 1 ? (char*)argv[1] : "memorias/memformat.bin";
    mount_memory(mempath);
    start_process(10, "ventas");

    printf("\n== CREAR ARCHIVO Y ESCRIBIR ==\n");
    osmFile* f1 = open_file(10, "log.txt", 'w');
    if (f1) {
        write_file(f1, "archivo_local.txt");
        close_file(f1);
    }

    printf("\n== LEER ARCHIVO DESDE MEMORIA ==\n");
    osmFile* f2 = open_file(10, "log.txt", 'r');
    if (f2) {
        read_file(f2, "copia_local.txt");
        close_file(f2);
    }

    printf("\n== ELIMINAR ARCHIVO ==\n");
    delete_file(10, "log.txt");

    finish_process(10);
    return 0;
}
