# euroTrack V4.1 — guida rapida

Modulo Eurorack per seguire battute, beat e durata della performance. La V4.1 del 20 ottobre 2026 e il firmware locale piu recente trovato; e un candidato compilato e verificato via software, con collaudo elettrico e temporale ancora da completare.

## Primo utilizzo

1. Collegare a CLKIN un clock compatibile a **4 PPQN**: un impulso per sedicesimo. A 120 BPM corrispondono 8 impulsi al secondo.
2. Il primo clock valido indica battuta 1 / beat 1 e genera il marker MEAS. L'uscita torna a generarlo ogni 1, 2, 4, 8 o 16 battute secondo il menu.
3. **T.S.** apre la selezione del metro; i clic successivi scorrono 4/4, 3/4, 6/8, 5/4, 7/8, 9/8 e 12/8. Dopo tre secondi la selezione si chiude. In 6/8 sono mostrati due gruppi di tre ottavi; in 7/8 tre gruppi 2+2+3.
4. **TIMER breve** avvia, mette in pausa o riprende. Tenuto circa un secondo ferma e azzera il tempo. Con TIME+BAR vengono riallineate anche le battute nelle azioni previste.
5. Tenere **T.S. e TIMER insieme per un secondo**, poi rilasciarli entrambi per il menu. T.S. sceglie la voce; TIMER modifica il valore.

## Otto voci di menu

1. MEASURE OUTPUT EVERY: marker ogni 1/2/4/8/16 battute.
2. INTERNAL CLOCK: ON/OFF; torna sempre OFF all'accensione. Per funzionare deve essere ON e il timer deve essere RUN.
3. INTERNAL TEMPO: 50–250 BPM a passi di 5; default 120.
4. MEASURE PULSE LENGTH: durata richiesta 5/10/20/50 ms.
5. RESET INPUT RESETS: solo BAR oppure BAR+TIME. L'azione avviene al clock successivo; BAR+TIME ferma e azzera anche il timer.
6. TIMER BUTTON CONTROLS: TIME oppure TIME+BAR.
7. DISPLAY VIEW: BAR/BEAT oppure TIMER. Cambia solo la vista, senza azzerare o mettere in pausa.
8. STORE SETTINGS: TIMER salva/esce.

Le impostazioni persistenti si salvano automaticamente dopo un secondo dall'ultima modifica: SAVE/EXIT non annulla i cambiamenti gia fatti. Attendere che `*` scompaia prima di spegnere. Il clock interno abilitato non viene salvato; il BPM impostato si.

## Clock e reset

Il clock esterno e prioritario e continua a far avanzare le battute anche quando il timer e fermo/in pausa. Dopo 1500 ms senza fronti validi, viene segnalato NO CLK oppure riparte l'interno se abilitato e timer RUN. La ripartenza interna e immediata, senza previsione della fase esterna. CLKOUT replica i fronti esterni via software: non e un buffer a latenza nulla e non elimina i disturbi elettrici.

## Installare o modificare

Per l'installazione piu semplice usare [Eurotrack_V4_1_SingleFile.ino](../firmware/Eurotrack_V4_1_SingleFile/Eurotrack_V4_1_SingleFile.ino) nella propria cartella omonima. Per lo sviluppo e disponibile la [versione modulare](../firmware/Eurotrack_V4_1/Eurotrack_V4_1.ino), che richiede SettingsStore.h nella stessa cartella. Non unire i due sketch.

Target: Nano classico ATmega328P a 16 MHz; OLED SSD1306 I2C 128×64 a 0x3C. Dipendenze e comandi nella [guida di sviluppo](DEVELOPMENT.md); [manuale tecnico italiano originale](../firmware/Eurotrack_V4_1/README.md) e [rapporto di verifica](../firmware/Eurotrack_V4_1/VALIDATION.md) inclusi.

Gli schematici corretti e i sorgenti CAD vanno aggiunti in [hardware/schematics](../hardware/schematics/README.md). Non sono stati pubblicati vecchi BOM/Gerber come pacchetto hardware gia verificato. Range sicuri dei jack, alimentazione, corrispondenza PCB/pin, jitter e margine dello stack richiedono verifica sul modulo reale.
