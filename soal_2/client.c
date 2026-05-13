#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define SERVER_PORT 9000
#define BUFFER_SIZE 4096

// Membaca respons sampai server diam (timeout pendek) lalu kembali
void read_response(int sock) {
    char buf[BUFFER_SIZE];
    fd_set readfds;
    struct timeval tv;
    int ret;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 200000;   // 0.2 detik timeout

        ret = select(sock + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select");
            break;
        } else if (ret == 0) {
            // Tidak ada data lagi, anggap respons selesai
            break;
        }

        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            if (n < 0) perror("recv");
            break;  // koneksi ditutup atau error
        }

        buf[n] = '\0';
        printf("%s", buf);
        fflush(stdout);
    }
}

int main() {
    int sock;
    struct sockaddr_in server_addr;

    // Buat socket dan koneksikan sekali
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    printf("Terhubung ke server pada port %d.\n", SERVER_PORT);
    printf("Masukkan perintah (Ctrl+D untuk keluar):\n");

    char buf[BUFFER_SIZE];
    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;

        // Hapus newline jika ada (opsional, server biasanya tidak peduli)
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n')
            buf[len-1] = '\0';

        // Jika kosong, lewati
        if (len == 0)
            continue;

        // Kirim perintah (tambahkan newline lagi karena server mungkin butuh)
        strcat(buf, "\n");
        if (send(sock, buf, strlen(buf), 0) < 0) {
            perror("send");
            break;
        }

        // Baca respons
        read_response(sock);
    }

    close(sock);
    printf("\nKoneksi ditutup.\n");
    return 0;
}