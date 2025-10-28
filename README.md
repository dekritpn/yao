# YAO: Yet Another Othello

Ini adalah implementasi sederhana dari game Othello (juga dikenal sebagai Reversi) yang dimainkan di terminal. Anda akan bermain sebagai bidak Hitam melawan AI sederhana yang bermain sebagai bidak Putih.

## Fitur

- **Antarmuka Berbasis Teks**: Mainkan langsung di terminal Anda.
- **Pemain vs AI**: Anda (Hitam) melawan AI (Putih).
- **AI Cerdas**: AI menggunakan algoritma Minimax dengan optimisasi Alpha-Beta Pruning untuk menentukan langkah terbaik.
- **Tampilan Langkah Legal**: Langkah yang valid akan ditandai dengan titik (`·`) di papan.
- **Perintah Dalam Game**:
    - `MOVE <koordinat>`: Untuk menempatkan bidak (mis., `MOVE D3`).
    - `UNDO`: Untuk membatalkan langkah terakhir Anda dan langkah AI.
    - `HINT`: Untuk meminta petunjuk langkah dari AI.
    - `PASS`: Untuk melewati giliran jika Anda tidak memiliki langkah legal.
    - `QUIT`: Untuk keluar dari permainan.

## Cara Kompilasi

Pastikan Anda memiliki kompiler C++ (seperti `g++`). Gunakan perintah berikut untuk mengompilasi kode:

```bash
g++ yao.cpp -o othello -std=c++17 -Wall
```

Perintah ini akan menghasilkan file eksekusi bernama `othello`.

## Cara Menjalankan

Setelah kompilasi berhasil, jalankan game dengan perintah berikut:

```bash
./othello
```

## Cara Bermain

1. Jalankan game.
2. Anda adalah pemain **Hitam (●)**.
3. Saat giliran Anda, masukkan perintah `MOVE` diikuti dengan koordinat (mis., `MOVE F5`).
4. AI akan secara otomatis mengambil gilirannya setelah Anda.
5. Permainan berakhir ketika seluruh papan terisi atau ketika kedua pemain tidak bisa melangkah lagi.
