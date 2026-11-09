"""Rosa: everything build_character.py needs to know about her (reference files, measurements in the pictures,
heights of her head, hair and clothes). Pixel values are measured in the reference images in ref/."""

NAME = "Rosa"
HEIGHT = 1.70          # hair top to sole, metres

# Full-body views (ref/): front, back, her left side (face to the image left), her right side (face to the right).
VIEW_FILES = {"front": "rosa_front.webp", "back": "rosa_back.webp",
              "left": "rosa_side_left.webp", "right": "rosa_side_right.webp"}

# Silhouettes above HEAD_SMOOTH_FROM are opened by HEAD_OPEN_PX (loose curls), between there and JAW_SMOOTH_TO
# (neck, jaw) by JAW_OPEN_PX.
HEAD_SMOOTH_FROM = 1.40
HEAD_OPEN_PX = 10
JAW_SMOOTH_TO = 1.555
JAW_OPEN_PX = 22

# Head close-up: front view left of HEAD_SPLIT_COL, profile (face to the image right) right of it. Replaces the
# full-body photos from Z_HEAD up (turtleneck). Placed by eye and chin height in the full-body front photo.
HEAD_FILE = "rosa_head.png"
HEAD_SPLIT_COL = 800
Z_HEAD = 1.45
BODY_EYE_ROW, BODY_EYE_COLS, BODY_CHIN_ROW = 93.6, (750.0, 786.0), 153.0
HEAD_FRONT = dict(eye_row=389.0, eye_cols=(387.5, 544.0), chin_row=656.0, hair_row=210.0, mouth_row=561.0,
                  mouth_half=65.0, alae_half=42.5)
HEAD_SIDE = dict(eye_row=380.0, chin_row=658.0, sellion_row=387.5, subnasale_row=500.0, crease_row=600.0)
# Half width and depth of the face (front edge to the side plane in front of the ears) at chin-0.4 cm, chin,
# chin+1 cm, lip crease, mouth, nose base, eyes, eyes+2 cm, hairline+0.4 cm.
FACE_W = (0.026, 0.030, 0.045, 0.055, 0.061, 0.067, 0.069, 0.068, 0.065)
FACE_D = (0.060, 0.065, 0.075, 0.085, 0.090, 0.098, 0.100, 0.100, 0.095)

# Right ear traced in the close-up profile (full-image pixels, clockwise from the top of the helix), the bowl
# (concha) centre, how far the ear sits from the face centre where it attaches and how much it tilts outwards.
EAR_OUTLINE = [(1265, 371.7), (1250, 378), (1235, 390), (1224, 410), (1221.7, 425), (1223, 440), (1227, 455),
               (1233, 467), (1241, 481), (1251.7, 493), (1268, 507), (1290, 517), (1303, 515), (1311.7, 510),
               (1314, 495), (1313, 480), (1310, 465), (1306, 450), (1300, 432), (1293, 413), (1288, 395),
               (1286.7, 380), (1278, 372)]
EAR_CONCHA = (1296, 447)
EAR_ATTACH_U = 0.064
EAR_TILT_DEG = 30.0

# Body shape: cross sections between these heights (the jacket) are a little boxier.
JACKET_Z = (0.80, 1.42)

# Hair. Vertices above HAIR_FROM that are not skin get strands in relief; the scalp (texture work) starts at
# SCALP_FROM, head islands get more texture space above HEAD_UV_FROM.
HAIR_FROM = 1.475
SCALP_FROM = 1.44
HEAD_UV_FROM = 1.40
# Outline of the back of the head smoothed over the angle around this centre (y, z), None = keep the silhouette.
BACK_OUTLINE_CENTRE = (0.02, 1.58)
# Hair-only patch in the profile half of the close-up (rows, cols) for the strands on the back and top.
HAIR_SRC = (75, 300, 290, 640)
# Bun (back photo + profile): centre (u from the face centre, z) on the back of the head, radius, spiral coils
# (radial pitch per turn, number of coils, relief height). The knot itself is an ellipsoid (half widths rx, ry, rz)
# whose back reaches the profile; beside it the back of the head is taken in by up to `depth` from SKULL_FROM up.
BUN = dict(u=0.0, z=1.57, radius=0.05, pitch=0.036, arms=2, ridge=0.0065,
           rx=0.055, ry=0.060, rz=0.065, depth=0.035)
SKULL_FROM = 1.47

# Loose curls: (u from the face centre, y, z) near the scalp, direction, length, tube radius, helix radius, turns,
# droop. Framing the face at the temples, two wisps on the forehead, behind the ears and at the nape, loops on the bun.
CURLS = (
    (-0.069, -0.022, 1.600, (-0.15, -0.10, -1.0), 0.100, 0.0042, 0.0085, 2.2, 0.1),
    (-0.073, -0.004, 1.578, (-0.25, 0.05, -1.0), 0.112, 0.0038, 0.0090, 2.5, 0.1),
    (0.069, -0.024, 1.602, (0.15, -0.10, -1.0), 0.092, 0.0042, 0.0085, 2.1, 0.1),
    (0.074, -0.006, 1.580, (0.25, 0.05, -1.0), 0.105, 0.0038, 0.0090, 2.4, 0.1),
    (-0.030, -0.080, 1.655, (-0.6, -0.3, -0.5), 0.042, 0.0030, 0.0060, 1.1, 0.3),
    (0.028, -0.082, 1.660, (0.55, -0.35, -0.45), 0.038, 0.0030, 0.0060, 1.0, 0.3),
    (-0.060, 0.065, 1.530, (-0.15, 0.2, -1.0), 0.080, 0.0040, 0.0080, 1.9, 0.0),
    (0.060, 0.065, 1.530, (0.15, 0.2, -1.0), 0.076, 0.0040, 0.0080, 1.8, 0.0),
    (-0.025, 0.095, 1.515, (-0.05, 0.25, -1.0), 0.062, 0.0038, 0.0075, 1.5, 0.0),
    (0.030, 0.095, 1.512, (0.05, 0.25, -1.0), 0.066, 0.0038, 0.0075, 1.6, 0.0),
    (-0.055, 0.145, 1.625, (-0.8, 0.3, -0.5), 0.040, 0.0048, 0.0095, 0.9, 0.5),
    (0.050, 0.150, 1.615, (0.8, 0.4, -0.5), 0.040, 0.0048, 0.0095, 0.9, 0.5),
    (-0.020, 0.170, 1.590, (-0.3, 0.7, -0.7), 0.045, 0.0045, 0.0090, 1.1, 0.4),
    (0.025, 0.160, 1.570, (0.4, 0.5, -0.8), 0.050, 0.0042, 0.0085, 1.3, 0.3),
)
