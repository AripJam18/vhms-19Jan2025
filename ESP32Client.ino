#include <WiFi.h>
#include "Nextion.h"
#include <Wire.h>
#include <SoftwareSerial.h>
#include "Adafruit_Thermal.h"

// WiFi credentials
const char* password = "password123";
char selectedSSID[32] = "";  // Placeholder for selected SSID

// Pin configurations
#define RX1 4  // Communication with Arduino Mega
#define TX1 5
#define RX2 16 // Communication with Nextion
#define TX2 17
#define PRINTER_RX 27 // Printer RX
#define PRINTER_TX 14 // Printer TX

// SoftwareSerial and Printer
SoftwareSerial printerSerial(PRINTER_RX, PRINTER_TX);
Adafruit_Thermal printer(&printerSerial);

// WiFi and client
WiFiClient client;

// Constants for start frame and end frame
#define STX 2  // Start of Text
#define ETX 3  // End of Text

// Buffers and variables
#define BUFFER_SIZE 10
String payloadBuffer[BUFFER_SIZE];
String buffer = "";  // Buffer untuk menampung data sementara
int bufferIndex = 0;
bool shouldSendData = false;
unsigned long startTime = 0;

// Unit name for the printer header
const char* unitName = "HD78140KM";

// Nextion components
NexButton BtnStart = NexButton(0, 1, "BtnStart");
NexButton BtnStop = NexButton(0, 2, "BtnStop");
NexButton BtnScan = NexButton(0, 25, "BtnScan");
NexCombo CmbSSID = NexCombo(0, 30, "CmbSSID");
NexText TxtStatus = NexText(0, 28, "TxtStatus");
NexText TxtSSID = NexText(0, 29, "TxtSSID");
NexText TxtData = NexText(0, 10, "TxtData");
NexText TxtKirim = NexText(0, 12, "TxtKirim");
NexText TxtJam = NexText(0, 3, "TxtJam");
NexNumber nRit = NexNumber(0, 27, "nRit");
NexText TxtClient = NexText(0, 15, "TxtClient");
NexText TxtTanggal = NexText(0, 14, "TxtTanggal");

NexTouch *nex_listen_list[] = {
  &BtnStart,
  &BtnStop,
  &BtnScan,
  NULL
};

// States
enum State {
  IDLE,
  CONNECTING,
  TRANSMITTING,
  DISCONNECTED
};
State currentState = IDLE;

// Button callbacks
void BtnScanPopCallback(void *ptr) {
  Serial.println("BtnScanPopCallback");

  TxtStatus.setText("Scanning for SSIDs...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    TxtStatus.setText("No networks found.");
    return;
  }

  String SSIDs = "";
  for (int i = 0; i < n; ++i) {
    if (i > 0) SSIDs += "\r\n";
    SSIDs += WiFi.SSID(i);
    Serial.println(WiFi.SSID(i));  // Debugging
  }

  String cmdTxt = String("CmbSSID.txt=\"") + String(n) + " Networks\"";
  sendCommand(cmdTxt.c_str());

  String cmdPath = String("CmbSSID.path=\"") + SSIDs + "\"";
  sendCommand(cmdPath.c_str());

  if (!recvRetCommandFinished()) {
    Serial.println("Error updating ComboBox.");
    TxtStatus.setText("Error updating ComboBox.");
    return;
  }

  TxtStatus.setText("Scan complete. Select SSID.");
}

void BtnStartPopCallback(void *ptr) {
  Serial.println("BtnStartPopCallback");
  CmbSSID.getSelectedText(selectedSSID, sizeof(selectedSSID));

  if (strcmp(selectedSSID, "Select SSID") == 0 || strlen(selectedSSID) == 0) {
    TxtStatus.setText("Select a valid SSID.");
    return;
  }

  Serial.printf("Selected SSID: %s\n", selectedSSID);
  TxtSSID.setText("Connecting to WiFi...");
  currentState = CONNECTING;
  TxtStatus.setText("CONNECTING");
}

void BtnStopPopCallback(void *ptr) {
  Serial.println("BtnStopPopCallback");
  stopConnection();
  // Hapus atau ganti dengan kode untuk menampilkan buffer
  printLast10Data();
}

void reconnect() {
  TxtStatus.setText("Reconnecting to server...");
  Serial.println("Attempting to reconnect to server...");

  int retries = 0;
  const int maxRetries = 5;
  while (!client.connect("192.168.4.1", 80)) {
    retries++;
    Serial.printf("Reconnect attempt %d/%d\n", retries, maxRetries);
    TxtStatus.setText("Reconnecting...");
    delay(1000);

    if (retries >= maxRetries) {
      TxtStatus.setText("Reconnect failed.");
      currentState = DISCONNECTED;
      return;
    }
  }
  currentState = TRANSMITTING;
}

void stopConnection() {
  TxtSSID.setText("Stopping connection...");
  shouldSendData = false;

  if (client.connected()) {
    client.stop();
    TxtSSID.setText("Disconnected from server.");
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    TxtSSID.setText("Disconnected from WiFi.");
  }

  delay(500);
  currentState = IDLE;
  TxtStatus.setText("IDLE");
}

void setup() {
  Serial.begin(115200);

  Serial1.begin(9600, SERIAL_8N1, RX1, TX1);
  Serial2.begin(9600, SERIAL_8N1, RX2, TX2);
  nexInit();

  printerSerial.begin(9600);
  printer.begin();
  printer.sleep();

  BtnStart.attachPop(BtnStartPopCallback, &BtnStart);
  BtnStop.attachPop(BtnStopPopCallback, &BtnStop);
  BtnScan.attachPop(BtnScanPopCallback, &BtnScan);

  TxtStatus.setText("System ready. Press SCAN.");
  Serial.println("System ready.");
}

void loop() {
  nexLoop(nex_listen_list);

  switch (currentState) {
    case IDLE:
      TxtStatus.setText("IDLE");
      break;

    case CONNECTING: {
      TxtStatus.setText("CONNECTING");
      Serial.printf("Connecting to WiFi: %s\n", selectedSSID);
      WiFi.begin(selectedSSID, password);

      unsigned long wifiTimeout = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - wifiTimeout < 10000) {
        delay(1000);
        TxtSSID.setText("Connecting...");
      }

      if (WiFi.status() == WL_CONNECTED) {
    TxtSSID.setText(WiFi.SSID().c_str());
    Serial.printf("Connected to WiFi: %s\n", WiFi.SSID().c_str());

    // Sambungkan WiFiClient ke server
    if (!client.connect("192.168.4.1", 80)) {
        Serial.println("Failed to connect to server.");
        TxtStatus.setText("Failed to connect to server.");
        currentState = DISCONNECTED;
        return;
       }
        Serial.println("WiFiClient connected to server.");
        TxtStatus.setText("TRANSMITTING");
        startTime = millis();
        shouldSendData = true;
        currentState = TRANSMITTING;
        } else {
        TxtSSID.setText("WiFi connection failed.");
        currentState = IDLE;
        TxtStatus.setText("IDLE");
       }

      break;
    }

    case TRANSMITTING:
        TxtStatus.setText("TRANSMITTING");

        if (!client.connected()) {
        Serial.println("Lost connection to server.");
        TxtStatus.setText("DISCONNECTED");
        currentState = DISCONNECTED;
        break;
        }

        // Proses data dari Serial1 (sesuai kode yang ada)
       while (Serial1.available()) {
        char c = Serial1.read();
        if (c == STX) {
            buffer = "";
        } else if (c == ETX) {
            processData(buffer);
            buffer = "";
        } else {
            buffer += c;
        }
      }
      break;

    case DISCONNECTED:
      TxtSSID.setText("DISCONNECTED");
      stopConnection();
      break;
  }

  delay(1000);
}

void processData(String data) {
    // Validasi data berdasarkan format yang diinginkan
    if (data.startsWith("#") && data.endsWith("#HD78101KM*")) {
        Serial.println("Data valid: " + data);
        TxtData.setText(data.c_str());  // Tampilkan data valid di TxtData

        // Ekstrak payload (angka kedua terakhir) dari data valid
        int lastSeparator = data.lastIndexOf('-');  // Posisi separator terakhir '-'
        int secondLastSeparator = data.lastIndexOf('-', lastSeparator - 1);  // Posisi separator kedua terakhir '-'

        if (secondLastSeparator != -1 && lastSeparator != -1) {
            // Potong data di antara separator kedua terakhir dan terakhir
            String payloadWithHash = data.substring(secondLastSeparator + 1, lastSeparator - 1); // Menghapus '*'
            
            // Hilangkan tanda '#' dari payload
            String payload = payloadWithHash.startsWith("#") ? payloadWithHash.substring(1) : payloadWithHash;

            // Simpan payload di buffer
            if (bufferIndex < BUFFER_SIZE) {
                payloadBuffer[bufferIndex++] = payload;
            } else {
                // Geser buffer jika penuh
                for (int i = 1; i < BUFFER_SIZE; i++) {
                    payloadBuffer[i - 1] = payloadBuffer[i];
                }
                payloadBuffer[BUFFER_SIZE - 1] = payload;
            }

            // Debugging: Cetak payload yang diekstrak
            Serial.println("Payload extracted: " + payload);
        } else {
            Serial.println("Error: Format data tidak sesuai untuk ekstraksi payload.");
        }

        // **Tambahkan tanggal, time, dan rit ke data**
        char dateBuffer[20];
        char timeBuffer[20];
        char ritBuffer[10];
        memset(dateBuffer, 0, sizeof(dateBuffer));
        memset(timeBuffer, 0, sizeof(timeBuffer));
        memset(ritBuffer, 0, sizeof(ritBuffer));

        // Ambil tanggal (TxtTanggal)
        if (TxtTanggal.getText(dateBuffer, sizeof(dateBuffer))) {
            cleanString(dateBuffer);  // Bersihkan string dari karakter tidak valid
        } else {
            strcpy(dateBuffer, "01-01-1970");  // Default jika gagal
        }

        // Ambil waktu (TxtJam)
        if (TxtJam.getText(timeBuffer, sizeof(timeBuffer))) {
            cleanString(timeBuffer);  // Bersihkan string dari karakter tidak valid
        } else {
            strcpy(timeBuffer, "00:00:00");  // Default jika gagal
        }

        // Ambil rit (nRit)
        uint32_t ritValue = 0;
        if (!nRit.getValue(&ritValue)) {
            ritValue = 0;  // Default jika gagal
        }
        sprintf(ritBuffer, "%d", ritValue);  // Konversi rit menjadi string

        // Tambahkan tanggal, time, dan rit ke data
        data += String("-#") + String(dateBuffer) + String("*-#") + String(timeBuffer) + String("*-#") + String(ritBuffer) + String("*");

        // Langsung kirim data ke server
        if (client.connected()) {
            Serial.printf("Sending data to server: %s\n", data.c_str());
            if (client.println(data)) {
                TxtKirim.setText(data.c_str());  // Tampilkan data yang dikirim
            } else {
                Serial.println("Send failed");
                TxtKirim.setText("Send failed");
                reconnect();
            }
        } else {
            Serial.println("Server not connected.");
            TxtKirim.setText("Server not connected.");
        }
    } else {
        // Data tidak valid
        Serial.println("Data tidak valid: " + data);
    }
}




// Fungsi tambahan untuk menampilkan data terakhir dari buffer (jika diinginkan)
void printLast10Data() {
    char timeBuffer[20];
    char clientName[50];
    char dateBuffer[20];
    memset(timeBuffer, 0, sizeof(timeBuffer));
    memset(clientName, 0, sizeof(clientName));
    memset(dateBuffer, 0, sizeof(dateBuffer));

    // Ambil nilai nRit sekali
    uint32_t ritValue = 0;
    if (nRit.getValue(&ritValue)) {
        Serial.printf("nRit: %d\n", ritValue);
    } else {
        Serial.println("Gagal mendapatkan nilai nRit.");
        ritValue = 0; // Nilai default jika gagal
    }

    // Ambil nilai TxtClient sekali
    if (TxtClient.getText(clientName, sizeof(clientName))) {
        cleanString(clientName); // Bersihkan string
        Serial.printf("Client Name: %s\n", clientName);
    } else {
        strcpy(clientName, "Unknown Client"); // Nilai default jika gagal
        Serial.println("Gagal mendapatkan nama client.");
    }

    // Ambil nilai TxtTanggal sekali
    if (TxtTanggal.getText(dateBuffer, sizeof(dateBuffer))) {
        cleanString(dateBuffer); // Bersihkan string
        Serial.printf("Tanggal: %s\n", dateBuffer);
    } else {
        strcpy(dateBuffer, "00-00-0000"); // Nilai default jika gagal
        Serial.println("Gagal mendapatkan tanggal.");
    }

    // Mulai mencetak ke printer
    printer.justify('C');                // Teks rata tengah
    printer.setSize('S');                // Ukuran teks medium
    // Cetak nama client dan tanggal
    printer.printf("Client: %s\n", clientName);
    printer.printf("Tanggal: %s\n", dateBuffer);
        // Cetak garis pemisah
    for (int i = 0; i < 30; i++) {
        printer.print('*'); // Cetak '-' satu per satu
    }
    printer.println();

    printer.justify('L');                // Teks rata kiri
    printer.setSize('S');                // Ukuran teks kecil

    // Header tabel
    printer.println("TIME     RIT     PAYLOAD");
        // Cetak garis pemisah
    for (int i = 0; i < 30; i++) {
        printer.print('*'); // Cetak '-' satu per satu
    }
    printer.println();

    // Cetak data dari buffer
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (!payloadBuffer[i].isEmpty()) {
            // Ambil nilai TxtJam setiap baris baru
            if (TxtJam.getText(timeBuffer, sizeof(timeBuffer))) {
                cleanString(timeBuffer); // Bersihkan dari karakter tidak valid
            } else {
                strcpy(timeBuffer, "00:00:00"); // Nilai default jika gagal
            }

            // Cetak data
            printer.printf("%s    %d       %s\n", timeBuffer, ritValue, payloadBuffer[i].c_str());
            Serial.printf("Buffer[%d]: TIME=%s RIT=%d PAYLOAD=%s\n", i, timeBuffer, ritValue, payloadBuffer[i].c_str());
            delay(100); // Tambahkan delay jika diperlukan
        }
    }

        // Cetak garis pemisah
    for (int i = 0; i < 30; i++) {
        printer.print('*'); // Cetak '-' satu per satu
    }
    printer.println();
    printer.setSize('L');
    printer.println(""); // Tambahkan baris kosong
    printer.sleep(); // Matikan printer setelah mencetak
    Serial.println("Pencetakan selesai.");
}

void cleanString(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        if (str[i] < 32 || str[i] > 126) { // ASCII dapat dicetak
            str[i] = '\0';
            break;
        }
    }
}



