#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include "Nextion.h"  // Library untuk Nextion

// Konfigurasi Wi-Fi
const char* ssid = "ESP32-Server";
const char* password = "password123";

WiFiServer server(80);  

// Konfigurasi SD Card
#define SD_CS 5 // Pin CS untuk SD Card

// Pin untuk komunikasi serial dengan Nextion
#define RXD2 16  
#define TXD2 17  
HardwareSerial mySerial(2);

// Objek Nextion untuk Gauge dan Text
NexGauge GaugeFL = NexGauge(0, 1, "GaugeFL");
NexText TxtFL = NexText(0, 13, "TxtFL");
NexGauge GaugeRL = NexGauge(0, 2, "GaugeRL");
NexText TxtRL = NexText(0, 15, "TxtRL");
NexGauge GaugePLM = NexGauge(0, 3, "GaugePLM");
NexText TxtPLM = NexText(0, 17, "TxtPLM");
NexGauge GaugeFR = NexGauge(0, 4, "GaugeFR");
NexText TxtFR = NexText(0, 19, "TxtFR");
NexGauge GaugeRR = NexGauge(0, 30, "GaugeRR");
NexText TxtRR = NexText(0, 32, "TxtRR");
NexText TxtClient = NexText(0, 20, "TxtClient");
NexText TxtStatus = NexText(0, 21, "TxtStatus");
NexText TxtJam = NexText(0, 11, "TxtJam");
NexText TxtTanggal = NexText(0, 10, "TxtTanggal");
NexText TxtSDCard = NexText(0, 22, "TxtSDCard");

unsigned long lastDataTime = 0;
const unsigned long timeoutInterval = 10000;

void setup() {
  Serial.begin(115200);

  // Wi-Fi setup
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  Serial.println(WiFi.softAPIP());
  server.begin();

  // Nextion setup
  mySerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  nexSerial = mySerial;
  nexInit();
  TxtStatus.setText("System Started");

  // Inisialisasi SPI dan SD card
  SPI.begin(18, 19, 23, SD_CS); // SCK, MISO, MOSI, CS
  if (SD.begin(SD_CS)) {
    Serial.println("SD card successfully mounted.");
  } else {
    Serial.println("Failed to mount SD card.");
    TxtSDCard.setText("Failed");
  }
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    TxtStatus.setText("Client Connected");
    Serial.println("Client Connected");
    lastDataTime = millis();

    // Baca permintaan HTTP dari klien
    String request = client.readStringUntil('\r');
    client.flush();

    // Periksa apakah klien meminta file /data.csv
    if (request.indexOf("GET /data.csv") >= 0) {
      serveCSVFile(client); // Fungsi untuk melayani file data.csv
    } else {
      // Proses data biasa
      while (client.connected()) {
        if (client.available()) {
          String data = client.readStringUntil('\n');
          data.trim();

          Serial.println("Received Data: " + data);

          // Periksa apakah data dimulai dengan '#' dan diakhiri dengan '*'
          if (data.startsWith("#") && data.endsWith("*")) {
            data = data.substring(1, data.length() - 1); // Menghapus '#' dan '*'
            displayDataOnNextion(data);  // Proses dan simpan data ke SD Card
          } else {
            Serial.println("Invalid data format received.");
          }

          lastDataTime = millis();
        }

        // Timeout jika client tidak mengirimkan data dalam waktu tertentu
        if (millis() - lastDataTime > timeoutInterval) {
          Serial.println("Client disconnected due to timeout.");
          client.stop();
          TxtStatus.setText("Client Timeout");
          break;
        }
      }

      if (!client.connected()) {
        TxtStatus.setText("Waiting for data");
        Serial.println("Waiting for data");
      }
    }

    client.stop(); // Pastikan koneksi dihentikan setelah permintaan selesai
  }
}


void displayDataOnNextion(String data) {
  // Menghilangkan karakter '#' dan '*' dari data
  data.replace("#", "");
  data.replace("*", "");

  // Array untuk menyimpan nilai parsial data
  String parts[9] = {"0", "0", "0", "0", "0", "HD78101KM", "01/01/2025", "00:00:00", "0"}; // Default values

  // Parsing data berdasarkan tanda '-'
  int index = 0;
  while (data.indexOf('-') > 0 && index < 8) { // Index hingga 8 untuk mencakup tanggal, waktu, dan rit
    int pos = data.indexOf('-');
    parts[index] = data.substring(0, pos);
    data = data.substring(pos + 1);
    index++;
  }
  parts[index] = data; // Isi elemen terakhir

  // Pastikan semua bagian memiliki nilai (jika kosong, gunakan default)
  for (int i = 0; i <= 8; i++) {
    if (parts[i] == "") {
      if (i == 5) parts[i] = "HD78101KM";   // Nama unit tetap
      else if (i == 6) parts[i] = "01/01/2025"; // Default date
      else if (i == 7) parts[i] = "00:00:00";   // Default time
      else if (i == 8) parts[i] = "0";          // Default rit
      else parts[i] = "0";  // Default untuk data tekanan dan payload
    }
  }

  // Tampilkan data di Nextion
  TxtClient.setText(parts[5].c_str());  // Nama dump truck
  TxtPLM.setText(parts[4].c_str());  // Payload

  float payload = parts[4].toFloat();
  GaugePLM.setValue(mapGaugeValue(payload, 0, 101.1, 0, 180));  // Pemetaan payload

  TxtFL.setText(parts[0].c_str());   // Tekanan FL
  float pressureFL = parts[0].toFloat();
  GaugeFL.setValue(mapGaugeValue(pressureFL, 0, 99.99, 0, 180));

  TxtFR.setText(parts[1].c_str());   // Tekanan FR
  float pressureFR = parts[1].toFloat();
  GaugeFR.setValue(mapGaugeValue(pressureFR, 0, 99.99, 0, 180));

  TxtRL.setText(parts[2].c_str());   // Tekanan RL
  float pressureRL = parts[2].toFloat();
  GaugeRL.setValue(mapGaugeValue(pressureRL, 0, 99.99, 0, 180));

  TxtRR.setText(parts[3].c_str());   // Tekanan RR
  float pressureRR = parts[3].toFloat();
  GaugeRR.setValue(mapGaugeValue(pressureRR, 0, 99.99, 0, 180));

  // Simpan data ke SD Card
  saveDataToSD(parts);
}

void saveDataToSD(String parts[]) {
  // Format: CLIENT,DATE,TIME,RIT,PAYLOAD
  const char* filename = "/data.csv";
  File file = SD.open(filename, FILE_APPEND);

  if (!file) {
    Serial.println("Failed to open file for writing.");
    TxtSDCard.setText("SD Write Failed");
    return;
  }

  // Ambil data yang diperlukan
  String client = parts[5];    // Nama dump truck
  String payload = parts[4] + "t"; // Payload dengan satuan (t)
  String rit = parts[8];       // Rit
  String date = parts[6];      // Tanggal dalam format "dd/mm/yyyy"
  String time = parts[7];      // Waktu dalam format "hh:mm:ss"

  // Parsing tanggal (dd-mm-yyyy)
  String day, month, year;
  int firstDash = date.indexOf('-');
  int secondDash = date.indexOf('-', firstDash + 1);

  if (firstDash != -1 && secondDash != -1) {
    day = date.substring(0, firstDash);
    month = date.substring(firstDash + 1, secondDash);
    year = date.substring(secondDash + 1);
  } else {
    // Jika format tanggal tidak valid, gunakan default
    day = "01";
    month = "01";
    year = "2025";
  }

  // Pastikan waktu hanya memuat hh:mm:ss
  if (time.indexOf(':') == -1) {
    time = "00:00:00"; // Default jika format waktu tidak valid
  }

  // Buat baris data CSV
  String dataLine = client + "," + year + "-" + month + "-" + day + "," + time + "," + rit + "," + payload + "\n";

  // Tulis data ke file
  file.print(dataLine);

  Serial.println("Data saved to SD card: " + dataLine);
  TxtSDCard.setText("Data Saved");
  file.close();
}


int mapGaugeValue(float value, float in_min, float in_max, int out_min, int out_max) {
  if (value < in_min) value = in_min;
  if (value > in_max) value = in_max;
  return (int)((value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}


void serveCSVFile(WiFiClient& client) {
  const char* filename = "/data.csv";

  if (!SD.exists(filename)) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println("File not found");
    return;
  }

  File file = SD.open(filename, FILE_READ);
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/csv");
  client.println("Connection: close");
  client.println();

  while (file.available()) {
    client.write(file.read());
  }

  file.close();
}
