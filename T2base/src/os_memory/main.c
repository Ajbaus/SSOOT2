#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>

int main(int argc, char const *argv[]) {
    // Ruta del archivo de memoria simulada
    char *mempath = argc > 1 ? (char*)argv[1] : "memorias/memformat.bin";

    // -----------------------------------------------------
    // 1️⃣ Montar y formatear memoria
    // -----------------------------------------------------
    printf("== MONTAJE Y FORMATEO ==\n");
    mount_memory(mempath);
    format_memory(mempath);

    // -----------------------------------------------------
    // 2️⃣ Procesos
    // -----------------------------------------------------
    printf("\n== CREAR Y LISTAR PROCESOS ==\n");
    start_process(10, "ventas");
    start_process(20, "reportes");
    start_process(30, "analytics");
    list_processes();
    printf("Slots libres: %d\n", processes_slots());

    // -----------------------------------------------------
    // 3️⃣ Terminar y limpiar
    // -----------------------------------------------------
    finish_process(20);
    printf("\n== Después de eliminar proceso 20 ==\n");
    list_processes();
    clear_all_processes();
    printf("\n== Después de limpiar todo ==\n");
    list_processes();

    // -----------------------------------------------------
    // 4️⃣ Recrear proceso y probar archivos
    // -----------------------------------------------------
    printf("\n== PRUEBA DE ARCHIVOS ==\n");
    start_process(10, "ventas");

    osmFile* f1 = open_file(10, "log.txt", 'w');
    if (f1) {
        write_file(f1, "archivo_local.txt");
        close_file(f1);
    }

    osmFile* f2 = open_file(10, "log.txt", 'r');
    if (f2) {
        read_file(f2, "copia_local.txt");
        close_file(f2);
    }

    delete_file(10, "log.txt");

    // -----------------------------------------------------
    // 5️⃣ Info final de slots y limpieza
    // -----------------------------------------------------
    printf("\n== INFORME FINAL ==\n");
    printf("Slots libres: %d\n", processes_slots());
    clear_all_processes();

    return 0;
}
