"""Villa Lindenhof – layout data for the demo level (draft v1, 2026-11-08).

Single source of truth for the floor plan drawing (villa_plan.py -> Docs/Villa/Villa-Grundriss.html)
and, later, for the Unreal level builder.

Units and axes: metres. Plan x = east, plan y = north, origin = south-west corner of the plot.
The street lies south of y = 0. In Unreal: X_cm = y * 100 (away from the street), Y_cm = x * 100.

Rooms are rectangles on the wall centre lines (x0, y0, x1, y1). Walls are derived from the room edges:
an edge with rooms on both sides is an interior wall (15 cm), otherwise an exterior wall (30 cm).
A room may lie inside another one (pantry in the kitchen); it is drawn on top and gets its own walls.
"""

NAME = "Villa Lindenhof"
ADDRESS = "Lindenallee 12"
PLOT = (0.0, 0.0, 60.0, 48.0)

EXTERIOR_WALL = 0.30
INTERIOR_WALL = 0.15

# Floor levels: z of the finished floor and clear room height in metres.
FLOORS = {
    "KG": {"name": "Keller", "z": -2.90, "height": 2.55},
    "EG": {"name": "Erdgeschoss", "z": 0.00, "height": 2.80},
    "OG": {"name": "1. Stock", "z": 3.10, "height": 2.70},
}

# kind: wohnen, schlafen, nass, service, verkehr, technik, wert, garage
ROOMS = [
    # ---- Haupthaus, Erdgeschoss (x 22..44, y 16..30) ----
    dict(id="eg_hwr", floor="EG", building="Haus", name="Hauswirtschaft", rect=(22, 16, 25.5, 22), kind="service",
         note="Waschmaschine, Putzschrank, Sicherungen fürs EG"),
    dict(id="eg_wc", floor="EG", building="Haus", name="Gäste-WC", rect=(25.5, 16, 29, 18.5), kind="nass",
         note="Kippfenster zur Straße – oft gekippt"),
    dict(id="eg_garderobe", floor="EG", building="Haus", name="Garderobe", rect=(25.5, 18.5, 29, 22), kind="verkehr",
         note="Mäntel, Schlüsselbrett (Garage, Kellertür)"),
    dict(id="eg_flur", floor="EG", building="Haus", name="Flur", rect=(22, 22, 29, 24), kind="verkehr",
         note="Wirtschaftsflur vom Seiteneingang zur Halle"),
    dict(id="eg_kueche", floor="EG", building="Haus", name="Küche", rect=(22, 24, 29, 30), kind="service",
         note="Kochinsel, Tür zum Kräutergarten", label_at=(26.3, 28.75)),
    dict(id="eg_speisekammer", floor="EG", building="Haus", name="Speise­kammer", rect=(22, 24, 24.5, 26.5), kind="service",
         note="Vorräte – Bargeld in der Mehldose?"),
    dict(id="eg_halle", floor="EG", building="Haus", name="Halle", rect=(29, 16, 36, 24), kind="verkehr",
         note="Eingangshalle mit Treppe nach oben, Alarm-Bedienteil an der Haustür", label_at=(32.5, 21.0)),
    dict(id="eg_kellerabgang", floor="EG", building="Haus", name="Keller­abgang", rect=(29, 16, 30.8, 22), kind="verkehr",
         note="Treppe in den Keller hinter der Kellertür", label_at=None),
    dict(id="eg_esszimmer", floor="EG", building="Haus", name="Esszimmer", rect=(29, 24, 36, 30), kind="wohnen",
         note="Tafel für 10, Anrichte mit Silberbesteck", label_at=(32.5, 28.45)),
    dict(id="eg_bibliothek", floor="EG", building="Haus", name="Bibliothek", rect=(36, 16, 44, 21), kind="wohnen",
         note="Regale an allen Wänden, Kamin, Erstausgaben", label_at=(39.2, 18.6)),
    dict(id="eg_geheimkammer", floor="EG", building="Haus", name="Geheim­kammer", rect=(42, 16, 44, 18.5), kind="wert",
         note="hinter einem drehbaren Bücherregal (optional)"),
    dict(id="eg_wohnzimmer", floor="EG", building="Haus", name="Wohnzimmer", rect=(36, 21, 44, 30), kind="wohnen",
         note="Kamin, Flügel, große Schiebetür zur Terrasse"),

    # ---- Haupthaus, 1. Stock ----
    dict(id="og_gaeste", floor="OG", building="Haus", name="Gästezimmer", rect=(22, 16, 27, 22), kind="schlafen",
         note="Doppelbett, Schrank"),
    dict(id="og_kind", floor="OG", building="Haus", name="Kinderzimmer", rect=(27, 16, 32, 22), kind="schlafen",
         note="Spardose, Spielkonsole"),
    dict(id="og_galerie", floor="OG", building="Haus", name="Galerie", rect=(32, 16, 36, 22), kind="verkehr",
         note="Treppe kommt hier an, Fenster über dem Eingang", label_at=(33.1, 19.0)),
    dict(id="og_flur", floor="OG", building="Haus", name="Flur", rect=(22, 22, 36, 24), kind="verkehr",
         note="Flur 1. Stock", label_at=(28.0, 23.0)),
    dict(id="og_arbeit", floor="OG", building="Haus", name="Arbeitszimmer", rect=(22, 24, 28, 30), kind="wohnen",
         note="Schreibtisch, Wandtresor hinter dem Gemälde, Laptop"),
    dict(id="og_bad", floor="OG", building="Haus", name="Bad", rect=(28, 24, 32, 30), kind="nass",
         note="Familienbad mit Wanne"),
    dict(id="og_jugend", floor="OG", building="Haus", name="Jugendzimmer", rect=(32, 24, 36, 30), kind="schlafen",
         note="Laptop, Kopfhörer"),
    dict(id="og_eltern", floor="OG", building="Haus", name="Eltern­schlafzimmer", rect=(36, 21, 44, 30), kind="schlafen",
         note="Bett, Nachttische (Schatulle), Balkon über der Terrasse", label_at=(38.9, 26.0)),
    dict(id="og_ankleide", floor="OG", building="Haus", name="Ankleide", rect=(36, 16, 39.5, 21), kind="wert",
         note="Schmuck, Uhrenbox, kleiner Möbeltresor"),
    dict(id="og_elternbad", floor="OG", building="Haus", name="Eltern­bad", rect=(39.5, 16, 44, 21), kind="nass",
         note="Dusche, Wanne, Milchglas"),

    # ---- Haupthaus, Keller ----
    dict(id="kg_technik", floor="KG", building="Haus", name="Technik", rect=(22, 16, 26, 22), kind="technik",
         note="Heizung, Hauptsicherungen, Alarmzentrale"),
    dict(id="kg_lager", floor="KG", building="Haus", name="Lager", rect=(26, 16, 29, 22), kind="service",
         note="Regale, Kartons, Werkzeugkiste"),
    dict(id="kg_treppe", floor="KG", building="Haus", name="Treppe", rect=(29, 16, 30.8, 22), kind="verkehr",
         note="Kellertreppe", label_at=None),
    dict(id="kg_wein", floor="KG", building="Haus", name="Weinkeller", rect=(30.8, 16, 37, 22), kind="wert",
         note="Regale mit Raritäten – schwer und zerbrechlich"),
    dict(id="kg_tresor", floor="KG", building="Haus", name="Tresorraum", rect=(37, 16, 44, 22), kind="wert",
         note="Hauptziel: Panzertür, großer Tresor, Bargeld, Gold"),
    dict(id="kg_flur", floor="KG", building="Haus", name="Kellerflur", rect=(22, 22, 44, 24), kind="verkehr",
         note="langer Gang, eine Lampe", label_at=(37.0, 23.0)),
    dict(id="kg_fitness", floor="KG", building="Haus", name="Fitness", rect=(22, 24, 29, 30), kind="wohnen",
         note="Geräte, Sauna in der Ecke", label_at=(24.6, 27.0)),
    dict(id="kg_sauna", floor="KG", building="Haus", name="Sauna", rect=(26.5, 27.5, 29, 30), kind="nass",
         note="Holzkabine"),
    dict(id="kg_hobby", floor="KG", building="Haus", name="Hobbyraum", rect=(29, 24, 37, 30), kind="wohnen",
         note="Billard, Bar mit Spirituosen", label_at=(33.6, 27.0)),
    dict(id="kg_abstell", floor="KG", building="Haus", name="Abstell­raum", rect=(37, 24, 40, 30), kind="service",
         note="alte Möbel, Koffer, Bilder"),
    dict(id="kg_vorraum", floor="KG", building="Haus", name="Keller­vorraum", rect=(40, 24, 44, 30), kind="verkehr",
         note="Stahltür zur Kelleraußentreppe"),

    # ---- Nebengebäude „Remise“ (x 3..15, y 18..25) ----
    dict(id="nb_garage", floor="EG", building="Remise", name="Doppelgarage", rect=(3, 18, 10, 25), kind="garage",
         note="zwei Autos, Sektionaltor elektrisch", label_at=(6.8, 23.85)),
    dict(id="nb_werkstatt", floor="EG", building="Remise", name="Werkstatt", rect=(10, 18, 15, 22.5), kind="service",
         note="Werkbank: Bolzenschneider, Brecheisen, Akkuschrauber"),
    dict(id="nb_geraete", floor="EG", building="Remise", name="Geräteraum", rect=(10, 22.5, 15, 25), kind="service",
         note="Rasenmäher, Leiter (reicht bis zum Balkon)"),
]

# Doors and gates (at least 1.0 m wide: nav agent radius 30 cm on a 10 cm grid). at = centre on the wall line; swing = side the leaf opens to (N/S/E/W), which also fixes
# the wall axis (N/S = wall runs east-west). hinge "-" = leaf hinged at the smaller coordinate end.
# type: haustuer, tuer, doppel, glas, glas2, schiebe, stahl, panzer, gitter, geheim, offen, tor, schiebetor, pforte
DOORS = [
    # Erdgeschoss
    dict(id="d_haustuer", floor="EG", at=(32.5, 16), width=1.2, type="haustuer", swing="N", hinge="-",
         locked=True, lock=6, name="Haustür", note="massiv, Mehrfachverriegelung, Alarm-Kontakt"),
    dict(id="d_seiteneingang", floor="EG", at=(22, 23), width=1.0, type="tuer", swing="E", hinge="-",
         locked=True, lock=4, name="Seiteneingang", note="Lieferanteneingang, Bewegungsmelder-Lampe"),
    dict(id="d_kuechentuer", floor="EG", at=(27.5, 30), width=1.0, type="glas", swing="S", hinge="+",
         locked=True, lock=3, name="Küchentür", note="Glastür zum Kräutergarten"),
    dict(id="d_terrasse_ess", floor="EG", at=(32.5, 30), width=1.4, type="glas2", swing="S", hinge="-",
         locked=True, lock=4, name="Terrassentür Esszimmer", note="nur innen verriegelt: aufhebeln oder Glas"),
    dict(id="d_schiebetuer", floor="EG", at=(40, 30), width=3.0, type="schiebe", swing="S", hinge="-",
         locked=True, lock=4, name="Hebeschiebetür Wohnzimmer", note="Glas einschlagen = sehr laut"),
    dict(id="d_halle_ess", floor="EG", at=(32.5, 24), width=1.6, type="doppel", swing="N", hinge="-",
         locked=False, lock=None, name="Halle → Esszimmer", note="Flügeltür"),
    dict(id="d_flur_halle", floor="EG", at=(29, 23), width=1.4, type="offen", swing="E", hinge="-",
         locked=False, lock=None, name="Flur → Halle", note="Durchgang"),
    dict(id="d_flur_kueche", floor="EG", at=(26.5, 24), width=1.0, type="tuer", swing="N", hinge="+",
         locked=False, lock=None, name="Flur → Küche", note=""),
    dict(id="d_kueche_ess", floor="EG", at=(29, 27), width=1.0, type="tuer", swing="E", hinge="+",
         locked=False, lock=None, name="Küche → Esszimmer", note="Pendeltür"),
    dict(id="d_speisekammer", floor="EG", at=(23.25, 26.5), width=1.0, type="tuer", swing="N", hinge="-",
         locked=False, lock=None, name="Speisekammer", note=""),
    dict(id="d_hwr", floor="EG", at=(23.75, 22), width=1.0, type="tuer", swing="S", hinge="-",
         locked=False, lock=None, name="Hauswirtschaft", note=""),
    dict(id="d_garderobe", floor="EG", at=(27.25, 22), width=1.0, type="tuer", swing="S", hinge="+",
         locked=False, lock=None, name="Garderobe", note=""),
    dict(id="d_wc", floor="EG", at=(27.25, 18.5), width=1.0, type="tuer", swing="S", hinge="-",
         locked=False, lock=None, name="Gäste-WC", note="Riegel innen"),
    dict(id="d_kellertuer", floor="EG", at=(30.8, 16.9), width=1.0, type="tuer", swing="E", hinge="-",
         locked=True, lock=3, name="Kellertür", note="Schlüssel hängt in der Garderobe", badge_side="swing"),
    dict(id="d_halle_wohn", floor="EG", at=(36, 22.9), width=1.4, type="doppel", swing="E", hinge="-",
         locked=False, lock=None, name="Halle → Wohnzimmer", note="Flügeltür"),
    dict(id="d_ess_wohn", floor="EG", at=(36, 27.2), width=2.0, type="offen", swing="E", hinge="-",
         locked=False, lock=None, name="Esszimmer → Wohnzimmer", note="breiter Durchgang"),
    dict(id="d_wohn_bib", floor="EG", at=(39.6, 21), width=1.6, type="doppel", swing="S", hinge="-",
         locked=False, lock=None, name="Wohnzimmer → Bibliothek", note="Flügeltür"),
    dict(id="d_geheim", floor="EG", at=(42, 17.25), width=1.0, type="geheim", swing="W", hinge="-",
         locked=False, lock=None, name="Geheimtür", note="Bücherregal dreht sich – erst finden"),

    # 1. Stock
    dict(id="o_gaeste", floor="OG", at=(24.5, 22), width=1.0, type="tuer", swing="S", hinge="+",
         locked=False, lock=None, name="Gästezimmer", note=""),
    dict(id="o_kind", floor="OG", at=(29.5, 22), width=1.0, type="tuer", swing="S", hinge="+",
         locked=False, lock=None, name="Kinderzimmer", note=""),
    dict(id="o_galerie", floor="OG", at=(34, 22), width=3.7, type="offen", swing="S", hinge="-",
         locked=False, lock=None, name="Galerie ↔ Flur", note="offen, Geländer an der Treppe"),
    dict(id="o_arbeit", floor="OG", at=(26, 24), width=1.0, type="tuer", swing="N", hinge="-",
         locked=True, lock=4, name="Arbeitszimmer", note="abgesperrt, Hausherr hat den Schlüssel"),
    dict(id="o_bad", floor="OG", at=(30, 24), width=1.0, type="tuer", swing="N", hinge="-",
         locked=False, lock=None, name="Bad", note=""),
    dict(id="o_jugend", floor="OG", at=(34, 24), width=1.0, type="tuer", swing="N", hinge="+",
         locked=False, lock=None, name="Jugendzimmer", note=""),
    dict(id="o_eltern", floor="OG", at=(36, 23), width=1.0, type="tuer", swing="E", hinge="+",
         locked=False, lock=None, name="Elternschlafzimmer", note=""),
    dict(id="o_ankleide", floor="OG", at=(37.75, 21), width=1.6, type="schiebe", swing="S", hinge="-",
         locked=False, lock=None, name="Ankleide", note="Schiebetür"),
    dict(id="o_elternbad", floor="OG", at=(42, 21), width=1.0, type="tuer", swing="S", hinge="+",
         locked=False, lock=None, name="Elternbad", note=""),
    dict(id="o_balkon", floor="OG", at=(40, 30), width=1.0, type="glas", swing="S", hinge="-",
         locked=True, lock=3, name="Balkontür", note="von außen nur aufhebeln (Leiter nötig)"),

    # Keller
    dict(id="k_treppe", floor="KG", at=(29.9, 22), width=1.5, type="offen", swing="N", hinge="-",
         locked=False, lock=None, name="Treppe → Kellerflur", note=""),
    dict(id="k_technik", floor="KG", at=(24, 22), width=1.0, type="stahl", swing="S", hinge="-",
         locked=True, lock=4, name="Technik", note="Feuerschutztür"),
    dict(id="k_lager", floor="KG", at=(27.5, 22), width=1.0, type="tuer", swing="S", hinge="+",
         locked=False, lock=None, name="Lager", note="Lattentür"),
    dict(id="k_wein", floor="KG", at=(34, 22), width=1.0, type="gitter", swing="S", hinge="-",
         locked=True, lock=3, name="Weinkeller", note="Gittertür"),
    dict(id="k_tresor", floor="KG", at=(40.5, 22), width=1.0, type="panzer", swing="S", hinge="-",
         locked=True, lock=8, name="Tresorraum", note="Panzertür: nur knacken (Tresore 8), nicht aufzubrechen"),
    dict(id="k_fitness", floor="KG", at=(25.5, 24), width=1.0, type="tuer", swing="N", hinge="-",
         locked=False, lock=None, name="Fitness", note=""),
    dict(id="k_sauna", floor="KG", at=(27.75, 27.5), width=1.0, type="glas", swing="S", hinge="+",
         locked=False, lock=None, name="Sauna", note=""),
    dict(id="k_hobby", floor="KG", at=(33, 24), width=1.6, type="doppel", swing="N", hinge="-",
         locked=False, lock=None, name="Hobbyraum", note=""),
    dict(id="k_abstell", floor="KG", at=(38.5, 24), width=1.0, type="tuer", swing="N", hinge="-",
         locked=False, lock=None, name="Abstellraum", note=""),
    dict(id="k_vorraum", floor="KG", at=(42, 24), width=1.0, type="tuer", swing="N", hinge="+",
         locked=False, lock=None, name="Kellervorraum", note=""),
    dict(id="k_aussen", floor="KG", at=(44, 28.9), width=1.0, type="stahl", swing="W", hinge="+",
         locked=True, lock=5, name="Kelleraußentür", note="Stahltür am Fuß der Außentreppe"),

    # Remise
    dict(id="r_tor", floor="EG", at=(6.5, 18), width=5.0, type="tor", swing="N", hinge="-",
         locked=True, lock=None, name="Garagentor", note="Sektionaltor, elektrisch (Elektronik oder Handsender)"),
    dict(id="r_werkstatt", floor="EG", at=(10, 20), width=1.0, type="tuer", swing="E", hinge="-",
         locked=False, lock=None, name="Garage → Werkstatt", note=""),
    dict(id="r_seitentuer", floor="EG", at=(15, 19.6), width=1.0, type="tuer", swing="W", hinge="-",
         locked=True, lock=3, name="Werkstatt-Seitentür", note=""),
    dict(id="r_geraete", floor="EG", at=(15, 23.75), width=1.0, type="tuer", swing="E", hinge="-",
         locked=True, lock=2, name="Geräteraum", note="Vorhängeschloss außen"),
    dict(id="r_garage_hinten", floor="EG", at=(4.5, 25), width=1.0, type="tuer", swing="S", hinge="+",
         locked=True, lock=3, name="Garage hinten", note="Tür zum Garten"),

    # Grundstück (Zaun)
    dict(id="g_einfahrt", floor="Aussen", at=(8, 0), width=4.0, type="schiebetor", swing="N", hinge="-",
         locked=True, lock=None, name="Einfahrtstor", note="elektrisch, Kamera und Gegensprechanlage"),
    dict(id="g_pforte", floor="Aussen", at=(32.5, 0), width=1.4, type="pforte", swing="N", hinge="-",
         locked=True, lock=3, name="Gartenpforte", note="Klingel, Briefkasten"),
    dict(id="g_hintertor", floor="Aussen", at=(52.6, 48), width=1.4, type="pforte", swing="S", hinge="-",
         locked=True, lock=2, name="Hintertor", note="Kette mit Vorhängeschloss, zum Feldweg"),
]

# Windows: at = centre on the wall line, axis "h" (wall runs east-west) or "v".
# type: fenster, bodentief, kipp (left tilted open), milch (frosted), keller (light well with grate)
WINDOWS = [
    # EG
    dict(floor="EG", at=(23.75, 16), width=1.2, axis="h", type="fenster"),
    dict(floor="EG", at=(27.25, 16), width=0.7, axis="h", type="kipp"),
    dict(floor="EG", at=(37.8, 16), width=1.2, axis="h", type="fenster"),
    dict(floor="EG", at=(40.4, 16), width=1.2, axis="h", type="fenster"),
    dict(floor="EG", at=(22, 19.2), width=1.2, axis="v", type="fenster"),
    dict(floor="EG", at=(22, 28.2), width=1.4, axis="v", type="fenster"),
    dict(floor="EG", at=(24.6, 30), width=1.4, axis="h", type="fenster"),
    dict(floor="EG", at=(30.4, 30), width=1.2, axis="h", type="bodentief"),
    dict(floor="EG", at=(34.6, 30), width=1.2, axis="h", type="bodentief"),
    dict(floor="EG", at=(37.3, 30), width=1.0, axis="h", type="bodentief"),
    dict(floor="EG", at=(42.8, 30), width=1.4, axis="h", type="bodentief"),
    dict(floor="EG", at=(44, 19.75), width=1.2, axis="v", type="fenster"),
    dict(floor="EG", at=(44, 23.4), width=1.6, axis="v", type="bodentief"),
    dict(floor="EG", at=(44, 28.4), width=1.4, axis="v", type="bodentief"),
    # OG
    dict(floor="OG", at=(24.5, 16), width=1.4, axis="h", type="fenster"),
    dict(floor="OG", at=(29.5, 16), width=1.4, axis="h", type="fenster"),
    dict(floor="OG", at=(33.6, 16), width=1.8, axis="h", type="fenster"),
    dict(floor="OG", at=(37.75, 16), width=1.0, axis="h", type="fenster"),
    dict(floor="OG", at=(41.75, 16), width=1.0, axis="h", type="milch"),
    dict(floor="OG", at=(22, 19.0), width=1.2, axis="v", type="fenster"),
    dict(floor="OG", at=(22, 23.0), width=0.8, axis="v", type="fenster"),
    dict(floor="OG", at=(22, 27.0), width=1.4, axis="v", type="fenster"),
    dict(floor="OG", at=(25.0, 30), width=1.4, axis="h", type="fenster"),
    dict(floor="OG", at=(30.0, 30), width=1.0, axis="h", type="milch"),
    dict(floor="OG", at=(34.0, 30), width=1.4, axis="h", type="kipp"),
    dict(floor="OG", at=(37.9, 30), width=1.2, axis="h", type="bodentief"),
    dict(floor="OG", at=(42.1, 30), width=1.2, axis="h", type="bodentief"),
    dict(floor="OG", at=(44, 18.5), width=1.0, axis="v", type="milch"),
    dict(floor="OG", at=(44, 24.0), width=1.4, axis="v", type="fenster"),
    dict(floor="OG", at=(44, 27.6), width=1.4, axis="v", type="fenster"),
    # KG (light wells)
    dict(floor="KG", at=(22, 19.0), width=1.0, axis="v", type="keller"),
    dict(floor="KG", at=(22, 27.0), width=1.0, axis="v", type="keller"),
    dict(floor="KG", at=(27.5, 16), width=0.8, axis="h", type="keller"),
    dict(floor="KG", at=(31.5, 30), width=1.0, axis="h", type="keller"),
    dict(floor="KG", at=(35.0, 30), width=1.0, axis="h", type="keller"),
    # Remise
    dict(floor="EG", at=(3, 20.5), width=1.2, axis="v", type="fenster"),
    dict(floor="EG", at=(3, 23.2), width=1.2, axis="v", type="fenster"),
    dict(floor="EG", at=(12.5, 18), width=1.6, axis="h", type="fenster"),
    dict(floor="EG", at=(12.5, 25), width=1.2, axis="h", type="kipp"),
]

# Stairs as they appear on each floor. enter = side where you step on (on this floor); label = walking line text.
STAIRS = [
    dict(floor="EG", rect=(34.2, 16.6, 35.8, 21.6), enter="S", style="stair", label="hoch · 1. Stock"),
    dict(floor="OG", rect=(34.2, 16.6, 35.8, 21.6), enter="S", style="hole", label="von der Halle"),
    dict(floor="EG", rect=(29.15, 17.4, 30.65, 21.85), enter="S", style="stair", label="Keller", label_side="S"),
    dict(floor="KG", rect=(29.15, 17.4, 30.65, 21.85), enter="N", style="stair", label="hoch", label_side="S"),
    # Kelleraußentreppe (outside, east of the house)
    dict(floor="Aussen", rect=(44.2, 24.4, 45.6, 28.0), enter="S", style="stair", label="runter", label_side="R"),
    dict(floor="KG", rect=(44.2, 24.4, 45.6, 28.0), enter="N", style="stair", label="hoch (außen)", label_side="R"),
]

# Simple furniture / fixtures for orientation: floor, rect, label.
FIXTURES = [
    # EG
    dict(floor="EG", rect=(25.0, 26.4, 27.4, 27.4), label="Insel"),
    dict(floor="EG", rect=(30.9, 25.5, 34.5, 26.9), label="Tafel"),
    dict(floor="EG", rect=(29.2, 24.9, 29.7, 26.6), label="Anrichte"),
    dict(floor="EG", rect=(36.2, 16.3, 36.7, 20.8), label="Regal"),
    dict(floor="EG", rect=(36.8, 20.3, 38.7, 20.8), label=""),
    dict(floor="EG", rect=(41.45, 16.3, 41.9, 18.4), label=""),
    dict(floor="EG", rect=(43.3, 18.8, 43.8, 20.8), label=""),
    dict(floor="EG", rect=(38.2, 16.8, 40.4, 17.5), label="Lesetisch"),
    dict(floor="EG", rect=(43.4, 24.6, 43.85, 26.4), label="Kamin"),
    dict(floor="EG", rect=(37.2, 26.0, 39.2, 28.6), label="Flügel"),
    dict(floor="EG", rect=(40.3, 23.5, 42.6, 24.4), label="Sofa"),
    # OG
    dict(floor="OG", rect=(23.0, 17.0, 24.8, 19.0), label="Bett"),
    dict(floor="OG", rect=(29.0, 16.6, 30.0, 18.6), label="Bett"),
    dict(floor="OG", rect=(23.0, 28.6, 25.4, 29.4), label="Schreibtisch"),
    dict(floor="OG", rect=(27.4, 25.4, 27.85, 26.6), label="Tresor"),
    dict(floor="OG", rect=(28.3, 28.3, 30.0, 29.8), label="Wanne"),
    dict(floor="OG", rect=(34.85, 24.3, 35.85, 26.3), label="Bett"),
    dict(floor="OG", rect=(41.2, 25.0, 43.8, 27.2), label="Bett"),
    dict(floor="OG", rect=(36.2, 18.2, 36.8, 20.8), label=""),
    dict(floor="OG", rect=(38.6, 16.2, 39.3, 17.3), label="Tresor"),
    dict(floor="OG", rect=(42.4, 16.25, 43.8, 17.6), label="Wanne"),
    # KG
    dict(floor="KG", rect=(22.3, 16.3, 23.6, 17.6), label="Heizung"),
    dict(floor="KG", rect=(24.6, 20.9, 25.8, 21.75), label="Alarm"),
    dict(floor="KG", rect=(31.1, 16.3, 36.7, 16.8), label="Weinregale"),
    dict(floor="KG", rect=(31.1, 18.3, 36.7, 18.8), label=""),
    dict(floor="KG", rect=(42.0, 16.3, 43.7, 18.0), label="Tresor"),
    dict(floor="KG", rect=(29.7, 27.2, 32.5, 28.8), label="Billard"),
    dict(floor="KG", rect=(35.4, 25.0, 36.7, 29.0), label="Bar"),
    dict(floor="KG", rect=(22.6, 24.6, 24.4, 25.4), label=""),
    # Remise
    dict(floor="EG", rect=(3.4, 18.5, 5.3, 23.0), label="Auto"),
    dict(floor="EG", rect=(7.6, 18.5, 9.6, 23.0), label="Oldtimer"),
    dict(floor="EG", rect=(10.3, 21.6, 13.4, 22.2), label="Werkbank"),
]

# ---- Außenanlagen ----

# Surfaces: kind (kies, pflaster, stein, holz, wasser, beet, strasse, gehweg, feldweg, nachbar),
# shape ("rect", (x0, y0, x1, y1)) | ("ring", (cx, cy, r_out, r_in)) | ("path", [(x, y), ...], width)
#      | ("circle", (cx, cy, r))
SURFACES = [
    dict(kind="strasse", shape=("rect", (-6, -9, 80, -2.2)), label="Lindenallee"),
    dict(kind="gehweg", shape=("rect", (-6, -2.2, 80, 0)), label=""),
    dict(kind="feldweg", shape=("rect", (-6, 48.4, 80, 51.6)), label="Feldweg"),
    dict(kind="nachbar", shape=("rect", (60.6, -0.4, 80, 48.4)), label=""),
    dict(kind="kies", shape=("rect", (6, 0, 10, 12)), label="Einfahrt"),
    dict(kind="kies", shape=("rect", (3, 12, 15, 18)), label="Vorplatz"),
    dict(kind="kies", shape=("rect", (10, 6.9, 27.6, 10.1)), label=""),
    dict(kind="kies", shape=("ring", (32.5, 8.5, 5.5, 2.4)), label=""),
    dict(kind="pflaster", shape=("rect", (31.0, 14.0, 34.0, 16.0)), label=""),
    dict(kind="pflaster", shape=("path", [(32.5, 0), (32.5, 3.2)], 1.6), label=""),
    dict(kind="pflaster", shape=("path", [(37.9, 8.5), (48, 8.5), (48, 32), (46, 32)], 1.4), label=""),
    dict(kind="pflaster", shape=("path", [(48, 32), (52.25, 32), (52.25, 38.2)], 1.2), label=""),
    dict(kind="pflaster", shape=("path", [(48, 23.9), (45.6, 23.9)], 1.2), label=""),
    dict(kind="pflaster", shape=("path", [(15, 15), (19.5, 15), (19.5, 32), (22, 32)], 1.4), label=""),
    dict(kind="pflaster", shape=("path", [(15, 19.6), (19.5, 19.6)], 1.2), label=""),
    dict(kind="pflaster", shape=("path", [(15, 23.75), (19.5, 23.75)], 1.2), label=""),
    dict(kind="pflaster", shape=("path", [(19.5, 23), (22, 23)], 1.2), label=""),
    dict(kind="pflaster", shape=("path", [(4.5, 25), (4.5, 27.5), (19.5, 27.5)], 1.0), label=""),
    dict(kind="trittstein", shape=("path", [(42, 42.5), (52.6, 44.6), (52.6, 47.8)], 0.9), label=""),
    dict(kind="stein", shape=("rect", (22, 30, 46, 34)), label="Terrasse"),
    dict(kind="holz", shape=("rect", (26, 34, 42, 43)), label=""),
    dict(kind="wasser", shape=("rect", (28, 36, 40, 41)), label="Pool"),
    dict(kind="holz", shape=("rect", (50, 38, 54.5, 42.5)), label="Pavillon"),
    dict(kind="beet", shape=("rect", (15.5, 30.5, 16.7, 34.5)), label=""),
    dict(kind="beet", shape=("rect", (17.2, 30.5, 18.4, 34.5)), label=""),
    dict(kind="beet", shape=("rect", (22.0, 14.6, 30.6, 15.6)), label=""),
    dict(kind="beet", shape=("rect", (34.4, 14.6, 44.0, 15.6)), label=""),
    dict(kind="beet", shape=("rect", (44.6, 16.0, 45.6, 23.6)), label=""),
    dict(kind="beet", shape=("circle", (32.5, 8.5, 2.4)), label=""),
    dict(kind="wasser", shape=("circle", (32.5, 8.5, 1.3)), label=""),
]

# Hedges: rect strips. height in metres (low hedges can be seen over).
HEDGES = [
    dict(rect=(0.3, 1.3, 1.3, 47.7), height=2.2, name="Thujahecke West"),
    dict(rect=(1.3, 46.7, 51.5, 47.7), height=2.2, name="Thujahecke Nord"),
    dict(rect=(53.7, 46.7, 59.7, 47.7), height=2.2, name="Thujahecke Nord"),
    dict(rect=(58.7, 1.3, 59.7, 46.7), height=2.2, name="Thujahecke Ost (Nachbar)"),
    dict(rect=(0.3, 0.3, 5.4, 1.1), height=1.2, name="Ligusterhecke Straße"),
    dict(rect=(10.6, 0.3, 31.4, 1.1), height=1.2, name="Ligusterhecke Straße"),
    dict(rect=(33.6, 0.3, 59.7, 1.1), height=1.2, name="Ligusterhecke Straße"),
    dict(rect=(42.6, 34.2, 43.4, 43.0), height=1.6, name="Sichtschutz Pool"),
]

# Fence along the plot boundary (1.8 m metal fence); gates are in DOORS with floor "Aussen".
FENCE = [(0, 0), (60, 0), (60, 48), (0, 48), (0, 0)]

# Trees: x, y, canopy radius, kind (laub, obst, nadel)
TREES = [
    (3.2, 4.2, 2.8, "laub"), (15.5, 4.0, 3.2, "laub"), (23.5, 4.2, 3.0, "laub"), (42.5, 4.0, 3.0, "laub"),
    (50.5, 4.2, 3.2, "laub"), (56.2, 7.0, 2.6, "laub"),
    (3.6, 30.5, 3.0, "laub"), (4.2, 41.5, 3.6, "laub"), (12.0, 43.6, 2.8, "nadel"), (19.6, 43.2, 2.4, "laub"),
    (51.5, 13.5, 2.0, "obst"), (55.2, 17.5, 2.0, "obst"), (51.5, 21.5, 2.0, "obst"), (55.2, 25.5, 2.0, "obst"),
    (51.8, 29.8, 2.2, "obst"), (56.2, 43.6, 3.2, "laub"), (46.2, 41.2, 1.8, "nadel"), (11.5, 36.0, 2.6, "laub"),
]

# Outdoor lights: x, y, kind, radius (light pool in metres), note.
# kinds: laterne (post lantern), poller (bollard), wand (wall light), bm (spot with motion sensor, off until
# triggered), pool (underwater), boden (ground spot at a tree)
LIGHTS = [
    (5.4, 0.7, "laterne", 5.0, "Torlaterne"), (10.6, 0.7, "laterne", 5.0, "Torlaterne"),
    (31.4, -0.45, "laterne", 4.0, "Pfortenlaterne"),
    (10.6, 3.5, "poller", 2.5, ""), (10.6, 7.5, "poller", 2.5, ""), (5.6, 11.0, "poller", 2.5, ""),
    (27.6, 11.0, "poller", 2.5, ""), (37.4, 11.0, "poller", 2.5, ""), (32.5, 2.6, "poller", 2.5, ""),
    (30.85, 15.55, "wand", 4.0, "Haustür"), (34.15, 15.55, "wand", 4.0, "Haustür"),
    (21.5, 24.0, "bm", 8.0, "Seiteneingang"), (3.6, 17.5, "bm", 9.0, "Garage"), (9.4, 17.5, "bm", 9.0, "Garage"),
    (15.45, 20.6, "wand", 3.5, "Werkstatt"), (28.35, 30.45, "wand", 3.5, "Küchentür"),
    (36.4, 30.45, "wand", 4.0, "Terrasse"), (44.45, 30.45, "wand", 4.0, "Terrasse"),
    (46.25, 24.9, "bm", 7.0, "Kelleraußentreppe"),
    (30.5, 36.3, "pool", 3.0, ""), (34.0, 36.3, "pool", 3.0, ""), (37.5, 36.3, "pool", 3.0, ""),
    (48.0, 15.0, "poller", 2.5, ""), (48.0, 22.0, "poller", 2.5, ""), (48.0, 29.0, "poller", 2.5, ""),
    (52.25, 41.7, "wand", 3.5, "Pavillon"), (55.2, 41.0, "boden", 3.0, "Eiche"), (24.0, 6.0, "boden", 3.0, "Linde"),
    (19.5, 18.0, "poller", 2.5, ""), (19.5, 28.0, "poller", 2.5, ""),
]

# Game markers: kind einstieg / ziel / sicherheit. floor "Aussen" = shown on the site plan.
MARKERS = [
    dict(code="E1", kind="einstieg", floor="EG", at=(33.4, 14.65), text="Haustür – Schloss 6, Alarm-Kontakt, von der Straße einsehbar"),
    dict(code="E2", kind="einstieg", floor="EG", at=(20.9, 21.5), text="Seiteneingang – Schloss 4, Bewegungsmelder"),
    dict(code="E3", kind="einstieg", floor="EG", at=(26.4, 31.0), text="Küchentür – Glas, Schloss 3, im Dunkeln"),
    dict(code="E4", kind="einstieg", floor="EG", at=(38.6, 31.0), text="Hebeschiebetür – aufhebeln oder Glas (sehr laut)"),
    dict(code="E5", kind="einstieg", floor="KG", at=(45.0, 29.3), text="Kelleraußentür – Stahl, Schloss 5, Bewegungsmelder"),
    dict(code="E6", kind="einstieg", floor="EG", at=(27.25, 15.1), text="Gäste-WC-Kippfenster – durchgreifen (Schlösser 3), nur ohne Rucksack"),
    dict(code="E7", kind="einstieg", floor="OG", at=(41.8, 31.2), text="Balkon – Leiter aus dem Geräteraum, Balkontür aufhebeln"),
    dict(code="E8", kind="einstieg", floor="Aussen", at=(54.3, 49.6), text="Hintertor – Kette knacken, Weg im Dunkeln"),
    dict(code="Z1", kind="ziel", floor="KG", at=(43.0, 18.75), text="Tresorraum – großer Tresor, Bargeld, Gold (Hauptziel)"),
    dict(code="Z2", kind="ziel", floor="OG", at=(26.4, 26.0), text="Arbeitszimmer – Wandtresor hinter dem Gemälde, Laptop"),
    dict(code="Z3", kind="ziel", floor="OG", at=(37.8, 18.0), text="Ankleide – Schmuck, Uhren, kleiner Möbeltresor"),
    dict(code="Z4", kind="ziel", floor="EG", at=(40.95, 18.9), text="Geheimkammer – Gemäldesammlung (optional)"),
    dict(code="Z5", kind="ziel", floor="KG", at=(33.9, 20.3), text="Weinkeller – Raritäten, schwer und zerbrechlich"),
    dict(code="Z6", kind="ziel", floor="EG", at=(30.25, 25.05), text="Esszimmer – Silberbesteck in der Anrichte"),
    dict(code="Z7", kind="ziel", floor="EG", at=(9.3, 23.7), text="Garage – Oldtimer (später: Fluchtwagen?)"),
    dict(code="A1", kind="sicherheit", floor="EG", at=(31.45, 18.25), text="Alarm-Bedienteil neben der Haustür"),
    dict(code="A2", kind="sicherheit", floor="KG", at=(23.6, 20.4), text="Alarmzentrale + Hauptsicherungen (Strom aus = Licht aus)"),
    dict(code="A3", kind="sicherheit", floor="Aussen", at=(11.6, -1.0), text="Kamera am Einfahrtstor"),
    dict(code="A4", kind="sicherheit", floor="Aussen", at=(63.5, 21.0), text="Nachbar – hört Lärm durch die Hecke"),
]

# Free labels on the site plan: x, y, text, size (relative), style
SITE_LABELS = [
    (8.0, 5.5, "Einfahrt", 0.9, "path"), (9.0, 14.9, "Vorplatz", 0.9, "path"), (32.5, 5.3, "Vorfahrt", 0.8, "path"),
    (34.0, 32.0, "Terrasse", 1.0, "area"), (34.0, 38.6, "Pool 12 × 5 m", 1.0, "water"),
    (52.25, 40.25, "Pavillon", 0.8, "area"), (17.0, 35.6, "Kräuter", 0.8, "area"),
    (55.3, 35.0, "Obstwiese", 1.0, "lawn"), (11.0, 40.0, "Garten", 1.2, "lawn"), (30.0, 45.0, "Liegewiese", 1.0, "lawn"),
    (34.0, -5.6, "Lindenallee", 1.3, "street"), (34.0, 50.0, "Feldweg", 1.0, "street"),
    (70.0, 34.0, "Nachbar­grundstück", 1.0, "street"), (9.0, 26.3, "Remise", 1.1, "building"),
    (41.0, 13.3, "Haupthaus", 1.1, "building")
]

NEIGHBOUR_HOUSE = (64.0, 14.0, 76.0, 27.0)
BALCONY = dict(floor="OG", rect=(37.0, 30.0, 43.0, 31.8), name="Balkon")
CANOPY = dict(rect=(30.6, 14.0, 34.4, 16.0), name="Vordach")
