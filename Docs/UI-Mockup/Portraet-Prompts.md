# Porträts der Crew – Prompts für die Bild-KI

Stand 2026-11-09. Die Porträts stehen auf den Profilkarten links (66 × 88) und oben im Profil rechts (60 × 80).
Sie sollen wie im Mockup `planung_v1.webp` aussehen: gemalt, ernst, dunkler Hintergrund. Ein Unterschied zum Mockup:
Das Spiel spielt **heute** (Laptop, Akku-Bohrer, Garagenöffner), deshalb tragen die Figuren zeitlose dunkle Kleidung
statt der 1920er-Mode aus dem Mockup.

## So geht's

1. Zuerst **Vic** erzeugen: Stil-Block + Vic-Block zusammen in die Bild-KI geben. Ein paar Varianten erzeugen und die
   beste nehmen.
2. Für **Rosa** (und später Kurt) das fertige Vic-Bild als **Referenzbild** hochladen und davor schreiben:
   *„Same painting style, lighting, background colour, framing and crop as the reference image, but a different
   person:“* – danach Stil-Block + Rosa-Block. So passen die Bilder zusammen.
3. Bilder speichern unter `Art/Portraits/vic.png`, `Art/Portraits/rosa.png` (JPG oder WebP gehen auch).
   Claude schneidet sie auf 3:4 zu, verkleinert sie und legt sie nach `Content/Slate/Portraits/`. Das Spiel lädt
   sie von dort; fehlt ein Bild, zeigt die Karte den Anfangsbuchstaben.

Format: **hochkant 3:4** (z. B. 1536 × 2048). Quadratisch geht auch, dann wird links und rechts etwas abgeschnitten.
Wichtig: Kopf und Schultern, Gesicht in der Bildmitte, oben etwas Luft. Keine Schrift, kein Rahmen.

## Stil-Block (bei jedem Bild gleich)

```
Painted character portrait for the user interface of a dark tactical heist game.
Head and shoulders only, three-quarter view, the head turned slightly towards the right edge
of the image. Face centred horizontally, eyes at about 40 % of the image height, a little
empty space above the head.
Present day, timeless dark clothing, nothing trendy.
Semi-realistic digital painting in a graphic-novel noir style: fine visible brush strokes,
muted and slightly desaturated colours, deep soft shadows, subtle film grain.
Lighting: warm tungsten key light from the left like a desk lamp, cool blue moonlight rim
light from the right.
Background: plain dark smoky blue-grey (#141B23), getting darker towards the edges, no
objects, no room.
Serious, calm, focused expression, mouth closed.
No text, no letters, no logo, no frame, no border, no watermark, no hands, no weapons.
Portrait orientation, aspect ratio 3:4.
```

## Vic – Schlösser und Kraft

```
The person: a man around 40, the crew's lock specialist and its strongest member. Lean but
broad-shouldered, weathered face with a strong jaw, short dark hair, three-day stubble, a
small old scar through the left eyebrow, calm watchful grey eyes. Dark wool flat cap,
charcoal work jacket with the collar turned up over a dark knit sweater.
```

## Rosa – Tresore und Elektronik

```
The person: a woman around 35, the crew's safecracker and electronics expert. Dark curly
hair loosely pinned up with a few strands falling into her face, sharp intelligent dark
eyes, one eyebrow slightly raised, small gold stud earrings. Black turtleneck under a dark
oxblood leather jacket. A doctor's stethoscope hangs around her neck – the tool she uses to
listen to safe locks.
```

## Kurt – Fahren und Aufpassen (optional, für später)

Kurt gibt es im Spiel noch nicht (er steht nur im Mockup). Wer ihn gleich mit erzeugt, hat später ein passendes Bild.

```
The person: a heavy-set man around 55, the crew's driver and lookout. Broad face, short grey
beard, thick neck, tired but alert eyes under heavy brows. Dark tweed flat cap, worn brown
leather driving jacket over a dark shirt.
```

## Falls etwas nicht passt

- Schrift oder Rahmen im Bild: am Ende noch einmal *„absolutely no text and no frame“* anhängen.
- Zu bunt oder zu hell: *„darker, more muted, low-key lighting“* anhängen.
- Lieber der Look aus dem Mockup (1920er): im Stil-Block *„Present day, timeless dark clothing“* ersetzen durch
  *„1920s clothing“*.
