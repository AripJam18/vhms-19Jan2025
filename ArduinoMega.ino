#include <stdlib.h> // Untuk fungsi random()
#include <Messages.h>
unsigned long previousMillis1 = 0;
const long interval1 = 5000;

const uint8_t ESCIT = 0x10;
const uint8_t STX = 2;
const uint8_t ETX = 3;
const uint8_t maxMsgLen = 100;

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);  // Serial2 pin 16 dan 17 (pin 16 sambung ke pin 4 esp32 dan pin 17 sambung ke pin 5 esp32 client)

  Serial.println(F("Starting..."));

  Messages::printMessage();
}

void loop() {
  Messages::sendMessage(Serial2, previousMillis1, interval1);
  getVHMS(); // Panggil fungsi untuk menghasilkan data acak
  delay(1000);
}

// Fungsi untuk menghasilkan nilai acak dan memproses data seolah-olah dari sensor
void getVHMS() {
  uint8_t buffer[maxMsgLen];
  size_t len = 20; // Panjang data simulasi
  bool chkOk = true; // Anggap checksum selalu benar dalam simulasi

  // Mengisi buffer dengan nilai acak untuk mensimulasikan data sensor
  for (size_t i = 0; i < len; ++i) {
    buffer[i] = random(0, 100); // Mengisi buffer dengan nilai acak 0-255
  }

  // Membatasi nilai agar hasil konversi tekanan/payload maksimal adalah 101.0
  buffer[3] = random(0, 1020); // Front-Left pressure
  buffer[4] = buffer[3] >> 8;

  buffer[5] = random(0, 1020); // Front-Right pressure
  buffer[6] = buffer[5] >> 8;

  buffer[7] = random(0, 1020); // Rear-Left pressure
  buffer[8] = buffer[7] >> 8;

  buffer[9] = random(0, 1020); // Rear-Right pressure
  buffer[10] = buffer[9] >> 8;

  buffer[13] = random(0, 1020); // Payload
  buffer[14] = buffer[13] >> 8;

  // Panggil fungsi data_V dengan buffer simulasi
  data_V(buffer, len, chkOk);
}

void data_V(const uint8_t* buffer, size_t len, bool chkOk) {
  if (len == 20) {
    // Membuat satu string untuk menampung semua data
    String dataString = "";

    // Menambahkan Front-Left suspension pressure
    dataString += F("#");
    dataString += getPressureValue(buffer, 3);
    dataString += F("*-"); // Tambahkan tanda pemisah dan * di akhir

    // Menambahkan Front-Right suspension pressure
    dataString += F("#");
    dataString += getPressureValue(buffer, 5);
    dataString += F("*-"); // Tambahkan tanda pemisah dan * di akhir

    // Menambahkan Rear-Left suspension pressure
    dataString += F("#");
    dataString += getPressureValue(buffer, 7);
    dataString += F("*-"); // Tambahkan tanda pemisah dan * di akhir

    // Menambahkan Rear-Right suspension pressure
    dataString += F("#");
    dataString += getPressureValue(buffer, 9);
    dataString += F("*-"); // Tambahkan tanda pemisah dan * di akhir

    // Menambahkan Payload
    dataString += F("#");
    dataString += getPayloadValue(buffer, 13);
    dataString += F("*-"); // Tambahkan tanda pemisah dan * di akhir

    // Menambahkan Identitas Atau Nama Unit (Dumptruck)
    dataString += F("#HD78101KM*");

    // Bungkus dataString dengan Start dan End Frame
    Serial2.print((char)STX); // Kirim Start Frame
    Serial2.print(dataString); // Kirim data
    Serial2.print((char)ETX); // Kirim End Frame

    // Cetak data ke Serial Monitor untuk debugging
    Serial.print(F("Data sent to ESP32: "));
    Serial.print((char)STX);
    Serial.print(dataString);
    Serial.println((char)ETX);
  }
}

// Fungsi untuk mendapatkan nilai tekanan suspensi sebagai angka saja
String getPressureValue(const uint8_t* buffer, size_t pos) {
  uint16_t susP = buffer[pos] + (buffer[pos + 1] << 8);
  float pressureMPa = (susP * 0.1) * 0.0980665; // Konversi ke MPa
  if (pressureMPa > 101.0) {
    pressureMPa = 101.0; // Batas maksimal
  }
  return String(pressureMPa, 2); // Format dengan 2 digit desimal
}

// Fungsi untuk mendapatkan nilai payload sebagai angka saja
String getPayloadValue(const uint8_t* buffer, size_t pos) {
  uint16_t pay = buffer[pos] + (buffer[pos + 1] << 8);
  float payloadValue = pay * 0.1; // Konversi payload
  if (payloadValue > 101.0) {
    payloadValue = 101.0; // Batas maksimal
  }
  return String(payloadValue, 1); // Format dengan 1 digit desimal
}
