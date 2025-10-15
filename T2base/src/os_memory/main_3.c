#ifdef MAIN_TEST_3
#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>
#include <string.h>

__attribute__((unused))
static const char* default_mem = "memorias/memformat.bin";

__attribute__((unused))
static void ensure_local_src(void) {
    FILE* f = fopen("archivo_local.txt", "wb");
    if (!f) return;
    for (int i = 0; i < 128; ++i)
        fprintf(f, "linea %03d: hola, memoria paginada!\n", i);
    fclose(f);
}

int main(int argc, char const* argv[]) {
    const char* mempath = (argc > 1) ? argv[1] : default_mem;
    int do_format = !(argc > 2 && strcmp(argv[2], "--no-format") == 0);

    printf("== TEST 3: I/O + Bitmap ==\n");
    ensure_local_src();

    mount_memory((char*)mempath);
    if (do_format) format_memory((char*)mempath);

    start_process(10, "ventas");

    osmFile* f1 = open_file(10, "log.txt", 'w');
    if (f1) { write_file(f1, "archivo_local.txt"); close_file(f1); }

    list_files(10);
    frame_bitmap_status();

    osmFile* f2 = open_file(10, "log.txt", 'r');
    if (f2) { read_file(f2, "copia_local.txt"); close_file(f2); }

    delete_file(10, "log.txt");
    list_files(10);
    frame_bitmap_status();

    clear_all_processes();
    return 0;
}
#endif
