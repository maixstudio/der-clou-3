# Referenzbilder für die 3D-Figuren – Prompts für die Bild-KI

Stand 2026-11-09. Aus diesen Bildern baut `Art/Characters/build_character.py --char <Name>` die 3D-Figur:
Umrisse von vorn und von der Seite ergeben die Form, die Fotos werden als Farbe aufgemalt, die Kopf-Nahaufnahme
liefert Gesicht und Profil. Rosa ist so entstanden. Je genauer die Bilder die Regeln unten einhalten, desto besser die
Figur.

## Was die Bilder brauchen (wichtiger als Schönheit)

- **Gleichmäßig grauer Hintergrund** (#767676), kein Boden, kein Schatten, nichts anderes im Bild.
- **T-Pose:** Arme waagrecht zur Seite, Handflächen nach unten, Finger gestreckt und geschlossen, Beine gerade, Füße
  etwa 20 cm auseinander.
- **Wie aus großer Entfernung fotografiert** (Teleobjektiv, keine Perspektive), Kamera auf Hüfthöhe.
- **Ganze Figur** vom Scheitel bis zur Sohle mit etwas Rand, mittig.
- **Weiches, gleichmäßiges Licht** von vorn, keine harten Schatten, keine farbigen Lichter. Die Farben im Bild werden
  die Farben der Figur.
- **Alle Ansichten zeigen dieselbe Person mit derselben Kleidung.**
- **Kein langer Mantel:** Eine Jacke bis zur Hüfte. Ein Mantel bis zu den Knien würde beim Gehen zwischen den Beinen
  zerreißen.
- Querformat **3:2** (z. B. 1536 × 1024), die Kopf-Nahaufnahme **2:1** (z. B. 1774 × 887).

## So geht's

1. **Vorderansicht zuerst:** Das Porträt (`Art/Portraits/vic.webp`) als Referenzbild hochladen, davor schreiben:
   *„Same man as in the reference image (face, cap, stubble, scar), but full body:“* – danach Bogen-Block +
   Personen-Block + Ansicht „Front“. Ein paar Varianten erzeugen, die mit der saubersten T-Pose nehmen.
2. **Die anderen drei Ansichten:** Die fertige Vorderansicht als Referenzbild hochladen, davor:
   *„Same person, same clothes, same pose, same lighting and the same grey background as the reference image, now:“*
   – danach Bogen-Block + Personen-Block + die jeweilige Ansicht.
3. **Kopf-Nahaufnahme:** Porträt und Vorderansicht als Referenz, Kopf-Block + Personen-Block.
4. Speichern unter `Art/Characters/<Name>/ref/` mit den Dateinamen aus der Tabelle (PNG, JPG oder WebP).

| Datei | Ansicht |
|---|---|
| `<name>_front` | von vorn |
| `<name>_back` | von hinten |
| `<name>_side_left` | seine **linke** Seite, Gesicht zeigt zum **linken** Bildrand |
| `<name>_side_right` | seine **rechte** Seite, Gesicht zeigt zum **rechten** Bildrand |
| `<name>_head` | Kopf: links von vorn, rechts im Profil mit dem Gesicht zum rechten Bildrand |
| `<name>_head_back` (gern) | Kopf von hinten, gleicher Ausschnitt wie die Vorderansicht im Kopfbild |

## Bogen-Block (jede Ganzkörper-Ansicht)

```
Character reference image for 3D modelling, one single view of one person.
Full body in a strict T-pose: both arms stretched straight out to the sides at shoulder height,
exactly horizontal, palms facing down, fingers straight and closed, thumbs alongside the index
fingers. Legs straight, feet about 20 cm apart, toes pointing forward. Head straight, eyes
looking ahead, neutral face, mouth closed.
Orthographic look: shot from far away with a long telephoto lens, no perspective distortion,
camera at hip height.
The whole figure from the top of the head to the soles is visible with a small margin, centred,
filling about 90 % of the image height.
Plain flat medium-grey studio background (#767676), no floor, no horizon line, no cast shadows,
no props.
Soft, even, neutral studio lighting from the front, no hard shadows, no rim light, no coloured
light, true colours.
Semi-realistic with photographic detail, sharp focus.
No text, no labels, no frame, no watermark.
Landscape image, aspect ratio 3:2.
```

## Ansichten (ans Ende anhängen)

- **Front:** `View: exactly from the front.`
- **Back:** `View: exactly from behind – we see his back, the back of his head and his heels.`
- **Left side:** `View: exactly from his left side. He faces the LEFT edge of the image. His left arm points straight at the camera, so it looks short; the right arm is hidden behind his body.`
- **Right side:** `View: exactly from his right side. He faces the RIGHT edge of the image. His right arm points straight at the camera, so it looks short; the left arm is hidden behind his body.`

## Kopf-Block

```
Head close-up reference for 3D modelling: two views of the same head side by side on one image.
Left half: the head exactly from the front. Right half: the same head in exact profile, the face
pointing to the RIGHT edge of the image. Both at the same size and the same height (eyes on the
same line, chin on the same line), head and neck down to the collar, small margin above the head.
Neutral face, mouth closed, eyes looking straight ahead.
Plain flat medium-grey studio background (#767676), soft even neutral studio lighting from the
front, no hard shadows, true colours. Photographic detail, sharp focus.
No text, no labels, no frame, no watermark.
Landscape image, aspect ratio 2:1.
```

Für `_head_back` (gern dazu): *„The same head exactly from behind, same size and framing as the front view of the
reference image, plain grey background, even light.“*

## Vic

Er trägt die Kleidung aus dem Porträt, aber die Jacke endet an der Hüfte.

```
The person: Vic, a man around 40, about 1.83 m tall, lean but broad-shouldered, the lock
specialist and strongest member of a heist crew. Weathered face with a strong jaw, short dark
hair, three-day stubble, a small old scar through the left eyebrow, calm grey eyes.
Dark charcoal wool flat cap. Charcoal work jacket that ends at the hips (not a long coat),
collar turned up, over a dark knit turtleneck sweater. Dark straight work trousers, not baggy.
Dark leather work boots. No gloves, no bag, nothing in his hands.
```

## Kurt (für später)

```
The person: Kurt, a heavy-set man around 55, about 1.78 m tall, the driver and lookout of a
heist crew. Broad face, short grey beard, thick neck, tired but alert eyes under heavy brows.
Dark tweed flat cap. Worn brown leather driving jacket that ends at the hips over a dark
shirt. Dark trousers, brown leather shoes. No gloves, no bag, nothing in his hands.
```

## Falls etwas nicht passt

- Arme hängen schräg statt waagrecht: *„arms exactly horizontal like a capital T“* anhängen.
- Schatten am Boden: *„no floor, no shadow, the figure floats on the plain grey background“* anhängen.
- Perspektive (Kopf zu groß, Füße zu klein): *„orthographic, telephoto lens from 30 metres away“* anhängen.
- Profil schaut in die falsche Richtung: Bild einfach horizontal spiegeln – das ist in Ordnung.
