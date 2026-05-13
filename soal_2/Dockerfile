FROM ubuntu:latest

# Set working directory di dalam container
WORKDIR /app
RUN ln -s /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 /lib64/ld-linux-x86-64.so.2 2>/dev/null || true

# Salin binary server dari host ke container
COPY server /app/server

# Pastikan binary dapat dieksekusi
RUN chmod +x /app/server

# Buat direktori db (nantinya akan ditimpa bind mount)
RUN mkdir -p /app/db

# Ekspose port server
EXPOSE 9000

# Jalankan server
CMD ["./server"]