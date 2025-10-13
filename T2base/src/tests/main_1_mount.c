#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>

int main() {
    mount_memory("memorias/memformat.bin");
    format_memory("memorias/memformat.bin");
    return 0;
}
