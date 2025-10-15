#ifdef MAIN_TEST_4
#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>
#include <string.h>

__attribute__((unused))
static const char* default_mem = "memorias/memformat.bin";

__attribute__((unused))
static void seed_src(const char* fname, int lines, const char* tag) {
    FILE* f = fopen(fname, "wb");
    if (!f) return;
    for (int i = 0; i < lines; ++i)
        fprintf(f, "[%s] linea %04d\n", tag, i);
    fclose(f);
}

int main(int argc, char const* argv[]) {
    const char* mempath = (argc > 1) ? argv[1] : default_mem;
    int do_format = !(argc > 2 && strcmp(argv[2], "--no-format") == 0);

    printf("== TEST 4: End-to-end ==\n");
    seed_src("a.txt", 256, "A");
    seed_src("b.txt", 128, "B");

    mount_memory((char*)mempath);
    if (do_format) format_memory((char*)mempath);

    start_process(10, "ventas");
    start_process(20, "reportes");
    start_process(30, "analytics");
    list_processes();

    osmFile* f10w = open_file(10, "log_a.txt", 'w');
    if (f10w) { write_file(f10w, "a.txt"); close_file(f10w); }

    osmFile* f10r = open_file(10, "log_a.txt", 'r');
    if (f10r) { read_file(f10r, "a_copia.txt"); close_file(f10r); }

    osmFile* f20w = open_file(20, "log_b.txt", 'w');
    if (f20w) { write_file(f20w, "b.txt"); close_file(f20w); }

    list_files(10);
    list_files(20);
    frame_bitmap_status();

    delete_file(10, "log_a.txt");
    delete_file(20, "log_b.txt");
    frame_bitmap_status();

    finish_process(30);
    list_processes();

    int cerrados = clear_all_processes();
    printf("Procesos cerrados: %d\n", cerrados);
    frame_bitmap_status();
    return 0;
}
#endif
