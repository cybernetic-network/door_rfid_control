/*
 * ESP32 Door Lock System dengan RFID dan Web Control
 * Fitur:
 * - Kontrol akses menggunakan RFID RC522
 * - Web interface dengan autentikasi
 * - Display OLED untuk status dan waktu
 * - Push button untuk indikator status
 * - Sinkronisasi waktu dengan NTP Server
 * - Dukungan untuk DFPlayer Mini dan suara
 */

// Include semua library yang dibutuhkan
#include <Adafruit_GFX.h>      // Library untuk grafik OLED
#include <Adafruit_SSD1306.h>  // Library untuk OLED display
#include <Wire.h>              // Library untuk komunikasi I2C
#include <EEPROM.h>            // Library untuk menyimpan data di memory
#include <WiFi.h>              // Library untuk koneksi WiFi
#include <WebServer.h>         // Library untuk web server
#include <NTPClient.h>         // Library untuk sinkronisasi waktu
#include <WiFiUdp.h>          // Library untuk UDP (diperlukan NTP)
#include <SPI.h>              // Library untuk komunikasi SPI
#include <MFRC522.h>          // Library untuk RFID RC522
#include <time.h>             // Library untuk manipulasi waktu
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// Definisi untuk manajemen kartu RFID
#define MAX_CARDS 10  // Maksimum kartu yang bisa disimpan
#define EEPROM_SIZE 512 // Ukuran EEPROM yang digunakan
#define CARDS_START_ADDR 0 // Alamat awal penyimpanan data kartu

// Struktur data untuk kartu RFID
struct RFIDCard {
  byte uid[4];
  char name[20];
  bool active;
};

// Array untuk menyimpan kartu yang terdaftar
RFIDCard registeredCards[MAX_CARDS];
int numCards = 0;  // Jumlah kartu yang terdaftar

// Halaman login
const char loginPage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <title>Sistem Penguncian Pintu - Login</title>
    <style>
        body { font-family: Arial; margin: 0; padding: 20px; background: #f0f0f0; }
        .container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 5px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        input { width: 100%; padding: 10px; margin: 10px 0; box-sizing: border-box; }
        button { width: 100%; padding: 10px; background: #4CAF50; color: white; border: none; border-radius: 3px; cursor: pointer; }
        button:hover { background: #45a049; }
    </style>
</head>
<body>
    <div class="container">
        <h2>Sistem Penguncian Pintu Cerdas</h2>
        <form method="post" action="/login">
            <input type="text" name="username" placeholder="Username" required><br>
            <input type="password" name="password" placeholder="Password" required><br>
            <button type="submit">Login</button>
        </form>
    </div>
</body>
</html>
)=====";

// Halaman kontrol
const char controlPage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <title>Kontrol Pintu & Status Monitor</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        :root {
            --primary-color: #4CAF50;
            --danger-color: #f44336;
            --bg-color: #f0f0f0;
            --card-bg: #ffffff;
            --text-color: #333333;
            --shadow: 0 4px 6px rgba(0,0,0,0.1);
            --transition: all 0.3s ease;
        }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background: var(--bg-color);
            color: var(--text-color);
        }

        .container {
            max-width: 800px;
            margin: 0 auto;
            background: var(--card-bg);
            padding: 30px;
            border-radius: 15px;
            box-shadow: var(--shadow);
        }

        h2 {
            color: var(--text-color);
            text-align: center;
            margin-bottom: 30px;
            font-size: 2.2em;
            border-bottom: 2px solid var(--primary-color);
            padding-bottom: 10px;
        }

        .section {
            margin: 25px 0;
            padding: 25px;
            border-radius: 12px;
            background: #f8f9fa;
            box-shadow: var(--shadow);
            transition: var(--transition);
        }

        h3 {
            color: var(--text-color);
            margin: 0 0 20px 0;
            font-size: 1.5em;
            display: flex;
            align-items: center;
            gap: 10px;
        }

        h3::before {
            content: '';
            display: inline-block;
            width: 4px;
            height: 24px;
            background: var(--primary-color);
            border-radius: 2px;
        }

        .status {
            font-size: 24px;
            text-align: center;
            margin: 15px 0;
            padding: 20px;
            border-radius: 10px;
            transition: var(--transition);
        }

        .pressed {
            background: var(--danger-color);
            color: white;
            animation: pulse 2s infinite;
        }

        .normal {
            background: var(--primary-color);
            color: white;
        }

        .controls {
            display: flex;
            justify-content: center;
            gap: 15px;
            margin-top: 25px;
        }

        button {
            padding: 15px 30px;
            font-size: 18px;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: var(--transition);
            text-transform: uppercase;
            font-weight: bold;
            letter-spacing: 1px;
        }

        .unlock {
            background: var(--primary-color);
            color: white;
        }

        .lock {
            background: var(--danger-color);
            color: white;
        }

        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 8px rgba(0,0,0,0.2);
        }

        button:active {
            transform: translateY(0);
        }

        @keyframes pulse {
            0% { transform: scale(1); }
            50% { transform: scale(1.02); }
            100% { transform: scale(1); }
        }

        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 30px;
            padding-bottom: 10px;
            border-bottom: 2px solid var(--primary-color);
        }

        .logout-btn {
            background: #757575;
            color: white;
            padding: 8px 16px;
            border-radius: 5px;
            text-decoration: none;
            transition: var(--transition);
            font-size: 14px;
            border: none;
            cursor: pointer;
        }

        .logout-btn:hover {
            background: #616161;
            transform: translateY(-2px);
        }

        .datetime {
            text-align: center;
            padding: 20px;
            background: var(--card-bg);
            border-radius: 10px;
            box-shadow: var(--shadow);
            margin-bottom: 20px;
        }

        .time {
            font-size: 2.5em;
            font-weight: bold;
            color: var(--text-color);
            margin-bottom: 5px;
        }

        .date {
            font-size: 1.2em;
            color: #666;
        }

        @media (max-width: 600px) {
            .container {
                padding: 15px;
            }
            
            .controls {
                flex-direction: column;
            }
            
            button {
                width: 100%;
            }
        }

        .menu {
            display: flex;
            justify-content: center;
            gap: 15px;
            margin-top: 20px;
        }

        .menu-btn {
            padding: 10px 20px;
            background: var(--primary-color);
            color: white;
            text-decoration: none;
            border-radius: 5px;
            transition: var(--transition);
        }

        .menu-btn:hover {
            transform: translateY(-2px);
            box-shadow: var(--shadow);
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h2>Sistem Penguncian Pintu Cerdas</h2>
            <button class="logout-btn" onclick="logout()">Logout</button>
        </div>

        <div class="datetime">
            <div class="time" id="time">--:--:--</div>
            <div class="date" id="date">-- --- ----</div>
        </div>
        
        <div class="section">
            <h3>Status Tombol</h3>
            <div class="status" id="buttonStatus">Loading...</div>
        </div>
        
        <div class="section">
            <h3>Kontrol Pintu</h3>
            <div class="status" id="doorStatus">Loading...</div>
            <div class="controls">
                <button class="unlock" onclick="controlDoor('unlock')">
                    Buka Pintu
                </button>
                <button class="lock" onclick="controlDoor('lock')">
                    Kunci Pintu
                </button>
            </div>
        </div>

        <div class="menu">
            <a href="/cards" class="menu-btn">Manajemen Kartu RFID</a>
        </div>
    </div>
    <script>
        function updateDateTime() {
            fetch('/datetime')
                .then(response => response.text())
                .then(data => {
                    const [timeStr, dateStr] = data.split('|');
                    document.getElementById('time').textContent = timeStr;
                    document.getElementById('date').textContent = dateStr;
                });
        }

        function logout() {
            window.location.href = '/logout';
        }

        function controlDoor(action) {
            const button = event.target;
            button.style.transform = 'scale(0.95)';
            
            fetch('/' + action)
                .then(response => response.text())
                .then(data => {
                    document.getElementById('doorStatus').textContent = 'Status Pintu: ' + data;
                    setTimeout(() => button.style.transform = '', 200);
                });
        }
        
        // Update semua status setiap 500ms
        setInterval(() => {
            updateDateTime();
            
            fetch('/buttonstatus')
                .then(response => response.text())
                .then(data => {
                    const statusDiv = document.getElementById('buttonStatus');
                    statusDiv.textContent = 'Status Tombol: ' + data;
                    statusDiv.className = 'status ' + (data === 'TERTUTUP' ? 'pressed' : 'normal');
                })
                .catch(() => {
                    window.location.href = '/';
                });
                
            fetch('/doorstatus')
                .then(response => response.text())
                .then(data => {
                    document.getElementById('doorStatus').textContent = 'Status Pintu: ' + data;
                })
                .catch(() => {
                    window.location.href = '/';
                });
        }, 500);
    </script>
</body>
</html>
)=====";

// Konfigurasi WiFi - Ganti dengan kredensial WiFi Anda
const char* ssid = "cctv";
const char* password = "fawwaz2020";

// Kredensial untuk login web interface
const char* www_username = "admin";
const char* www_password = "babakan2";

// Definisi pin-pin yang digunakan
const int buttonPin = 12;     // Pin untuk push button
const int doorLock = 2;       // Pin untuk relay/solenoid door lock

// Pin-pin untuk RFID RC522
#define SS_PIN    5    // Serial Select Pin
#define RST_PIN   27   // Reset Pin
#define SCK_PIN   18   // Serial Clock Pin
#define MISO_PIN  19   // Master In Slave Out Pin
#define MOSI_PIN  23   // Master Out Slave In Pin

// Pin untuk DFPlayer
#define MP3_RX 16  // RX2
#define MP3_TX 17  // TX2

// Inisialisasi objek untuk setiap modul
Adafruit_SSD1306 display(128, 64, &Wire, -1);  // OLED 128x64 pixel
MFRC522 mfrc522(SS_PIN, RST_PIN);                 // RFID reader
WebServer server(80);                          // Web server di port 80
WiFiUDP ntpUDP;                               // UDP untuk NTP
NTPClient timeClient(ntpUDP, "pool.ntp.org"); // Client NTP
SoftwareSerial mySoftwareSerial(MP3_RX, MP3_TX);
DFRobotDFPlayerMini myDFPlayer;

// Mode operasi sistem
#define NORMAL_MODE 0
#define REGISTER_MODE 1
#define REGISTER_TIMEOUT 30000  // 30 detik timeout untuk registrasi

// Variabel global untuk status dan pengaturan
int operationMode = NORMAL_MODE;  // Mode operasi default
unsigned long registerModeTimeout = 0;  // Timeout untuk mode registrasi
bool doorStatus = false;  // Status pintu: false = terkunci, true = terbuka
unsigned long lastNTPUpdate = 0;  // Waktu terakhir update NTP
const unsigned long NTP_UPDATE_INTERVAL = 30000; // Interval update NTP (30 detik)
String lastRFIDRead = "";  // Menyimpan UID kartu terakhir yang dibaca
unsigned long doorOpenTime = 0; // Waktu ketika pintu dibuka
const unsigned long DOOR_OPEN_DURATION = 5000; // Durasi pintu terbuka (5 detik)
bool buttonState = false; // Status tombol: false = tidak ditekan, true = ditekan

// Variabel untuk animasi bola
int ballX = 0;          // Posisi X bola
int ballY = 60;         // Posisi Y bola (mulai dari bawah)
float ballSpeedX = 1;   // Kecepatan horizontal
float ballSpeedY = -3;  // Kecepatan vertical (negatif = ke atas)
const int BALL_SIZE = 5;// Ukuran bola
const int TOP_Y = 38;   // Batas atas (di bawah garis pembatas)
const int BOTTOM_Y = 63;// Batas bawah (diubah ke 63, ujung layar OLED)
unsigned long lastAnimationUpdate = 0;
const int ANIMATION_INTERVAL = 30; // Update animasi lebih cepat

// Variabel untuk teks marquee
const char scrollText[] = "SILAHKAN TEMPELKAN KARTU    ";  // Spasi di akhir untuk jeda
int textX = 128;  // Mulai dari kanan layar
const int TEXT_Y = 38;  // Posisi Y teks
unsigned long lastScrollUpdate = 0;
const int SCROLL_INTERVAL = 30;  // Kecepatan scroll dipercepat sedikit

// Variabel untuk status RFID
bool showRFIDStatus = false;
String rfidMessage = "";
unsigned long rfidDisplayTime = 0;
const unsigned long RFID_DISPLAY_DURATION = 2000; // Tampilkan status RFID selama 2 detik

// Sound file numbers
#define SOUND_ACCESS_GRANTED 1  // 0001.mp3
#define SOUND_ACCESS_DENIED 2   // 0002.mp3
#define SOUND_CARD_REGISTERED 3 // 0003.mp3

// Fungsi untuk menggambar animasi bola memantul
void drawAnimation() {
  if (millis() - lastAnimationUpdate >= ANIMATION_INTERVAL) {
    // Update posisi bola
    ballX += ballSpeedX;
    ballY += ballSpeedY;
    
    // Pantulan di batas atas
    if (ballY <= TOP_Y) {
      ballY = TOP_Y;
      ballSpeedY = -ballSpeedY;
    }
    
    // Pantulan di batas bawah
    if (ballY >= BOTTOM_Y) {
      ballY = BOTTOM_Y;
      ballSpeedY = -ballSpeedY;
    }
    
    // Reset posisi ketika mencapai ujung kanan
    if (ballX >= display.width()) {
      ballX = 0;
      ballY = BOTTOM_Y;
    }
    
    lastAnimationUpdate = millis();
  }
  
  // Gambar bola
  display.fillCircle(ballX, ballY, BALL_SIZE/2, WHITE);
  
  // Gambar garis tanah
  display.drawLine(0, BOTTOM_Y, display.width(), BOTTOM_Y, WHITE);
}

// Fungsi untuk menggambar teks marquee
void drawScrollingText() {
  if (millis() - lastScrollUpdate >= SCROLL_INTERVAL) {
    textX--;
    // Reset posisi teks ketika sudah keluar layar
    int textWidth = strlen(scrollText) * 6;  // 6 pixel per karakter
    if (textX < -textWidth) {
      textX = display.width();
    }
    lastScrollUpdate = millis();
  }
  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  // Gambar teks utama
  display.setCursor(textX, TEXT_Y);
  display.print(scrollText);
  
  // Gambar teks berikutnya untuk efek marquee yang mulus
  if (textX < 0) {
    display.setCursor(textX + strlen(scrollText) * 6, TEXT_Y);
    display.print(scrollText);
  }
}

// Fungsi untuk menampilkan status RFID
void showRFIDMessage(String message, bool isAuthorized) {
  showRFIDStatus = true;
  rfidMessage = message;
  rfidDisplayTime = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Status RFID:");
  
  // Garis pembatas
  display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
  
  // Tampilkan pesan di tengah
  display.setCursor(0, 25);
  display.setTextSize(1);
  display.println(message);
  
  // Tampilkan status akses
  display.setCursor(0, 45);
  display.println(isAuthorized ? "Akses Diterima" : "Akses Ditolak");
  
  display.display();
}

// Fungsi untuk update tampilan OLED
void updateOLED() {
  // Jika sedang menampilkan status RFID, cek apakah waktunya sudah habis
  if (showRFIDStatus) {
    if (millis() - rfidDisplayTime >= RFID_DISPLAY_DURATION) {
      showRFIDStatus = false;
    } else {
      return; // Jangan update tampilan utama jika masih menampilkan status RFID
    }
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  
  // Status tombol
  display.setCursor(0,0);
  display.print("Status: ");
  display.println(digitalRead(buttonPin) == LOW ? "TERKUNCI" : "TERBUKA");
  
  // Tampilkan waktu dan tanggal
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&epochTime);
  
  char timeStr[20];
  char dateStr[20];
  
  // Format waktu dan tanggal
  sprintf(timeStr, "%02d:%02d:%02d", ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  sprintf(dateStr, "%02d/%02d/%04d", ptm->tm_mday, ptm->tm_mon+1, ptm->tm_year+1900);
  
  // Hitung posisi tengah untuk jam dan tanggal
  int16_t timeWidth = strlen(timeStr) * 6; // Perkiraan lebar teks (6 pixel per karakter)
  int16_t dateWidth = strlen(dateStr) * 6;
  int16_t timeX = (display.width() - timeWidth) / 2;
  int16_t dateX = (display.width() - dateWidth) / 2;
  
  // Tampilkan jam dan tanggal di tengah
  display.setCursor(timeX, 15);
  display.print(timeStr);
  display.setCursor(dateX, 25);
  display.print(dateStr);
  
  // Garis pembatas di bawah jam
  display.drawLine(0, 35, display.width(), 35, SSD1306_WHITE);
  
  // Tambahkan teks marquee
  drawScrollingText();
  
  // Tambahkan animasi bola
  drawAnimation();
  
  display.display();
}

// Fungsi untuk mengecek kartu RFID
void checkRFID() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  
  // Baca UID kartu
  String cardUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    cardUID += String(mfrc522.uid.uidByte[i], HEX);
  }
  cardUID.toUpperCase();
  
  if (operationMode == REGISTER_MODE) {
    // Mode registrasi - simpan kartu baru
    if (numCards < MAX_CARDS) {
      // Cek apakah kartu sudah terdaftar
      bool alreadyRegistered = false;
      for (int i = 0; i < numCards; i++) {
        bool match = true;
        for (int j = 0; j < 4; j++) {
          if (mfrc522.uid.uidByte[j] != registeredCards[i].uid[j]) {
            match = false;
            break;
          }
        }
        if (match) {
          alreadyRegistered = true;
          break;
        }
      }

      if (!alreadyRegistered) {
        // Simpan kartu baru
        for (int i = 0; i < 4; i++) {
          registeredCards[numCards].uid[i] = mfrc522.uid.uidByte[i];
        }
        String cardName = "Kartu " + String(numCards + 1);
        strncpy(registeredCards[numCards].name, cardName.c_str(), 19);
        registeredCards[numCards].name[19] = '\0';
        registeredCards[numCards].active = true;
        numCards++;
        saveCardsToEEPROM();
        
        showRFIDMessage("Kartu Baru Terdaftar:\n" + cardUID, true);
        playSound(SOUND_ACCESS_GRANTED);
      } else {
        showRFIDMessage("Kartu sudah\nterdaftar", false);
        playSound(SOUND_ACCESS_DENIED);
      }
    } else {
      showRFIDMessage("Jumlah kartu maksimum\ntercapai", false);
      playSound(SOUND_ACCESS_DENIED);
    }
    
    // Kembali ke mode normal
    operationMode = NORMAL_MODE;
    
  } else {
    // Mode normal - cek akses
    bool isAuthorized = false;
    for (int i = 0; i < numCards; i++) {
      if (registeredCards[i].active) {
        bool match = true;
        for (int j = 0; j < 4; j++) {
          if (mfrc522.uid.uidByte[j] != registeredCards[i].uid[j]) {
            match = false;
            break;
          }
        }
        if (match) {
          isAuthorized = true;
          break;
        }
      }
    }
    
    if (isAuthorized) {
      doorStatus = true;
      doorOpenTime = millis();
      digitalWrite(doorLock, HIGH);
      showRFIDMessage("Akses Diterima:\n" + cardUID, true);
      playSound(SOUND_ACCESS_GRANTED);
    } else {
      showRFIDMessage("Akses Ditolak:\n" + cardUID, false);
      playSound(SOUND_ACCESS_DENIED);
    }
  }
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void playSound(uint8_t soundNumber) {
  if (myDFPlayer.available()) {
    myDFPlayer.play(soundNumber);
  }
}

// Fungsi untuk mengecek status button
void checkButton() {
  buttonState = (digitalRead(buttonPin) == LOW);
  if (buttonState) {  // Button ditekan
    Serial.println("Status Tombol: DITEKAN");
  }
}

// Fungsi untuk mengecek timer pintu
void checkDoorTimer() {
  if (doorStatus && (millis() - doorOpenTime >= DOOR_OPEN_DURATION)) {
    doorStatus = false;
    digitalWrite(doorLock, LOW);
    Serial.println("Pintu otomatis terkunci setelah 5 detik");
  }
}

// Fungsi untuk mengubah status pintu
void toggleDoor() {
  if (!doorStatus) {  // Jika pintu sedang terkunci
    doorStatus = true;
    doorOpenTime = millis();  // Catat waktu pembukaan
    digitalWrite(doorLock, HIGH);
  }
}

// ====== Web Server Handler Functions ======

// Handler untuk halaman utama
void handleRoot() {
  // Cek autentikasi
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
  server.send(200, "text/html", loginPage);
}

// Handler untuk proses login
void handleLogin() {
  // Cek autentikasi
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
    
  // Validasi kredensial login
  if (server.hasArg("username") && server.hasArg("password")) {
    if (server.arg("username") == www_username && server.arg("password") == www_password) {
      server.send(200, "text/html", controlPage);
    } else {
      server.send(401, "text/plain", "Kredensial tidak valid");
    }
  }
}

// Handler untuk halaman kontrol
void handleControl() {
  // Cek autentikasi
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
  server.send(200, "text/html", controlPage);
}

// Handler untuk mendapatkan status tombol
void handleButtonStatus() {
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
  
  // Kirim status berdasarkan kondisi tombol
  server.send(200, "text/plain", digitalRead(buttonPin) == LOW ? "TERTUTUP" : "TERBUKA");
}

// Handler untuk mendapatkan status pintu
void handleDoorStatus() {
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
  
  server.send(200, "text/plain", doorStatus ? "UNLOCKED" : "LOCKED");
}

// Handler untuk mengunci pintu
void handleLock() {
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
  
  doorStatus = false;
  digitalWrite(doorLock, LOW);
  server.send(200, "text/plain", "LOCKED");
}

// Handler untuk membuka pintu
void handleUnlock() {
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
  
  doorStatus = true;
  doorOpenTime = millis();  // Catat waktu pembukaan
  digitalWrite(doorLock, HIGH);
  server.send(200, "text/plain", "UNLOCKED");
}

// Handler untuk logout
void handleLogout() {
  server.sendHeader("Location", "/");
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("Set-Cookie", "ESPSESSIONID=0");
  server.send(301);
}

// Handler untuk mendapatkan waktu dan tanggal
void handleDateTime() {
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
  
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&epochTime);
  
  char timeStr[20];
  char dateStr[50];
  char response[100];
  
  // Format waktu HH:MM:SS
  sprintf(timeStr, "%02d:%02d:%02d", ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  
  // Format tanggal dengan nama bulan
  const char* months[] = {"Januari", "Februari", "Maret", "April", "Mei", "Juni", 
                         "Juli", "Agustus", "September", "Oktober", "November", "Desember"};
  sprintf(dateStr, "%d %s %d", ptm->tm_mday, months[ptm->tm_mon], ptm->tm_year + 1900);
  
  // Gabungkan waktu dan tanggal dengan separator |
  sprintf(response, "%s|%s", timeStr, dateStr);
  
  server.send(200, "text/plain", response);
}

// Fungsi untuk memuat data kartu dari EEPROM
void loadCardsFromEEPROM() {
  numCards = EEPROM.read(CARDS_START_ADDR);
  if (numCards > MAX_CARDS) numCards = 0;
  
  for (int i = 0; i < numCards; i++) {
    int addr = CARDS_START_ADDR + 1 + (i * sizeof(RFIDCard));
    EEPROM.get(addr, registeredCards[i]);
  }
}

// Fungsi untuk menyimpan data kartu ke EEPROM
void saveCardsToEEPROM() {
  EEPROM.write(CARDS_START_ADDR, numCards);
  for (int i = 0; i < numCards; i++) {
    int addr = CARDS_START_ADDR + 1 + (i * sizeof(RFIDCard));
    EEPROM.put(addr, registeredCards[i]);
  }
  EEPROM.commit();
}

// Handler untuk menampilkan daftar kartu
void handleListCards() {
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
    
  String content = "<html><head>";
  content += "<title>Manajemen Kartu RFID</title>";
  content += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  content += "<style>";
  content += "body { font-family: Arial; margin: 0; padding: 20px; background: #f0f0f0; }";
  content += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
  content += "table { width: 100%; border-collapse: collapse; margin: 20px 0; }";
  content += "th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }";
  content += "th { background: #4CAF50; color: white; }";
  content += ".btn { padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer; margin: 5px; }";
  content += ".btn-add { background: #4CAF50; color: white; }";
  content += ".btn-delete { background: #f44336; color: white; }";
  content += ".btn-back { background: #2196F3; color: white; }";
  content += ".btn-register { background: #FF9800; color: white; }";
  content += ".header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }";
  content += ".actions { display: flex; gap: 10px; }";
  content += "#registerStatus { margin: 20px 0; padding: 10px; border-radius: 4px; text-align: center; }";
  content += ".status-waiting { background: #FFF3E0; color: #E65100; }";
  content += ".status-ready { background: #E8F5E9; color: #2E7D32; }";
  content += "</style></head><body>";
  content += "<div class='container'>";
  content += "<div class='header'>";
  content += "<h2>Manajemen Kartu RFID</h2>";
  content += "<div class='actions'>";
  content += "<button class='btn btn-back' onclick='window.location.href=\"/control\"'>Kembali ke Panel Kontrol</button>";
  content += "<button class='btn btn-register' onclick='startRegister()'>Daftarkan Kartu Baru</button>";
  content += "<button class='btn btn-add' onclick='showAddForm()'>Tambah Kartu Manual</button>";
  content += "</div>";
  content += "</div>";
  
  content += "<div id='registerStatus' class='status-ready'>Siap mendaftarkan kartu baru</div>";
  
  // Tabel kartu yang terdaftar
  content += "<table><tr><th>UID</th><th>Nama</th><th>Status</th><th>Aksi</th></tr>";
  for (int i = 0; i < numCards; i++) {
    if (registeredCards[i].active) {
      content += "<tr><td>";
      for (int j = 0; j < 4; j++) {
        if (registeredCards[i].uid[j] < 0x10) content += "0";
        content += String(registeredCards[i].uid[j], HEX);
      }
      content += "</td><td>" + String(registeredCards[i].name) + "</td>";
      content += "<td>Aktif</td>";
      content += "<td><button class='btn btn-delete' onclick='deleteCard(" + String(i) + ")'>Hapus</button></td></tr>";
    }
  }
  content += "</table>";
  
  // Form untuk menambah kartu manual
  content += "<div id='addForm' style='display:none;'>";
  content += "<h3>Tambah Kartu Manual</h3>";
  content += "<form action='/addcard' method='POST'>";
  content += "UID Kartu (8 karakter hex): <input type='text' name='uid' pattern='^[0-9A-Fa-f]{8}$' required><br>";
  content += "Nama: <input type='text' name='name' maxlength='19' required><br>";
  content += "<input type='submit' value='Tambah Kartu' class='btn btn-add'>";
  content += "</form></div>";
  
  // JavaScript untuk interaktivitas
  content += "<script>";
  content += "let checkStatusInterval;";
  
  content += "function showAddForm() {";
  content += "  document.getElementById('addForm').style.display = 'block';";
  content += "}";
  
  content += "function startRegister() {";
  content += "  fetch('/startregister').then(response => {";
  content += "    if(response.ok) {";
  content += "      checkRegisterStatus();";
  content += "    }";
  content += "  });";
  content += "}";
  
  content += "function checkRegisterStatus() {";
  content += "  let statusElement = document.getElementById('registerStatus');";
  content += "  if (!statusElement) {";
  content += "    statusElement = document.createElement('div');";
  content += "    statusElement.id = 'registerStatus';";
  content += "    document.querySelector('.btn-register').after(statusElement);";
  content += "  }";
  content += "  fetch('/registerstatus').then(response => response.text()).then(status => {";
  content += "    statusElement.textContent = status;";
  content += "    if (status !== 'Timeout' && status !== 'Siap') {";
  content += "      setTimeout(checkRegisterStatus, 1000);";
  content += "    }";
  content += "  });";
  content += "}";
  
  content += "function deleteCard(index) {";
  content += "  if(confirm('Anda yakin ingin menghapus kartu ini?')) {";
  content += "    fetch('/deletecard', {";
  content += "      method: 'POST',";
  content += "      headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
  content += "      body: 'index=' + index";
  content += "    }).then(() => window.location.reload());";
  content += "  }";
  content += "}";
  content += "</script>";
  content += "</div></body></html>";
  
  server.send(200, "text/html", content);
}

// Handler untuk menambah kartu baru
void handleAddCard() {
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
    
  if (numCards >= MAX_CARDS) {
    server.send(400, "text/plain", "Jumlah kartu maksimum tercapai");
    return;
  }
  
  String uidStr = server.arg("uid");
  String name = server.arg("name");
  
  // Konversi string UID ke bytes
  for (int i = 0; i < 4; i++) {
    String byteStr = uidStr.substring(i*2, i*2+2);
    registeredCards[numCards].uid[i] = strtol(byteStr.c_str(), NULL, 16);
  }
  
  // Simpan nama
  strncpy(registeredCards[numCards].name, name.c_str(), 19);
  registeredCards[numCards].name[19] = '\0';
  registeredCards[numCards].active = true;
  
  numCards++;
  saveCardsToEEPROM();
  
  server.sendHeader("Location", "/cards");
  server.send(303);
}

// Handler untuk menghapus kartu
void handleDeleteCard() {
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
    
  int index = server.arg("index").toInt();
  if (index >= 0 && index < numCards) {
    // Hapus kartu dengan menggeser array
    for (int i = index; i < numCards - 1; i++) {
      memcpy(&registeredCards[i], &registeredCards[i+1], sizeof(RFIDCard));
    }
    numCards--;
    saveCardsToEEPROM();
  }
  
  server.send(200, "text/plain", "Kartu dihapus");
}

// Handler untuk memulai mode registrasi
void handleStartRegister() {
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
    
  operationMode = REGISTER_MODE;
  registerModeTimeout = millis() + REGISTER_TIMEOUT;
  
  server.send(200, "text/plain", "Mode registrasi diaktifkan");
}

// Handler untuk mengecek status registrasi
void handleRegisterStatus() {
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
    
  String status;
  if (operationMode == REGISTER_MODE) {
    unsigned long remainingTime = (registerModeTimeout - millis()) / 1000;
    if (remainingTime > 0) {
      status = "Menunggu kartu... " + String(remainingTime) + "s";
    } else {
      operationMode = NORMAL_MODE;
      status = "Timeout";
    }
  } else {
    status = "Siap";
  }
  
  server.send(200, "text/plain", status);
}

// Fungsi untuk setup - dijalankan sekali saat ESP32 mulai
void setup() {
  Serial.begin(115200);  // Inisialisasi Serial Monitor
  
  // Konfigurasi pin-pin GPIO
  pinMode(buttonPin, INPUT_PULLUP);  // Button dengan pull-up internal
  pinMode(doorLock, OUTPUT);         // Pin door lock sebagai output
  digitalWrite(doorLock, LOW);       // Mulai dengan pintu terkunci
  
  // Inisialisasi OLED Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Alamat I2C 0x3C
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);  // Jika gagal, program berhenti
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Inisialisasi RFID Reader
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);  // Mulai komunikasi SPI
  mfrc522.PCD_Init();  // Inisialisasi RFID reader
  
  // Inisialisasi DFPlayer
  mySoftwareSerial.begin(9600);
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println("DFPlayer Mini tidak terdeteksi");
  } else {
    Serial.println("DFPlayer Mini siap digunakan");
    myDFPlayer.setTimeOut(500);
    myDFPlayer.volume(25);  // Set volume (0-30)
  }
  
  // Koneksi ke WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  display.print("WiFi Connected Alamat IP : ");
  display.println(WiFi.localIP());
  display.display();
  delay(5000);

  
  // Konfigurasi NTP Client
  timeClient.begin();
  timeClient.setTimeOffset(25200);  // GMT+7 untuk Indonesia (7*3600 detik)
  
  // Setup route handler untuk web server
  server.on("/", HTTP_GET, handleRoot);           // Halaman utama
  server.on("/login", HTTP_POST, handleLogin);    // Handle login
  server.on("/control", HTTP_GET, handleControl); // Halaman kontrol
  server.on("/lock", HTTP_GET, handleLock);       // Endpoint untuk mengunci
  server.on("/unlock", HTTP_GET, handleUnlock);   // Endpoint untuk membuka
  server.on("/buttonstatus", HTTP_GET, handleButtonStatus); // Endpoint status tombol
  server.on("/doorstatus", HTTP_GET, handleDoorStatus);     // Endpoint status pintu
  server.on("/logout", HTTP_GET, handleLogout);   // Endpoint untuk logout
  server.on("/datetime", HTTP_GET, handleDateTime); // Endpoint untuk waktu dan tanggal
  
  // Tambah endpoint untuk manajemen kartu
  server.on("/cards", HTTP_GET, handleListCards);
  server.on("/addcard", HTTP_POST, handleAddCard);
  server.on("/deletecard", HTTP_POST, handleDeleteCard);
  server.on("/startregister", HTTP_GET, handleStartRegister);
  server.on("/registerstatus", HTTP_GET, handleRegisterStatus);
  
  server.begin();  // Mulai web server
  
  // Inisialisasi EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Baca data kartu dari EEPROM
  loadCardsFromEEPROM();
}

// Loop utama - dijalankan terus menerus
void loop() {
  server.handleClient();  // Handle permintaan web client
  checkRFID();           // Cek kartu RFID
  updateOLED();          // Update tampilan OLED
  checkButton();         // Cek status button
  checkDoorTimer();      // Cek timer pintu
  
  // Update waktu dari NTP server setiap interval tertentu
  if (millis() - lastNTPUpdate > NTP_UPDATE_INTERVAL) {
    timeClient.update();
    lastNTPUpdate = millis();
  }
  
  // Cek timeout mode registrasi
  if (operationMode == REGISTER_MODE && millis() > registerModeTimeout) {
    operationMode = NORMAL_MODE;
  }
}
