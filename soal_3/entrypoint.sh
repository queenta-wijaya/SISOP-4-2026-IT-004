#!/bin/bash
set -e

echo "[*] Membuat group dan user..."

# Hapus grup staff bawaan Ubuntu (GID 50) jika ada
if getent group staff | grep -q '^staff:x:50:'; then
    groupdel staff
fi

# Buat grup staff dan readonly
getent group staff >/dev/null || groupadd -g 1001 staff
getent group readonly >/dev/null || groupadd -g 1002 readonly

# Buat user (tanpa primary group staff/readonly dulu)
id member &>/dev/null || useradd -m -s /bin/false member
id contributor &>/dev/null || useradd -m -s /bin/false contributor
id librarian &>/dev/null || useradd -m -s /bin/false librarian

# Tambahkan user ke grup yang sesuai (anggota tambahan)
usermod -aG readonly member
usermod -aG staff contributor
usermod -aG staff librarian

# Set password Samba
(echo "member123"; echo "member123") | smbpasswd -a -s member
(echo "contrib456"; echo "contrib456") | smbpasswd -a -s contributor
(echo "lib789"; echo "lib789") | smbpasswd -a -s librarian

smbpasswd -e member
smbpasswd -e contributor
smbpasswd -e librarian

echo "[*] Mengatur permission direktori koleksi..."

mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs

chown root:staff /libraryit/ebooks /libraryit/papers
chmod 775 /libraryit/ebooks /libraryit/papers

chown root:staff /libraryit/sourcecode
chmod 750 /libraryit/sourcecode

chown root:staff /libraryit/docs
chmod 775 /libraryit/docs          # Penting: 775 agar librarian bisa tulis

echo "[*] Memulai transformasi log..."

mkdir -p /var/log/samba
chmod 755 /var/log/samba
touch /var/log/samba/libraryit.log
chmod 644 /var/log/samba/libraryit.log

# Parsing log standar Samba (log.smbd) ke format yang diminta
tail -F /var/log/samba/log.smbd | while read -r line; do
    timestamp=$(date "+%Y-%m-%d %H:%M:%S")
    if echo "$line" | grep -q "connect to service"; then
        user=$(echo "$line" | sed -n 's/.*as user \([^ ]*\).*/\1/p')
        share=$(echo "$line" | sed -n 's/.*connect to service \([^ ]*\).*/\1/p')
        echo "[$timestamp] [INFO] [$user] [CONNECT] [$share]" >> /var/log/samba/libraryit.log
    elif echo "$line" | grep -q "NT_STATUS_ACCESS_DENIED"; then
        user=$(echo "$line" | grep -oP 'user=\K[^ ]*')
        file=$(echo "$line" | grep -oP 'file=\K[^ ]*')
        [ -z "$file" ] && file="unknown"
        echo "[$timestamp] [WARNING] [$user] [DENIED] [$file]" >> /var/log/samba/libraryit.log
    elif echo "$line" | grep -qE "opened file.*write|create_file|write_file|open.*O_WRONLY"; then
        user=$(echo "$line" | grep -oP 'user=\K[^ ]*')
        file=$(echo "$line" | grep -oP 'file=\K[^ ]*')
        echo "[$timestamp] [INFO] [$user] [WRITE] [$file]" >> /var/log/samba/libraryit.log
    fi
done &

echo "[*] Memulai Samba..."
exec smbd --foreground --no-process-group