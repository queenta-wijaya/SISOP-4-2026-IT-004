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

/* Global: source directory dan konten tujuan.txt (dibangun saat start) */
static const char *source_dir = NULL;
static char *tujuan_content = NULL;
static size_t tujuan_size = 0;

/* Ekstrak fragmen KOORD: dari file 1.txt..7.txt */
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

/* ============== FUSE callbacks ============== */

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

static int kenz_release(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") != 0) {
        close(fi->fh);
    }
    return 0;
}

static const struct fuse_operations kenz_oper = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open    = kenz_open,
    .read    = kenz_read,
    .release = kenz_release,
};

/* ============== main ============== */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_directory> <mount_point>\n", argv[0]);
        return 1;
    }

    source_dir = realpath(argv[1], NULL);
    if (!source_dir) {
        perror("realpath source_dir");
        return 1;
    }

    /* Bangun konten tujuan.txt sekarang */
    tujuan_content = build_tujuan_content();
    tujuan_size = strlen(tujuan_content);

    /* Pindahkan mount point ke posisi terakhir di argv untuk FUSE */
    argv[1] = argv[2];   /* mount point jadi argv[1] sekarang (argc menyesuaikan) */
    argc--;

    /* source_dir sudah disimpan, tidak perlu dioper ke FUSE args */
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int ret = fuse_main(args.argc, args.argv, &kenz_oper, NULL);
    fuse_opt_free_args(&args);

    free(tujuan_content);
    free((void*)source_dir);
    return ret;
}