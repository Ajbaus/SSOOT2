#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>

int main() {
    mount_memory("memorias/memformat.bin");
    start_process(10, "ventas");

    osmFile* f1 = open_file(10, "log.txt", 'w');
    write_file(f1, "archivo_local.txt");
    close_file(f1);

    osmFile* f2 = open_file(10, "log.txt", 'r');
    read_file(f2, "copia_local.txt");
    close_file(f2);

    delete_file(10, "log.txt");
    finish_process(10);
    return 0;
}
