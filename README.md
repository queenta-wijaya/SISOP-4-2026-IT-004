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
Bagian ini membangin isi `tujuan.txt` sebagai hasil akhir dari soal. Bagian ini pertama akan membaca file sumber (file yang di-unzip tadi). Untuk langkah selanjutnya, program akan membuat buffer fragmen yang nanti akan diisi dengan gabungan semua fragmen KOORD. Program akan mencari bagian yang berisi kata `KOORD` dan mengambil isinya dengan skip spasi dan hapus newline. Langkah terakhir, isi yang sudah diambil ditambahkan prefix `Tujuan Mas Amba: ` dalam file baru.
```c
static int kenz_getattr(const char *path, struct stat *stbuf,
                        struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        /* Root directory */
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (strcmp(path, "/tujuan.txt") == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = tujuan_size;
        return 0;
    }

    /* Untuk file passthrough: stat file asli */
    char fpath[PATH_MAX];
    snprintf(fpath, sizeof(fpath), "%s/%s", source_dir, path + 1);
    int res = lstat(fpath, stbuf);
    if (res == -1)
        return -errno;
    return 0;
}
```
Bagian ini berfungsi untuk memberi data mengenai file atau folder saat mengakses filesystem, seperti menentukan apakah path ada di direktori root, file virtual, atau file asli. 
```c
static int kenz_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    /* Daftar file dari source_dir */
    DIR *dp = opendir(source_dir);
    if (!dp)
        return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        filler(buf, de->d_name, NULL, 0, 0);
    }
    closedir(dp);

    /* Tambahkan file virtual */
    filler(buf, "tujuan.txt", NULL, 0, 0);

    return 0;
}
```
Bagian ini merupakan callback FUSE yang berfungsi untuk menampilkan isi dari direktori saat menjalankan command seperti `ls`.
```c
static int kenz_open(const char *path, struct fuse_file_info *fi) {
    /* File tujuan.txt tidak punya backing fd */
    if (strcmp(path, "/tujuan.txt") == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES;
        return 0;
    }

    /* File passthrough */
    char fpath[PATH_MAX];
    snprintf(fpath, sizeof(fpath), "%s/%s", source_dir, path + 1);
    int fd = open(fpath, fi->flags);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}
```
Bagian ini merupakan callback FUSE yang menangani proses saat membuka file. Jika file yang dibuka adalah `tujuan.txt`, fungsi memastikan file tersebut hanya bisa diakses dalam mode baca karena file ini merupakan file virtual dan tidak memiliki file descriptor asli.
```c
static int kenz_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        if (offset < (off_t)tujuan_size) {
            if (offset + size > tujuan_size)
                size = tujuan_size - offset;
            memcpy(buf, tujuan_content + offset, size);
        } else {
            size = 0;
        }
        return size;
    }

    /* File passthrough */
    int fd = fi->fh;
    int res = pread(fd, buf, size, offset);
    if (res == -1)
        return -errno;
    return res;
}
```
Bagian ini merupakan callback FUSE yang menangani proses saat membaca isi file. Ketika membaca file `tujuan.txt`, fungsi mengambil data langsung dari buffer virtual, menyesuaikan jumlah byte yang dibaca, dan menyalinnya ke `buf`.
```c
static int kenz_release(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") != 0) {
        close(fi->fh);
    }
    return 0;
}
```
Bagian ini merupakan callback FUSE yang dipanggil ketika file ditutup setelah digunakan. Untuk `tujua.txt`, file tidak perlu ditutup karena bersifat virtual. Tapi, jika file yang ditutup bukan `tujuan.txt` maka fungsi akan menutup file descriptor.
```c
static const struct fuse_operations kenz_oper = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open    = kenz_open,
    .read    = kenz_read,
    .release = kenz_release,
};
```
Bagian ini merupakan struktur dari `fuse_operation` yang berfungsi sebagai daftar callback yang memberitahu FUSE fungsi mana yang ia harus panggil saat terjadi operasi filesystem. Bagian ini berisi `kenz_getattr` untuk mengambil metadata file, `kenz_readdir` untuk menampilkan isi direktori, `kenz_open` untuk membuka file, `kenz_read` untuk membaca file, dan `kenz_release` untuk menutup file setelah selesai digunakan.  
Kemudian, file `kenz_rencue.c` di-compile menggunakan command:
```
gcc -Wall kenz_rescue.c -o kenz_rescue $(pkg-config fuse3 --cflags --libs)
```
Setelah di-compile, buat directory `mnt` dengan:
```
./kenz_rescue amba_files mnt
```
Terakhir, jalankan fuse dengan:
```
./kenz_rescue amba_files mnt
```
### Output
