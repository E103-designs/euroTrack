# Eurotrack V4.2

Firmware in un unico file per Nano classico ATmega328P, 16 MHz, OLED SSD1306 I2C 128x64 a 0x3C. CLOCK IN resta a **4 PPQN: un impulso per sedicesimo**. Preparato l'8 novembre 2026 sulla V4.1 presente nella cartella Firmware, senza modificare la versione precedente.

## Attivare RUN da RESET IN

1. Tenere entrambi i pulsanti per 1 secondo e rilasciarli.
2. Con METER raggiungere **RESET INPUT MODE**, voce **5/10**.
3. Con TIMER scegliere **RUN GATE**.
4. Raggiungere **SAVE/EXIT**, voce **10/10**, e confermare con TIMER.

In questa modalità RESET IN, pin D3, diventa il comando RUN del sequencer:

- **LOW -> HIGH:** il timer riparte da 00:00:00; BAR/BEAT tornano a 1/1 e il pattern MEAS riparte dal primo passo. Il primo CLOCK successivo identifica il primo sedicesimo. La durata del timer si misura dal fronte RUN, non dal primo CLOCK.
- **HIGH mantenuto:** timer, conteggio e MEAS procedono. CLOCK IN continua a fornire il tempo; con clock interno abilitato viene usato l'oscillatore interno secondo la normale priorità del clock esterno.
- **HIGH -> LOW:** timer e conteggio si fermano; la durata raggiunta e la posizione restano visibili. MEAS va LOW e gli impulsi swing ancora in attesa vengono cancellati. Lo stato a schermo è **STOP**.
- **Nuovo HIGH:** nuova performance da zero; non riprende il tempo accumulato.
- Se il firmware si avvia, o si seleziona RUN GATE, con l'ingresso già HIGH, il timer parte senza richiedere un ulteriore fronte.

In RUN GATE il jack ha il controllo del trasporto: i clic e la pressione lunga del pulsante TIMER non avviano, sospendono o azzerano la performance. TIMER continua a funzionare nel menu, e la combinazione per aprirlo resta disponibile. Il parametro TIMER BUTTON CONTROLS viene conservato ma è superato dal comando RUN.

Le altre opzioni di RESET INPUT MODE restano **BAR** e **BAR+TIME**, con reset applicato al CLOCK successivo come nella V4.1. Passare da una modalità di ingresso a un'altra arresta il timer, cancella le uscite programmate e acquisisce il nuovo contesto di ingresso. RUN GATE non cambia CLOCK THRU: in presenza di clock esterno, THRU continua a copiarne la forma d'onda, anche durante STOP.

## Nuove funzioni di MEAS OUT

Nel menu scegliere **MEAS OUTPUT MODE**, voce **8/10**. La stessa uscita D6 assume una delle seguenti funzioni:

| Modalità | Funzione |
|---|---|
| MEASURE | Impulso sulla prima battuta e ogni 1, 2, 4, 8 o 16 battute; comportamento precedente |
| CLOCK | Clock dritto a sedicesimi, 4 PPQN, sincronizzato alla sorgente attiva |
| SWING | Clock a sedicesimi con ritardo alternato regolabile |
| SAMBA | Variazione sincopata di 16 passi, ispirata al samba |
| BOSSA | Pattern di 32 passi ispirato alla bossa clave |
| SON 3-2 | Clave son 3-2 su 32 passi |
| TRESILLO | Tre attacchi su 16 passi, raggruppamento 3+3+2 ottavi |

**CLOCK SWING**, voce **9/10**, offre **50, 55, 60, 65, 70, 75%** e agisce soltanto in modalità SWING. Il valore iniziale è 60%; 50% è dritto. Si ritarda il secondo sedicesimo di ogni coppia mantenendo il successivo sedicesimo pari sulla griglia del master. A 120 BPM, 60% produce intervalli di 150 e 100 ms; 75% produce 187,5 e 62,5 ms. Il clock di ingresso e CLOCK THRU non vengono swingati.

I preset generano **una sola voce di trigger**, non un arrangiamento completo di percussioni. Le sequenze esatte, numerando da 1, sono:

| Preset | Lunghezza in sedicesimi | Passi con impulso |
|---|---:|---|
| SAMBA | 16 | 1, 4, 5, 7, 9, 12, 13, 15 |
| BOSSA | 32 | 1, 7, 13, 21, 27 |
| SON 3-2 | 32 | 1, 7, 13, 21, 25 |
| TRESILLO | 16 | 1, 7, 13 |

La griglia dei preset è fissa: 16 passi corrispondono a una battuta di 4/4, 32 a due battute. Non vengono riscalati quando si sceglie un altro metro, quindi nei metri dispari o composti possono produrre cicli che attraversano le battute. RESET, un nuovo RUN HIGH, il cambio di metro, il cambio di sorgente clock e il cambio di modalità MEAS riallineano la fase al primo passo. Cambiare il valore di swing cancella gli eventi pendenti e riallinea la fase dell'uscita.

**OUTPUT PULSE LENGTH**, voce **4/10**, conserva le durate 5/10/20/50 ms. Per CLOCK, SWING e preset la durata viene limitata a metà dell'intervallo più corto previsto, così resta uno spazio LOW tra impulsi; il primo impulso dopo l'acquisizione di una sorgente, prima di conoscere il periodo, è al massimo 5 ms. In MEASURE la durata resta quella selezionata. **MEASURE OUTPUT EVERY**, voce 1, ha effetto soltanto in MEASURE.

In RUN GATE anche i nuovi clock/preset si fermano a LOW. Nelle modalità BAR/BAR+TIME, MEAS continua a seguire CLOCK IN anche se il cronometro viene messo in pausa, come il conteggio precedente. Un impulso ritardato di oltre 5 ms rispetto alla scadenza viene scartato e segnalato con `!`, evitando raffiche di recupero. Con perdita della coda in RUN il trasporto viene arrestato; un nuovo ciclo LOW/HIGH lo risincronizza e cancella l'avviso.

## Due viste e menu

Le viste **BAR/BEAT** e **TIMER** restano selezionabili in **DISPLAY VIEW**, voce 7/10, con tutti gli elementi della V4.1. La vista TIMER mantiene HH:MM:SS grande. Il menu è ora:

| Voce | Parametro |
|---:|---|
| 1/10 | MEASURE OUTPUT EVERY |
| 2/10 | INTERNAL CLOCK |
| 3/10 | INTERNAL TEMPO |
| 4/10 | OUTPUT PULSE LENGTH |
| 5/10 | RESET INPUT MODE |
| 6/10 | TIMER BUTTON CONTROLS |
| 7/10 | DISPLAY VIEW |
| 8/10 | MEAS OUTPUT MODE |
| 9/10 | CLOCK SWING |
| 10/10 | STORE SETTINGS / SAVE/EXIT |

Le impostazioni vengono salvate automaticamente un secondo dopo l'ultima modifica. L'asterisco `*` resta visibile durante l'attesa e la scrittura. Attendere che scompaia prima di togliere alimentazione per conservare l'ultima modifica. Nuove installazioni e migrazioni mantengono RESET=BAR e MEAS=MEASURE: le funzioni nuove si attivano dal menu.

## Installazione

IDE desktop: aprire **Eurotrack_V4_2_SingleFile.ino come nuovo sketch**. Nel Cloud Editor puoi invece sostituire tutto il contenuto del vecchio sketch già configurato con il contenuto del nuovo `.ino`, conservando la scelta delle librerie. Se importi un nuovo sketch nel Cloud, configura nuovamente Adafruit BusIO come indicato sotto. Non aggiungere il file come seconda scheda di codice accanto alla V4.1. Il nome della cartella deve corrispondere al nome del file senza `.ino`; non serve SettingsStore.h.

Dipendenze verificate: Arduino AVR Boards 1.8.6, Adafruit SSD1306 2.5.17, Adafruit GFX 1.12.6, Adafruit BusIO 1.17.4. Nel Cloud Editor selezionare esplicitamente Adafruit BusIO 1.17.4 e premere INCLUDE se viene scelta EIS al suo posto; la sola presenza di più librerie non è l'errore, il conflitto si manifesta come `multiple definition`.

I pin restano D2 CLOCK IN, D3 RESET/RUN IN, D4 METER attivo LOW con pull-up, D5 CLOCK THRU, D6 MEAS OUT, D7 TIMER attivo HIGH con pull-down esterno, A4/A5 OLED.

I salvataggi V4.0 e V4.1 vengono letti e migrati allo schema 3 conservando i parametri precedenti. Doppio record, CRC, commit finale, scrittura asincrona, indirizzi 32/64 e dimensione di 11 byte restano presenti. Le impostazioni nuove vengono inserite nei bit prima riservati. Non cancellare l'EEPROM durante l'aggiornamento se si desidera migrare. Un firmware precedente non legge lo schema 3 e potrebbe recuperare un vecchio record rimasto, oppure le impostazioni V3.9 agli indirizzi 0..5.

## Verifica e limiti

Compilazione Nano, test simulati con sanitizzatori e controllo delle schermate sono riportati in VALIDATION.md e release/build_manifest.json. Nessun firmware è stato caricato sulla scheda. La temporizzazione degli impulsi è cooperativa con controllo fra piccoli blocchi OLED; solo la chiusura su RUN LOW è eseguita direttamente dall'ISR. Prima dell'uso sul prodotto vanno misurati jitter, forme d'onda e livelli con il circuito effettivo.

Il cronometro conserva il limite della V4.1: circa 49,7 giorni per una singola sessione senza riavvio; il display delle ore ritorna a 00 dopo 99.

Riferimenti: [modalità RUN nel manuale ALM](https://assets.busycircuits.com/docs/alm034-manual.pdf), [didattica sulle clave di Berklee](https://pulse.berklee.edu/?id=4&lesson=14). I preset sono quelli esplicitati nella tabella, non una riproduzione del firmware di altri moduli.
