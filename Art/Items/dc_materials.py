"""Shared material table for the item props (Blender previews and the Unreal import read it).

Each mesh material slot is named after one entry here. In Unreal every entry becomes a material
instance MI_Item_<Name> of the master material M_ItemSurface (or M_ItemGlass).

tex       texture set from dc_textures (T_<tex>_BC/_N/_ORM)
tint      sRGB colour multiplied onto the base colour
tiling    texture repeats per metre (UVs are 1 unit = 1 m, except 'fit' parts = 0..1)
rough     multiplier on the roughness map
metal     metallic (multiplied with the ORM blue channel)
"""

MATERIALS = {
    # Wood
    "Wood_Walnut": dict(tex="Wood_Walnut", tint=(1.0, 1.0, 1.0), tiling=0.8, rough=1.0, metal=0.0),
    "Wood_Oak": dict(tex="Wood_Oak", tint=(1.0, 1.0, 1.0), tiling=0.8, rough=1.0, metal=0.0),
    "Wood_Mahogany": dict(tex="Wood_Mahogany", tint=(1.0, 1.0, 1.0), tiling=0.8, rough=0.9, metal=0.0),
    # Textiles
    "Fabric_Red": dict(tex="Fabric", tint=(0.62, 0.17, 0.14), tiling=12.0, rough=1.0, metal=0.0),
    "Fabric_Blue": dict(tex="Fabric", tint=(0.27, 0.36, 0.52), tiling=12.0, rough=1.0, metal=0.0),
    "Fabric_Navy": dict(tex="Fabric", tint=(0.18, 0.22, 0.34), tiling=14.0, rough=1.0, metal=0.0),
    "Fabric_Olive": dict(tex="Fabric", tint=(0.42, 0.44, 0.28), tiling=16.0, rough=0.95, metal=0.0),
    "Fabric_Black": dict(tex="Fabric", tint=(0.17, 0.17, 0.18), tiling=10.0, rough=1.0, metal=0.0),
    "Wool_Brown": dict(tex="Fabric", tint=(0.46, 0.34, 0.25), tiling=8.0, rough=1.0, metal=0.0),
    "Velvet_Red": dict(tex="Fabric", tint=(0.48, 0.06, 0.10), tiling=24.0, rough=1.0, metal=0.0),
    "Felt_Red": dict(tex="Fabric", tint=(0.55, 0.09, 0.08), tiling=30.0, rough=1.0, metal=0.0),
    # Leather
    "Leather_Brown": dict(tex="Leather", tint=(0.50, 0.30, 0.17), tiling=6.0, rough=1.0, metal=0.0),
    "Leather_Black": dict(tex="Leather", tint=(0.16, 0.15, 0.15), tiling=6.0, rough=0.9, metal=0.0),
    "Leather_Green": dict(tex="Leather", tint=(0.20, 0.30, 0.22), tiling=5.0, rough=0.9, metal=0.0),
    # Metals
    "Steel": dict(tex="BrushedMetal", tint=(0.86, 0.86, 0.87), tiling=3.0, rough=1.0, metal=1.0),
    "Aluminium": dict(tex="BrushedMetal", tint=(0.78, 0.79, 0.81), tiling=8.0, rough=1.1, metal=1.0),
    "Chrome": dict(tex="Fine", tint=(0.96, 0.96, 0.97), tiling=4.0, rough=0.4, metal=1.0),
    "Brass": dict(tex="Fine", tint=(0.93, 0.74, 0.40), tiling=4.0, rough=1.0, metal=1.0),
    "Gold": dict(tex="Fine", tint=(1.00, 0.79, 0.36), tiling=10.0, rough=0.6, metal=1.0),
    "Silver": dict(tex="Fine", tint=(0.97, 0.97, 0.98), tiling=10.0, rough=0.6, metal=1.0),
    "Gilt": dict(tex="Fine", tint=(0.86, 0.66, 0.32), tiling=6.0, rough=1.5, metal=1.0),
    "Iron_Dark": dict(tex="Fine", tint=(0.28, 0.28, 0.29), tiling=4.0, rough=1.8, metal=1.0),
    "Bronze": dict(tex="Bronze", tint=(1.0, 1.0, 1.0), tiling=3.0, rough=1.0, metal=1.0),
    "Paint_Red": dict(tex="Paint", tint=(0.72, 0.09, 0.07), tiling=4.0, rough=1.0, metal=1.0),
    "Paint_Safe": dict(tex="Paint", tint=(0.20, 0.27, 0.24), tiling=2.0, rough=1.0, metal=1.0),
    # Lacquer, plastics
    "Lacquer_Black": dict(tex="Fine", tint=(0.035, 0.035, 0.04), tiling=2.0, rough=0.5, metal=0.0),
    "Ivory": dict(tex="Fine", tint=(0.93, 0.90, 0.82), tiling=6.0, rough=1.3, metal=0.0),
    "Plastic_Black": dict(tex="Fine", tint=(0.05, 0.05, 0.055), tiling=6.0, rough=2.5, metal=0.0),
    "Rubber_Black": dict(tex="Fine", tint=(0.04, 0.04, 0.04), tiling=6.0, rough=4.0, metal=0.0),
    "Screen_Black": dict(tex="Fine", tint=(0.02, 0.02, 0.025), tiling=4.0, rough=1.0, metal=0.0),
    # Stone, ceramics
    "Marble": dict(tex="Marble", tint=(1.0, 1.0, 1.0), tiling=2.0, rough=1.0, metal=0.0),
    "Porcelain": dict(tex="Fine", tint=(0.95, 0.95, 0.93), tiling=4.0, rough=0.6, metal=0.0),
    "Porcelain_Blue": dict(tex="Fine", tint=(0.13, 0.24, 0.58), tiling=4.0, rough=0.6, metal=0.0),
    # Paper goods
    "Cardboard": dict(tex="Cardboard", tint=(1.0, 1.0, 1.0), tiling=2.0, rough=1.0, metal=0.0),
    "Paper": dict(tex="Paper", tint=(1.0, 1.0, 1.0), tiling=4.0, rough=1.0, metal=0.0),
    "Binder_Blue": dict(tex="Paper", tint=(0.22, 0.36, 0.62), tiling=4.0, rough=0.9, metal=0.0),
    "Banknote": dict(tex="Banknote", tint=(1.0, 1.0, 1.0), tiling=1.0, rough=1.0, metal=0.0),
    "Canvas": dict(tex="Painting", tint=(1.0, 1.0, 1.0), tiling=1.0, rough=1.0, metal=0.0),
    # Room and decoration (room surfaces use M_RoomSurface: world-aligned, tiling per metre)
    "Floor_Planks": dict(tex="FloorPlanks", tint=(1.0, 1.0, 1.0), tiling=0.5, rough=1.4, metal=0.0, room=True),
    "Wallpaper": dict(tex="Wallpaper", tint=(1.0, 1.0, 1.0), tiling=1.0, rough=1.0, metal=0.0, room=True),
    "Ground_Concrete": dict(tex="Concrete", tint=(1.0, 1.0, 1.0), tiling=0.33, rough=1.0, metal=0.0, room=True),
    "Paint_White": dict(tex="Paint", tint=(0.88, 0.87, 0.83), tiling=2.0, rough=1.5, metal=0.0),
    "Lampshade_Cream": dict(tex="Fabric", tint=(0.95, 0.86, 0.68), tiling=10.0, rough=1.0, metal=0.0, emissive=1.6),
    "Glass_Green": dict(tex="Fine", tint=(0.08, 0.42, 0.22), tiling=4.0, rough=0.35, metal=0.0, emissive=0.35),
    "Bulb": dict(tex="Fine", tint=(1.0, 0.85, 0.6), tiling=1.0, rough=0.3, metal=0.0, emissive=25.0),
    # Transparent / gems
    "Glass": dict(tex=None, tint=(0.85, 0.92, 0.95), tiling=1.0, rough=0.05, metal=0.0, glass=True),
    "Gem": dict(tex="Fine", tint=(0.92, 0.95, 1.0), tiling=10.0, rough=0.15, metal=0.7),
}


def srgb_to_linear(c):
    return tuple((x / 12.92) if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)
