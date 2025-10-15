#define _GNU_SOURCE
#include "os_memory_API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ============================ Estado global ============================ */
static char* g_path = NULL;

/* ============================ Structs on-disk ============================ */
/* PCB: 256B => [1 valid][14 name][1 pid][10 file entries * 24B] */
typedef struct __attribute__((packed)) {
    uint8_t valid;                 /* 0 libre / 1 ocupado */
    char    name[PROC_NAME_LEN];   /* sin '\0' */
    uint8_t pid;
    uint8_t file_table[FILES_PER_PROC * FILE_ENTRY_SIZE];
} PCBEntryDisk;

typedef struct __attribute__((packed)) {
    uint8_t  valid;                /* 0 libre / 1 ocupado */
    char     name[FILE_NAME_LEN];  /* sin '\0' */
    uint8_t  size40[5];            /* little-endian (40 bits) */
    uint32_t vaddr;                /* vaddr (VPN<<15 | offset) */
} FileEntryDisk;

/* ============================ Utilidades básicas ============================ */
static long pcb_off(int idx) { return (long)(idx * PCB_SIZE); }
static long ipt_off(int pfn) { return (long)(PCBS_SIZE + (pfn * 3)); }
static long bmp_off(void)    { return (long)(PCBS_SIZE + IPT_SIZE);  }

static uint64_t u40_read(const uint8_t b[5]) {
    return (uint64_t)b[0] | ((uint64_t)b[1]<<8) | ((uint64_t)b[2]<<16)
         | ((uint64_t)b[3]<<24) | ((uint64_t)b[4]<<32);
}
static void u40_write(uint8_t b[5], uint64_t v) {
    b[0] = (uint8_t)(v      );
    b[1] = (uint8_t)(v >>  8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
    b[4] = (uint8_t)(v >> 32);
}

/* open file helpers */
static FILE* open_ro(void){ return g_path ? fopen(g_path,"rb") : NULL; }
static FILE* open_rw(void){
    if(!g_path) return NULL;
    FILE* f=fopen(g_path,"r+b");
    if(!f) f=fopen(g_path,"rb+");
    return f;
}

/* ============================ PCB helpers ============================ */
static int pcb_read(FILE* f, int idx, PCBEntryDisk* out){
    if(idx<0||idx>=PCB_COUNT) return -1;
    fseek(f, pcb_off(idx), SEEK_SET);
    return (fread(out,1,sizeof(*out),f)==sizeof(*out))?0:-1;
}
static int pcb_write(FILE* f, int idx, const PCBEntryDisk* in){
    if(idx<0||idx>=PCB_COUNT) return -1;
    fseek(f, pcb_off(idx), SEEK_SET);
    return (fwrite(in,1,sizeof(*in),f)==sizeof(*in))?0:-1;
}
static int pcb_find_by_pid(FILE* f, int pid){
    for(int i=0;i<PCB_COUNT;i++){
        PCBEntryDisk pcb; if(pcb_read(f,i,&pcb)!=0) return -1;
        if(pcb.valid==1 && pcb.pid==(uint8_t)pid) return i;
    }
    return -1;
}
static int pcb_first_free(FILE* f){
    for(int i=0;i<PCB_COUNT;i++){
        PCBEntryDisk pcb; if(pcb_read(f,i,&pcb)!=0) return -1;
        if(pcb.valid!=1) return i;
    }
    return -1;
}

/* ============================ FileEntry helpers ============================ */
static int fe_read(const PCBEntryDisk* pcb, int fe_idx, FileEntryDisk* out){
    if(fe_idx<0||fe_idx>=FILES_PER_PROC) return -1;
    memcpy(out, &pcb->file_table[fe_idx*FILE_ENTRY_SIZE], FILE_ENTRY_SIZE);
    return 0;
}
static int fe_write(PCBEntryDisk* pcb, int fe_idx, const FileEntryDisk* in){
    if(fe_idx<0||fe_idx>=FILES_PER_PROC) return -1;
    memcpy(&pcb->file_table[fe_idx*FILE_ENTRY_SIZE], in, FILE_ENTRY_SIZE);
    return 0;
}
static int fe_find_name(const PCBEntryDisk* pcb, const char* name){
    for(int i=0;i<FILES_PER_PROC;i++){
        FileEntryDisk fe; fe_read(pcb,i,&fe);
        if(fe.valid==1){
            char n[FILE_NAME_LEN+1]={0};
            memcpy(n,fe.name,FILE_NAME_LEN);
            if(strcmp(n,name)==0) return i;
        }
    }
    return -1;
}
static int fe_first_free(const PCBEntryDisk* pcb){
    for(int i=0;i<FILES_PER_PROC;i++){
        FileEntryDisk fe; fe_read(pcb,i,&fe);
        if(fe.valid!=1) return i;
    }
    return -1;
}

/* ============================ BITMAP helpers ============================ */
static int bmp_get(FILE* f, uint32_t bit){
    long off = bmp_off() + (bit>>3);
    fseek(f, off, SEEK_SET);
    int c = fgetc(f);
    if(c==EOF) return -1;
    return (c >> (bit & 7)) & 1;
}
static int bmp_set(FILE* f, uint32_t bit, int val){
    long off = bmp_off() + (bit>>3);
    fseek(f, off, SEEK_SET);
    int c = fgetc(f);
    if(c==EOF) return -1;
    if(val) c |=  (1<<(bit&7));
    else    c &= ~(1<<(bit&7));
    fseek(f, off, SEEK_SET);
    return (fputc(c,f)==EOF)?-1:0;
}
static int bmp_first_free(FILE* f){
    for(uint32_t i=0;i<TOTAL_FRAMES;i++){
        int b=bmp_get(f,i); if(b<0) return -1; if(!b) return (int)i;
    }
    return -1;
}

/* ============================ IPT helpers (3B por entrada) ============================ */
/* Layout 24b: [bit0 valid][bits1..10 pid(10)][bits11..23 vpn(13)]  (LE) */
static int ipt_get(FILE* f, int pfn, int* valid, uint16_t* pid, uint16_t* vpn){
    fseek(f, ipt_off(pfn), SEEK_SET);
    uint8_t b[3]; if(fread(b,1,3,f)!=3) return -1;
    uint32_t raw = (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16);
    *valid = raw & 1u;
    *pid   = (raw>>1) & 0x3FFu;
    *vpn   = (raw>>11)& 0x1FFFu;
    return 0;
}
static int ipt_set(FILE* f, int pfn, int valid, uint16_t pid, uint16_t vpn){
    uint32_t raw = (valid&1) | (((uint32_t)pid&0x3FF)<<1) | (((uint32_t)vpn&0x1FFF)<<11);
    uint8_t b[3] = { raw&0xFF, (raw>>8)&0xFF, (raw>>16)&0xFF };
    fseek(f, ipt_off(pfn), SEEK_SET);
    return (fwrite(b,1,3,f)==3)?0:-1;
}
static int ipt_find_pfn(FILE* f, uint16_t pid, uint16_t vpn){
    for(int pfn=0;pfn<(int)TOTAL_FRAMES;pfn++){
        int v; uint16_t p,vp;
        if(ipt_get(f,pfn,&v,&p,&vp)!=0) return -1;
        if(v && p==pid && vp==vpn) return pfn;
    }
    return -1;
}

/* ============================ Utils virtuales ============================ */
typedef struct { uint32_t start; uint32_t end; } vinterval;

/* Construye arreglo de intervalos [vaddr, vaddr+size) de archivos del proceso */
static int build_intervals(const PCBEntryDisk* pcb, vinterval* out, int cap, int* n_out){
    int n=0;
    for(int i=0;i<FILES_PER_PROC;i++){
        FileEntryDisk fe; fe_read(pcb,i,&fe);
        if(fe.valid!=1) continue;
        uint64_t size=u40_read(fe.size40);
        if(size==0) continue;
        if(n<cap){
            out[n].start = fe.vaddr;
            out[n].end   = fe.vaddr + (uint32_t)size;
        }
        n++;
    }
    *n_out=n;
    return 0;
}

/* Busca primer hueco libre para 'need_bytes' contiguos en vaddr (0..128MiB) */
static uint32_t find_first_free_vaddr(const PCBEntryDisk* pcb, uint64_t need_bytes){
    const uint64_t VMAX = (uint64_t)4096 * FRAME_SIZE; /* 128MiB */
    if(need_bytes==0) need_bytes=1;

    vinterval iv[FILES_PER_PROC]; int n=0;
    build_intervals(pcb, iv, FILES_PER_PROC, &n);

    /* ordenar por start (insertion; n<=10) */
    for(int i=1;i<n;i++){
        vinterval key=iv[i]; int j=i-1;
        while(j>=0 && iv[j].start>key.start){ iv[j+1]=iv[j]; j--; }
        iv[j+1]=key;
    }

    uint32_t cur=0;
    for(int i=0;i<n;i++){
        if((uint64_t)iv[i].start - (uint64_t)cur >= need_bytes) return cur;
        if(iv[i].end > cur) cur = iv[i].end;
    }
    if(VMAX - (uint64_t)cur >= need_bytes) return cur;
    return 0xFFFFFFFFu; /* sin espacio */
}

/* Avanza vpn/ofs para la siguiente página */
static inline void next_page(uint32_t* vaddr){
    uint32_t v=*vaddr;
    (void)v;
    uint16_t vpn = vpn_of(v);
    vpn++;
    *vaddr = ((uint32_t)vpn<<15);
}

/* ============================ API GENERAL ============================ */
void mount_memory(char* memory_path){
    if(g_path) free(g_path);
    g_path = strdup(memory_path);
    printf("Memoria montada en: %s\n", g_path);
}

/* Crea archivo desde cero (2GiB + headers) para partir “limpio” */
int format_memory(char* memory_path){
    FILE* f = fopen(memory_path,"wb");
    if(!f){ perror("format_memory"); return -1; }

    /* cabeceras en cero */
    size_t headers = PCBS_SIZE + IPT_SIZE + BITMAP_SIZE;
    void* zero = calloc(1, headers);
    fwrite(zero,1,headers,f);
    free(zero);

    /* data de 2 GiB en cero (puede demorar) */
    const size_t chunk = 1<<20; /* 1MiB */
    void* z = calloc(1, chunk);
    for(size_t left = (size_t)TOTAL_FRAMES*FRAME_SIZE; left>0; ){
        size_t w = left>chunk?chunk:left;
        fwrite(z,1,w,f);
        left -= w;
    }
    free(z);
    fclose(f);
    return 0;
}

void list_processes(void){
    FILE* f=open_ro(); if(!f){ puts("Error al abrir memoria."); return; }
    printf("\nProcesos activos:\n");
    for(int i=0;i<PCB_COUNT;i++){
        PCBEntryDisk pcb; if(pcb_read(f,i,&pcb)!=0){ fclose(f); return; }
        if(pcb.valid==1){
            char name[PROC_NAME_LEN+1]={0}; memcpy(name,pcb.name,PROC_NAME_LEN);
            printf("PID: %-3u | Nombre: %-14s | Index: %d\n", pcb.pid, name, i);
        }
    }
    fclose(f);
}

int processes_slots(void){
    FILE* f=open_ro(); if(!f) return -1;
    int freec=0;
    for(int i=0;i<PCB_COUNT;i++){
        PCBEntryDisk pcb; if(pcb_read(f,i,&pcb)!=0){ fclose(f); return -1; }
        if(pcb.valid!=1) freec++;
    }
    fclose(f);
    return freec;
}

void list_files(int process_id){
    FILE* f=open_ro(); if(!f){ puts("Error al abrir memoria."); return; }
    int idx = pcb_find_by_pid(f, process_id);
    if(idx<0){ printf("Proceso %d no encontrado.\n", process_id); fclose(f); return; }

    PCBEntryDisk pcb; pcb_read(f, idx, &pcb);
    for(int i=0;i<FILES_PER_PROC;i++){
        FileEntryDisk fe; fe_read(&pcb,i,&fe);
        if(fe.valid==1){
            char name[FILE_NAME_LEN+1]={0}; memcpy(name,fe.name,FILE_NAME_LEN);
            uint64_t size=u40_read(fe.size40);
            uint16_t vpn = vpn_of(fe.vaddr);
            printf("0x%X %llu 0x%08X %s\n",
                   vpn,(unsigned long long)size,fe.vaddr,name);
        }
    }
    fclose(f);
}

void frame_bitmap_status(void){
    FILE* f=open_ro(); if(!f){ puts("Error al abrir memoria."); return; }
    unsigned used=0;
    for(uint32_t i=0;i<TOTAL_FRAMES;i++){
        int b=bmp_get(f,i); if(b<0){ fclose(f); puts("Error leyendo bitmap."); return; }
        if(b) used++;
    }
    fclose(f);
    printf("USADOS: %u LIBRES: %u\n", used, (unsigned)TOTAL_FRAMES-used);
}

/* ============================ PROCESOS ============================ */
int start_process(int process_id, char* process_name){
    FILE* f=open_rw(); if(!f){ puts("Error al abrir memoria."); return -1; }
    if(pcb_find_by_pid(f, process_id)>=0){ fclose(f); return -1; }

    int freei = pcb_first_free(f);
    if(freei<0){ fclose(f); return -1; }

    PCBEntryDisk pcb; memset(&pcb,0,sizeof(pcb));
    pcb.valid=1; pcb.pid=(uint8_t)process_id;
    if(process_name){
        size_t n=strnlen(process_name, PROC_NAME_LEN);
        memcpy(pcb.name, process_name, n);
    }
    int r=pcb_write(f,freei,&pcb);
    fclose(f);
    return r;
}

/* libera todas las páginas mapeadas por el pid (IPT + bitmap) y limpia el PCB */
int finish_process(int process_id){
    FILE* f=open_rw(); if(!f){ puts("Error al abrir memoria."); return -1; }
    int idx=pcb_find_by_pid(f, process_id);
    if(idx<0){ fclose(f); return -1; }

    /* limpiar IPT y bitmap del pid */
    for(int pfn=0;pfn<(int)TOTAL_FRAMES;pfn++){
        int v; uint16_t p,vp;
        if(ipt_get(f,pfn,&v,&p,&vp)!=0){ fclose(f); return -1; }
        if(v && p==(uint16_t)process_id){
            if(ipt_set(f,pfn,0,0,0)!=0 || bmp_set(f,pfn,0)!=0){ fclose(f); return -1; }
        }
    }
    /* invalidar PCB */
    PCBEntryDisk zero; memset(&zero,0,sizeof(zero));
    pcb_write(f, idx, &zero);
    fclose(f);
    return 0;
}

int clear_all_processes(void){
    FILE* f=open_ro(); if(!f) return -1;
    int closed=0;
    for(int i=0;i<PCB_COUNT;i++){
        PCBEntryDisk pcb; if(pcb_read(f,i,&pcb)!=0){ fclose(f); return -1; }
        if(pcb.valid==1){ fclose(f); finish_process(pcb.pid); closed++; f=open_ro(); if(!f) return -1; }
    }
    fclose(f);
    return closed;
}

int file_table_slots(int process_id){
    FILE* f=open_ro(); if(!f) return -1;
    int idx=pcb_find_by_pid(f, process_id);
    if(idx<0){ fclose(f); return -1; }
    PCBEntryDisk pcb; pcb_read(f,idx,&pcb);
    int freec=0;
    for(int i=0;i<FILES_PER_PROC;i++){
        FileEntryDisk fe; fe_read(&pcb,i,&fe);
        if(fe.valid!=1) freec++;
    }
    fclose(f);
    return freec;
}

/* ============================ ARCHIVOS ============================ */
osmFile* open_file(int process_id, char* file_name, char mode){
    FILE* f=open_rw(); if(!f){ puts("Error al abrir memoria."); return NULL; }
    int idx=pcb_find_by_pid(f,process_id);
    if(idx<0){ fclose(f); printf("Proceso %d no encontrado.\n", process_id); return NULL; }

    PCBEntryDisk pcb; pcb_read(f,idx,&pcb);

    int pos = fe_find_name(&pcb,file_name);
    if(pos>=0){
        if(mode=='r'){ /* abrir existente */
            FileEntryDisk fe; fe_read(&pcb,pos,&fe);
            osmFile* d=calloc(1,sizeof(*d));
            d->process_id=process_id;
            strncpy(d->file_name,file_name,14); d->file_name[14]='\0';
            d->mode='r';
            d->size = u40_read(fe.size40);
            d->vaddr= fe.vaddr;
            fclose(f);
            return d;
        }
        /* ya existe para 'w' */
        fclose(f); printf("Error: archivo '%s' ya existe.\n", file_name); return NULL;
    }

    if(mode!='w'){ fclose(f); printf("Archivo '%s' no existe.\n", file_name); return NULL; }

    /* crear nueva entrada */
    int freee = fe_first_free(&pcb);
    if(freee<0){ fclose(f); printf("Tabla de archivos llena.\n"); return NULL; }

    FileEntryDisk fe; memset(&fe,0,sizeof(fe));
    fe.valid = 1;
    memcpy(fe.name, file_name, strlen(file_name)>14?14:strlen(file_name));
    memset(fe.size40, 0, 5);
    fe.vaddr=0; /* se define al escribir */

    fe_write(&pcb, freee, &fe);
    pcb_write(f, idx, &pcb);
    fclose(f);

    osmFile* d=calloc(1,sizeof(*d));
    d->process_id=process_id;
    strncpy(d->file_name,file_name,14); d->file_name[14]='\0';
    d->mode='w';
    d->size=0; d->vaddr=0;
    return d;
}

/* escribe cruzando páginas; asigna PFN libre si (pid,vpn) no existe */
int write_file(osmFile* fd, char* src_path){
    if(!fd || fd->mode!='w'){ errno=EINVAL; return -1; }

    FILE* mem=open_rw(); FILE* src=fopen(src_path,"rb");
    if(!mem || !src){ if(mem)fclose(mem); if(src)fclose(src); perror("write_file"); return -1; }

    /* ubicar PCB y entrada del archivo */
    int idx=pcb_find_by_pid(mem, fd->process_id);
    if(idx<0){ fclose(mem); fclose(src); return -1; }
    PCBEntryDisk pcb; pcb_read(mem,idx,&pcb);
    int fe_idx=fe_find_name(&pcb, fd->file_name);
    if(fe_idx<0){ fclose(mem); fclose(src); return -1; }

    /* tamaño de origen */
    if(fseeko(src,0,SEEK_END)!=0){ fclose(mem); fclose(src); return -1; }
    uint64_t need = (uint64_t)ftello(src);
    if(fseeko(src,0,SEEK_SET)!=0){ fclose(mem); fclose(src); return -1; }

    /* asignar vaddr inicial si es la primera escritura */
    FileEntryDisk fe; fe_read(&pcb,fe_idx,&fe);
    if(u40_read(fe.size40)==0 && fe.vaddr==0){
        uint32_t start = find_first_free_vaddr(&pcb, need);
        if(start==0xFFFFFFFFu){ fclose(mem); fclose(src); printf("Sin espacio virtual.\n"); return -1; }
        fe.vaddr = start;
    }

    uint32_t cur_vaddr = fe.vaddr;
    uint64_t left = need;
    char buf[4096];

    while(left>0){
        uint16_t vpn = vpn_of(cur_vaddr);
        uint16_t off = off_of(cur_vaddr);

        /* pfn para (pid,vpn) */
        int pfn = ipt_find_pfn(mem, (uint16_t)fd->process_id, vpn);
        if(pfn<0){
            pfn = bmp_first_free(mem);
            if(pfn<0){ fclose(mem); fclose(src); printf("Sin frames físicos.\n"); return -1; }
            if(bmp_set(mem, pfn, 1)!=0 || ipt_set(mem, pfn, 1, (uint16_t)fd->process_id, vpn)!=0){
                fclose(mem); fclose(src); return -1;
            }
        }

        size_t room = FRAME_SIZE - off;               /* espacio que queda en la página */
        size_t chunk = (left<room)? (size_t)left : room;
        if (chunk > sizeof(buf)) chunk = sizeof(buf); /* cap al tamaño del buffer */

        size_t n = fread(buf,1,chunk,src);
        if(n==0) break; /* EOF inesperado o error */

        uint64_t pa = paddr_abs((uint16_t)pfn, off);
        if(fseeko(mem, (off_t)pa, SEEK_SET)!=0){ fclose(mem); fclose(src); return -1; }
        if(fwrite(buf,1,n,mem)!=n){ fclose(mem); fclose(src); return -1; }

        left      -= n;
        cur_vaddr += (uint32_t)n;

        /* si justo llenamos la página, saltamos a la siguiente */
        if(off + n == FRAME_SIZE){
            cur_vaddr &= ~0x7FFFu;
            next_page(&cur_vaddr);
        }
    }

    /* actualizar tamaño y persistir */
    u40_write(fe.size40, need);
    fe_write(&pcb, fe_idx, &fe);
    pcb_write(mem, idx, &pcb);

    fd->size = need;
    fd->vaddr= fe.vaddr;

    fclose(mem); fclose(src);
    return (int)need;
}

/* lee respetando mapeos; error si falta una página mapeada */
int read_file(osmFile* fd, char* dest_path){
    if(!fd || fd->mode!='r'){ errno=EINVAL; return -1; }

    FILE* mem=open_ro(); FILE* dst=fopen(dest_path,"wb");
    if(!mem || !dst){ if(mem)fclose(mem); if(dst)fclose(dst); perror("read_file"); return -1; }

    /* ubicar PCB y entrada del archivo */
    int idx=pcb_find_by_pid(mem, fd->process_id);
    if(idx<0){ fclose(mem); fclose(dst); return -1; }
    PCBEntryDisk pcb; pcb_read(mem,idx,&pcb);
    int fe_idx=fe_find_name(&pcb, fd->file_name);
    if(fe_idx<0){ fclose(mem); fclose(dst); return -1; }
    FileEntryDisk fe; fe_read(&pcb,fe_idx,&fe);
    uint64_t left = u40_read(fe.size40);
    uint32_t cur_vaddr = fe.vaddr;

    char buf[4096];
    while(left>0){
        uint16_t vpn = vpn_of(cur_vaddr);
        uint16_t off = off_of(cur_vaddr);

        int pfn = ipt_find_pfn(mem, (uint16_t)fd->process_id, vpn);
        if(pfn<0){ fclose(mem); fclose(dst); printf("Falta mapeo para VPN=0x%X\n", vpn); return -1; }

        size_t room  = FRAME_SIZE - off;
        size_t chunk = (left<room)? (size_t)left : room;
        if (chunk > sizeof(buf)) chunk = sizeof(buf);

        uint64_t pa = paddr_abs((uint16_t)pfn, off);
        if(fseeko(mem, (off_t)pa, SEEK_SET)!=0){ fclose(mem); fclose(dst); return -1; }
        size_t n=fread(buf,1,chunk,mem);
        if(n==0) break;

        if(fwrite(buf,1,n,dst)!=n){ fclose(mem); fclose(dst); return -1; }

        left      -= n;
        cur_vaddr += (uint32_t)n;
        if(off + n == FRAME_SIZE){
            cur_vaddr &= ~0x7FFFu;
            next_page(&cur_vaddr);
        }
    }

    fclose(mem); fclose(dst);
    return 0;
}

/* elimina entrada y libera PFN de sus páginas (asumimos no comparten VPN) */
void delete_file(int process_id, char* file_name){
    FILE* f=open_rw(); if(!f){ puts("Error al abrir memoria."); return; }

    int idx=pcb_find_by_pid(f,process_id);
    if(idx<0){ fclose(f); printf("Proceso %d no encontrado.\n", process_id); return; }
    PCBEntryDisk pcb; pcb_read(f,idx,&pcb);
    int fe_idx=fe_find_name(&pcb,file_name);
    if(fe_idx<0){ fclose(f); printf("Archivo '%s' no encontrado.\n",file_name); return; }

    FileEntryDisk fe; fe_read(&pcb,fe_idx,&fe);
    uint64_t size = u40_read(fe.size40);
    uint32_t cur_vaddr = fe.vaddr;

    /* liberar cada VPN cubierta por el archivo */
    while(size>0){
        uint16_t vpn = vpn_of(cur_vaddr);
        uint16_t off = off_of(cur_vaddr);
        size_t room = FRAME_SIZE - off;
        size_t step = (size<room)? (size_t)size : room;

        int pfn = ipt_find_pfn(f, (uint16_t)process_id, vpn);
        if(pfn>=0){
            ipt_set(f, pfn, 0, 0, 0);
            bmp_set(f, pfn, 0);
        }

        size -= step;
        cur_vaddr += (uint32_t)step;
        if(off + step == FRAME_SIZE){
            cur_vaddr &= ~0x7FFFu;
            next_page(&cur_vaddr);
        }
    }

    /* invalidar entrada */
    FileEntryDisk zero; memset(&zero,0,sizeof(zero));
    fe_write(&pcb,fe_idx,&zero);
    pcb_write(f,idx,&pcb);
    fclose(f);
}

void close_file(osmFile* file_desc){
    if(!file_desc) return;
    free(file_desc);
}
__attribute__((destructor))
static void cleanup_global_path(void) {
    if (g_path) free(g_path);
}

