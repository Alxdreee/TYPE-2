import serial
import sounddevice as sd
import sys
import time

SERIAL_PORT = "COM4" 
SAMPLE_RATE = 44100
CHANNELS = 2
FRAMES_PER_BLOCK = 512
BYTES_PER_BLOCK = FRAMES_PER_BLOCK * CHANNELS * 2

def main():
    print(f"Connexion au synthétiseur TYPE-2 sur {SERIAL_PORT}...")
    try:
        # DTR=True est OBLIGATOIRE sur l'USB Natif de l'ESP32-S3 pour qu'il accepte de transmettre
        ser = serial.Serial(SERIAL_PORT, baudrate=115200, timeout=1.0)
        ser.dtr = True 
        ser.rts = True
        time.sleep(0.5)
        ser.reset_input_buffer()
    except Exception as e:
        print(f"Impossible d'ouvrir {SERIAL_PORT}. Erreur : {e}")
        sys.exit(1)

    stream = sd.RawOutputStream(samplerate=SAMPLE_RATE, channels=CHANNELS, dtype='int16')
    stream.start()

    print("En attente du flux audio...")
    audio_actif = False

    try:
        while True:
            if not audio_actif:
                ser.write(b'A')
                ser.flush()
                time.sleep(0.3)
                
                # Vérifie si le synthé commence à répondre
                if ser.in_waiting > 0:
                    print("\n>>> FLUX AUDIO ACTIF ! Joue sur ton synthé <<<")
                    audio_actif = True
                    ser.reset_input_buffer()
            else:
                # Synchronisation pare-balles : on lit jusqu'à trouver la balise "TYP2"
                ser.read_until(b'TYP2')
                
                # On lit exactement le bloc audio de 2048 octets
                pcm_data = ser.read(BYTES_PER_BLOCK)
                if len(pcm_data) == BYTES_PER_BLOCK:
                    stream.write(pcm_data)
                    
    except KeyboardInterrupt:
        print("\nArrêt du pont audio...")
    finally:
        ser.write(b'S')
        time.sleep(0.1)
        ser.close()
        stream.stop()
        stream.close()

if __name__ == "__main__":
    main()