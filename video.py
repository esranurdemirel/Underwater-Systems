#!/usr/bin/env python3
import serial
import time
import sys
import threading

SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200

# Threading ile asenkron olay takibi için Event flag
DONE_FLAG = threading.Event()

def serial_listener(ser):
    """
    Arka planda sürekli Arduino'yu dinleyerek E-Stop durumlarını
    ve görev tamamlanma (DONE) bilgisini asenkron olarak alır.
    """
    while True:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line == "DONE":
                    DONE_FLAG.set()
                elif line == "ESTOP_ON":
                    print("\n[ACİL DURUM] NFC E-Stop AKTİF EDİLDİ! Tüm Görevler Beklemede (Motorlar 1480 PWM).")
                elif line == "ESTOP_OFF":
                    print("\n[ACİL DURUM] NFC E-Stop KAPATILDI! Görev Kaldığı Yerden Devam Ediyor.")
                elif line:
                    pass
        except Exception as e:
            pass
        time.sleep(0.01)

def connect_arduino():
    try:
        print(f"[BİLGİ] {SERIAL_PORT} üzerinden Arduino'ya bağlanılıyor...")
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        # Arduino boot süresi
        print("[BİLGİ] Arduino reset/arming bekleniyor (6 saniye)...")
        time.sleep(6)

        # Dinleyici thread'i başlat
        listener = threading.Thread(target=serial_listener, args=(ser,), daemon=True)
        listener.start()

        return ser
    except serial.SerialException as e:
        print(f"[HATA] Seri port açılamadı: {e}")
        sys.exit(1)

def send_command(ser, command):
    print(f"> Gönderiliyor: {command}")
    ser.write((command + "\n").encode('utf-8'))

def send_and_wait(ser, command):
    DONE_FLAG.clear()
    send_command(ser, command)
    print("  [BEKLENİYOR] 'DONE' yanıtı bekleniyor...")

    # DONE bayrağı set edilene kadar her saniye PING at (Timeout önleyici)
    while not DONE_FLAG.is_set():
        # wait(1.0) fonksiyonu, bayrak kalkarsa hemen True döner, kalkmazsa 1sn bekler False döner.
        event_happened = DONE_FLAG.wait(timeout=1.0)
        if not event_happened:
            # 1 Saniye doldu ve DONE hala gelmediyse Heartbeat PING gönder
            ser.write(("PING\n").encode('utf-8'))

    print(f"  [ALINDI] Görev Tamamlandı -> {command}")

def get_input(prompt, default_val):
    val = input(f"{prompt} [Varsayılan: {default_val}]: ").strip()
    return int(val) if val else default_val

def main():
    print("=== SU ALTI ARACI GÖREV YÖNETİCİSİ (SÜRE BAZLI) ===")

    # Kullanıcı Girdileri
    fwd_left = get_input("İleri gidiş Sol Motor PWM", 1600)
    fwd_right = get_input("İleri gidiş Sağ Motor PWM", 1600)

    turn_left = get_input("Sağ Dönüş Sol Motor Max PWM", 1600)
    turn_right = get_input("Sağ Dönüş Sağ Motor Max PWM", 1360)

    circle_left = get_input("Daire Çizme Sol Motor Max PWM", 1600)
    circle_right = get_input("Daire Çizme Sağ Motor Max PWM", 1360)

    # Süre Girdileri
    duration = get_input("İleri gitme süresi (Saniye)", 15)
    turn90_duration = get_input("90 Derece Sağ Dönüş Süresi (Saniye)", 3)
    circle360_duration = get_input("360 Derece Dönüş Süresi (Saniye)", 10)

    ser = connect_arduino()

    # Parametreleri Kur
    send_command(ser, f"SET_FORWARD {fwd_left} {fwd_right}")
    time.sleep(0.1)
    send_command(ser, f"SET_TURN {turn_left} {turn_right}")
    time.sleep(0.1)
    send_command(ser, f"SET_CIRCLE {circle_left} {circle_right}")
    time.sleep(0.1)

    # Başlangıç ve Geri Sayım (Görev hemen başlamaz)
    input("\n[HAZIR] Görevi başlatmak (geri sayımı tetiklemek) için ENTER'a basın...")

    print("\nGörev 5 saniye sonra başlayacak... (Aracı suya bırakın)")
    for i in range(5, 0, -1):
        print(i)
        time.sleep(1)
    print("Başlıyor...\n")

    # Görev Sırası (Sürelerle birlikte gönderiliyor)
    send_and_wait(ser, f"FORWARD_START {duration}")
    send_and_wait(ser, f"TURN90_RIGHT {turn90_duration}")
    send_and_wait(ser, f"FORWARD_START {duration}")
    send_and_wait(ser, f"CIRCLE360 {circle360_duration}")

    send_and_wait(ser, f"TURN90_RIGHT {turn90_duration}")
    send_and_wait(ser, f"FORWARD_START {duration}")
    send_and_wait(ser, f"TURN90_RIGHT {turn90_duration}")
    send_and_wait(ser, f"FORWARD_START {duration}")
    send_and_wait(ser, "STOP")

    print("\n[BİLGİ] Görev Tamamlandı. Araç Başlangıç Noktasında.")
    ser.close()

if __name__ == "__main__":
    main()