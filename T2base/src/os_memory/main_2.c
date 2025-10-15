#ifdef MAIN_TEST_2
#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>
#include <string.h>

__attribute__((unused))
static const char* default_mem = "memorias/memformat.bin";

int main(int argc, char const* argv[]) {
    const char* mempath = (argc > 1) ? argv[1] : default_mem;
    int do_format = !(argc > 2 && strcmp(argv[2], "--no-format") == 0);

    printf("== TEST 2: Tabla de archivos por proceso ==\n");
    mount_memory((char*)mempath);
    if (do_format) format_memory((char*)mempath);

    start_process(10, "ventas");
    printf("Slots de archivos libres (pid=10): %d\n", file_table_slots(10));

    osmFile* fw = open_file(10, "log.txt", 'w');
    if (fw) close_file(fw);

    list_files(10);
    printf("Slots de archivos libres (pid=10): %d\n", file_table_slots(10));

    osmFile* fr = open_file(10, "log.txt", 'r');
    if (fr) close_file(fr);

    delete_file(10, "log.txt");
    list_files(10);
    printf("Slots de archivos libres (pid=10): %d\n", file_table_slots(10));

    clear_all_processes();
    return 0;
}
#endif
