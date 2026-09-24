"""
Generador de un Dominio de prueba en formato .vox (MagicaVoxel), spike paso 9, prueba A.

Genera "El Bosque Negro": una losa de suelo en dos tonos, un camino de ocho casillas en curva,
un trono de piedra (Nicho) al final, una Fuente (brasero con acento) y árboles muertos y rocas
por reglas con semilla. Es un punto de partida para retocar en MagicaVoxel, no el asset final.

Uso:
    python Scripts/art/gen_bosque_vox.py                      # Saved/Art/bosque_negro.vox, semilla 1
    python Scripts/art/gen_bosque_vox.py --seed 7 --out ruta.vox

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

TAM = 64        # lado del suelo
GROSOR = 6      # grosor de la losa
ALTO = 40       # altura total del modelo
CASILLA = 6     # lado de una casilla en vóxeles


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default=os.path.join("Saved", "Art", "bosque_negro.vox"))
    args = ap.parse_args()

    rng = random.Random(args.seed)
    vox = {}  # (x, y, z) -> índice de color

    suelo(vox, rng)
    camino = casillas_en_curva()
    for (cx, cy) in camino:
        casilla(vox, cx, cy)
    trono(vox, *camino[-1])
    fuente(vox, 50, 12)
    for _ in range(6):
        colocar_libre(vox, rng, camino, lambda x, y: arbol(vox, rng, x, y), margen=4)
    for _ in range(5):
        colocar_libre(vox, rng, camino, lambda x, y: roca(vox, rng, x, y), margen=2)

    escribir_vox(args.out, vox)
    print(f"{args.out}: {len(vox)} voxeles, {len(camino)} casillas, semilla {args.seed}")


def suelo(vox, rng):
    # Manchas de dos tonos: ruido suave por celdas de 4x4 con algo de azar por vóxel.
    for x in range(TAM):
        for y in range(TAM):
            base = 2 if (math.sin(x * 0.35) + math.cos(y * 0.27) + rng.random() * 0.8) > 0.6 else 1
            for z in range(GROSOR):
                vox[(x, y, z)] = base if z == GROSOR - 1 else 1


def casillas_en_curva():
    # Ocho casillas desde la esquina (4,4) hasta cerca del centro, en arco. Coordenadas de la esquina de cada placa.
    puntos = []
    for i in range(8):
        t = i / 7.0
        ang = -math.pi / 2 + t * (math.pi / 2)
        r = 40 - t * 22
        cx = int(34 + r * math.cos(ang) * 0.75) - CASILLA // 2
        cy = int(44 + r * math.sin(ang)) - CASILLA // 2
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
    bx, by = cx + 1, cy + CASILLA  # justo detrás de la casilla
    for x in range(bx, bx + 4):
        for y in range(by, by + 3):
            for z in range(GROSOR + 1, GROSOR + 4):
                vox[(x, y, z)] = 4
    for x in range(bx, bx + 4):
        for z in range(GROSOR + 1, GROSOR + 11):
            vox[(x, by + 2, z)] = 4 if z < GROSOR + 10 or x in (bx, bx + 3) else 6
    for z in range(GROSOR + 4, GROSOR + 8):
        vox[(bx - 1, by, z)] = 4
        vox[(bx + 4, by, z)] = 4


def fuente(vox, cx, cy):
    # Brasero de 6x6x5 hueco con el acento dentro.
    for x in range(cx, cx + 6):
        for y in range(cy, cy + 6):
            for z in range(GROSOR, GROSOR + 5):
                borde = x in (cx, cx + 5) or y in (cy, cy + 5)
                if z == GROSOR or borde:
                    vox[(x, y, z)] = 4
                elif z >= GROSOR + 3:
                    vox[(x, y, z)] = 7
                else:
                    vox[(x, y, z)] = 6


def arbol(vox, rng, x, y):
    # Tronco retorcido: paseo aleatorio hacia arriba con dos o tres ramas cortas. Sin hojas.
    alto = rng.randint(12, 20)
    px, py = x, y
    for z in range(GROSOR, GROSOR + alto):
        grosor = 2 if z < GROSOR + alto // 2 else 1
        for dx in range(grosor):
            for dy in range(grosor):
                vox[(px + dx, py + dy, z)] = 5
        if rng.random() < 0.35:
            px += rng.choice((-1, 0, 1))
            py += rng.choice((-1, 0, 1))
        if z > GROSOR + alto // 2 and rng.random() < 0.25:
            rama(vox, rng, px, py, z)
    return alto


def rama(vox, rng, x, y, z):
    dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
    for i in range(1, rng.randint(3, 6)):
        vox[(x + dx * i, y + dy * i, z + i // 2)] = 5


def roca(vox, rng, x, y):
    rx, ry, rz = rng.randint(2, 4), rng.randint(2, 4), rng.randint(2, 3)
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
        if all(abs(x - (cx + 3)) > CASILLA + margen or abs(y - (cy + 3)) > CASILLA + margen for (cx, cy) in camino) \
                and not (44 < x < 60 and 6 < y < 22):
            pintar(x, y)
            return


def escribir_vox(ruta, vox):
    os.makedirs(os.path.dirname(ruta) or ".", exist_ok=True)
    voxeles = [(x, y, z, c) for (x, y, z), c in vox.items()
               if 0 <= x < TAM and 0 <= y < TAM and 0 <= z < ALTO]

    size = struct.pack("<3i", TAM, TAM, ALTO)
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
