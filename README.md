# 🐣 Netes.io - Smart Egg Incubator with ESP32 + Firebase + MQTT

**Netes.io** adalah sistem inkubator telur otomatis berbasis **ESP32** yang dilengkapi dengan kontrol suhu, kelembaban, dan motor rotasi otomatis. Sistem ini terhubung dengan **Firebase Realtime Database** untuk manajemen pengaturan jarak jauh dan juga dapat dikendalikan secara manual melalui **MQTT**.

## 📸 Tampilan Umum

* 🌡️ Monitoring suhu dan kelembaban secara realtime
* 🔁 Motor rotasi otomatis dan manual
* ☀️ Kontrol lampu pemanas otomatis berdasarkan pengaturan jenis kelamin
* ☁️ Sinkronisasi data dengan Firebase
* 💬 Kontrol manual via MQTT (HiveMQ broker)
* 📈 Riwayat sensor dan event dicatat ke Firebase

---

## 🧰 Fitur Utama

| Fitur                | Deskripsi                                                                       |
| -------------------- | ------------------------------------------------------------------------------- |
| 🔌 Otomatisasi Relay | Relay1 (motor rotasi), Relay2 (lampu pemanas) dikontrol otomatis/manual         |
| 📡 MQTT Integration  | Dapat menerima perintah manual via MQTT                                         |
| 🔥 Sensor DHT22      | Membaca suhu & kelembaban tiap 30 detik                                         |
| ☁️ Firebase RTDB     | Semua data dan pengaturan disimpan ke Firebase                                  |
| 📅 Jadwal Rotasi     | Otomatisasi siklus rotasi telur berdasarkan interval jam                        |
| 🧠 Jenis Kelamin     | Kontrol suhu berdasarkan target jenis kelamin (jantan/betina/seimbang)          |
| 📜 Logging           | Semua event (sensor, rotasi, pengurangan durasi) dicatat ke Firebase `/riwayat` |

---

## 🛠️ Hardware

* **ESP32 Dev Board**
* **DHT22 sensor**
* **2x Relay Module**
* **Lampu pemanas**
* **Motor rotasi telur (AC/DC motor)**
* **Koneksi WiFi**
* **Catu daya untuk motor dan lampu**

---

## 📦 Struktur Firebase

```json
{
  "jadwal_rotasi": 6, // jam
  "jenis_kelamin": "jantan" | "betina" | "seimbang",
  "durasi_inkubasi": 21, // hari
  "status": {
    "relay1": true,
    "relay2": false
  },
  "status_devices": {
    "relay1": true,
    "relay2": false
  },
  "riwayat": {
    "timestamp1": {
      "suhu": 36.5,
      "kelembaban": 55.2,
      "type": "suhu_kelembaban"
    },
    "timestamp2": {
      "type": "rotasi_telur",
      "detail": "rotasi_telur_24detik"
    },
    ...
  }
}
```

---

## 🔁 MQTT Topic yang Digunakan

| Topic                           | Payload      | Fungsi                                                 |
| ------------------------------- | ------------ | ------------------------------------------------------ |
| `esp32/GXrelay2`                | `ON` / `OFF` | Menyalakan/mematikan lampu pemanas secara manual       |
| `netesio/relay1/manual_trigger` | `start`      | Menjalankan rotasi motor selama 24 detik secara manual |

Broker MQTT default: `broker.hivemq.com:1883`

---

## 🧪 Setup dan Instalasi

### 1. Clone Repo

```bash
git clone https://github.com/namamu/netesio.git
cd netesio
```

### 2. Konfigurasi WiFi & Firebase

Edit di bagian atas kode:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

#define API_KEY "FIREBASE_WEB_API_KEY"
#define DATABASE_URL "https://your-project-id.firebaseio.com"
#define USER_EMAIL "your@email.com"
#define USER_PASSWORD "yourpassword"
```

### 3. Instal Library Arduino

Wajib instal:

* `WiFi.h` (built-in ESP32)
* `PubSubClient` (MQTT)
* `DHT sensor library`
* `Firebase ESP Client by Mobizt`

### 4. Upload ke ESP32

Pilih board: **ESP32 Dev Module**
Upload kode menggunakan Arduino IDE atau PlatformIO

---

## 🧠 Cara Kerja

1. Saat ESP32 menyala:

   * Terkoneksi ke WiFi dan Firebase
   * Subscribe ke topic MQTT
   * Baca data suhu & kelembaban tiap 30 detik
   * Jika suhu terlalu rendah (berdasarkan `jenis_kelamin`), nyalakan lampu (relay2)
   * Jalankan motor rotasi tiap X jam (default: 6 jam, ON selama 24 detik)
   * Data sensor dan event dikirim ke Firebase `/riwayat`

2. Tiap 24 jam:

   * `durasi_inkubasi` dikurangi 1 hari
   * Dicatat ke Firebase

3. Jika perintah MQTT diterima:

   * Lampu atau motor akan dikendalikan secara manual
   * Manual control lampu hanya aktif 1 menit (timeout)

---

## 📊 Log Firebase (Contoh)

```json
"riwayat/1718758888123": {
  "suhu": 36.7,
  "kelembaban": 58.1,
  "timestamp": 1718758888123,
  "type": "suhu_kelembaban"
}
```

---

## 🧑‍💻 Pengembang

**Netes.io** dibuat oleh:

| Nama                     | NIM          |
| ------------------------ | ------------ |
| Nabila Choirunisa        | 232410102059 |
| Febriani M. Sitanggang   | 232410102062 |
| Jeje Dava Putra Octavian | 232410102067 |
| Fahriz Septian Rozaq     | 232410102084 |

---

## 💡 Ide Pengembangan Lanjut

* Tambahkan LCD/Nextion untuk kontrol lokal
* Buzzer jika suhu ekstrem
* Telegram/WhatsApp notification
* ESP-NOW antar node
* Mode darurat saat WiFi mati

---

## ⚠️ Disclaimer

Pastikan perangkat keras dan sambungan listrik telah diuji secara **aman** sebelum digunakan dalam waktu lama.

---

## 📄 Lisensi

MIT License

---

Kalau kamu ingin saya bantu buatkan juga **file `platformio.ini`** atau file **contoh `Firebase` JSON struktur**, tinggal bilang aja!
