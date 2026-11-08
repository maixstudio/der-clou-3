# Planung einfach machen – Konzept (entschieden 2026-11-08)

Anlass: User-Feedback 2026-11-08 – „knacken“ öffnet nicht, Fehler erst am Schluss bemerkt, kein Zustand der Türen im Plan, Einfügen in der Mitte sperrig, „Signal geben“ nicht auffindbar, Wunsch nach Geist der Figur und nach Varianten zum Ausprobieren.
Entstanden aus einer Analyse (4 Agenten) und einem Design-Panel (3 Konzepte, 2 Gutachter, 1 Synthese). Gewinner: „Das Spiel denkt mit“ plus das Beste der anderen.

**Entscheidung des Users zum Warteort:** Liegt eine verschlossene Tür auf dem Weg, geht die Figur **bis zur Tür**, wartet **seitlich daneben**, bis die Tür im Plan aufgesperrt ist, öffnet sie dann selbst und geht weiter.

## Für den User (Kurzfassung)

Kurz gesagt: Türen verhalten sich jetzt so, wie man es erwartet. Probleme siehst du schon beim Klick und nicht erst am Ende. Einfügen, Abstimmen und Zurücknehmen geht mit einem Klick.

Türen: Wer durch eine Tür muss, öffnet sie unterwegs selbst (ca. 1 Sekunde, die Tür knarrt). Nur eine verschlossene Tür hält auf. Dann geht die Figur bis zur Tür, wartet seitlich daneben, bis jemand sie im Plan aufsperrt, und geht danach durch. „knacken und durchgehen“ ist ein einziger Klick.

Dein Fall: Vic wählen → Hintertür → „Schloss knacken“. Rosa wählen → in den Flur klicken. Fertig. In Rosas Zeile steht „wartet 4 s auf Vic (Hintertür knacken)“ und „öffnet unterwegs Hintertür“. Du brauchst kein Zeichen und keinen Öffnen-Schritt, und nichts wird rot. Dein alter Weg über „Warten …“ funktioniert jetzt auch.

Probleme sofort: Macht ein Klick den Plan kaputt, wird der Klick-Ring rot. Über der Leiste steht dann z. B. „Geht so nicht: Bürotür bleibt verschlossen“, dazu ein Knopf, der es repariert, etwa [Vic knackt sie vorher]. Oben in der Leiste steht immer „Plan geht auf“ oder rot „Plan hängt: Rosa ab Schritt 2“. Schon im Menü steht unter jedem Eintrag, was passieren wird („wartet 4 s auf Vic“ oder rot „geht nicht: Bürotür bleibt zu“).

Im Raum: Das rote Schloss verschwindet, sobald das Knacken geplant ist. Die Tür steht offen, sobald jemand im Plan durchgeht. Oben links steht, welchen Moment du gerade siehst. Ein Geist der gewählten Figur zeigt, wo ihr nächster Schritt anfängt. Bleibt sie irgendwo hängen, ist er dort rot.

Einfügen: Klick auf einen Schritt in der Liste, dann kommen neue Klicks direkt dahinter (gelbe Zeile „Neue Schritte kommen hierhin“). Mit Esc oder „ans Ende“ hängst du wieder hinten an. Schritte lassen sich mit der Maus an eine andere Stelle ziehen.

Abstimmen: Klick auf Vics Schritt, dann auf den Knopf „Rosa wartet hierauf“. Das Warten hängt an Vics Schritt, auch wenn du ihn verschiebst.

Zurücknehmen: „Rückgängig“ (Strg+Z) nimmt jede Änderung zurück, auch „Plan leeren“.

Zum Vergleichen gibt es oben rechts „Varianten …“: Geist als Figur oder als Stecknadel · Hilfe als Leiste oder als Menü an der Maus · Einfügen hinter dem gewählten Schritt oder nur per Knopf.

(Entschieden: Sie geht hin und wartet neben der Tür.)

## Spielregeln

- Wer durchgeht, öffnet: Eine geschlossene, nicht verschlossene Tür auf dem Weg öffnet die Figur selbst (ca. 1 s, „Tür knarrt“). Das gilt in Planung, Abspielen und Ausprobieren; ein eigener „Öffnen“-Schritt ist nicht nötig.
- Knacken öffnet nicht: „Schloss knacken“ sperrt nur auf. Die Tür bleibt zu, bis jemand durchgeht oder sie öffnet; „knacken und durchgehen“ macht beides mit einem Klick.
- Verschlossene Tür auf dem Weg: Die Figur geht bis zur Tür, wartet seitlich daneben, bis jemand die Tür im Plan aufsperrt, öffnet sie dann und geht weiter.
- Sperrt im Plan niemand auf, geht der Schritt nicht. Der Grund nennt, wer es könnte („Vic könnte sie knacken, ca. 2 s“).
- Niemand knackt von selbst: Knacken mit Zeit, Lärm und möglichem Fehlschlag gibt es nur als geplanten Schritt.
- Beide Hände voll (z. B. Gemälde): Die Figur kann keine geschlossene Tür öffnen. Sie wartet, bis jemand anders sie öffnet, oder legt vorher ab.
- Offene Türen bleiben offen. Niemand schließt hinter sich – nur mit dem Schritt „Schließen“.
- Wer ein Schloss geknackt hat und danach stehen bleibt oder wartet, tritt einen Schritt zur Seite, damit der Nächste an die Klinke kommt.
- Warten auf eine andere Figur hängt an deren Schritt, nicht mehr an einer eigenen Zeichen-Zeile. Wird der Schritt verschoben, wartet sie weiter genau darauf; wird er gelöscht, fällt das Warten mit weg.
- Gibt eine Figur im Einbruch beim Knacken auf, wartet die andere nicht ewig: Sie hält an, und das Spiel meldet „Rosa kommt nicht weiter: Hintertür bleibt verschlossen“ (Vorstufe der Störungs-Pause).

## Kern (immer an)

### Türregel „Wer durchgeht, öffnet“ fertigstellen
Ein Gehen-, Tür- oder Gegenstands-Schritt, dessen Weg durch eine geschlossene, nicht verschlossene Tür führt, geht einfach. Die Figur öffnet die Tür unterwegs (ca. 1 s); beim Abspielen erscheint der Lärmring „Tür knarrt“. In der Zeile steht „öffnet unterwegs Hintertür“.

Ist die Tür verschlossen, wartet die Figur dort, wo sie steht, bis die geplante Figur aufgesperrt hat. Die Zeile sagt in Cyan „wartet 4 s auf Vic (1. Hintertür knacken)“, beim Abspielen steht über dem Kopf „wartet auf Vic“. Sperrt niemand auf, steht dort rot „Hintertür bleibt verschlossen – Vic könnte sie knacken (ca. 2 s)“. Die rote Nummer steht dann dort, wo die Figur hängen bleibt, und nicht mehr in der Gartenecke.

Mit vollen Händen (Gemälde) steht da „Hintertür: keine Hand frei – erst ablegen oder jemand öffnet sie“. Wer ein Schloss geknackt hat und danach stehen bleibt, tritt zur Seite. Gibt Vic beim Knacken auf, hält Rosa an und es kommt die rote Meldung „Rosa kommt nicht weiter: Hintertür bleibt verschlossen (Vic hat aufgegeben)“.

### Tür-Menü und Texte, die sagen, was los ist
Verschlossene Tür: Als erster Eintrag steht „knacken und durchgehen“ (ein Klick, zwei Schritte), danach „Schloss knacken“ und „Durchgehen“. Geschlossene Tür: zuerst „Durchgehen“, dann „Öffnen“ (zum Offenlassen für die anderen). Offene Tür: „Durchgehen“ und „Schließen“.

Der Titel nennt die Tür und ihren Zustand im Plan („Hintertür · verschlossen / plant: Rosa“) statt „Tür (verschlossen)“. Gehen-Schritte heißen nach dem Raum („gehen (Flur)“). Wartezeilen nennen die Tätigkeit, auf die gewartet wird.

Beim Ausprobieren öffnet die Figur bei einem Klick hinter eine geschlossene Tür diese unterwegs. An einer verschlossenen Tür sagt sie „Hintertür ist verschlossen“.

### Rückgängig und Wiederholen
Oben in der Leiste stehen neben „Planen | Ausprobieren“ die Knöpfe „Rückgängig“ und „Wiederholen“. Sie sind ausgegraut, wenn es nichts zurückzunehmen gibt. Der Tooltip sagt genau, was zurückgenommen wird („Rückgängig: „Hintertür knacken“ gelöscht · Strg+Z“).

Jede Planänderung lässt sich zurücknehmen: Einfügen, Ziehen, Löschen, Warten, Lösungen und auch „Plan leeren“. Deshalb braucht „Plan leeren“ keinen zweiten Klick mehr; die Meldung sagt „Plan geleert – „Rückgängig“ holt ihn zurück“.

Zwei Schritte aus einem Klick, etwa „knacken und durchsuchen“, nimmt ein einziges Rückgängig zurück.

### Sofort-Warnung und Plan-Status
Macht ein Klick den Plan kaputt, wird der Klick-Ring rot. Über der Leiste erscheint eine rote Hilfe-Leiste, die bleibt, bis man reagiert: „Geht so nicht: Rosa kommt nicht zum Wandtresor – Bürotür bleibt verschlossen.“ Dazu gibt es [Rückgängig] und [Später]; später kommen dort die Lösungsknöpfe dazu.

Oben in der Leiste steht an festem Platz „Plan geht auf · fertig 0:41“ oder rot „Plan hängt: Rosa ab Schritt 2 – Bürotür bleibt zu [Lösen …]“. Hängt der Plan, wird aus „Abspielen“ der normale Knopf „Trotzdem abspielen“.

In der Liste ist nur der erste Problem-Schritt rot, sein Grund steht in 12 pt. Die Schritte dahinter stehen zusammen in einer grauen Zeile „Schritte 3–5 kommen nicht dran“. In der Zeitleiste zeigt ein schraffierter Balken „2 hängt“ statt übereinanderliegender roter Schilder. Die Profilkarte sagt „hängt: Bürotür zu“.

Hängt die Figur schon vorher, steht im Menü unter jedem Eintrag rot „kommt nicht dran – Rosa hängt bei Schritt 2“. Ist alles wieder gut, kommt die grüne Meldung „Passt jetzt – Plan geht auf“.

### Türen zeigen den Plan im Raum
In der Planung zeigen die Türen ihren geplanten Zustand zu dem Moment, den der Raum gerade zeigt. Oben links neben „Planung“ steht, welcher Moment das ist: „Raum zeigt: nach dem Plan (0:41)“, „… nach Rosas Schritt 2 (0:12)“ oder „… 0:20 (Zeitleiste)“.

Nach einem geplanten Knacken ist das rote Schloss weg. Sobald jemand im Plan durchgeht, steht das Türblatt offen, vom Öffnenden weg geschwenkt. Über Türen, die sich im Plan ändern, steht ein kleines Schild („aufgesperrt 0:04 · Vic“, „offen ab 0:05 · Rosa“).

Die gewählte Zeile sagt, was sie ändert („Hintertür: verschlossen → aufgesperrt“). Menütitel und Info sagen dasselbe, z. B. „Hintertür · aufgesperrt (Vic, 0:04)“ und in der Info „Im Plan: 0:04 Vic knackt · 0:05 Rosa öffnet“.

Beim Abspielen und beim Ausprobieren zeigen die Türen wieder ihren echten Zustand.

### Geist der gewählten Figur
Ohne Zeitleisten-Vorschau steht genau ein Geist im Raum, und zwar von der gewählten Figur. Er steht dort, wo ihr nächster Schritt anfängt: am Ende ihres Plans („Rosa danach“) oder hinter dem gewählten Schritt („Rosa nach 2“), mit Blick zum Ziel.

Bleibt die Figur hängen, steht der Geist rot an dieser Stelle („Rosa hängt: Bürotür zu“).

Standard ist eine abgedunkelte, etwas kleinere Figur mit hellem Ring; ein Klick auf den Geist wählt die Figur. Bei einem Klick in die Zeitleiste zeigen wie bisher alle Figuren als Stecknadeln, wo sie dann sind.

### Warten hängt am Schritt – „Rosa wartet hierauf“
Wählt man Vics Schritt in der Liste, steht neben „↑ früher / ↓ später / × löschen“ in Cyan der Knopf „Rosa wartet hierauf“. Ein Klick genügt: Rosa bekommt „warten, bis Vic „Hintertür knacken“ erledigt hat“, und zwar vor ihrem ersten Schritt, der sonst zu früh anfangen würde. Die Meldung sagt, wohin das Warten kam: „Rosa wartet jetzt vor ihrem Schritt 2 „gehen (Flur)“ auf Vic“. Danach heißt der Knopf „Rosa wartet nicht mehr“; ein zweiter Klick nimmt es wieder weg.

Eine eigene Zeile „Rosa das Zeichen geben“ gibt es nicht mehr. Vics Zeile zeigt „→ Rosa wartet“, die Zeitleiste einen Cyan-Pfeil.

Verschiebt man Vics Schritt, wartet Rosa weiter genau darauf. Löscht man ihn, sagt die Meldung „Schritt gelöscht – Rosas Warten darauf auch (Rückgängig)“.

Beim Abspielen sagt Vic am Ende des Schritts „Zeichen an Rosa“. „Warten …“ bei Rosa macht dasselbe von ihrer Seite aus.

### Einfügen hinter dem gewählten Schritt
Klickt man einen Schritt in der Liste oder seinen Block in der Zeitleiste an, wird er gelb. Darunter erscheint die gelbe Zeile „+ Neue Schritte kommen hierhin“ mit dem Knopf „ans Ende“. Geist und Türen zeigen jetzt den Moment nach diesem Schritt.

Jeder neue Schritt kommt dorthin: Boden, Tür, Gegenstand, „ablegen“, „Warten …“ und auch eine Lösung aus der Hilfe-Leiste. Der neue Schritt ist danach selbst der gewählte, so bleibt eine Folge von Klicks in ihrer Reihenfolge. Die Meldung sagt „Rosa, neuer Schritt 2: Kommode durchsuchen (nach „gehen (Flur)“)“; die neue Zeile leuchtet kurz auf, und die Liste rollt zu ihr.

Menüs und „Hände dann:“ rechnen mit dem Stand an dieser Stelle. Ein Brecheisen, das erst in Schritt 5 aufgehoben wird, gibt es in Schritt 2 also noch nicht.

Esc oder „ans Ende“ hängt wieder hinten an; ein zweiter Klick auf die gewählte Zeile auch. Entf oder Rücktaste löscht den gewählten Schritt (sonst den letzten), mit Meldung.

### Schritte mit der Maus ziehen
Man packt eine Zeile und zieht sie nach oben oder unten. Eine gelbe Linie zeigt, wo sie landet, am Mauszeiger steht „→ vor Schritt 2“.

Loslassen verschiebt den Schritt; ein Rückgängig nimmt das zurück. Die Meldung sagt „Rosa: „Kommode durchsuchen“ ist jetzt Schritt 2“. Wer auf den Schritt wartet, wartet weiter darauf.

Über den Zeilen einer anderen Figur steht „Schritte bleiben bei ihrer Figur“. „↑ früher / ↓ später“ bleiben für kleine Verschiebungen.

### Lösungen per Klick (Probelauf im Hintergrund)
Die Hilfe-Leiste bietet bis zu zwei geprüfte Lösungen als Knöpfe an, die erste gelb. Beispiele: „Vic knackt sie vorher“ (darunter klein „Rosa wartet ca. 5 s · fertig 0:33“), „Rosa knackt sie selbst (ca. 6 s)“, „erst Brecheisen aufheben (+3 s)“, „Vic öffnet ihr die Tür“, „Gemälde vorher ablegen“.

Angeboten wird nur, was das Spiel vorher im Hintergrund durchgerechnet hat: Danach geht der Schritt, und nichts anderes geht kaputt. Ein Klick fügt die Lösung ein und lässt sich mit einem Rückgängig zurücknehmen. Die Meldung sagt, was wohin kam: „Vic, neuer Schritt 2: Bürotür knacken – Rosa wartet ca. 5 s darauf“.

Gibt es keine Lösung, sagt die Leiste warum („niemand hier kann sie knacken“) und bietet „Schritt löschen“ an.

### Menü sagt vorher, was passiert
Unter jedem Eintrag im Planungs-Menü steht eine kurze Zeile, was für die gewählte Figur an der Einfügestelle herauskommt: „2 s · 100 %“, in Cyan „wartet 4 s auf Vic“ oder rot „geht nicht: Bürotür bleibt zu“. Rote Einträge bleiben anklickbar und öffnen sofort die Hilfe.

Was die Figur gar nicht kann, ist grau und nennt, wer es kann: „Dietrich-Set fehlt – Vic kann es“.

Der Titel zeigt die Tür so, wie sie dann ist: „Hintertür · Vic knackt sie um 0:04“. Der Tooltip hat die lange Form mit Dauer, Chance, „öffnet unterwegs …“ und „fertig um“.

### Varianten-Schalter zum Ausprobieren
In der Planung steht oben rechts im Rahmen der Knopf „Varianten …“. Er öffnet oben rechts (an Stelle der Info) ein kleines Fenster mit drei Umschaltern im Stil von „Planen | Ausprobieren“. Zu jedem Umschalter erklärt ein Satz, was sich ändert.

Umschalten wirkt sofort und bleibt gespeichert, auch nach einem Neustart.

## Umschalter zum Vergleichen („Varianten …“)

- **Geist der gewählten Figur:** Figur (abgedunkelt, etwas kleiner, mit hellem Ring) | Stecknadel (wie die Vorschau heute) (Standard: Figur (abgedunkelt, etwas kleiner, mit hellem Ring)). Das war seine eigene Idee. Beim Spielen zeigt sich, ob eine Figur von oben klarer ist als eine Stecknadel oder ob man sie mit der echten Figur verwechselt.
- **Hilfe bei Problemen:** Leiste über dem Plan | Menü an der Maus (Standard: Leiste über dem Plan). Die Leiste steht ruhig an festem Platz und bleibt, bis man reagiert. Das Menü erscheint genau dort, wo er geklickt hat: Das ist schneller, aber es springt ins Bild und verschwindet beim Wegklicken. Beide zeigen dieselben geprüften Lösungen.
- **Neue Schritte einfügen:** hinter dem gewählten Schritt | nur mit Knopf „+ danach einfügen“ (Standard: hinter dem gewählten Schritt). Am schnellsten ist: Schritt anklicken, weiterklicken. Die Gefahr: Wer eine Zeile nur zum Lesen anklickt, fügt den nächsten Klick in der Mitte ein. Mit Knopf passiert das nie, es kostet aber einen Klick mehr.

## Später

- „Warten direkt an der Tür“ als Regel-Variante: Die Figur geht vor und wartet seitlich der Tür. Nur, falls der User es nach dem Test will. Das braucht Zwischenhalte im Weg in Simulation, Zeitleiste und Runner.
- Störungs-Pause aus Plan-Naechste-Schritte Schritt 4 (Nochmal versuchen · Schritt überspringen · Selbst steuern). Sie ersetzt die einfache Meldung „kommt nicht weiter“.
- Nebel N2–N6: Türen mit „Schloss ?“. Der Plan rechnet erst einmal mit „aufgesperrt“ und markiert „zeigt sich vor Ort“. Vorhersage, Lösungen, Tür-Aussehen und Geist zeigen nur, was bekannt ist. Eine verschlossene Überraschung wird zur Störungs-Pause.
- Live umplanen (N5): dieselbe Einfügestelle, dann hinter dem laufenden Schritt. Der Probelauf startet dabei vom echten Zustand und den restlichen Schritten der Runner.
- Lärm-Vorhersage in der Zeitleiste, auch für automatisches Türöffnen („Tür knarrt“, 320 cm), damit nichts Ungeplantes den Nachbarn weckt.
- „Hinter sich schließen“ als Option je Durchgang – wichtig, sobald Wachen dazukommen.
- Ab drei Figuren: „Wer wartet hierauf? …“ als kleines Menü. Die Spuren-Leiste (eine Spur je Figur mit Karten) erst, wenn Liste und Zeitleiste dann zu eng werden.
- „Vorspielen“ (Geister laufen den Plan im Raum ab) oder eine Zwiebelschicht, falls der Geist beim User gut ankommt.
- Geplante Zustände an Möbeln und Gegenständen (Tresortür offen, Gegenstand genommen) – das gehört zum Blender-Chat.
- Eigenes halbdurchsichtiges Material M_PlanGhost statt des Engine-Debug-Materials: gekocht und außerhalb von /Game/DerClou/Items.
- Tür-Standseite nach Erreichbarkeit statt nach Blickrichtung wählen. Das behebt einen latenten Fehler bei Türen in quer stehenden Wänden.
- Gesperrte Menü-Einträge mit Abhilfe planbar machen, z. B. „aufbrechen – erst Brecheisen holen“ fügt beide Schritte ein.
- Zeitleiste: Zoom und Blöcke ziehen; Übergeben von Gegenständen zwischen Figuren.

## Technischer Bauplan (englisch, für die Umsetzung)

### 1. Türregel fertigstellen, zwei Fehler beheben, committen
Before building, confirm rules 3 and 5 with the user: wait where she stands, and nobody picks automatically.

Build items 1–5 of feature 1 on top of the uncommitted working copy:
- door-wait kinds and the release fix in ApplyEvent
- WaitForLabel
- the 'Vic könnte sie knacken' reasons
- FindDoorsOnWay, plus the first-not-open door in DoorOnWay with the nav-settle wait and RunnerMaxPassTries
- the picker steps aside

New self-tests go into a new Private/DCPlanSelfTests.cpp (ADCPlayerController members; the dispatch stays in BeginPlay; unique names for constants in anonymous namespaces because of the unity build).

Build with the editor closed and run all 13 existing self-tests. Then commit and push on claude/sharp-rubin-uqa487 with a German message, adding only own files: never DerClou3.uproject with the local engine GUID, and never git add -A, because the Blender chat works in Art/ and /Game/DerClou/Items. Every later step ends the same way: CLAUDE.md 'Stand', commit, push.

Dateien: Private/DCHeistState.cpp, Public/DCHeistState.h, Private/DCPlanRunner.cpp, Public/DCPlanRunner.h, Private/DCPlayerController.cpp, Public/DCPlayerController.h, Private/DCPlanSelfTests.cpp (new)

Selbsttest: DoorWay (already updated in the working copy): Vic only picks, Rosa waits for him and opens the door herself, the red marker is not at the origin, and the playback wait text names the Hintertür.

House: the straight-into-the-office reason names both doors and 'Vic könnte'. New variant: pick Hintertür → pick Bürotür → walk in, with no Öffnen steps. It is fully possible and plays into the Büro.

New -DCSelfTest=AutoDoor (L_House01):
(1) The user's case: Vic PickLock Hintertür, Rosa Move into the Flur. Expect: not blocked, WaitFor == Vic, WaitForLabel contains 'Hintertür knacken', Detail contains 'öffnet unterwegs'. Playback: Rosa ends in the Flur, the Hintertür is open, |played − planned| < 3 s.
(2) Hintertür unlocked and open, Bürotür unlocked but closed, Rosa Move into the Büro: plans without a wait and plays without hanging (the InWay[0] bug).
(3) Rosa picks up the Gemälde (both hands) and walks into the Schlafzimmer: the reason contains 'keine Hand frei', WaitFor stays None and BlockedSince stays put, although Vic's unrelated steps fire events.

Screenshots autodoor_plan.png, autodoor_done.png.

### 2. Tür-Menü, klare Texte, Ausprobieren, Aufgeben-Meldung
Build feature 2 completely, plus item 6 of feature 1 ('waiting in vain' in playback). Add a test-only switch UDCPlanRunnerComponent::bTestFailPicks, so a self-test can make a pick fail three times.

Dateien: Private/DCPlayerController.cpp, Private/DCHeistState.cpp, Public/DCHeistState.h, Private/DCTimelinePanel.cpp, Private/DCPlanRunner.cpp, Public/DCPlanRunner.h, Private/DCPlanSelfTests.cpp

Selbsttest: AutoDoor extended:
- OpenDoorMenu(Hintertür) for Vic + RadialMenu->ChooseOption('knacken und durchgehen') adds two steps. The menu title contains 'Hintertür', Rosa's Move label contains 'Flur', the wait line contains 'auf Vic'.
- Ausprobieren: Rosa floor-clicked into the Schlafzimmer (door closed, unlocked) arrives within 10 s and the door is open. Floor-clicked into the Büro (locked), she says '… ist verschlossen' and the step is Failed.
- Playback with Vic's bTestFailPicks: Rosa stops, the toast contains 'kommt nicht weiter', and IsPlaybackFinished() becomes true within 30 s.

Screenshot autodoor_menu.png. All old self-tests still pass (Noise uses direct-mode walking).

### 3. Rückgängig und Wiederholen
Build feature 3:
- history + FDCPlanEditScope in UDCPlanSubsystem
- InsertTasks and MoveTaskTo with one broadcast
- Undo/Redo in the controller (planning only)
- two buttons in row A
- 'Plan leeren' with one click and an undo toast

Dateien: Public/DCPlan.h, Private/DCPlan.cpp, Public/DCPlayerController.h, Private/DCPlayerController.cpp, Public/DCTimelinePanel.h, Private/DCTimelinePanel.cpp

Selbsttest: PlanInventory gets an undo round:
- RemoveTask → Undo gives the same task ids in the same order.
- An IssueTasks pair is one Undo.
- ClearAll → Undo restores all plans; Redo applies it again.
- GetUndoText names the change.
- The Undo button is disabled when the history is empty.

### 4. Sofort-Warnung und Plan-Status
Build feature 4 without fixes:
- block causes in the simulation and in FDCTaskCheck
- FindProblems and the EditPlan wrapper
- help slot with [Rückgängig] / [Später]
- status slot with [Lösen …] and 'Trotzdem abspielen'
- collapsed rows and the hatched 'hängt' bar
- crew-card reason and red click ring
- red 'kommt nicht dran' details via FDCRadialOption::Tone

Add the help bar and the status slot to UI-Leitfaden §7.

Dateien: Public/DCHeistState.h, Private/DCHeistState.cpp, Public/DCSkills.h, Private/DCCrewCharacter.cpp, Public/DCPlayerController.h, Private/DCPlayerController.cpp, Private/DCTimelinePanel.cpp, Private/DCCrewBar.cpp, Public/DCRadialMenu.h, Private/DCRadialMenu.cpp, Private/DCPlanSelfTests.cpp, Docs/UI-Leitfaden.md

Selbsttest: New -DCSelfTest=PlanHelp (L_House01), part 1:
- Rosa Move into the Büro with no picks planned: PendingHelp is set and names the Hintertür, the click ring is red, the status starts with 'Plan hängt: Rosa ab Schritt 1', and the play label is 'Trotzdem abspielen'.
- Two more Rosa steps: no second help; the list shows one red row and one grey 'kommen nicht dran' row; the card text starts with 'hängt'.
- Vic then picks Hintertür and Bürotür: toast 'Passt jetzt', status 'Plan geht auf'.

Screenshots planhelp_bar.png at 1600×900 and 1920×1080 (-ResX/-ResY).

### 5. Türen zeigen den Plan
Build feature 5:
- Swing in FDCSimDoor + ADCDoor::SwingSignFor
- the StartDoors/DoorChanges log and GetDoorAt
- Effect lines on steps
- GetViewTime / DescribeViewMoment / UpdateWorldLook
- ADCDoor::SetPlanLook (visual only) with its label
- planned door title and Info line
- frame text 'Raum zeigt: …'

Dateien: Public/DCHeistState.h, Private/DCHeistState.cpp, Public/DCDoor.h, Private/DCDoor.cpp, Private/DCPlayerController.cpp, Private/DCTimelinePanel.cpp, Private/DCFrame.cpp, Private/DCPlanSelfTests.cpp

Selbsttest: New -DCSelfTest=PlanLook (L_House01): Vic PickLock Hintertür, Rosa Move into the Flur.
- GetDoorAt(Hintertür): locked at 0; unlocked and closed at Vic's End (By = Vic); open after Rosa's opening (By = Rosa).
- In planning ('nach dem Plan') the Hintertür hides its padlock, its shown amount reaches 1 within 1 s and its label contains 'Rosa'. At the same time IsOpen() is false, IsLocked() is true and the nav path garden→Flur is still partial (the look is visual only).
- Picking Vic's step 1: the frame text contains 'nach Vics Schritt 1', the panel is closed again and the padlock stays hidden.
- StartPlayback clears the look.

Screenshots plan_look_end.png, plan_look_step1.png.

### 6. Geist der gewählten Figur
Build feature 6:
- ADCPlanGhost as a figure in opaque, dimmed colour. Check once in the editor whether M_SimpleUnlitTranslucent's alpha works before using it.
- LookAt on scheduled tasks.
- UpdateGhosts with one ghost for the focus crew member; keep the pin look in code for the variant.
- A click on the ghost selects that crew member.

Dateien: Public/DCPlanGhost.h (new), Private/DCPlanGhost.cpp (new), Public/DCHeistState.h, Private/DCHeistState.cpp, Private/DCPlayerController.cpp, Private/DCPlanSelfTests.cpp

Selbsttest: PlanLook extended:
- Rosa's ghost stands within 80 cm of her last Move target.
- After removing Vic's pick it is red, more than 100 cm from the world origin and within 150 cm of Rosa's start.
- CrewForClickedActor(ghost) returns Rosa.
- With a preview time set there is no figure ghost, only the pins.

Screenshot ghost_figure.png.

### 7. Warten hängt am Schritt
Build feature 7:
- LinkWait, and the RemoveTask cascade that returns a count
- ToggleWaitOn with automatic placement
- cyan button in the picked row
- ApplyWaitChoice without a Signal (still appends; step 8 moves it to the insertion point)
- 'Zeichen an Rosa' when the step ends
- new DescribeWaitFor text
- UI-Leitfaden §6: 'Zeichen'

Dateien: Public/DCPlan.h, Private/DCPlan.cpp, Public/DCPlayerController.h, Private/DCPlayerController.cpp, Private/DCTimelinePanel.cpp, Private/DCHeistState.cpp, Private/DCPlanRunner.cpp, Private/DCWaitDialog.cpp, Docs/UI-Leitfaden.md

Selbsttest: Signals rewritten (L_Room01): Rosa cracks the safe, Vic loots it.
- ToggleWaitOn(Rosa, 0, Vic) puts a WaitFor (Ref = Rosa's step id) before Vic's loot step; no plan contains a Signal step; Vic's loot starts at or after the End of Rosa's crack.
- MoveTask on Rosa's steps keeps the link; a second ToggleWaitOn removes it.
- RemoveTask of Rosa's crack removes Vic's wait (count 1), and one Undo restores both.
- An old-style plan (Signal + WaitFor inserted directly) still simulates and plays.
- In playback Vic's wait text contains 'Rosa', and Rosa says 'Zeichen an Vic'.

PlanInventory's wait-dialog part is updated (no Signal row). Screenshots signal_vic.png, signal_rosa.png.

### 8. Einfügen hinter dem gewählten Schritt
Build feature 8:
- GetInsertIndex, and PlanInsert for every entry point
- FDCSimOptions with FDCSimCapture, plus InsertState/InsertInventory
- GetMenuState/GetMenuInventory in both menus, in 'Hände dann:' and in the profile's plan view
- yellow insertion row + 'ans Ende', scroll-to and row flash
- row click = pick, without preview pins
- keys Esc, Entf and Rücktaste
- wait-dialog sentence, and the wait goes to the insertion point

Dateien: Public/DCPlayerController.h, Private/DCPlayerController.cpp, Public/DCHeistState.h, Private/DCHeistState.cpp, Public/DCPlan.h, Private/DCPlan.cpp, Public/DCTimelinePanel.h, Private/DCTimelinePanel.cpp, Private/DCInventoryPanel.cpp, Private/DCWaitDialog.cpp, Private/DCPlanSelfTests.cpp

Selbsttest: New -DCSelfTest=Insert (L_House01). Vic: PickLock Hintertür, PickLock Bürotür. Rosa: Move into the Flur, Item PickLock Wandtresor.
- FocusStep(Rosa, 0), then IssueTask(Loot Kommode): it becomes Rosa's step 2, the picked step is 2, the toast contains 'nach', and the safe step is now 3 and not blocked.
- At that insertion point GetMenuState shows the Bürotür still locked, and the door-menu title names Vic.
- Vic with PickUp Brecheisen as step 3: picking his step 1 greys 'aufbrechen' at the Schreibtisch-Schublade (crowbar not yet carried).
- Esc → GetInsertIndex == INDEX_NONE; the Delete key path removes the picked step.

Screenshot insert_line.png.

### 9. Schritte ziehen
Build feature 9:
- FDCStepDragOp
- drop gap via ComputeDropIndex
- MoveTaskTo / MoveStepTo
- pause rebuilds while dragging; auto-scroll at the list edge

Add 'Schritte mit der Maus ziehen' to 'Noch vom User zu bestätigen' in CLAUDE.md, because the Slate drag itself is only tested through its functions.

Dateien: Public/DCTimelinePanel.h, Private/DCTimelinePanel.cpp, Public/DCPlan.h, Private/DCPlan.cpp, Public/DCPlayerController.h, Private/DCPlayerController.cpp, Private/DCPlanSelfTests.cpp

Selbsttest: Insert extended:
- MoveStepTo(Rosa, 2 → 0), the function OnDrop calls, gives the expected order; a WaitFor on the moved step still resolves; one Undo restores the order.
- ComputeDropIndex returns 0, 1 and Num for points above, between and below the rows.

### 10. Probelauf, Weg-Cache und Lösungen per Klick
Build feature 10:
- FDCPlanSource and the plan override in FDCSimOptions
- Probe and the way cache (cleared on nav rebuild)
- DCPlanFix::Build with probe-checked candidates
- ApplyFix as one undo step
- fix buttons first in the help bar; 'Lösen …' reopens it

Log probe and Refresh times.

Dateien: Public/DCHeistState.h, Private/DCHeistState.cpp, Public/DCPlanFix.h (new), Private/DCPlanFix.cpp (new), Public/DCPlayerController.h, Private/DCPlayerController.cpp, Private/DCTimelinePanel.cpp, Private/DCPlanSelfTests.cpp

Selbsttest: PlanHelp part 2:
- Vic picks only the Hintertür, Rosa Move into the Büro: the help offers 'Vic knackt sie vorher' and no self-pick for Rosa (she has no Dietrich-Set).
- ApplyFix(0): Vic gets a Bürotür PickLock, Rosa's step is not blocked, and one Undo removes the fix again.
- AddTask(Vic, Force Schreibtisch-Schublade) without the crowbar gives BlockCause MissingTool, and the fix 'erst Brecheisen aufheben' (the crowbar lies in the garden) makes it work.
- The log shows probe and Refresh times; Refresh with the cache is no slower than before.

Screenshot planhelp_fix.png.

### 11. Vorhersage im Menü
Build feature 11:
- per-entry probes when the menu opens (planning only, enabled entries only; pairs as both steps)
- one-fact detail by priority, with tones
- 'wer kann es' for grey entries
- planned door title with who and when from the door log
- budget fallback above 30 ms

Dateien: Public/DCPlayerController.h, Private/DCPlayerController.cpp, Public/DCRadialMenu.h, Private/DCRadialMenu.cpp, Public/DCHeistState.h, Private/DCHeistState.cpp, Private/DCPlanSelfTests.cpp

Selbsttest: PlanHelp part 3:
- With Vic's Bürotür pick planned and Rosa in the Flur, OpenDoorMenu(Bürotür) for Rosa: the 'Durchgehen' detail contains 'wartet' and 'Vic' (tone Waits), and the title names Vic and a time.
- Without Vic's pick the detail is red and contains 'bleibt'.
- With Rosa stuck at step 1, every entry says 'kommt nicht dran'.
- Rosa's grey 'Schloss knacken' says 'Vic kann es'.
- The menu-open time is logged (target < 30 ms).

Screenshot forecast_menu.png.

### 12. Varianten-Schalter, Abschluss und Doku
Build feature 12 with the three toggles:
- ghost as pin or figure (switch in UpdateGhosts)
- help as a menu: UDCRadialMenu::Open at the click position with the fixes, Rückgängig and Später
- insert only via a '+ danach einfügen' button in the picked row

Then update the docs:
- CLAUDE.md: Stand, self-tests, and 'Noch vom User zu bestätigen' (all three variants, dragging with the mouse, help bar, door look)
- Docs/Plan-Naechste-Schritte.md
- Docs/UI-Leitfaden.md

Commit and push.

Dateien: Public/DCPlan.h, Private/DCPlan.cpp, Public/DCFrame.h, Private/DCFrame.cpp, Public/DCVariantsPanel.h (new), Private/DCVariantsPanel.cpp (new), Private/DCPlayerController.cpp, Private/DCTimelinePanel.cpp, Private/DCHeistState.cpp, Private/DCPlanSelfTests.cpp, CLAUDE.md, Docs/Plan-Naechste-Schritte.md, Docs/UI-Leitfaden.md

Selbsttest: New -DCSelfTest=Variants (L_House01). Each variant is set through UDCPlanSubsystem, as -DCVariant= would:
- Ghost=Pin vs Figure: screenshots ghost_pin.png and ghost_figure.png.
- Help=Menu: after a blocking floor click the radial menu is open, and ChooseOption('Vic knackt') fixes the plan.
- Insert=Button: picking a step keeps GetInsertIndex == INDEX_NONE until '+ danach einfügen'.
- The values survive a write/read through GGameUserSettingsIni.

All self-tests pass at 1600×900 and at 1920×1080.

### Umsetzungs-Details je Kern-Funktion

#### Türregel „Wer durchgeht, öffnet“ fertigstellen
Base: the uncommitted working copy. It already has DoorCrossings, ToOpen/OpenedOnWay/WayNote, bWalkBlocked with EndLocation at the stuck spot, GetPositionAt → Path[0], the runner's PassingDoor + Crew->InteractWithDoor(GoThrough), the updated DoorWay test and the inventory-flicker fix. Keep its 'wait where she stands'; plan and playback already agree on it.

(1) Release only on the right door change (DCHeistState.cpp)
- FCursor::WaitDoors becomes TArray<FDCDoorWait{FName Door; bool bForLock;}>.
- ApplyEvent copies the waited doors before Event.Apply. It releases a bWalkPending cursor only when a waited door changed the relevant way: bForLock means bLocked true→false or bOpen false→true; hands full means bOpen false→true.
- Only then does it set Pending.WaitFor/WaitForName/WaitForIndex and the new FDCScheduledTask::WaitForLabel = DescribeTask((*Cursors[Event.CrewIndex].Tasks)[Event.TaskIndex]).
- Bug today: '!bLocked || bOpen' frees a hands-full wait on any event, moves BlockedSince and names whoever caused that event.

(2) Final 'never' loop
- bWalkBlocked + locked: '{Tür} bleibt verschlossen – Vic könnte sie knacken (ca. 2 s)'. Find the helper by looping over all cursors with Crew->PlanCheckDoor(Door, EDCDoorAction::PickLock, Cursor.Inventory). The stuck figure counts too: 'Rosa könnte sie selbst knacken'. If nobody can: '– niemand hier kann sie knacken'.
- Hands full: '{Tür}: keine Hand frei – erst ablegen oder jemand öffnet sie'.

(3) Rename FindClosedDoorsOnWay → FindDoorsOnWay (DCHeistState.h:182, .cpp:1621). It returns the doors in order and is empty when the nav path is free. Add CanReach(From, To).

(4) DCPlanRunner.cpp DoorOnWay
- Use the first door in that list that is NOT open. Bug today: InWay[0] can be the already open Hintertür, so Rosa waits forever for a closed Bürotür behind it.
- All doors open means the nav mesh is not rebuilt yet: wait up to RunnerNavSettleSeconds (1.5 s), then StartApproach.
- Locked: wait text from the schedule task, 'wartet auf Vic (Hintertür knacken)' via WaitFor/WaitForLabel, else 'wartet, bis Hintertür aufgesperrt ist'.
- Closed but unlocked: PassingDoor/InteractWithDoor(GoThrough) as in the WIP, counting PassTries. After RunnerMaxPassTries (3): Say('kommt nicht durch') + FinishTask(Failed).

(5) Picker steps aside: in FinishTask(Done) after a Door PickLock, when there is no next task or the next one is Wait/WaitFor, call Crew->MoveToLocation(Door->GetStandPoint(Loc, false) + Door->GetActorRightVector().GetSafeNormal2D() * WaitAside).

(6) Waiting in vain (live)
- While Approach waits at a locked door, read the planned unlocker (schedule task WaitFor + WaitForIndex) from that crew member's runner.
- If its status is Failed: Say('kommt nicht weiter'), ShowToast on the first player controller (red), FinishTask(Failed) + Stop(). IsPlaybackFinished() must count that runner as finished.

Constants in anonymous namespaces need project-unique names (unity build).

#### Tür-Menü und Texte, die sagen, was los ist
OpenDoorMenu (DCPlayerController.cpp:318-383) reads the door from the planned state:
- locked → 'knacken und durchgehen' = IssueTasks({PickLock, GoThrough}), built like the 'knacken und durchsuchen' pair (477-505); then PickLock, GoThrough
- closed → GoThrough, Open
- open → GoThrough, Close

Title: new UDCHeistStateSubsystem::DescribePlannedDoor(Door) gives '{Name} · verschlossen|zu|offen'. It replaces Door->DescribeState(), which shows the live state and the generic word 'Tür'. Feature 5 adds who and when.

Texts:
- DescribeTask(Move) → 'gehen (Flur)' via ADCRoomVolume::FindAt(this, Task.Location)->GetRoomName(); just 'gehen' outside rooms.
- Wait line in RebuildList (636-652) → 'wartet {0} s auf {1} ({2}. {3})' with WaitForIndex + 1 and WaitForLabel, at 12 pt.
- DescribeActivityAt → 'wartet auf Vic (Hintertür knacken)'.
- Over the head it stays 'wartet auf Vic' (PlainText, at most three words).

Ausprobieren:
- HandleClick (676-686) uses IssueTask(Move), so the runner opens doors on the way.
- The click ring is red only when !Heist->CanReach(From, To).
- At a locked door the runner's direct-mode branch says '{Tür} ist verschlossen' and fails the step.

#### Rückgängig und Wiederholen
UDCPlanSubsystem (DCPlan.h/.cpp):
- FDCPlanHistoryEntry {TArray<FDCCrewPlan> Plans; FText What;}; UndoStack/RedoStack capped at 50.
- RAII FDCPlanEditScope(UDCPlanSubsystem&, const FText& What). The outermost scope pushes a copy and clears RedoStack; inner scopes do nothing. OnPlanChanged fires once when the outermost scope ends, so a pair, a fix or a cascade is one undo entry and one re-simulation.
- Every mutator opens a scope: AddTask, InsertTask, new InsertTasks(Crew, Index, Tasks), RemoveTask, RemoveLastTask, MoveTask, new MoveTaskTo(Crew, From, To), LinkWait (feature 7), ClearAll.
- Undo()/Redo() swap Plans and broadcast. Add CanUndo/CanRedo/GetUndoText/GetRedoText.
- Mode, speed and the self-test fields are not part of the history.

ADCPlayerController::Undo/Redo (planning only):
- SetFocus(Crew, INDEX_NONE) and toast 'Zurückgenommen: {What}'.
- PlayerTick handles Strg+Z, Strg+Y and Strg+Umschalt+Z.

UDCTimelinePanel row A:
- PanelButtons 'Rückgängig' and 'Wiederholen' right of the mode switch, enabled from CanUndo/CanRedo, tooltip from GetUndoText.
- 'Plan leeren' loses ClearArmedUntil/IsClearArmed. ClearPlan toasts 'Plan geleert – „Rückgängig“ holt ihn zurück'.

#### Sofort-Warnung und Plan-Status
Causes:
- FDCScheduledTask gets EDCBlockCause BlockCause {None, DoorLocked, DoorShut, MissingTool, HandsFull, CannotDo, ContainerLocked, Gone, WaitNever, AfterBlocked}, plus FName BlockTarget, FName BlockTool and FText ShortReason ('Bürotür zu').
- Set them in BeginTask's door wait, in ConditionHolds (new out param), in StartWork's Block() (taken from FDCTaskCheck), in the final loop and in both 'kommt nicht dran' Rest loops.
- FDCTaskCheck (DCSkills.h) gets Cause and MissingTool, set in ADCCrewCharacter::CheckToolAndHands, PlanCheckDoor and PlanCheckItem.
- FDCSchedule::FindProblems() returns the root problems (no AfterBlocked) in time order.

Edit wrapper ADCPlayerController::EditPlan(const FText& What, TFunctionRef<void()> Edit):
- Note the root-problem step Ids, run the edit inside an FDCPlanEditScope (Refresh runs synchronously on the broadcast), then compare.
- A new root problem sets PendingHelp {Crew, StepId, Text} and turns the click ring red (HandleClick: Marker->Show(Hit, !bNewProblem)).
- All problems gone: green toast 'Passt jetzt – Plan geht auf'.
- A step added behind an existing problem only toasts '… – kommt erst dran, wenn Schritt 2 geht'.
- IssueTask(s), PlanDrop, ApplyWaitChoice, MoveStep, RemoveStep and Undo/Redo all go through EditPlan.

UDCTimelinePanel:
- (a) Help slot in the Root overlay just above the bar: hit-testable SBorder, red frame, 13-pt text, [Rückgängig] [Später]. Planning only; the toast moves up while it shows.
- (b) Row A status slot, fixed width 460: 'Plan geht auf · fertig 0:41', or red 'Plan hängt: Rosa ab Schritt 2 – Bürotür bleibt zu' with [Lösen …]. Lösen … calls FocusStep + ADCCameraPawn::FocusOn and reopens the help.
- (c) Two play boxes switched by visibility: yellow 'Abspielen' or normal 'Trotzdem abspielen'.
- (d) RebuildList: only the first blocked row is red, with its reason at 12 pt; the AfterBlocked rows after it collapse into one grey row.
- (e) RebuildTracks (~1029-1035): one hatched bar from BlockedSince to the track end, labelled 'N hängt'.

Menus and card:
- FDCRadialOption gets EDCOptionTone Tone {Normal, Waits, Problem}. UDCRadialMenu::Open colours the detail line with CoordinationColor or StepBlockedTextColor.
- When the selected crew member is stuck before the insertion point, every planning entry gets the red detail 'kommt nicht dran – Rosa hängt bei Schritt 2'. The entries stay clickable.
- DCCrewBar Status (124-128): 'hängt: {ShortReason}'.

#### Türen zeigen den Plan im Raum
Door log in the simulation:
- FDCSimDoor gets float Swing (±1).
- New ADCDoor::SwingSignFor(const FVector& Opener) const, split out of OpenAwayFrom. The Open, GoThrough and on-the-way events capture the opener's location and set Swing.
- FDCSchedule gets TArray<FDCSimDoor> StartDoors and TArray<FDCDoorChange> DoorChanges {Time, Door, bLocked, bOpen, Swing, CrewIndex, TaskIndex}.
- ApplyEvent copies State.Doors before Event.Apply and appends each difference. Keep the log out of the Refresh signature.
- New GetDoorAt(FName Door, float Time, FDCSimDoor& Out, const FDCDoorChange** By) const.
- FDCScheduledTask::Effect is built after Simulate from the task's own changes.

Which moment is shown:
- GetViewTime() = PreviewTime if ≥ 0, else the picked step's End, else Schedule.Duration.
- DescribeViewMoment() supplies the frame text.

Door look, visual only:
- ADCDoor::SetPlanLook(bool bShow, bool bLocked, bool bOpen, float Swing, const FText& Label).
- Padlock visibility moves from UpdateState (141-144) into RefreshLook().
- New ShownAmount/ShownSign drive the Hinge in Tick (304-313) while the look is on.
- bOpen, OpenAmount (read by DCCrewCharacter.cpp:507), GetOpenAmount and both nav blockers stay real. That way the nav mesh never rebuilds and no OnNavigationRebuilt → Refresh loop starts.
- The label is a camera-facing TextRender above the door, through DCVisuals::PlainText, only on doors that change.

UDCHeistStateSubsystem::UpdateWorldLook():
- Pushes each door's state at GetViewTime(), only when it changed.
- Called from Refresh, SetFocus and SetPreviewTime.
- Clears all looks outside Planning and in StartPlayback.

Texts:
- DescribePlannedDoor(Door, Time) gets who and when ('Hintertür · aufgesperrt (Vic, 0:04)', 'Hintertür · verschlossen – Vic knackt sie um 0:04').
- ADCDoor::GetInfoLines adds 'Im Plan: …' from DoorChanges.
- The picked row shows Effect at 12 pt.
- UDCFrame (DCFrame.cpp 96-112): right of 'Planung' a 12-pt muted 'Raum zeigt: …', planning only.

#### Geist der gewählten Figur
UpdateGhosts:
- With PreviewTime ≥ 0: unchanged, one pin per crew member.
- Otherwise exactly one ghost for FocusCrew if it has steps, at GetPositionAt(Crew, GetViewTime() capped at that crew member's End). Label 'Rosa danach' or 'Rosa nach 2'.
- If one of her steps is blocked before that moment: a red ghost at the stuck spot (EndLocation, or Path[0] for bWalkBlocked) labelled 'Rosa hängt: {ShortReason}'.
- Facing comes from the new FDCScheduledTask::LookAt (door centre or pickup location, set in BeginTask).

Look 'Figur' (default): new ADCPlanGhost (Public/DCPlanGhost.h, Private/DCPlanGhost.cpp, NotPlaceable).
- Body cylinder, head sphere and nose cube in the proportions of ADCCrewCharacter.cpp:116-131, at 85 %.
- Crew colour × 0.45 via DCVisuals::Paint (opaque). Use a dynamic /Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent at 40 % only if a one-time editor check shows its alpha works. No new assets, nothing under /Game/DerClou/Items.
- Light floor ring and a camera-facing label.
- QueryOnly collision on ECC_Visibility. HandleClick maps a ghost click to Select(its crew) via CrewForClickedActor, instead of planning a walk onto it.

Look 'Stecknadel': the existing ADCPlanMarker::InitGhost/SetGhost.

#### Warten hängt am Schritt – „Rosa wartet hierauf“
Data:
- A WaitFor's Ref now points straight at the giver's step Id. ConditionHolds (State.DoneTasks) and UDCPlanRunnerComponent::ConditionMet (Runner->FindTask(Ref)) already accept any step.
- EDCPlanTaskType::Signal stays only for old plans.

UDCPlanSubsystem:
- LinkWait(FName Waiter, int32 InsertAt, FName Giver, const FGuid& GiverStep), in one edit scope.
- RemoveTask/RemoveLastTask also remove every WaitFor whose Ref is the removed Id, and return the count for the toast.

ADCPlayerController::ToggleWaitOn(FName Giver, int32 Step, FName Waiter):
- If the link exists, remove it ('Rosa wartet nicht mehr auf Vic').
- Otherwise insert before the waiter's first non-WaitFor task whose scheduled Start is earlier than the giver task's End; if there is none, append. The toast says where.
- ApplyWaitChoice with AfterStep → LinkWait, without a Signal.

UDCTimelinePanel picked-row buttons (759-784): one cyan PanelButton per other crew member, 'Rosa wartet hierauf' / 'Rosa wartet nicht mehr', tooltip 'Rosa fängt ihren nächsten Schritt erst an, wenn Vic hiermit fertig ist'.

Texts and playback:
- DescribeWaitFor, non-signal branch: 'warten, bis Vic „Hintertür knacken“ erledigt hat', without a step number because numbers shift.
- DCPlanRunner::FinishTask(Done): if any WaitFor has Ref == this Id → Crew->Say('Zeichen an Rosa').
- UI-Leitfaden §6: 'Zeichen' instead of 'Signal'.
- A link that makes two figures wait for each other shows up as a new WaitNever problem through feature 4's help.

#### Einfügen hinter dem gewählten Schritt
Insertion point:
- UDCHeistStateSubsystem::GetInsertIndex(FName Crew) = FocusStep + 1 when FocusCrew == Crew and a step is picked. In the variant 'nur mit Knopf' only after '+ danach einfügen'. Otherwise INDEX_NONE, which means append.
- One controller entry, ADCPlayerController::PlanInsert(const TArray<FDCPlanTask>&), for IssueTask, IssueTasks, PlanDrop, ApplyWaitChoice and ApplyFix: Plans->InsertTasks(Crew, Index, Tasks) inside EditPlan.
- Then SetFocus(Crew, Index + Num − 1), so the next click follows.
- Toast 'Rosa, neuer Schritt 2: … (nach „…“)'; LastAddedId drives a 0.8-s row flash.

State at the insertion point:
- Simulate(…, const FDCSimOptions* Options) with FDCSimCapture {FName Crew; int32 Index;}.
- When that cursor runs BeginTask for Next == Index (not the bWalkPending re-entry) or finishes its plan, copy State, clone the inventory (CopyFrom inside the existing FMuteScope), and keep Time, Location and bReached.
- Refresh keeps InsertState/InsertInventory. GetMenuState()/GetMenuInventory(Crew) return them while an insertion point exists, else PlannedState/GetPlannedInventory.
- Users: OpenDoorMenu (326-337), OpenPickupMenu (392-397), the 'Hände danach' line (then 'Hände dann:') and UDCInventoryPanel's plan view ('Stand nach Schritt 2').
- bReached == false → the menu title adds 'Rosa hängt vorher bei Schritt N'.

List and keys:
- A fixed-height yellow row after the picked row, with 'ans Ende' = SetFocus(Crew, INDEX_NONE).
- Keep a TSharedPtr<SScrollBox> and call ScrollDescendantIntoView on the picked or new row.
- Row click = pick, second click = unpick. It no longer sets PreviewTime; the room shows 'nach Schritt N' via GetViewTime. Timeline block clicks behave the same.
- PlayerTick: Esc unpicks first; Entf/Rücktaste → RemoveStep(picked), else RemoveLastStep, with a toast.
- UDCWaitDialog sentence (173-176): 'Das wird Schritt N: Rosa wartet nach „…“'.
- Bar hint line at 12 pt: 'Klick auf Boden, Tür oder Gegenstand = neuer Schritt (hinter dem gewählten, sonst am Ende)'.

#### Schritte mit der Maus ziehen
FDCStepDragOp : FDragDropOperation, modelled on FDCItemDragOp (DCItemWidgets.cpp 89-157). The decorator is a mini row 'N. Label' plus the hint '→ vor Schritt 2'.

Starting the drag: in planning, a row's OnMouseButtonDown returns Handled().DetectDrag(row, EKeys::LeftMouseButton), and OnDragDetected starts the op.

Dropping: an SBorder around ListRows handles both events.
- OnDragOver finds the gap from the cached row mid-points with the pure helper ComputeDropIndex(const TArray<float>& RowMidYs, float MouseY) and draws a yellow 3-px line in an overlay slot.
- OnDrop → ADCPlayerController::MoveStepTo(From, To) → UDCPlanSubsystem::MoveTaskTo: one broadcast, one undo entry.
- Toast 'Rosa: „…“ ist jetzt Schritt 2'; the moved step stays picked.

Details:
- NativeTick skips RebuildRows while FSlateApplication::Get().IsDragDropping().
- The list scrolls by itself when the cursor is within 16 px of its edge.
- Rows of another crew member refuse the drop ('Schritte bleiben bei ihrer Figur').
- Its own op type keeps the bar's MakeSwallowZone for item drops unaffected.
- ↑/↓ stay.

#### Lösungen per Klick (Probelauf im Hintergrund)
Probe:
- FDCSimOptions gets const TMap<FName, TArray<FDCPlanTask>>* Plans.
- Every plan read inside Simulate goes through a small FDCPlanSource (GetTasks/FindTask, override first): Cursor.Tasks (737), StartWork's WaitFor branch (1064-1076) and a DescribeWaitFor overload that takes the source. Today DescribeWaitFor reads UDCPlanSubsystem directly, so a probe would name the wrong steps.
- UDCHeistStateSubsystem::Probe(FName Crew, int32 InsertAt, const TArray<FDCPlanTask>& Tasks, const TArray<FDCPlanEdit>& Extra = {}) const returns FDCProbe {bBlocked, Cause, ShortReason, End, Waited, WaitForName, WaitForLabel, OpenedOnWay, NewlyBlocked}.
- Candidates get temporary Guids. A probe never touches Schedule, PlannedState, inventories or markers.

Speed:
- Cache WalkSeconds/WayThroughDoors results in the subsystem: a mutable TMap keyed by From/To rounded to 5 cm, holding seconds and path.
- Clear it in OnNavigationRebuilt and OnWorldBeginPlay. Refresh uses the cache too.
- Log 'Probe x ms' and 'Refresh x ms'.

Fixes (new Public/DCPlanFix.h + Private/DCPlanFix.cpp, namespace DCPlanFix):
- TArray<FDCPlanFix> Build(const UDCHeistStateSubsystem&, FName Crew, int32 Index); FDCPlanFix {FText Label; FText Detail; TArray<FDCPlanEdit> Edits;}; FDCPlanEdit {FName Crew; int32 InsertAt; FDCPlanTask Task;}.
- Candidates by BlockCause:
  - DoorLocked → her own PickLock right before the step, and every other crew member's PickLock at the start or end of their plan (keep the better one).
  - DoorShut → another crew member's Open, or a Drop of the held item.
  - MissingTool → PickUp of the nearest ADCItemPickup holding BlockTool, before the step.
  - HandsFull → Drop.
  - ContainerLocked → PickLock by whoever can.
  - WaitNever/Gone → 'Schritt löschen'.
- A candidate counts only if Probe(Extra) shows the blocked step working and NewlyBlocked is empty. At most two are kept, ordered by plan end. Detail comes from the probe ('Rosa wartet ca. 5 s · fertig 0:33').

Applying a fix:
- ADCPlayerController::ApplyFix(int32) runs in one EditPlan ('Lösung: Vic knackt die Bürotür'); the toast says what went where.
- The help bar shows the fixes first; the first uses PrimaryButtonStyle.
- The variant 'Menü an der Maus' opens the same list in UDCRadialMenu at the click position: title = ShortReason, entries = the fixes, 'Rückgängig', 'Später'.

#### Menü sagt vorher, was passiert
Probing:
- In planning, MakeTaskOption and the pair options call Heist->Probe(Selected, GetInsertIndex, {Task…}) once when the menu opens.
- Only entries whose ability check passes are probed. The shared walk is cached after the first one.

Detail line, one fact by priority (Tone set to match):
- red 'kommt nicht dran – hängt bei Schritt N' (from feature 4, no probe)
- red 'geht nicht: {ShortReason}'
- cyan 'wartet {s} s auf {Name}'
- otherwise DescribeShort ('2 s · 100 %')
The tooltip keeps the long form: duration, chance, way note, 'fertig um'.

Other rules:
- Grey entries: run PlanCheck for the other crew members and add who can ('Dietrich-Set fehlt – Vic kann es').
- The title uses DescribePlannedDoor(Door, InsertTime) with the door log ('Hintertür · Vic knackt sie um 0:04').
- At a door someone else unlocks later, 'Schloss knacken' is offered only if this figure can do it.
- Red entries stay clickable and lead to the help.

Budget: log the menu-open time. Above 30 ms, fall back to one probe (the walk to the target) for all entries, plus the door log.

#### Varianten-Schalter zum Ausprobieren
Settings:
- UDCPlanSubsystem::Variants {EDCGhostLook Ghost = Figure; EDCHelpStyle Help = Bar; EDCInsertMode Insert = AfterPicked;}.
- Loaded and saved with GConfig in GGameUserSettingsIni (section 'DerClou.Variants').
- Self-tests can set them with -DCVariant=Ghost:Pin,Help:Menu,Insert:Button.
- Each change logs 'Variant …' and calls Heist->Refresh().

Panel:
- UDCFrame: a small 'Varianten …' button at the right end of the top strip, planning only.
- It toggles the new UDCVariantsPanel (Public/DCVariantsPanel.h, Private/DCVariantsPanel.cpp, Slate) in the context slot top right. It closes Info/Durchsuchen while open, because the guide allows only one panel there.
- Three rows, each a two-part switch like 'Planen | Ausprobieren' plus one 12-pt sentence on what changes; × closes it.
- The insert variant adds a '+ danach einfügen' button to the picked row.
