#ifdef MAIN_TEST_1
#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>
#include <string.h>

__attribute__((unused))
static const char* default_mem = "memorias/memformat.bin";

int main(int argc, char const* argv[]) {
    const char* mempath = (argc > 1) ? argv[1] : default_mem;
    int do_format = !(argc > 2 && strcmp(argv[2], "--no-format") == 0);

    printf("== TEST 1: Montaje + PCBs ==\n");
    mount_memory((char*)mempath);
    if (do_format) format_memory((char*)mempath);

    start_process(10, "ventas");
    start_process(20, "reportes");
    start_process(30, "analytics");

    list_processes();
    printf("PCB slots libres: %d\n", processes_slots());

    printf("\n-- finish_process(20)\n");
    finish_process(20);
    list_processes();

    printf("\n-- clear_all_processes()\n");
    clear_all_processes();
    list_processes();
    return 0;
}
#endif
