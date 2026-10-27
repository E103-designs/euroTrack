# Verifica tecnica — 20 ottobre 2026

## Ambiente e compilazione

- Target: `arduino:avr:nano:cpu=atmega328`, ATmega328P 16 MHz.
- Arduino CLI 1.5.1; Arduino AVR Boards 1.8.6; avr-gcc 7.3.0-atmel3.6.1-arduino7.
- Librerie locali: Adafruit SSD1306 2.5.17, Adafruit GFX 1.12.6, Adafruit BusIO 1.17.4.
- Build con `--warnings all`. Le quattro segnalazioni di parametri `tag` inutilizzati provengono da `cores/arduino/new.cpp`; nessuna segnalazione dal codice Eurotrack.

Comando equivalente dalla cartella che contiene lo sketch:

```sh
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328 \
  --build-path /private/tmp/eurotrack-v41-build --warnings all Eurotrack_V4_1
```

La V3.9 è stata compilata con lo stesso core, librerie e target, senza modificarne il sorgente. I numeri definitivi e gli hash sono in `release/build_manifest.json`.

| Memoria | V4.0 | V4.1 |
|---|---:|---:|
| Flash | 19.566 B | 20.066 B |
| RAM statica | 586 B | 594 B |
| Framebuffer sullo heap | 1.024 B | 1.024 B |
| RAM prima di overhead allocatore e stack | 438 B | 430 B |

L'integrazione aggiunge 500 byte di flash e 8 byte di RAM statica. Gli indirizzi e la dimensione dei record EEPROM restano invariati.

**RAM:** il riepilogo Arduino conteggia i globali ma non il framebuffer SSD1306 allocato con `malloc()`. Ai globali vanno quindi aggiunti **1024 byte** e l'overhead dell'allocatore. Il margine rimanente deve ospitare lo stack e le chiamate degli interrupt; il semplice «29% di memoria dinamica» non rappresenta l'occupazione totale. Serve un test del massimo uso di stack sul dispositivo.

## Test host eseguiti

`tests/run_tests.sh` compila il firmware incluso direttamente nel banco di test C++17 con AddressSanitizer e UndefinedBehaviorSanitizer. Usa la libreria **Adafruit GFX reale** per il disegno e sostituisce soltanto Arduino, EEPROM, I2C e framebuffer SSD1306 con adattatori di test. I risultati non equivalgono a un'emulazione del microcontrollore.

Esito: **PASS**, nessun errore dei sanitizzatori, per i seguenti scenari:

- Tutti i 7 metri: primo impulso, passaggi fra gruppi e numero esatto di sedicesimi per battuta; BPM a 120.
- Raggruppamento del 7/8 (4+4+6 sedicesimi).
- Tutti i 5 intervalli MEASURE OUT, su 34 battute; tutte le 4 durate di impulso.
- RESET ordinato dopo CLOCK già accodati; RESET che azzera anche il timer solo sul CLOCK successivo.
- Passaggio di `micros()` e `millis()` oltre il valore massimo a 32 bit.
- Ritorno da battuta 9999 a 1 senza alterare la cadenza del gate.
- Fronti ravvicinati scartati e overflow esplicito con ripartenza sincronizzata.
- Rimbalzo del pulsante, start/pausa/ripresa, pressione lunga, combinazione menu senza modifiche accidentali e navigazione.
- Priorità elettrica dell'uscita esterna rispetto a un vecchio impulso interno; ritorno al clock interno; soglia di timeout a 1500 ms; spegnimento del clock interno.
- Accumulo frazionario a 135 BPM per 1000 periodi, senza deriva cumulativa di arrotondamento.
- EEPROM vuota, importazione V3.9, interruzione del salvataggio a ogni fase del protocollo a due record, fallback in caso di corruzione di ogni singolo bit del record più recente (88 casi), avvolgimento del numero di sequenza su 300 salvataggi e nessuna riscrittura se i valori sono invariati.
- OLED: 64 transazioni dati da 16 byte più comando iniziale, contenuto del frame immutabile durante l'invio e funzionamento degradato dopo NACK.
- Rendering di 17 schermate (8 principali, 8 menu e un secondo valore della voce DISPLAY VIEW), ispezionate visivamente. Le anteprime provengono dalle funzioni del firmware, con il font Adafruit effettivo.

Nella verifica V4.0 è stata anche ispezionata la sequenza macchina AVR della scrittura EEPROM, il cui avvio hardware è rimasto invariato: i comandi EEMPE ed EEPE risultano due `sbi` consecutivi nella sezione atomica. Questo verifica il vincolo di avvio della programmazione sul target compilato. La simulazione host non modella il tempo di programmazione EEPROM né un brownout elettrico reale.

## Verifiche aggiunte nella V4.1

- Record EEPROM schema 1 della V4.0, con parametri non predefiniti e sequenza 255: importazione senza perdita delle impostazioni, passaggio della sequenza a 0 e aggiornamento anche a valori invariati.
- Interruzione del passaggio schema 1 → schema 2 in ogni fase di scrittura: al riavvio le impostazioni musicali restano identiche.
- Scelta TIMER mantenuta dopo salvataggio e riavvio; rifiuto di valori della vista fuori intervallo.
- Cambio tramite il pulsante TIMER nella voce DISPLAY VIEW: battuta, beat, suddivisione, stato e origine del timer, reset pendente, clock, scadenza interna e livelli delle uscite restano invariati. Viene soltanto programmato il salvataggio della scelta.
- Navigazione delle 8 voci, SAVE/EXIT nella posizione aggiornata e ritorno dalla voce 8 alla 1.
- Cambio di vista durante l'invio OLED: il framebuffer in trasmissione resta immutabile; la nuova vista viene disegnata al frame seguente.

## File PCB esaminati e limiti

Nella verifica iniziale V4.0 sono stati controllati gli elenchi dei file dei due ZIP indicati (BACK 05/10/2026 e FRONT 30/09/2026) e la distinta `BOM_euroTrack_2026-06-16.csv`, che identifica Nano e SSD1306. Gli ZIP contengono Gerber, forature e layer grafici; non è stato usato il file «How-to-order-PCB.txt» come istruzione operativa.

Non è stata fatta una ricostruzione della netlist dai tracciati né una verifica elettrica della PCB. La BOM è di giugno, precedente ai Gerber di settembre/ottobre. Per un riscontro definitivo dei pin e delle protezioni serve lo schema della revisione effettivamente assemblata, oppure una verifica di continuità sul prototipo.

## Collaudi ancora da eseguire sul prototipo

Queste prove non risultano eseguite; sono condizioni di qualifica da definire e registrare prima del rilascio commerciale.

1. **Temporizzazione con analizzatore logico:** misurare CLOCK IN, CLOCK THRU e MEASURE OUT insieme. Provare tutte le durate gate, 50/120/250 BPM, tutti i metri, apertura menu, ripetuti salvataggi e aggiornamenti display. Registrare errore massimo e distribuzione del jitter, fissando la specifica del prodotto prima dell'accettazione.
2. **Clock e reset:** provare impulsi stretti, duty-cycle estremi, scollegamento a livello HIGH/LOW, riavvio del master, RESET vicino ai fronti, passaggio esterno/interno e reset continuo. THRU è una copia software delle transizioni, non un buffer hardware a latenza nulla.
3. **Guasto display:** avvio senza OLED, NACK, scollegamento e linea SDA/SCL bloccata. Verificare che il timeout reale consenta ancora le prestazioni dichiarate; la reinizializzazione è intenzionalmente rinviata finché non si torna inattivi.
4. **Alimentazione e salvataggio:** togliere e ripristinare alimentazione durante ogni fase del salvataggio; verificare recupero di un record coerente. Controllare brownout/fuse effettivi e bootloader del Nano assemblato. Il CRC non sostituisce una corretta alimentazione.
5. **Memoria e durata:** misurare il massimo uso di stack con OLED, pulsanti e interrupt concorrenti; prova prolungata, inclusi i rollover e uso ripetuto del menu. Confrontare la stabilità del clock interno con una sorgente di riferimento: il firmware non corregge la tolleranza dell'oscillatore del Nano.
6. **Circuito assemblato:** verificare pin, polarità dei due pulsanti, livelli e protezioni degli ingressi/uscite secondo lo schema aggiornato; la verifica software non dimostra queste proprietà.

Non è stato caricato alcun firmware su una scheda e non sono state modificate le PCB o il sorgente V3.9 originale.
