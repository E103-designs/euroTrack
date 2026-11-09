# Verifica V4.2 - 8 novembre 2026

## Compilazione

Target `arduino:avr:nano:cpu=atmega328`, ATmega328P a 16 MHz. Arduino CLI 1.5.1, core Arduino AVR Boards 1.8.6, Adafruit SSD1306 2.5.17, Adafruit GFX 1.12.6, Adafruit BusIO 1.17.4.

Comando, dalla cartella Firmware:

```sh
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328 \
  --build-path /private/tmp/eurotrack-v42-build --warnings all Eurotrack_V4_2_SingleFile
```

Build riuscita. Le quattro segnalazioni di parametro `tag` inutilizzato nelle compilazioni iniziali provengono dal `new.cpp` del core AVR; non dal firmware. Dimensioni e hash definitivi sono in `release/build_manifest.json`.

La RAM statica riportata da Arduino non include il framebuffer SSD1306, allocato sullo heap: aggiungere **1024 byte**, oltre all'overhead dell'allocatore. Il margine prima di stack e allocatore e' indicato nel manifest. Questo valore non sostituisce la misura del massimo uso di stack sul Nano.

## Test simulati

`tests/run_tests.sh` compila direttamente l'intero `.ino` con C++17, AddressSanitizer e UndefinedBehaviorSanitizer. Usa la libreria Adafruit GFX effettiva; pin, timer, I2C, framebuffer ed EEPROM sono adattatori host. La prova non e' un'emulazione AVR e non modella elettricamente gli ingressi o i tempi di programmazione EEPROM.

Risultato: **PASS**, nessun errore dei sanitizzatori, per:

- Tutti i 7 metri: primo impulso, passaggio di battuta, conteggio dei sedicesimi e lettura BPM.
- MEASURE ogni 1/2/4/8/16 battute, su 34 battute per configurazione; gate 5/10/20/50 ms; scadenze oltre il rollover di micros().
- Ordine CLOCK/RESET e RESET BAR+TIME; filtro dei fronti ravvicinati; recupero dopo overflow.
- Pulsanti, pausa/ripresa, pressione lunga, combinazione menu, navigazione su tutte le 10 voci e cambio di vista.
- Clock interno a 135 BPM su 1000 periodi, accumulo frazionario e priorita' del CLOCK THRU esterno.
- Primo CLOCK dopo un intervallo superiore a un intero ciclo di micros(): il filtro viene riarmato quando scade la presenza del clock esterno.
- RUN LOW che impedisce l'avanzamento; HIGH che avvia da zero; LOW che conserva durata e posizione; nuovo HIGH che riallinea tutto. Comando TIMER disabilitato fuori dal menu mentre RUN ha il controllo.
- Durata ricavata dal timestamp dell'interrupt anche quando HIGH e LOW vengono elaborati in ritardo; rollover di micros() e millis() durante la performance.
- Ingresso gia' HIGH all'avvio del firmware e durante la selezione del modo RUN; clock interno che parte e si ferma con lo stesso comando.
- RUN verificato in tutte le 7 modalita' MEAS: durante LOW nessun avanzamento e nessun nuovo impulso, alla ripartenza primo passo e battuta 1.
- Tutte le modalita' CLOCK/SWING/preset con tutti i 6 valori swing su 64 passi, confrontando le uscite con liste di tempi e passi attesi indipendenti dalle maschere del firmware.
- Swing al 75% e gate richiesto da 50 ms a 250 BPM: durata accorciata e spazio LOW positivo fra tutti gli impulsi.
- Cancellazione dello swing interno pendente anche con pausa manuale del timer.
- Cancellazione dello swing pendente con un LOW/HIGH arrivato prima del loop; nessun riuso di impulsi di una performance precedente grazie al controllo dell'epoca RUN.
- ISR di discesa RUN richiamata durante una transazione OLED simulata: MEAS va LOW nel callback, prima di completare la transazione.
- Scarto di eventi ritardati oltre 5 ms, senza raffiche di recupero; cambio modo, RESET e cambiamento di fase dell'uscita.
- EEPROM vuota, scritture interrotte a ogni fase con un record valido precedente, 88 corruzioni di singolo bit del record recente, ripiego sul record precedente, 300 salvataggi oltre il rollover di sequenza, assenza di scritture con parametri invariati e rifiuto di valori fuori intervallo.
- Importazione record V4.0 e V4.1, con parametri non predefiniti e sequenza 255; interruzione della migrazione allo schema 3 in ogni fase, senza modificare le impostazioni musicali precedenti.
- Frame OLED immutabile durante il trasferimento e sospensione delle trasmissioni dopo NACK.
- Rendering di 24 schermate, inclusi entrambi i layout, STOP/RUN, tutte le voci e tutti i preset. Controllo visivo dei limiti 10/10, 9999 battute, 99:59:59, selezione metro e indicatore di salvataggio.

## Struttura EEPROM

Record di 11 byte ai medesimi indirizzi 32 e 64. Magic 0xE4, versione 3, sequenza, parametri, CRC-8, commit finale 0xA5. Sono leggibili anche le versioni 1 e 2. I parametri nuovi occupano i bit riservati nel byte 7:

- bit 0..1: RESET INPUT MODE, valori validi 0..2;
- bit 2..4: MEAS OUTPUT MODE, valori validi 0..6;
- bit 5..7: SWING, indice 0..5 corrispondente a 50..75%.

Il byte 8 mantiene TIMER MODE e DISPLAY VIEW. La validazione controlla anche i bit riservati nei record vecchi. Un cambio intenzionale di ruolo RESET/RUN scarta il vecchio contesto di eventi, acquisisce il livello presente e arresta/riparte il trasporto in modo coerente.

## Schema consultato

E' stata ispezionata visivamente la pagina dello schema del 28 ottobre presente in `EUROTRACK/euroTrack SCHEMATIC`. RST e' collegato a D3 tramite R3; C7 e C8 sono verso massa, non in serie sul percorso del segnale. Il percorso rappresentato e' quindi compatibile con la lettura di un livello mantenuto. Si tratta di una lettura dello schema, non di una misura della PCB assemblata; i condensatori e le impedenze reali influiscono sui fronti rilevati.

## Da collaudare sul Nano

Non e' stato caricato alcun firmware su hardware. Prima del rilascio occorre:

1. Collegare il RUN reale del sequencer e misurare HIGH/LOW, timer, BAR/BEAT e MEAS insieme: prove di stop durante menu, OLED e salvataggi, HIGH all'accensione e ripartenze ravvicinate.
2. Misurare jitter e durata di CLOCK, SWING 50..75% e preset con analizzatore logico; confrontare anche le forme d'onda sul jack dopo il circuito di uscita. Il controllo del tempo e' cooperativo: non si dichiara precisione al microsecondo.
3. Provare cambi improvvisi di tempo, perdita CLOCK, ritorno al clock interno, impulsi sporchi e overflow. Transizioni troppo rapide o non rilevate dal circuito non possono essere ricostruite dal firmware.
4. Verificare timeout I2C e RUN LOW con OLED assente o linea bloccata. La reinizializzazione OLED resta rinviata ai periodi senza performance e senza clock attivo.
5. Interrompere fisicamente l'alimentazione durante i salvataggi e misurare il margine di stack. Il CRC e la simulazione del commit non qualificano da soli il comportamento durante brownout.

Il `.ino` V4.1 precedente e i suoi salvataggi sorgente non sono stati modificati. I record EEPROM precedenti saranno aggiornati soltanto quando il nuovo firmware gira realmente sulla scheda.
