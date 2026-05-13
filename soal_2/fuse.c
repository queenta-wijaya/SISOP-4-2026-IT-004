/*
 * fuse.c - FUSE filesystem dengan enkripsi XOR 0x76 dan ekstensi .enc
 */

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

static char *source_dir = NULL;   // direktori asli (encrypted_storage)

// XOR key
#define ENC_KEY 0x76

// Ambil path asli di storage untuk direktori (tanpa .enc)
static void to_dir_path(char *buf, const char *path) {
    sprintf(buf, "%s%s", source_dir, path);
}

// Ambil path asli di storage untuk file reguler (dengan .enc)
static void to_enc_path(char *buf, const char *path) {
    sprintf(buf, "%s%s.enc", source_dir, path);
}

// Enkripsi / dekripsi XOR (simetris)
static void xor_data(char *buf, size_t len) {
    for (size_t i = 0; i < len; i++)
        buf[i] ^= ENC_KEY;
}

static int fs_getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi) {
    (void)fi;
    char tmp[1024];
    memset(stbuf, 0, sizeof(struct stat));

    // Coba sebagai direktori
    to_dir_path(tmp, path);
    int res = lstat(tmp, stbuf);
    if (res == -1) {
        // Jika bukan direktori, coba sebagai file .enc
        to_enc_path(tmp, path);
        res = lstat(tmp, stbuf);
        if (res == -1)
            return -errno;
    }
    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    (void)offset; (void)fi; (void)flags;
    char tmp[1024];
    to_dir_path(tmp, path);

    DIR *dp = opendir(tmp);
    if (!dp)
        return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12; // perkiraan mode

        // Jika nama berakhiran .enc, tampilkan tanpa .enc
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        char name[256];
        strcpy(name, de->d_name);
        int len = strlen(name);
        if (len > 4 && strcmp(name + len - 4, ".enc") == 0) {
            name[len - 4] = '\0';  // hilangkan .enc
        }

        if (filler(buf, name, &st, 0, 0))
            break;
    }
    closedir(dp);
    return 0;
}

static int fs_mkdir(const char *path, mode_t mode) {
    char tmp[1024];
    to_dir_path(tmp, path);
    int res = mkdir(tmp, mode);
    if (res == -1)
        return -errno;
    return 0;
}

static int fs_rmdir(const char *path) {
    char tmp[1024];
    to_dir_path(tmp, path);
    int res = rmdir(tmp);
    if (res == -1)
        return -errno;
    return 0;
}

static int fs_create(const char *path, mode_t mode,
                     struct fuse_file_info *fi) {
    char tmp[1024];
    to_enc_path(tmp, path);
    int fd = open(tmp, fi->flags | O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd == -1)
        return -errno;
    fi->fh = fd;
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    char tmp[1024];
    to_enc_path(tmp, path);
    int fd = open(tmp, fi->flags);
    if (fd == -1)
        return -errno;
    fi->fh = fd;
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    (void)path;
    ssize_t res = pread(fi->fh, buf, size, offset);
    if (res == -1)
        return -errno;
    xor_data(buf, res);
    return res;
}

static int fs_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    (void)path;
    // Buat salinan untuk dienkripsi
    char *enc_buf = malloc(size);
    if (!enc_buf)
        return -ENOMEM;
    memcpy(enc_buf, buf, size);
    xor_data(enc_buf, size);

    ssize_t res = pwrite(fi->fh, enc_buf, size, offset);
    free(enc_buf);
    if (res == -1)
        return -errno;
    return res;
}

static int fs_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi) {
    char tmp[1024];
    to_enc_path(tmp, path);
    int res;
    if (fi) {
        res = ftruncate(fi->fh, size);
    } else {
        int fd = open(tmp, O_WRONLY);
        if (fd == -1)
            return -errno;
        res = ftruncate(fd, size);
        close(fd);
    }
    if (res == -1)
        return -errno;
    return 0;
}

static int fs_unlink(const char *path) {
    char tmp[1024];
    to_enc_path(tmp, path);
    int res = unlink(tmp);
    if (res == -1)
        return -errno;
    return 0;
}

static int fs_access(const char *path, int mask) {
    char tmp[1024];
    // Coba direktori dulu
    to_dir_path(tmp, path);
    int res = access(tmp, mask);
    if (res == -1) {
        to_enc_path(tmp, path);
        res = access(tmp, mask);
    }
    if (res == -1)
        return -errno;
    return 0;
}

static int fs_utimens(const char *path, const struct timespec tv[2],
                      struct fuse_file_info *fi) {
    (void)fi;
    char tmp[1024];
    to_dir_path(tmp, path);
    int res = utimensat(AT_FDCWD, tmp, tv, 0);
    if (res == -1) {
        to_enc_path(tmp, path);
        res = utimensat(AT_FDCWD, tmp, tv, 0);
    }
    if (res == -1)
        return -errno;
    return 0;
}

static struct fuse_operations fs_ops = {
    .getattr    = fs_getattr,
    .readdir    = fs_readdir,
    .mkdir      = fs_mkdir,
    .rmdir      = fs_rmdir,
    .create     = fs_create,
    .open       = fs_open,
    .read       = fs_read,
    .write      = fs_write,
    .truncate   = fs_truncate,
    .unlink     = fs_unlink,
    .access     = fs_access,
    .utimens    = fs_utimens,
};

int main(int argc, char *argv[]) {
    // source_dir diambil dari argumen pertama (jika ada)
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source_directory> [fuse options]\n", argv[0]);
        return 1;
    }
    source_dir = realpath(argv[1], NULL);
    if (!source_dir) {
        perror("realpath");
        return 1;
    }
    // Geser argumen agar FUSE tidak melihat direktori source
    argv[1] = argv[0];
    argv++;
    argc--;

    return fuse_main(argc, argv, &fs_ops, NULL);
}