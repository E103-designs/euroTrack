# Eurotrack V4.1 — distribuzione in un unico file

Questa versione risolve l'errore `SettingsStore.h: No such file or directory` incorporando il contenuto dell'header originale direttamente nel firmware. Non richiede file locali aggiuntivi: restano necessarie le normali librerie Arduino indicate sotto. La logica musicale, le due viste e il formato EEPROM della V4.1 sono invariati.

## Aprire o importare

- IDE desktop: estrarre lo ZIP e aprire `Eurotrack_V4_1_SingleFile/Eurotrack_V4_1_SingleFile.ino`. Il nome della cartella deve corrispondere al nome del file senza `.ino`.
- Editor web: creare/importare uno sketch con il contenuto completo di `Eurotrack_V4_1_SingleFile.ino`, sostituendo il vecchio sorgente. Il nuovo sorgente non contiene `#include "SettingsStore.h"`.
- Usare un nuovo sketch o sostituire quello esistente. Non aggiungere questo `.ino` come seconda scheda di codice accanto al vecchio `.ino`, perché conterrebbe un secondo `setup()` e un secondo `loop()`.

Target: Nano classico ATmega328P, 16 MHz, CLOCK 4 PPQN, SSD1306 I2C 128×64 a 0x3C. Librerie della compilazione verificata: Arduino AVR Boards 1.8.6, Adafruit SSD1306 2.5.17, Adafruit GFX 1.12.6, Adafruit BusIO 1.17.4.

Nel menu, **DISPLAY VIEW (7/8)** alterna BAR/BEAT e TIMER; SAVE/EXIT è la voce 8/8. La scelta viene conservata in EEPROM. Impostazioni e pin restano identici alla V4.1.

## Perché non usare l'header alternativo suggerito

Il problema segnalato è la mancanza di un file durante la compilazione. L'header alternativo modifica anche il comportamento:

- usa magic a indirizzo 0 e dati a partire da 1, sovrascrivendo i byte riservati alla migrazione V3.9;
- non legge i record V4.0/V4.1 a indirizzi 32 e 64;
- elimina CRC, doppio record e commit finale;
- usa `EEPROM.put()` sincrono e lascia `service()` vuoto;
- porta il tempo predefinito da 120 a 100 BPM e accetta intervalli gate non previsti, come 3 o 5.

Qui viene incorporata esattamente la classe originale, mantenendo salvataggio asincrono, controlli e migrazione.

## Verifica

La build AVR e l'intera suite di regressione V4.1 vengono eseguite sul nuovo file completo, con risultati registrati in `release/build_manifest.json`. Non viene caricato automaticamente alcun firmware sulla scheda. Restano da eseguire i collaudi hardware già indicati nel rapporto della V4.1.

La struttura degli sketch e i file aggiuntivi sono descritti nella [specifica ufficiale Arduino](https://docs.arduino.cc/arduino-cli/sketch-specification/).
