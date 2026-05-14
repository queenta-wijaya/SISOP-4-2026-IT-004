# SISOP-4-2026-IT-004
Nama  : Ni Putu Maqueenta Wijaya  
NRP   : 5027251004  
## Laporan
### Soal 1
Soal pertama meminta kita untuk mendownload arsip `Amba Files` dari link:
```
https://drive.google.com/file/d/1nLXFhptDo2mnUlZsw8pTWyAVpV49W20U/view?usp=drive_link
```
Setelah di-download, isi file tersebut di-unzip kemudian zipfile asli dihapus dengan command
```
rm -f amba_files.zip
```
Setelah unzip dan menghapus file zip asli, soal 1 meminta untuk membuat file `kenz_rescue.c` yang menerima dua argumen yakni `souce_directory` dan `mount_directory`. Isi dari file `kenz_rescue.c` adalah sebagai berikut:
```c
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

static const char *source_dir = NULL;
static char *tujuan_content = NULL;
static size_t tujuan_size = 0;
```
Berisi header dan definisi versi FUSE, library yang digunakan, dan variabel global.
```c
static char* build_tujuan_content(void) {
    const int count = 7;
    const char *files[] = {"1.txt","2.txt","3.txt","4.txt","5.txt","6.txt","7.txt"};

    char *fragments = strdup("");       /* akan menampung gabungan fragmen */
    size_t frag_len = 0;

    for (int i = 0; i < count; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", source_dir, files[i]);

        FILE *fp = fopen(path, "r");
        if (!fp) {
            fprintf(stderr, "Error opening %s\n", path);
            free(fragments);
            exit(1);
        }

        char line[4096];
        int found = 0;
        while (fgets(line, sizeof(line), fp)) {
            /* Cari baris dengan prefix "KOORD:" */
            if (strncmp(line, "KOORD:", 6) == 0) {
                const char *start = line + 6;            /* setelah "KOORD:" */
                while (*start == ' ') start++;           /* lewati spasi */
                /* Hapus newline di akhir jika ada */
                size_t len = strlen(start);
                if (len > 0 && start[len-1] == '\n') len--;
                /* Tambahkan ke fragments */
                fragments = realloc(fragments, frag_len + len + 1);
                memcpy(fragments + frag_len, start, len);
                frag_len += len;
                fragments[frag_len] = '\0';
                found = 1;
                break;  /* ambil baris pertama yang cocok saja */
            }
        }
        fclose(fp);
        if (!found) {
            fprintf(stderr, "KOORD: not found in %s\n", files[i]);
            free(fragments);
            exit(1);
        }
    }

    /* Gabungkan prefix + fragmen + newline */
    const char *prefix = "Tujuan Mas Amba: ";
    size_t prefix_len = strlen(prefix);
    size_t total = prefix_len + frag_len + 1 + 1; /* +1 newline +1 null */
    char *result = malloc(total);
    snprintf(result, total, "%s%s\n", prefix, fragments);
    free(fragments);
    return result;
}
```
Bagian ini membangin isi `tujuan.txt` sebagai hasil akhir dari soal. Bagian ini pertama akan membaca file sumber (file yang di-unzip tadi). 
Untuk langkah selanjutnya, program akan membuat buffer fragmen yang nanti akan diisi dengan gabungan semua fragmen KOORD. 
