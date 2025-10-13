#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>

int main() {
    mount_memory("memorias/memformat.bin");

    start_process(10, "ventas");
    start_process(20, "reportes");
    start_process(30, "analytics");

    printf("\nProcesos activos:\n");
    list_processes();

    finish_process(20);
    printf("\nDespués de eliminar proceso 20:\n");
    list_processes();

    clear_all_processes();
    printf("\nDespués de limpiar todo:\n");
    list_processes();

    return 0;
}
