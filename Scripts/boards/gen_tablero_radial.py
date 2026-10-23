"""
Generador de un tablero radial con las reglas del 21/10/2026 (GDD 01, decisiones del autor): seis cuñas alrededor
del Altar, cada una con una columna recta desde el Altar, un anillo, el Nicho en un ramal corto por la parte central,
la Fuente en el anillo, un Nodo en la fila exterior, un acceso al Altar y dos conexiones con cada vecino (una interior
y otra exterior). Entre 13 y 14 casillas por Dominio, con variaciones por Dominio para que no sean seis copias.

Es un punto de partida para el editor web (Scripts/board-editor): se carga, se retoca y se descarga. No sustituye al
criterio del autor ni al informe de simulación.

Dos variantes: --variante 1 (seis cuñas con la misma plantilla y pequeñas variaciones; es tablero_82.json) y
--variante 2 (seis cuñas de forma distinta, anillo, bucle con triángulo, escalera, espiral, estrella y cruz, con más
conexiones; es tablero_86.json). La variante 2 la pidió el autor el 21/10/2026 al ver que la 1 era seis copias.

Uso (desde la raíz del proyecto):
    python -I Scripts/boards/gen_tablero_radial.py --variante 2 --out Content/Data/Boards/tablero_86.json [--preview salida.png]

Escribe el JSON en el formato de Content/Data/Boards (parameters, domains, tiles, edges, editor.positions) y comprueba
en local las reglas que también comprueban el editor y el cargador (R-TAB-1 a 5, Nicho con una conexión, Fuentes por
Dominio, Nodos por efecto) y las de diseño del 21/10 (especiales con una o dos conexiones, distancia mínima, accesos).
"""
import argparse
import json
import math
import os
import sys

CX, CY = 500, 500               # el Altar, en coordenadas del editor (y hacia abajo)
DOMAIN_COUNT = 6
DOMAIN_NAMES = ["El Banquete", "El Fiordo Encadenado", "El Río de la Noche", "El Descenso", "La Ruina Tormentosa",
                "El Bosque Negro"]
SECTOR_START_DEG = 270          # el Dominio 0 abajo, como en tablero_35; cada Dominio 60 grados más
R_OUTER = 480                   # radio de la fila exterior
# Regla 9 (un décimo del radio) sería 48; el kit pide más. A la escala de esta mesa (unos 0,106 cm por unidad) 74
# unidades son 7,8 cm entre centros: la losa de 6,3 cm y un hueco de 1,5. Con 13-14 losas por Dominio en una mesa de
# 148 cm no cabe un tramo de sendero (2,4 cm) entre todas las vecinas: en las más juntas las losas van casi pegadas,
# como en una rejilla, y el sendero de piedrecillas solo aparece en las conexiones largas. Para senderos en todas,
# 11-12 losas por Dominio o una mesa mayor.
MIN_DISTANCE = 74

# Efectos de los Nodos, uno por Dominio en la fila exterior: 2 Planares, 2 Duelo, 2 Oráculos (parámetro que el autor
# puede dejar en 1 Oráculo quitando un Nodo en el editor).
NODE_EFFECTS = {"planar": 2, "duel": 2, "oracle": 2}


def polar(domain, r, v):
    """Posición en el editor de un punto de la cuña: r radial desde el Altar (unidades del editor), v lateral
    (positivo hacia el Dominio siguiente)."""
    base = math.radians(SECTOR_START_DEG + 60 * domain)
    radius = math.hypot(r, v)
    angle = base + math.atan2(v, r)
    return [round(CX + radius * math.cos(angle)), round(CY + radius * math.sin(angle))]


def wedge(domain):
    """Casillas y conexiones de una cuña, con variaciones por Dominio. Devuelve (casillas, conexiones locales,
    nombres de los puntos de enlace con los vecinos)."""
    tiles = {}      # nombre -> (r, v, tipo)
    edges = []      # (nombre, nombre)

    def add(name, r, v, kind="normal"):
        tiles[name] = (r, v, kind)

    # Columna recta desde el Altar (regla 2): sin especiales.
    add("c1", 110, 0)
    add("c2", 185, 0)
    # Anillo (regla 3): c2 - L1 - L2 - B1 - M - B2 - R2 - R1 - c2.
    add("L1", 250, -75)
    add("R1", 250, 75)
    add("L2", 330, -95)
    add("R2", 330, 95)
    add("B1", 405, -80)
    add("B2", 405, 80)
    add("M", 400, 0)
    edges += [("c1", "c2"), ("c2", "L1"), ("L1", "L2"), ("L2", "B1"), ("B1", "M"), ("M", "B2"), ("B2", "R2"),
              ("R2", "R1"), ("R1", "c2")]
    # Nicho (regla 4): ramal corto por la parte central, anclado en un sitio distinto según el Dominio.
    anchor = ["M", "L2", "R2", "M", "R2", "L2"][domain]
    if anchor == "M":
        add("N", 315, 0, "niche")
    elif anchor == "L2":
        add("N", 300, -20, "niche")
    else:
        add("N", 300, 20, "niche")
    edges.append(("N", anchor))
    # Fuente (regla 5): en el anillo, en una casilla con solo dos conexiones (regla 8: L1, R1, B1 y B2 llevan además
    # el enlace con el vecino o con la fila exterior), nunca donde ancla el Nicho; varía por Dominio.
    source = ["L2", "R2", "L2", "R2", "M", "M"][domain]
    r, v, _ = tiles[source]
    tiles[source] = (r, v, "powerSource")
    # Fila exterior (regla 6): tres o cuatro casillas; el Nodo en la del medio, que solo toca a sus dos vecinas.
    # Los extremos de la fila van cerca del borde del sector, para que el enlace exterior con el vecino sea corto
    # (a escala de mesa, unos 10-13 cm en vez de 30) sin pisar la distancia mínima.
    wide = domain in (1, 3, 5)
    if wide:
        add("O1", R_OUTER, -215)
        add("O2", R_OUTER, -72, "powerNode")
        add("O3", R_OUTER, 72)
        add("O4", R_OUTER, 215)
        edges += [("O1", "O2"), ("O2", "O3"), ("O3", "O4"), ("B1", "O1"), ("B2", "O4")]
        outer_left, outer_right = "O1", "O4"
    else:
        add("O1", R_OUTER, -185)
        add("O2", R_OUTER, 0, "powerNode")
        add("O3", R_OUTER, 185)
        edges += [("O1", "O2"), ("O2", "O3"), ("B1", "O1"), ("B2", "O3")]
        outer_left, outer_right = "O1", "O3"
    return tiles, edges, {"inner_left": "L1", "inner_right": "R1", "outer_left": outer_left, "outer_right": outer_right}


def wedge_v2(domain, nichos="centro"):
    """Variante 2 (pedida por el autor el 21/10/2026 al ver que las seis cuñas de la variante 1 eran iguales): seis
    cuñas con formas distintas y más conexiones. Cada una respeta las mismas reglas: columna desde el Altar sin
    especiales, Nicho con una conexión, Fuente y Nodo con dos conexiones como mucho, un enlace interior y otro exterior
    con cada vecino, distancia mínima.
    nichos: "centro", el Nicho por la parte central de la cuña (a 3-7 pasos del Altar), o "lejos", colgando del extremo
    exterior (7-8 pasos). El 23/10/2026 el informe de simulación dio Ecos al 1 % con los Nichos cerca (objetivo del GDD 03,
    15-35 %) y la carrera cerrada a los 135 s de 300: con un dado, un Nicho a 3-6 pasos se alcanza en uno o dos turnos."""
    tiles, edges = {}, []

    def add(name, r, v, kind="normal"):
        tiles[name] = (r, v, kind)

    def link(*pairs):
        edges.extend(pairs)

    add("c1", 110, 0)
    if domain == 0:
        # A. Anillo con fila exterior: la cuña de la variante 1, con el Nicho colgando hacia dentro del fondo del anillo.
        add("c2", 185, 0)
        add("L1", 250, -75); add("R1", 250, 75); add("L2", 330, -95, "powerSource"); add("R2", 330, 95)
        n_pos, n_anchor = ((500, 270), "O3") if nichos == "lejos" else ((315, 0), "M")
        add("B1", 405, -80); add("B2", 405, 80); add("M", 400, 0); add("N", *n_pos, "niche")
        add("O1", 480, -185); add("O2", 480, 0, "powerNode"); add("O3", 480, 185)
        link(("c1", "c2"), ("c2", "L1"), ("L1", "L2"), ("L2", "B1"), ("B1", "M"), ("M", "B2"), ("B2", "R2"), ("R2", "R1"),
             ("R1", "c2"), ("N", n_anchor), ("O1", "O2"), ("O2", "O3"), ("B1", "O1"), ("B2", "O3"))
        links = ("L1", "R1", "O1", "O3")
    elif domain == 1:
        # B. Bucle grande con triángulo interior: el triángulo c2-IL-IM-IR junto al Altar, un bucle largo que pasa por
        # la fila exterior (con el Nodo dentro del bucle) y el Nicho en el hueco central, colgando del vértice IM.
        add("c2", 185, 0)
        add("IL", 260, -80); add("IM", 330, 0); add("IR", 260, 80)
        add("OL1", 340, -150, "powerSource"); add("OL2", 430, -130); add("OR1", 340, 150); add("OR2", 430, 130)
        n_pos, n_anchor = ((515, 275), "X4") if nichos == "lejos" else ((410, 0), "IM")
        add("N", *n_pos, "niche")
        add("X1", 495, -190); add("X2", 495, -65, "powerNode"); add("X3", 495, 65); add("X4", 495, 190)
        link(("c1", "c2"), ("c2", "IL"), ("c2", "IR"), ("IL", "IM"), ("IM", "IR"), ("IL", "OL1"), ("OL1", "OL2"), ("IR", "OR1"),
             ("OR1", "OR2"), ("OL2", "X1"), ("OR2", "X4"), ("X1", "X2"), ("X2", "X3"), ("X3", "X4"), (n_anchor, "N"), ("IM", "OR1"))
        links = ("IL", "IR", "X1", "X4")
    elif domain == 2:
        # C. Escalera: dos carriles con tres travesaños (por c2, por la Fuente M1 y por M2) más la fila exterior; sin
        # diagonales ni cubo, para que no se parezca a la estrella (corrección del autor del 21/10). El Nicho cuelga
        # de la columna, entre los dos carriles.
        add("c2", 190, 0)
        add("L1", 265, -80); add("L2", 345, -100); add("L3", 425, -110)
        add("R1", 265, 80); add("R2", 345, 100); add("R3", 425, 110)
        n_pos, n_anchor = ((515, 250), "X3") if nichos == "lejos" else ((268, 0), "c2")
        add("M1", 350, 0, "powerSource"); add("M2", 430, 0); add("N", *n_pos, "niche")
        add("X1", 495, -165); add("X2", 508, 0, "powerNode"); add("X3", 495, 165)
        link(("c1", "c2"), ("c2", "L1"), ("c2", "R1"), ("L1", "L2"), ("L2", "L3"), ("R1", "R2"), ("R2", "R3"), ("L2", "M1"),
             ("M1", "R2"), ("L3", "M2"), ("M2", "R3"), ("N", n_anchor), ("L3", "X1"), ("X1", "X2"), ("X2", "X3"), ("R3", "X3"))
        links = ("L1", "R1", "X1", "X3")
    elif domain == 3:
        # D. Espiral: el anillo gira y, desde el fondo, un brazo vuelve hacia dentro (B1-S1) hasta el Nicho, que queda
        # en el corazón de la cuña junto a la columna.
        add("c2", 185, 0)
        add("L1", 255, -85); add("L2", 340, -115); add("B1", 425, -25); add("B2", 425, 95)
        n_pos, n_anchor = ((515, 260), "X4") if nichos == "lejos" else ((265, 0), "S1")
        add("R2", 340, 115, "powerSource"); add("R1", 255, 85); add("S1", 345, 0); add("N", *n_pos, "niche")
        add("X1", 470, -190); add("X2", 500, -75, "powerNode"); add("X3", 500, 50); add("X4", 495, 175)
        link(("c1", "c2"), ("c2", "R1"), ("R1", "R2"), ("R2", "B2"), ("B2", "B1"), ("B1", "L2"), ("L2", "L1"), ("L1", "c2"),
             ("B1", "S1"), (n_anchor, "N"), ("S1", "L2"), ("L2", "X1"), ("X1", "X2"), ("X2", "X3"), ("X3", "X4"), ("B2", "X4"),
             ("B1", "X3"))
        links = ("L1", "R1", "X1", "X4")
    elif domain == 4:
        # E. Estrella: un cubo H con seis radios (dos de ellos al Nicho y al fondo) dentro de un anillo; la Fuente en un
        # apéndice F entre el anillo y la fila exterior.
        add("c2", 185, 0)
        add("A1", 265, -95); add("A2", 265, 95); add("L", 345, -125); add("R", 345, 125)
        n_pos, n_anchor = ((510, 270), "X3") if nichos == "lejos" else ((265, 0), "H")
        add("A3", 425, -120); add("A4", 420, 115); add("A5", 430, 0); add("H", 345, 0); add("N", *n_pos, "niche")
        add("F", 400, -200, "powerSource")
        add("X1", 490, -185); add("X2", 505, 0, "powerNode"); add("X3", 490, 185)
        link(("c1", "c2"), ("c2", "A1"), ("c2", "A2"), ("A1", "L"), ("L", "A3"), ("A3", "A5"), ("A5", "A4"), ("A4", "R"),
             ("R", "A2"), ("H", "A1"), ("H", "A2"), ("H", "L"), ("H", "R"), ("H", "A5"), (n_anchor, "N"), ("A3", "F"), ("F", "X1"),
             ("X1", "X2"), ("X2", "X3"), ("A4", "X3"))
        links = ("A1", "A2", "X1", "X3")
    else:
        # F. Cruz: la columna llega hasta el fondo y de ella salen dos bucles laterales; a media columna, el Nicho a un
        # lado y el Nodo al otro, como los brazos de una cruz.
        add("c2", 190, 0); add("c3", 275, 0); add("c4", 360, 0); add("c5", 445, 0)
        add("L1", 275, -105); add("L2", 360, -170); add("B1", 445, -125)
        add("R1", 275, 105); add("R2", 360, 170, "powerSource"); add("B2", 445, 125)
        n_pos, n_anchor = ((520, 125), "B2") if nichos == "lejos" else ((355, -85), "c4")
        add("N", *n_pos, "niche"); add("Q", 355, 85, "powerNode")
        add("X1", 495, -215); add("X3", 495, 215)
        link(("c1", "c2"), ("c2", "c3"), ("c3", "c4"), ("c4", "c5"), ("c3", "L1"), ("L1", "L2"), ("L2", "B1"), ("B1", "c5"),
             ("c3", "R1"), ("R1", "R2"), ("R2", "B2"), ("B2", "c5"), (n_anchor, "N"), ("c4", "Q"), ("Q", "R1"), ("B1", "X1"),
             ("B2", "X3"))
        links = ("L1", "R1", "X1", "X3")
    return tiles, edges, dict(zip(("inner_left", "inner_right", "outer_left", "outer_right"), links))


def build(variant=1, nichos="centro"):
    tiles = [{"id": 0, "type": "altar"}]
    positions = {"0": [CX, CY]}
    edges = []
    links = []
    ids = {}
    next_id = 1
    for d in range(DOMAIN_COUNT):
        local, local_edges, link = wedge(d) if variant == 1 else wedge_v2(d, nichos)
        for name, (r, v, kind) in local.items():
            ids[(d, name)] = next_id
            tile = {"id": next_id, "type": kind, "domain": d}
            tiles.append(tile)
            positions[str(next_id)] = polar(d, r, v)
            next_id += 1
        for a, b in local_edges:
            edges.append([ids[(d, a)], ids[(d, b)]])
        edges.append([0, ids[(d, "c1")]])  # regla 7: un acceso al Altar
        links.append(link)
    # Regla 7: dos conexiones con cada vecino, una interior y otra exterior.
    for d in range(DOMAIN_COUNT):
        e = (d + 1) % DOMAIN_COUNT
        edges.append([ids[(d, links[d]["inner_right"])], ids[(e, links[e]["inner_left"])]])
        edges.append([ids[(d, links[d]["outer_right"])], ids[(e, links[e]["outer_left"])]])
    # Los efectos de los Nodos no van en el archivo: al cargar se reparten según parameters.powerNodeEffects (R-TAB-5).
    board = {
        "parameters": {"powerSourcesPerDomain": 1, "powerNodeEffects": NODE_EFFECTS, "maxSealedTiles": 3,
                       "runeReservePerSource": 4},
        "domains": [{"id": d, "name": DOMAIN_NAMES[d]} for d in range(DOMAIN_COUNT)],
        "tiles": tiles,
        "edges": sorted(edges, key=lambda e: (min(e), max(e))),
        "editor": {"positions": positions},
    }
    return board


def check(board):
    """Las comprobaciones del editor y del cargador, más las de diseño del 21/10. Devuelve (errores, avisos)."""
    errs, warns = [], []
    tiles = {t["id"]: t for t in board["tiles"]}
    pos = {int(k): v for k, v in board["editor"]["positions"].items()}
    n = {t: set() for t in tiles}
    for a, b in board["edges"]:
        if a == b or a not in tiles or b not in tiles:
            errs.append(f"R-TAB-1: conexión {a}-{b} inválida")
            continue
        n[a].add(b)
        n[b].add(a)
    if [t["id"] for t in board["tiles"]] != list(range(len(tiles))):
        errs.append("R-TAB-1: identificadores no consecutivos")
    altars = [t for t in tiles.values() if t["type"] == "altar"]
    if len(altars) != 1:
        errs.append(f"R-TAB-2: {len(altars)} Altares")
    p = board["parameters"]
    wanted_nodes = sum(p["powerNodeEffects"].values())
    nodes = [t for t in tiles.values() if t["type"] == "powerNode"]
    if len(nodes) != wanted_nodes:
        errs.append(f"R-TAB-5: {len(nodes)} Nodos y los efectos suman {wanted_nodes}")
    for d in range(DOMAIN_COUNT):
        mine = [t for t in tiles.values() if t.get("domain") == d]
        count = len(mine)
        if not 12 <= count <= 15:
            warns.append(f"Dominio {d}: {count} casillas (regla 1: de 12 a 15)")
        if not any(tiles[o]["type"] == "altar" for t in mine for o in n[t["id"]]):
            errs.append(f"R-TAB-3: Dominio {d} sin conexión con el Altar")
        altar_links = sum(1 for t in mine for o in n[t["id"]] if tiles[o]["type"] == "altar")
        if altar_links != 1:
            warns.append(f"Dominio {d}: {altar_links} accesos al Altar (regla 7: uno)")
        e = (d + 1) % DOMAIN_COUNT
        links = sum(1 for t in mine for o in n[t["id"]] if tiles[o].get("domain") == e)
        if links == 0:
            errs.append(f"R-TAB-3: anillo roto entre {d} y {e}")
        elif not 1 <= links <= 2:
            warns.append(f"Dominios {d}-{e}: {links} conexiones (regla 7: una o dos)")
        niches = [t for t in mine if t["type"] == "niche"]
        if len(niches) != 1:
            errs.append(f"R-TAB-4: Dominio {d} con {len(niches)} Nichos")
        elif len(n[niches[0]["id"]]) != 1:
            errs.append(f"R-TAB-4c: el Nicho del Dominio {d} tiene {len(n[niches[0]['id']])} conexiones")
        sources = [t for t in mine if t["type"] == "powerSource"]
        if len(sources) != p["powerSourcesPerDomain"]:
            errs.append(f"R-TAB-4: Dominio {d} con {len(sources)} Fuentes")
        for t in mine:
            if t["type"] in ("powerSource", "powerNode", "niche") and len(n[t["id"]]) > 2:
                errs.append(f"Regla 8: la casilla especial {t['id']} ({t['type']}) tiene {len(n[t['id']])} conexiones")
        # Regla 3: un anillo dentro del Dominio (más conexiones internas que casillas menos una).
        internal = sum(1 for a, b in board["edges"] if tiles[a].get("domain") == d and tiles[b].get("domain") == d)
        if internal < count:
            warns.append(f"Dominio {d}: sin anillo interno ({internal} conexiones para {count} casillas)")
        # Regla 2: la columna desde el Altar no lleva especiales.
        entry = next(t for t in mine for o in n[t["id"]] if tiles[o]["type"] == "altar")
        if entry["type"] != "normal":
            errs.append(f"Regla 2: la entrada del Dominio {d} (casilla {entry['id']}) es {entry['type']}")
    # Regla 9: distancia mínima entre centros.
    ids = sorted(pos)
    for i, a in enumerate(ids):
        for b in ids[i + 1:]:
            dist = math.dist(pos[a], pos[b])
            if dist < MIN_DISTANCE:
                errs.append(f"Regla 9: casillas {a} y {b} a {dist:.0f} (mínimo {MIN_DISTANCE:.0f})")
    # Todo conectado.
    seen, stack = {0}, [0]
    while stack:
        for o in n[stack.pop()]:
            if o not in seen:
                seen.add(o)
                stack.append(o)
    if len(seen) != len(tiles):
        errs.append(f"Grafo no conexo: {len(tiles) - len(seen)} casillas sueltas")
    # Dos conexiones no se cruzan (el sendero de una pasaría por encima del de la otra) ni pasan por encima de una
    # casilla ajena a ellas.
    segs = [(a, b) for a, b in board["edges"]]
    for i, (a, b) in enumerate(segs):
        for c, d in segs[i + 1:]:
            if len({a, b, c, d}) == 4 and _cross(pos[a], pos[b], pos[c], pos[d]):
                errs.append(f"Las conexiones {a}-{b} y {c}-{d} se cruzan")
        for t in pos:
            if t not in (a, b) and _point_segment_distance(pos[t], pos[a], pos[b]) < MIN_DISTANCE / 2:
                errs.append(f"La conexión {a}-{b} pasa por encima de la casilla {t}")
    return errs, warns


def _cross(p, q, r, s):
    def orient(a, b, c):
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    return orient(p, q, r) * orient(p, q, s) < 0 and orient(r, s, p) * orient(r, s, q) < 0


def _point_segment_distance(p, a, b):
    ax, ay = a
    dx, dy = b[0] - ax, b[1] - ay
    t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / (dx * dx + dy * dy)))
    return math.dist(p, (ax + t * dx, ay + t * dy))


def edge_stats(board):
    """Longitud de la conexión más larga y cuántas superan 150 unidades (unos 16 cm en la mesa)."""
    pos = {int(k): v for k, v in board["editor"]["positions"].items()}
    lengths = sorted(math.dist(pos[a], pos[b]) for a, b in board["edges"])
    return lengths[-1], sum(1 for l in lengths if l > 150), lengths[0]


def fit_canvas(board, radius=497):
    """Encoge las posiciones alrededor del Altar para que la casilla más lejana quede dentro del lienzo del editor
    (viewBox 0..1000). No cambia la mesa: ComputeBoardLayout saca la escala de la casilla más lejana, así que la
    forma física es la misma; solo cambian las unidades del editor. Se aplica después de check(), que mide en las
    unidades de las plantillas."""
    pos = board["editor"]["positions"]
    farthest = max(math.dist((CX, CY), p) for p in pos.values())
    if farthest <= radius:
        return
    k = radius / farthest
    for key, (x, y) in pos.items():
        pos[key] = [round(CX + (x - CX) * k), round(CY + (y - CY) * k)]


def preview(board, path):
    from PIL import Image, ImageDraw
    scale = 1.6
    im = Image.new("RGB", (int(1000 * scale), int(1000 * scale)), (24, 24, 28))
    dr = ImageDraw.Draw(im)
    pos = {int(k): (v[0] * scale, v[1] * scale) for k, v in board["editor"]["positions"].items()}
    tiles = {t["id"]: t for t in board["tiles"]}
    palette = [(70, 110, 200), (200, 60, 60), (60, 170, 80), (190, 70, 170), (60, 170, 180), (210, 180, 50)]
    for a, b in board["edges"]:
        dr.line([pos[a], pos[b]], fill=(90, 90, 100), width=4)
    for t in board["tiles"]:
        x, y = pos[t["id"]]
        r = 22 if t["type"] != "altar" else 46
        color = (240, 150, 30) if t["type"] == "altar" else palette[t["domain"]]
        if t["type"] == "niche":
            dr.rectangle([x - r, y - r, x + r, y + r], fill=color, outline=(230, 230, 230), width=3)
        elif t["type"] == "powerSource":
            dr.ellipse([x - r, y - r, x + r, y + r], fill=color, outline=(230, 230, 230), width=3)
            dr.line([x - r * 0.6, y, x + r * 0.6, y], fill=(255, 255, 255), width=3)
            dr.line([x, y - r * 0.6, x, y + r * 0.6], fill=(255, 255, 255), width=3)
        elif t["type"] == "powerNode":
            dr.polygon([(x, y - r), (x + r, y), (x, y + r), (x - r, y)], fill=color, outline=(230, 230, 230), width=3)
        else:
            dr.ellipse([x - r, y - r, x + r, y + r], fill=color)
        dr.text((x - 8, y - 6), str(t["id"]), fill=(255, 255, 255))
    im.save(path)


def main():
    ap = argparse.ArgumentParser(prog="gen_tablero_radial.py")
    ap.add_argument("--out", default=os.path.join("Content", "Data", "Boards", "tablero_radial.json"))
    ap.add_argument("--preview", default=None)
    ap.add_argument("--variante", type=int, choices=(1, 2), default=2,
                    help="1: seis cuñas iguales (tablero_82.json); 2: seis cuñas distintas con más conexiones")
    ap.add_argument("--nichos", choices=("centro", "lejos"), default="centro",
                    help="solo variante 2: Nicho por la parte central (3-7 pasos del Altar) o en el extremo exterior (7-8 pasos)")
    args = ap.parse_args()
    board = build(args.variante, args.nichos)
    errs, warns = check(board)
    for w in warns:
        print("AVISO:", w)
    for e in errs:
        print("ERROR:", e)
    if errs:
        sys.exit(1)
    longest, long_count, shortest = edge_stats(board)
    fit_canvas(board)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(board, f, ensure_ascii=False, indent=1)
    total = len(board["tiles"])
    per = [sum(1 for t in board["tiles"] if t.get("domain") == d) for d in range(DOMAIN_COUNT)]
    print(f"Tablero escrito: {args.out} con {total} casillas (Altar + {per} por Dominio), {len(board['edges'])} conexiones")
    print(f"Conexión más corta {shortest:.0f}, más larga {longest:.0f}; {long_count} superan 150 unidades")
    if args.preview:
        preview(board, args.preview)
        print(f"Vista previa: {args.preview}")


if __name__ == "__main__":
    main()
