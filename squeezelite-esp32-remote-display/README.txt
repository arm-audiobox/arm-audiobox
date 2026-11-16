LMS Touch-Fernbedienung – getesteter Stand 0.13.1
Build-Kennung auf dem Gerät: 0.13.1-overlay-test (unverändert übernommen).

Hardware: ESP32-2432S028 USB-C, ESP32 mit 4 MB Flash, ST7789 und XPT2046.
Querformat 320 × 240. Cover rechts, Musikdaten links. Keine Audiowiedergabe auf dem ESP32.

Einrichtung:
Mit dem offenen WLAN LMS-Remote verbinden, http://192.168.4.1 öffnen.
WLAN-Daten und LMS-IP oder DNS-Namen eintragen, Vorgabe lms.lan, Port 9000.
Gespeicherte Daten bleiben auf dem Gerät. .lan nutzt den DNS-Server des Netzwerks.
BOOT kurz: Touch kalibrieren; BOOT 2,5 Sekunden halten: WLAN-Einrichtung öffnen.

Bedienung:
Playername, Lautsprecher und WLAN in der Kopfzeile öffnen ihre Overlays.
Unten: vorheriger Titel, Play/Pause, nächster Titel, Menü.
Alle Bedienmenüs werden als Overlays angezeigt. Zurück liegt rechts, Schließen links.
Außerhalb tippen oder eingestellte Inaktivitätszeit: direkt zur Wiedergabe.
Hoch/Runter blättert Listen. Ordner kurz antippen zum Öffnen, länger halten für Aktionen.
Gerät > Einstellungen: Sprache, Touch, WLAN, Rückkehrzeit, Helligkeit und Nachtmodus.
Musikdaten und Playernamen bleiben in der vom LMS gelieferten Sprache.

Helligkeit und Nachtmodus:
Normale Helligkeit und Nachthelligkeit per Schieberegler, 5–100 %, werden gespeichert.
Nachtmodus standardmäßig täglich 22:00–07:00 Uhr bei 10 %.
Start/Ende mit Hoch-/Runter-Tasten für Stunden und Minuten einstellen; Halten wiederholt.
Speichern links, Zurück rechts. Nicht gespeicherte Zeitänderungen werden verworfen.
Außerhalb der Nachtzeit gilt die normale Helligkeit. Der Nachtmodus hellt nicht auf.
Die Vorschau beim Bearbeiten zeigt den jeweils eingestellten Helligkeitswert.
Uhrzeit kommt lokal vom LMS; Zeitzone Europa/Berlin mit Sommer-/Winterzeit.
Ohne gültige Uhrzeit ist die Zeitsteuerung noch nicht aktiv.

Standby:
Power-Symbol schaltet den ausgewählten LMS-Player aus.
Nach 10 Sekunden ohne Bedienung bei Pause/Aus erscheint die Flipclock mit Datum.
Berührung zeigt die Bedienung, ohne Musik zu starten. Danach erscheint die Uhr erneut.

Firmware und Build:
Aktuell: firmware/cyd-st7789-v0.13.1-update.bin (Offset 0x10000).
Erstinstallation: firmware/cyd-st7789-v0.13.1-factory.bin (Offset 0x0).
Das Komplettimage überschreibt den Einstellungsbereich; Update behält bestehende Daten.
Build: pio run -e cyd-st7789
Upload: pio run -e cyd-st7789 -t upload --upload-port PORT
Details, Voraussetzungen und Prüfungen: README.md, firmware/manifest.json, SHA256SUMS.
