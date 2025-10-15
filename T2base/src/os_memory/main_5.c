#ifdef MAIN_TEST_5
#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

__attribute__((unused))
static const char* default_mem = "memorias/memformat.bin";

static int create_local_file(const char* name, size_t bytes, char fill) {
    FILE* f = fopen(name, "wb");
    if(!f) return -1;
    size_t chunk = 4096;
    char buf[4096];
    memset(buf, fill, sizeof(buf));
    size_t left = bytes;
    while(left){
        size_t w = left>chunk?chunk:left;
        if(fwrite(buf,1,w,f)!=w){ fclose(f); return -1; }
        left -= w;
    }
    fclose(f);
    return 0;
}

static int compare_files(const char* a, const char* b) {
    FILE* fa = fopen(a,"rb");
    FILE* fb = fopen(b,"rb");
    if(!fa || !fb){ if(fa)fclose(fa); if(fb)fclose(fb); return -1; }
    int r = 0;
    char ba[4096], bb[4096];
    while(1){
        size_t na = fread(ba,1,sizeof(ba),fa);
        size_t nb = fread(bb,1,sizeof(bb),fb);
        if(na!=nb || memcmp(ba,bb,na)!=0){ r=1; break; }
        if(na==0) break;
    }
    fclose(fa); fclose(fb);
    return r; /* 0 == equal, 1 == different */
}

int main(int argc, char** argv){
    const char* mempath = (argc>1)? argv[1]: default_mem;
    int do_format = 1;
    if(argc>2 && strcmp(argv[2],"--no-format")==0) do_format = 0;

    printf("=== MAIN_TEST_5 (stress end-to-end) ===\n");
    mount_memory((char*)mempath);
    if(do_format) {
        printf("Formateando (puede tardar un poco)...\n");
        if(format_memory((char*)mempath)!=0){ printf("format failed\n"); return 1; }
    }

    int pid = 123;
    if(start_process(pid, "tester")!=0){ printf("start_process failed\n"); return 1; }

    /* crear archivos locales: small(1KB), med(100KB), big(300KB) */
    create_local_file("local_small.bin", 1024, 'S');
    create_local_file("local_med.bin", 100*1024, 'M');
    create_local_file("local_big.bin", 300*1024, 'B');

    /* abrir y escribir */
    osmFile* fs = open_file(pid, "small.bin", 'w');
    write_file(fs, "local_small.bin");
    close_file(fs);

    osmFile* fm = open_file(pid, "med.bin", 'w');
    write_file(fm, "local_med.bin");
    close_file(fm);

    osmFile* fb = open_file(pid, "big.bin", 'w');
    write_file(fb, "local_big.bin");
    close_file(fb);

    printf("\n-- listado de archivos (PID %d) --\n", pid);
    list_files(pid);

    printf("\n-- bitmap (despues de escritura) --\n");
    frame_bitmap_status();

    /* leer a archivos copia_*.bin */
    osmFile* rs = open_file(pid, "small.bin", 'r');
    read_file(rs, "copy_small.bin");
    close_file(rs);

    osmFile* rm = open_file(pid, "med.bin", 'r');
    read_file(rm, "copy_med.bin");
    close_file(rm);

    osmFile* rb = open_file(pid, "big.bin", 'r');
    read_file(rb, "copy_big.bin");
    close_file(rb);

    /* comparar */
    printf("\n-- comparar archivos originales vs copias --\n");
    printf("small equal? %s\n", compare_files("local_small.bin","copy_small.bin")==0 ? "YES":"NO");
    printf("med   equal? %s\n", compare_files("local_med.bin",  "copy_med.bin")==0   ? "YES":"NO");
    printf("big   equal? %s\n", compare_files("local_big.bin",  "copy_big.bin")==0   ? "YES":"NO");

    /* borrar archivos y verificar bitmap baja */
    delete_file(pid, "small.bin");
    delete_file(pid, "med.bin");
    delete_file(pid, "big.bin");

    printf("\n-- bitmap (despues de borrado) --\n");
    frame_bitmap_status();

    /* terminar proceso */
    finish_process(pid);
    printf("\n-- bitmap (despues finish_process) --\n");
    frame_bitmap_status();

    /* limpieza de ficheros locales de prueba */
    unlink("local_small.bin"); unlink("local_med.bin"); unlink("local_big.bin");
    unlink("copy_small.bin");  unlink("copy_med.bin");  unlink("copy_big.bin");

    printf("\n=== TEST 5 terminado ===\n");
    return 0;
}
#endif
