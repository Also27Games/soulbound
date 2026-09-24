"""
Generador de un Dominio de prueba en formato .vox (MagicaVoxel), spike paso 9, prueba A.

Genera "El Bosque Negro": una losa de suelo en dos tonos, un camino de ocho casillas en curva,
un trono de piedra (Nicho) al final, una Fuente (brasero con acento) y árboles muertos y rocas
por reglas con semilla. Es un punto de partida para retocar en MagicaVoxel, no el asset final.

Uso:
    python Scripts/art/gen_bosque_vox.py                      # Saved/Art/bosque_negro.vox, semilla 1, suelo de 64
    python Scripts/art/gen_bosque_vox.py --seed 7 --escala 2  # suelo de 128 (casilla de 12), todo el doble de fino
    python Scripts/art/gen_bosque_vox.py --escala 2 --separar # un .vox por pieza: suelo con camino, trono, fuente,
                                                              # tres arboles y dos rocas, para instanciar en Unreal

Solo usa la biblioteca estándar. Formato .vox: cabecera "VOX ", chunk MAIN con SIZE, XYZI y RGBA.
Ejes: x e y horizontales, z hacia arriba. Un solo modelo (máximo 256 por eje).
"""
import argparse
import math
import os
import random
import struct

# Paleta de ocho colores, desaturados. El índice 0 es "vacío" en .vox; los colores empiezan en 1.
PALETA = {
    1: (52, 44, 40),     # tierra oscura
    2: (68, 58, 50),     # tierra clara
    3: (120, 116, 108),  # piedra clara (casillas)
    4: (84, 82, 78),     # piedra media (trono, rocas)
    5: (58, 48, 38),     # madera muerta
    6: (24, 26, 36),     # negro azulado (profundo, hueco)
    7: (90, 210, 220),   # acento cian (magia)
    8: (200, 190, 170),  # hueso claro (borde de casilla)
}

ESCALA = 1      # 1: suelo de 64; 2: suelo de 128 (se fija desde --escala)
TAM = 64        # lado del suelo
GROSOR = 6      # grosor de la losa
ALTO = 40       # altura total del modelo
CASILLA = 6     # lado de una casilla en vóxeles


def aplicar_escala(escala):
    global ESCALA, TAM, GROSOR, ALTO, CASILLA
    ESCALA = escala
    TAM, GROSOR, ALTO, CASILLA = 64 * escala, 6 * escala, min(40 * escala, 255), 6 * escala


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default=os.path.join("Saved", "Art", "bosque_negro.vox"))
    ap.add_argument("--escala", type=int, default=1, choices=(1, 2))
    ap.add_argument("--separar", action="store_true", help="un .vox por pieza en la carpeta de --out")
    args = ap.parse_args()
    aplicar_escala(args.escala)

    rng = random.Random(args.seed)
    camino = casillas_en_curva()

    if args.separar:
        carpeta = os.path.dirname(args.out) or "."
        piezas = {}
        v = {}
        suelo(v, rng)
        for (cx, cy) in camino:
            casilla(v, cx, cy)
        piezas["suelo_con_camino"] = v
        v = {}
        trono(v, 0, -CASILLA)
        piezas["trono"] = v
        v = {}
        fuente(v, 0, 0)
        piezas["fuente"] = v
        for i in range(3):
            v = {}
            arbol(v, rng, 4 * ESCALA, 4 * ESCALA)
            piezas[f"arbol_{i + 1}"] = v
        for i in range(2):
            v = {}
            roca(v, rng, 5 * ESCALA, 5 * ESCALA)
            piezas[f"roca_{i + 1}"] = v
        for nombre, v in piezas.items():
            ruta = os.path.join(carpeta, f"{nombre}.vox")
            escribir_vox(ruta, v, recortar=(nombre != "suelo_con_camino"))
            print(f"{ruta}: {len(v)} voxeles")
        return

    vox = {}  # (x, y, z) -> índice de color
    suelo(vox, rng)
    for (cx, cy) in camino:
        casilla(vox, cx, cy)
    trono(vox, *camino[-1])
    fuente(vox, 50 * ESCALA, 12 * ESCALA)
    for _ in range(6):
        colocar_libre(vox, rng, camino, lambda x, y: arbol(vox, rng, x, y), margen=4 * ESCALA)
    for _ in range(5):
        colocar_libre(vox, rng, camino, lambda x, y: roca(vox, rng, x, y), margen=2 * ESCALA)

    escribir_vox(args.out, vox)
    print(f"{args.out}: {len(vox)} voxeles, {len(camino)} casillas, semilla {args.seed}, escala {ESCALA}")


def suelo(vox, rng):
    # Manchas de dos tonos: ruido suave por celdas de 4x4 con algo de azar por vóxel.
    for x in range(TAM):
        for y in range(TAM):
            base = 2 if (math.sin(x * 0.35 / ESCALA) + math.cos(y * 0.27 / ESCALA) + rng.random() * 0.8) > 0.6 else 1
            for z in range(GROSOR):
                vox[(x, y, z)] = base if z == GROSOR - 1 else 1


def casillas_en_curva():
    # Ocho casillas desde la esquina (4,4) hasta cerca del centro, en arco. Coordenadas de la esquina de cada placa.
    puntos = []
    for i in range(8):
        t = i / 7.0
        ang = -math.pi / 2 + t * (math.pi / 2)
        r = (40 - t * 22) * ESCALA
        cx = int(34 * ESCALA + r * math.cos(ang) * 0.75) - CASILLA // 2
        cy = int(44 * ESCALA + r * math.sin(ang)) - CASILLA // 2
        cx = max(1, min(TAM - CASILLA - 1, cx))
        cy = max(1, min(TAM - CASILLA - 1, cy))
        puntos.append((cx, cy))
    return puntos


def casilla(vox, cx, cy):
    z = GROSOR
    for x in range(cx, cx + CASILLA):
        for y in range(cy, cy + CASILLA):
            borde = x in (cx, cx + CASILLA - 1) or y in (cy, cy + CASILLA - 1)
            vox[(x, y, z)] = 8 if borde else 3


def trono(vox, cx, cy):
    # Asiento de piedra al fondo de la última casilla, respaldo de 10 de alto.
    e = ESCALA
    bx, by = cx + 1 * e, cy + CASILLA  # justo detrás de la casilla
    for x in range(bx, bx + 4 * e):
        for y in range(by, by + 3 * e):
            for z in range(GROSOR + 1, GROSOR + 1 + 3 * e):
                vox[(x, y, z)] = 4
    for x in range(bx, bx + 4 * e):
        for y in range(by + 2 * e, by + 3 * e):
            for z in range(GROSOR + 1, GROSOR + 1 + 10 * e):
                vox[(x, y, z)] = 4 if z < GROSOR + 10 * e or x < bx + e or x >= bx + 3 * e else 6
    for z in range(GROSOR + 4 * e, GROSOR + 8 * e):
        for dy in range(e):
            vox[(bx - 1, by + dy, z)] = 4
            vox[(bx + 4 * e, by + dy, z)] = 4


def fuente(vox, cx, cy):
    # Brasero de 6x6x5 hueco con el acento dentro (escalado).
    e = ESCALA
    for x in range(cx, cx + 6 * e):
        for y in range(cy, cy + 6 * e):
            for z in range(GROSOR, GROSOR + 5 * e):
                borde = x < cx + e or x >= cx + 5 * e or y < cy + e or y >= cy + 5 * e
                if z < GROSOR + e or borde:
                    vox[(x, y, z)] = 4
                elif z >= GROSOR + 3 * e:
                    vox[(x, y, z)] = 7
                else:
                    vox[(x, y, z)] = 6


def arbol(vox, rng, x, y):
    # Tronco retorcido: paseo aleatorio hacia arriba con dos o tres ramas cortas. Sin hojas.
    alto = rng.randint(12, 20) * ESCALA
    px, py = x, y
    for z in range(GROSOR, GROSOR + alto):
        grosor = (2 if z < GROSOR + alto // 2 else 1) * ESCALA
        for dx in range(grosor):
            for dy in range(grosor):
                vox[(px + dx, py + dy, z)] = 5
        if rng.random() < 0.35 / ESCALA:
            px += rng.choice((-1, 0, 1))
            py += rng.choice((-1, 0, 1))
        if z > GROSOR + alto // 2 and rng.random() < 0.25 / ESCALA:
            rama(vox, rng, px, py, z)
    return alto


def rama(vox, rng, x, y, z):
    dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
    for i in range(1, rng.randint(3, 6) * ESCALA):
        for g in range(ESCALA):
            vox[(x + dx * i + dy * g, y + dy * i + dx * g, z + i // (2 * ESCALA))] = 5


def roca(vox, rng, x, y):
    rx, ry, rz = rng.randint(2, 4) * ESCALA, rng.randint(2, 4) * ESCALA, rng.randint(2, 3) * ESCALA
    for dx in range(-rx, rx + 1):
        for dy in range(-ry, ry + 1):
            for dz in range(0, rz + 1):
                if (dx / rx) ** 2 + (dy / ry) ** 2 + (dz / rz) ** 2 <= 1.0:
                    vox[(x + dx, y + dy, GROSOR + dz)] = 4
    return rz


def colocar_libre(vox, rng, camino, pintar, margen):
    # Busca un punto lejos de las casillas y de la fuente y pinta ahí.
    for _ in range(200):
        x, y = rng.randint(margen, TAM - margen - 1), rng.randint(margen, TAM - margen - 1)
        if all(abs(x - (cx + CASILLA // 2)) > CASILLA + margen or abs(y - (cy + CASILLA // 2)) > CASILLA + margen for (cx, cy) in camino) \
                and not (44 * ESCALA < x < 60 * ESCALA and 6 * ESCALA < y < 22 * ESCALA):
            pintar(x, y)
            return


def escribir_vox(ruta, vox, recortar=False):
    os.makedirs(os.path.dirname(ruta) or ".", exist_ok=True)
    if recortar and vox:
        # Pieza suelta: se recorta a su caja y se apoya en z=0 para instanciarla en Unreal.
        xs = [x for (x, _, _) in vox]
        ys = [y for (_, y, _) in vox]
        zs = [z for (_, _, z) in vox]
        mx, my, mz = min(xs), min(ys), min(zs)
        vox = {(x - mx, y - my, z - mz): c for (x, y, z), c in vox.items()}
        tx, ty, tz = max(xs) - mx + 1, max(ys) - my + 1, max(zs) - mz + 1
    else:
        tx, ty, tz = TAM, TAM, ALTO
    tx, ty, tz = min(tx, 256), min(ty, 256), min(tz, 256)
    voxeles = [(x, y, z, c) for (x, y, z), c in vox.items()
               if 0 <= x < tx and 0 <= y < ty and 0 <= z < tz]

    size = struct.pack("<3i", tx, ty, tz)
    xyzi = struct.pack("<i", len(voxeles)) + b"".join(struct.pack("<4B", x, y, z, c) for x, y, z, c in voxeles)
    paleta = bytearray()
    for i in range(1, 257):
        r, g, b = PALETA.get(i, (0, 0, 0))
        paleta += struct.pack("<4B", r, g, b, 255)
    rgba = bytes(paleta)

    def chunk(cid, contenido, hijos=b""):
        return cid + struct.pack("<2i", len(contenido), len(hijos)) + contenido + hijos

    hijos = chunk(b"SIZE", size) + chunk(b"XYZI", xyzi) + chunk(b"RGBA", rgba)
    datos = b"VOX " + struct.pack("<i", 150) + chunk(b"MAIN", b"", hijos)
    with open(ruta, "wb") as f:
        f.write(datos)


if __name__ == "__main__":
    main()
