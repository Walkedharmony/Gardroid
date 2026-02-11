# 📱 Gardroid - Visual Novel Resource Extractor for Android

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Android_11+-green.svg)
![C++](https://img.shields.io/badge/C++-17-blue.svg)

**Gardroid** pada dasarnya adalah versi portingan Android dari tool ekstraktor VN PC seperti GARbro. Karena proses ekstrak file VN itu butuh komputasi yang lumayan berat, core parser di aplikasi ini dibangun full menggunakan native C++ (NDK) biar performanya tetap cepat, sementara UI-nya ditulis pakai Java. 

Project ini dibikin buat mempermudah proses ekstrak, preview, sampai repack aset Visual Novel langsung dari HP tanpa perlu buka laptop.

## ✨ Support Engine

Saat ini Gardroid bisa membaca dan memproses format dari beberapa engine VN berikut:

### ⚙️ KiriKiri Engine
* **.xp3 Archives:** Parsing, ekstrak, dan repack.
* **.tlg Images:** Native decode dan on-the-fly preview untuk gambar `TLG5` ke Android Bitmap.
* **.ks Scripts:** Otomatis *descramble* script Kirikiri pas diekstrak atau di-preview. 
* **.scn / PSB:** Parsing skenario engine M2 (PSB format), decode LZSS, dan ekstrak struktur datanya jadi `.txt` biar gampang dibaca/diedit.

### ⚙️ Artemis Engine
* **.pfs Archives:** Ekstrak dan repack arsip PFS.
  **.ast Scripts:** Ekstrak teks dialog dan deteksi bahasa otomatis dari file skenario AST pakai Regex.

### ⚙️ Ethornell / M2 Engine (BGI)
* **.arc Archives:** Parsing dan ekstrak PackFile / BURIKO ARC20.
* **bgi Images:** Native decoder gambar BGI.

## 🛠️ Cara Penggunaan (Usage)

Navigasi di aplikasi ini dibikin semirip mungkin dengan file manager pada umumnya. 

* **Ekstrak Massal / Folder:** Tekan tahan (*Long press*) pada folder untuk me-yeleksi. Setelah terpilih, tap tombol FAB (ikon Download) di pojok kanan bawah untuk mulai mengekstrak.
* **Ekstrak 1 File Saja:** Tap ikon **titik tiga (Three dots)** di sebelah kanan file spesifik yang mau diambil, lalu pilih opsi extract.
* **Lokasi Hasil Ekstrak:** Hasil ekstraksi bakal otomatis dibuatkan folder bernama `Gardroid_Extracted`. Lokasinya ada di direktori yang sama dengan tempat file arsip VN game-mu berada.
* **Cara Repack (contoh ke .xp3):** Masuk ke dalam direktori hasil ekstrakan tadi (misal: `Gardroid_Extracted/data`), lalu tap ikon **titik tiga** di sebelah kanan folder tersebut dan pilih opsi repack.

## 🏗️ Build dari Source
Buat yang mau nge-build sendiri atau ikut berkontribusi:
1. Clone repo ini: `git clone https://github.com/Walkedharmony/Gardroid.git`
2. Buka pakai **Android Studio**.
3. Pastikan **NDK** dan **CMake** (minimal v3.22.1) udah keinstall dari SDK Manager.
4. Sync Gradle, lalu klik Build/Run.
   
## 📜 Lisensi
Project ini open-source di bawah **MIT License** [LICENSE](LICENSE)

## 🙏 Credits
* Referensi code [GARbro](https://github.com/morkt/GARbro).
* Author Zero Novel | [Walkedharmony](https://github.com/Walkedharmony)
