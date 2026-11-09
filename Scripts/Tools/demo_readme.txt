LINDENHOF – Demo (früher Arbeitsstand)
======================================

Ein taktisches Einbruchs-Planungsspiel: Du planst Schritt für Schritt, wer aus der Crew
wann was macht, und spielst den Plan dann ab.

Dabei sind:
- Villa Lindenhof – der eigentliche Einbruch (Garten, Erdgeschoss, Keller mit Tresorraum)
- Testhaus und Testraum – kleine Übungen für Türen, Wege und Abstimmung
- Crew: Vic (Schlösser) und Rosa (Tresore)


STARTEN
-------
1. Die ZIP-Datei ganz entpacken (Rechtsklick > "Alle extrahieren ..."), z. B. auf den Desktop.
   Nicht direkt aus der ZIP-Datei heraus starten.
2. Im entpackten Ordner "Lindenhof.exe" doppelklicken.
3. Zeigt Windows "Der Computer wurde durch Windows geschützt": auf "Weitere Informationen"
   und dann auf "Trotzdem ausführen" klicken. (Das Spiel ist nicht signiert, deshalb warnt Windows.)

Der erste Start dauert etwas länger, weil die Grafikkarte die Shader vorbereitet.
Eine Installation ist nicht nötig.


SYSTEM
------
- Windows 10 oder 11, 64 Bit
- Grafikkarte mit DirectX 12 (Shader Model 6), etwa ab NVIDIA GeForce GTX 1070 oder
  AMD Radeon RX 5600, aktueller Grafiktreiber (ältere Karten: siehe unten, DirectX 11)
- 8 GB Arbeitsspeicher, ca. 2 GB freier Speicherplatz

Ruckelt es: Esc > Optionen > Qualität niedriger stellen > Übernehmen.


FALLS ES NICHT STARTET
----------------------
- Meldung mit "VCRUNTIME140.dll", "MSVCP140.dll" oder "DirectX":
  Einmal  Engine\Extras\Redist\en-us\vc_redist.x64.exe  ausführen (installiert die
  Laufzeitbibliotheken von Microsoft) und das Spiel neu starten.
- Meldung, dass DirectX 12 oder Shader Model 6 fehlt, oder das Bild bleibt schwarz:
  Grafiktreiber aktualisieren, oder "Lindenhof mit DirectX 11.bat" doppelklicken. Damit läuft das
  Spiel auch auf älteren Grafikkarten (DirectX 11), mit etwas einfacherer Grafik.


BEDIENUNG
---------
Kamera
  W A S D                          Kamera verschieben
  Q / E                            drehen
  rechte oder mittlere Maustaste   ziehen = drehen und neigen
  Mausrad                          zoomen
  F                                Kamera folgt wieder der gewählten Figur
  Bild auf / Bild ab               Stockwerk wechseln (Villa)

Crew
  Figur oder Karte links anklicken, Tasten 1 bis 9    Figur wählen

Planen
  Boden anklicken                  die Figur geht dorthin (neuer Schritt im Plan)
  Tür, Möbel, Gegenstand anklicken Menü mit Aktionen: knacken, durchsuchen, mitnehmen ...
  "Warten ..."                     die Figur wartet eine Zeit lang oder auf eine andere Figur
  Schritt in der Liste anklicken   auswählen; neue Schritte kommen dann dahinter
  Entf oder Rücktaste              gewählten (sonst letzten) Schritt löschen
  Strg + Z                         letzte Änderung zurücknehmen
  Leertaste                        Plan abspielen bzw. anhalten
  Esc                              Menü schließen bzw. Pausemenü (Speichern, Laden, Optionen)

Unten zeigt die Leiste, ob der Plan aufgeht. Hängt er, steht dort rot, woran es liegt,
und "Lösen ..." zeigt die Stelle.


SPIELSTÄNDE UND EINSTELLUNGEN
-----------------------------
Der Plan wird automatisch gespeichert ("Fortsetzen" im Hauptmenü). Eigene Stände:
Esc > Plan speichern. Spielstände und Einstellungen liegen unter
  %LOCALAPPDATA%\DerClou3\Saved


ENTFERNEN
---------
Den entpackten Ordner löschen. Wer auch Spielstände und Einstellungen loswerden will,
löscht zusätzlich den Ordner  %LOCALAPPDATA%\DerClou3


Das ist ein früher Arbeitsstand, kein fertiges Spiel – Fehler und Platzhalter-Kisten
gehören noch dazu. Rückmeldungen sind willkommen!
