"""
Generador del diorama de El Bosque Negro en low-poly, prueba C de dirección de arte (ronda 6: el kit de
producción, decidido el 15/10/2026 tras la prueba de conjunto en Unreal).

Ronda 6 respecto a la 5: el Nicho y su trono van al extremo exterior del Dominio, de cara al asiento; la
losa de la Fuente queda libre y el pilón es una estructura aparte con su runa flotante; hay pieza propia
de Altar, estructuras para los tres Nodos (arco Planar, espadas de Duelo, orbe del Oráculo) y un Sello;
y se exporta un suelo liso (sin senderos pintados) para que el juego coloque losas, senderos y
estructuras desde los datos del tablero. kit.json describe cada pieza (triángulos, caja, anclaje).

Construye con código, dentro de Blender y sin ventana, un Dominio con forma de cuña de 60 grados: estrecho
junto al Altar, en el centro del tablero, y ancho hacia fuera, sobre una peana negra con canto. Estilo
aprobado en la ronda 4: facetado, aristas vivas, color plano por vértice con una paleta de ocho colores, sin
texturas, y oclusión ambiental calculada por rayos y multiplicada en el color por vértice.

Contiene el grafo del Dominio: camino principal de once losas (conectado al Altar y al Dominio vecino) y
ramal sin salida de cuatro; cada conexión se ve en el suelo como una franja de tierra clara de un tercio
del ancho de una losa, bordeada de piedrecillas. Tres losas especiales: Nicho (tarima con hueco, al final
del ramal, con el trono detrás), Fuente de Poder (pilón con agua oscura y un toque de cian tenue, junto al
brasero) y Nodo de Poder (hexagonal, con una marca tallada). Algunas losas normales están partidas.

Escenografía fuera del camino, con margen de media losa y separada entre sí: seis árboles muertos (tres
variantes), un árbol caído, cinco rocas, huesos, cadena, lápida caída, estacas, una jaula, velas
derretidas, un cuervo en el trono y tres charcos de agua estancada. Lo alto va al perímetro, y ninguna
pieza puede dejar una losa oculta del todo desde ninguno de los cuatro asientos de la mesa: se comprueba
con rayos al colocarla y se informa al final. Una ficha de prueba (carta de 6x9 cm) da la escala.

Uso (desde la raíz del proyecto):
    blender -b --factory-startup -P Scripts/art/gen_bosque_lowpoly.py -- --seed 1 --out Saved/Art/prueba-c/ronda5

Salida en --out: vista_sentada.png, vista_cenital.png, vista_asiento_<sur|este|norte|oeste>.png,
glb/<pieza>.glb, glb/bosque_negro_conjunto.glb, colocacion.json (grafo, instancias, triángulos,
visibilidad) y bosque_negro.blend para abrirlo en Blender.

Unidades en metros. El origen es el centro del tablero (centro del Altar): los seis Dominios son esta misma
pieza girada de 60 en 60 grados. Ejes de Blender: la cuña se abre hacia -y, donde se sienta el jugador;
z arriba. Parte de la disposición y la paleta de la prueba A (gen_bosque_vox.py), no de su técnica.
"""
import argparse
import json
import math
import os
import random
import sys
import time

import bmesh
import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Euler, Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import delaunay_2d_cdt

# ---------------------------------------------------------------------------------------------------------
# Paleta: ocho colores en sRGB. El cian es el único acento de color.
# ---------------------------------------------------------------------------------------------------------
TIERRA_OSCURA, TIERRA_CLARA, PIEDRA_CLARA, PIEDRA_MEDIA, MADERA, NEGRO, CIAN, HUESO = range(8)
PALETA = [
    (44, 37, 34),     # tierra oscura
    (92, 78, 64),     # tierra clara (senderos del grafo): más clara que en la ronda 4, para que se lean
    (126, 122, 114),  # piedra clara (casillas)
    (106, 103, 97),   # piedra media (trono, rocas, piedras de sendero)
    (116, 102, 86),   # madera muerta, blanqueada
    (24, 26, 36),     # negro azulado (hueco, hierro, grietas, agua)
    (90, 210, 220),   # acento cian (magia)
    (205, 194, 172),  # hueso claro (borde de casilla, huesos, cera, ficha)
]
# Índices que no son colores nuevos, sino un color de la paleta con otro material:
CIAN_TENUE = 20   # cian con emisión débil (el agua de la Fuente de Poder)
AGUA = 21         # negro con material brillante (charcos): lo único que refleja la luz
COLOR_DE = {CIAN_TENUE: CIAN, AGUA: NEGRO}
MATERIAL_DE = {CIAN: 1, CIAN_TENUE: 2, AGUA: 3}   # índice de material; el resto, 0 (paleta mate)

# ---------------------------------------------------------------------------------------------------------
# Parámetros del diorama, con su valor inicial. Todos en metros salvo que se diga otra cosa.
# ---------------------------------------------------------------------------------------------------------
R_INTERIOR = 0.15           # apotema del Altar hexagonal: el Dominio empieza en su borde
R_EXTERIOR = 0.74           # radio del borde exterior del tablero
MEDIO_ANGULO = math.radians(30)
PEANA_ALTO = 0.018          # alto de la peana negra sobre la mesa
PEANA_CANTO = 0.008         # cuánto sobresale la peana del borde de la tierra
GROSOR_SUELO = 0.012        # grosor de la tierra sobre la peana
SUELO_Z = PEANA_ALTO + GROSOR_SUELO  # altura de referencia del suelo sobre la mesa
SEPARACION_SUELO = 0.016    # distancia mínima entre vértices del suelo (faceta de 1,5 a 2 cm)
RELIEVE = 0.003             # amplitud del ruido del suelo
LOSA_MEDIO = 0.031          # semilado de una losa: cabe la peana de la ficha y se sigue viendo el reborde
MARGEN_LOSA = LOSA_MEDIO    # margen libre alrededor de cada losa y sendero: media losa
ANCHO_SENDERO = 2 * LOSA_MEDIO / 3  # franja de tierra clara de cada conexión: un tercio del ancho de losa
# Camino principal (curva Catmull-Rom): sale de la punta, junto al Altar, baja pegado al lado derecho,
# cruza el Dominio por el medio y sale por la izquierda hacia el Dominio vecino. Ronda 6: va más por
# dentro que en la ronda 5, para dejar la franja exterior al ramal del Nicho.
CAMINO_PRINCIPAL = [(0.05, -0.215), (0.11, -0.27), (0.16, -0.33), (0.17, -0.40), (0.12, -0.455),
                    (0.03, -0.48), (-0.07, -0.475), (-0.16, -0.45), (-0.25, -0.41)]
N_PRINCIPAL = 11
# Ramal sin salida hacia el Nicho: sale en T de una losa del principal y baja hacia el borde exterior,
# donde está el trono de cara al asiento del jugador (decisión del autor del 15/10/2026: el Nicho y el
# trono van al extremo exterior del Dominio; en la ronda 5 estaban junto al Altar). Dos losas: con el
# paso de este trazado no cabe más entre el principal y el trono.
RAMAL_DESDE = 6             # índice de la losa del principal de la que sale
RAMAL_CONTROL = [(0.01, -0.54), (0.0, -0.585), (-0.01, -0.62)]  # radial, hacia el borde exterior
N_RAMAL = 2
FUENTE_EN = 3               # losa del principal que es la Fuente de Poder
NODO_EN = 9                 # losa del principal que es el Nodo de Poder
FICHA_EN = 2                # losa del principal donde se pone la ficha de prueba
N_PARTIDAS = 3              # losas normales partidas en dos (las demás, enteras e iguales)
N_ARBOLES, N_ROCAS, N_HUESOS, N_CHARCOS, N_VELAS = 6, 5, 2, 3, 2
BORDE_ALTO = 0.075          # lo alto va a menos de esto del perímetro del sector
HUECOS = (0.03, 0.018, 0.008)  # hueco libre entre piezas vecinas, de más a menos exigente
# Escala de cada pieza sobre las medidas con las que está modelada.
ESCALA_TRONO, ESCALA_BRASERO, ESCALA_ARBOL, ESCALA_ROCA = 1.35, 1.4, 1.35, 1.4
# Ficha de prueba: carta vertical de 6x9 cm sobre peana circular.
FICHA_ANCHO, FICHA_ALTO, PEANA_DIAMETRO = 0.06, 0.09, 0.045

# Oclusión ambiental en el color por vértice.
AO_RAYOS = 20               # rayos por esquina de cara
AO_DISTANCIA_PIEZA = 0.025  # alcance en las piezas (se calculan solas, contra sí mismas y un suelo plano)
AO_DISTANCIA_SUELO = 0.035  # alcance en el suelo (se calcula con todo lo que tiene encima)
AO_FUERZA = 0.65            # cuánto oscurece un hueco cerrado del todo

# Cámaras y render. Cuatro asientos alrededor de la mesa, a SENTADA_DISTANCIA del borde del tablero.
RES_X, RES_Y = 1280, 800
SENTADA_ALTURA = 0.60       # sobre el plano de la mesa
SENTADA_DISTANCIA = 0.80    # desde el borde exterior del tablero
SENTADA_PICADO = 30.0       # grados por debajo de la horizontal (vista sentada principal)
SENTADA_FOCAL = 40.0        # mm sobre sensor de 36 mm (unos 48 grados de campo horizontal)
ASIENTOS = (("sur", 270), ("este", 0), ("norte", 90), ("oeste", 180))  # ángulo alrededor del tablero

# Presupuesto de triángulos (ronda 5): techo por Dominio, que no es un objetivo, y topes por pieza.
TRIS_MAX_DOMINIO = 30000
TRIS_MAX = {"arbol_": 800, "trono": 3000}


# ---------------------------------------------------------------------------------------------------------
# Utilidades de geometría. Cada pieza se construye en un bmesh con una capa entera por cara ("pal") que
# guarda el índice de la paleta; al pasar a malla se convierte en color por vértice (por esquina de cara).
# ---------------------------------------------------------------------------------------------------------
def nuevo_bm():
    bm = bmesh.new()
    return bm, bm.faces.layers.int.new("pal")


def cara(bm, capa, verts, color):
    f = bm.faces.new(verts)
    f[capa] = color
    return f


def anillos_a_caras(bm, capa, a, b, color):
    # Une dos anillos del mismo número de vértices con quads; si b es un solo vértice, cierra en abanico.
    n = len(a)
    for k in range(n):
        if len(b) == 1:
            cara(bm, capa, [a[k], a[(k + 1) % n], b[0]], color)
        else:
            cara(bm, capa, [a[k], a[(k + 1) % n], b[(k + 1) % n], b[k]], color)


def tubo(bm, capa, puntos, radios, lados, color, rng, irregular=0.18, tapa=False):
    """Tubo facetado a lo largo de una polilínea. Un radio 0 en el último punto lo cierra en punta.
    La sección se gira y se deforma al azar en cada anillo para que no parezca torneado."""
    anillos, normal_prev = [], None
    for i, p in enumerate(puntos):
        if i == 0:
            t = puntos[1] - puntos[0]
        elif i == len(puntos) - 1:
            t = puntos[i] - puntos[i - 1]
        else:
            t = puntos[i + 1] - puntos[i - 1]
        t.normalize()
        # Marco por transporte paralelo, para que el tubo no se retuerza en los quiebros.
        if normal_prev is None:
            eje = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            n = t.cross(eje).normalized()
        else:
            n = (normal_prev - t * normal_prev.dot(t)).normalized()
        b = t.cross(n)
        normal_prev = n
        if radios[i] <= 1e-6:
            anillos.append([bm.verts.new(p)])
            continue
        fase = rng.uniform(-irregular, irregular) * 2
        anillo = []
        for k in range(lados):
            a = 2 * math.pi * k / lados + fase
            r = radios[i] * rng.uniform(1 - irregular, 1 + irregular)
            anillo.append(bm.verts.new(p + (n * math.cos(a) + b * math.sin(a)) * r))
        anillos.append(anillo)
    for a, b in zip(anillos, anillos[1:]):
        anillos_a_caras(bm, capa, a, b, color)
    if tapa and len(anillos[0]) > 2:
        cara(bm, capa, list(reversed(anillos[0])), color)


def torno(bm, capa, perfil, lados, color_por_tramo, rng, irregular=0.08, centro=(0, 0)):
    """Sólido de revolución facetado alrededor de un eje vertical, cerrado por los dos extremos con un
    vértice central. perfil: lista de (radio, z). color_por_tramo: uno por tramo (len(perfil) + 1)."""
    cx, cy = centro
    fase = rng.uniform(0, math.pi)
    abajo = bm.verts.new((cx, cy, perfil[0][1] - 0.0005))
    anillos = [[abajo]]
    for r, z in perfil:
        anillo = []
        for k in range(lados):
            a = 2 * math.pi * k / lados + fase
            rr = r * rng.uniform(1 - irregular, 1 + irregular)
            anillo.append(bm.verts.new((cx + math.cos(a) * rr, cy + math.sin(a) * rr, z)))
        anillos.append(anillo)
    anillos.append([bm.verts.new((cx, cy, perfil[-1][1] - 0.0008))])
    for i, (a, b) in enumerate(zip(anillos, anillos[1:])):
        if len(a) == 1:
            n = len(b)
            for k in range(n):
                cara(bm, capa, [a[0], b[(k + 1) % n], b[k]], color_por_tramo[i])
        else:
            anillos_a_caras(bm, capa, a, b, color_por_tramo[i])


def casco(bm, capa, puntos, color):
    """Envolvente convexa de una nube de puntos: la base de rocas, bloques, esquirlas y huesos."""
    vs = [bm.verts.new(p) for p in puntos]
    res = bmesh.ops.convex_hull(bm, input=vs)
    sobrantes = {v for v in res["geom_interior"] + res["geom_unused"] if isinstance(v, bmesh.types.BMVert)}
    if sobrantes:
        bmesh.ops.delete(bm, geom=list(sobrantes), context="VERTS")
    caras = [g for g in res["geom"] if isinstance(g, bmesh.types.BMFace) and g.is_valid]
    for f in caras:
        f[capa] = color
    return caras


def caja(bm, capa, centro, tam, color, matriz=None):
    """Caja recta, opcionalmente transformada: paneles, cartas, rayas de inscripción."""
    sx, sy, sz = (t / 2 for t in tam)
    puntos = [Vector((x * sx, y * sy, z * sz)) + Vector(centro)
              for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)]
    if matriz is not None:
        puntos = [matriz @ p for p in puntos]
    return casco(bm, capa, puntos, color)


def bloque_tosco(bm, capa, centro, tam, rng, color, rot=(0, 0, 0), mella=0.35):
    """Caja con las esquinas desbastadas al azar: piedra labrada a golpes."""
    sx, sy, sz = (t / 2 for t in tam)
    menor = min(sx, sy, sz)
    puntos = []
    for cx in (-1, 1):
        for cy in (-1, 1):
            for cz in (-1, 1):
                c = Vector((cx * sx, cy * sy, cz * sz))
                for eje in range(3):
                    d = Vector((0, 0, 0))
                    d[eje] = -(cx, cy, cz)[eje] * rng.uniform(0.15, mella) * menor * 2
                    ruido = Vector([rng.uniform(-0.08, 0.08) * menor for _ in range(3)])
                    puntos.append(c + d + ruido)
    m = Euler(rot).to_matrix()
    return casco(bm, capa, [m @ p + Vector(centro) for p in puntos], color)


def esquirla(bm, capa, base, alto, ancho, inclinacion, rng, color, rota=False):
    """Pincho de piedra o cristal: base cuadrada irregular y punta desplazada. rota=True lo deja truncado."""
    b = Vector(base)
    puntos = []
    for k in range(4):
        a = math.pi / 2 * k + rng.uniform(-0.4, 0.4)
        puntos.append(b + Vector((math.cos(a), math.sin(a), 0)) * ancho * rng.uniform(0.7, 1.1))
    punta = b + Vector((inclinacion[0], inclinacion[1], 1.0)) * alto
    if rota:
        for k in range(3):
            a = 2 * math.pi * k / 3 + rng.uniform(-0.5, 0.5)
            puntos.append(punta + Vector((math.cos(a), math.sin(a), rng.uniform(-0.3, 0.3))) * ancho * 0.6)
    else:
        puntos.append(punta)
    return casco(bm, capa, puntos, color)


def octaedro(bm, capa, centro, radio, color):
    c = Vector(centro)
    ejes = ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1))
    return casco(bm, capa, [c + Vector(e) * radio for e in ejes], color)


def elipsoide(bm, capa, centro, radios, n, rng, color, matriz=None, z_min=None):
    """Casco de n puntos sobre un elipsoide: cuerpos, cabezas, terrones."""
    puntos = []
    while len(puntos) < n:
        v = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))).normalized()
        if z_min is not None and v.z < z_min:
            continue
        puntos.append(Vector(centro) + Vector((v.x * radios[0], v.y * radios[1], v.z * radios[2])))
    if matriz is not None:
        puntos = [matriz @ p for p in puntos]
    return casco(bm, capa, puntos, color)


def grieta(bm, capa, puntos, radio, rng, color=NEGRO):
    """Grieta o surco tallado: tubo fino y oscuro medio hundido en una superficie."""
    tubo(bm, capa, [Vector(p) for p in puntos], [radio] * len(puntos), 4, color, rng, 0.0)


def eslabon(bm, capa, centro, eje_largo, eje_ancho, rx, ry, r, color, seg=6, seg_menor=3):
    """Eslabón ovalado de cadena: toro facetado alargado según eje_largo."""
    normal = eje_largo.cross(eje_ancho).normalized()
    anillos = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        c = centro + eje_largo * math.cos(a) * rx + eje_ancho * math.sin(a) * ry
        radial = (eje_largo * math.cos(a) * ry + eje_ancho * math.sin(a) * rx).normalized()
        anillos.append([bm.verts.new(c + (radial * math.cos(b) + normal * math.sin(b)) * r)
                        for b in (2 * math.pi * k / seg_menor + 0.3 for k in range(seg_menor))])
    for i in range(seg):
        anillos_a_caras(bm, capa, anillos[i], anillos[(i + 1) % seg], color)


def calavera(bm, capa, matriz, rng, color=HUESO):
    """Calavera de 2,2 cm: cráneo y cara por separado, cuencas y nariz como octaedros negros. Mira a -y."""
    elipsoide(bm, capa, (0, 0, 0.0105), (0.011, 0.013, 0.0105), 24, rng, color, matriz, z_min=-0.35)
    elipsoide(bm, capa, (0, -0.007, 0.004), (0.0075, 0.006, 0.005), 12, rng, color, matriz)
    for x in (-0.0045, 0.0045):
        octaedro(bm, capa, matriz @ Vector((x, -0.0115, 0.0105)), 0.0028, NEGRO)
    octaedro(bm, capa, matriz @ Vector((0, -0.0128, 0.0065)), 0.0015, NEGRO)


def hueso_largo(bm, capa, ext_a, ext_b, r, rng):
    """Hueso largo: caña facetada con dos nudos en cada extremo."""
    c = (ext_a + ext_b) / 2
    tubo(bm, capa, [ext_a, c + Vector((0, 0, r * 0.2)), ext_b], [r, r * 0.75, r], 5, HUESO, rng, 0.08)
    eje = (ext_b - ext_a).normalized()
    lado = eje.cross(Vector((0, 0, 1)))
    lado = lado.normalized() if lado.length > 1e-4 else Vector((1, 0, 0))
    for extremo, sentido in ((ext_a, -1), (ext_b, 1)):
        for s in (-1, 1):
            nudo = extremo + lado * s * r * 0.8 + eje * sentido * r * 0.4
            casco(bm, capa, [nudo + Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))).normalized()
                             * r * 1.25 for _ in range(9)], HUESO)


def verts_nuevos(bm, antes):
    return [v for v in bm.verts if v not in antes]


# ---------------------------------------------------------------------------------------------------------
# Cuña, grafo y terreno
# ---------------------------------------------------------------------------------------------------------
def dist_borde(x, y):
    """Distancia (con signo: negativa fuera) de (x, y) al perímetro del sector: borde del Altar, arco
    exterior y los dos lados rectos, que son rayos desde el centro del tablero a ±30 grados de -y."""
    c, s = math.cos(MEDIO_ANGULO), math.sin(MEDIO_ANGULO)
    return min(-y - R_INTERIOR, R_EXTERIOR - math.hypot(x, y), -(x * c + y * s), -(-x * c + y * s))


def dentro_cuna(x, y, margen=0.0):
    """True si (x, y) cae dentro del Dominio con al menos `margen` de holgura a todos los bordes."""
    return dist_borde(x, y) >= margen


def vec(p):
    return Vector(p) if len(p) == 3 else Vector((p[0], p[1], 0))


def catmull_rom(puntos, por_tramo=24):
    ps = [puntos[0]] + list(puntos) + [puntos[-1]]
    salida = []
    for i in range(1, len(ps) - 2):
        p0, p1, p2, p3 = (vec(ps[j]) for j in (i - 1, i, i + 1, i + 2))
        for s in range(por_tramo):
            t = s / por_tramo
            salida.append(0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                                 + (3 * p1 - p0 - 3 * p2 + p3) * t ** 3))
    salida.append(vec(puntos[-1]))
    return salida


def longitudes(denso):
    acum = [0.0]
    for a, b in zip(denso, denso[1:]):
        acum.append(acum[-1] + (b - a).length)
    return acum


def puntos_a_distancia(denso, distancias):
    """Posición y tangente sobre la polilínea densa a cada longitud de arco pedida (se recorta al final)."""
    acum = longitudes(denso)
    salida = []
    for d in distancias:
        d = min(d, acum[-1])
        for i in range(1, len(acum)):
            if acum[i] >= d - 1e-12:
                t = (d - acum[i - 1]) / max(acum[i] - acum[i - 1], 1e-9)
                salida.append((denso[i - 1].lerp(denso[i], t), (denso[i] - denso[i - 1]).normalized()))
                break
    return salida


def dist_polilinea(x, y, poli):
    p = Vector((x, y, 0))
    mejor = 1e9
    for a, b in zip(poli, poli[1:]):
        ab = b - a
        t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-12)))
        mejor = min(mejor, (a + ab * t - p).length)
    return mejor


def dist_caminos(x, y, caminos):
    # Sin caminos (el suelo liso del kit, ronda 6), todo está lejos de cualquier sendero.
    return min((dist_polilinea(x, y, c) for c in caminos), default=1e9)


def suave(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


class Ruido:
    """Suma de ondas planas con dirección, frecuencia y fase por semilla. Da valores en [-1, 1] aprox."""

    def __init__(self, rng, ondas, fmin, fmax):
        self.ondas = []
        for _ in range(ondas):
            a = rng.uniform(0, 2 * math.pi)
            self.ondas.append((math.cos(a), math.sin(a), rng.uniform(fmin, fmax), rng.uniform(0, 2 * math.pi)))

    def __call__(self, x, y):
        return sum(math.sin((dx * x + dy * y) * f + ph) for dx, dy, f, ph in self.ondas) / math.sqrt(len(self.ondas))


class Terreno:
    """Altura del suelo en cada punto: ruido bajo, algunos montículos lejos del grafo y zonas aplanadas
    donde se asientan las losas, los senderos, el trono y el brasero. Las piezas se colocan leyendo esta
    misma función."""

    def __init__(self, rng, grafo, zonas_planas):
        self.grafo = grafo
        self.zonas = zonas_planas  # (x, y, radio)
        self.relieve = Ruido(rng, 5, 15, 45)
        self.tono = Ruido(rng, 4, 18, 50)
        self.montes = []
        for _ in range(rng.randint(4, 6)):
            for _ in range(500):
                x, y = rng.uniform(-0.35, 0.35), rng.uniform(-R_EXTERIOR, -R_INTERIOR)
                if dentro_cuna(x, y, 0.03) and dist_caminos(x, y, grafo) > 0.08:
                    break
            self.montes.append((x, y, rng.uniform(0.004, 0.009), rng.uniform(0.03, 0.06)))

    def libre(self, x, y):
        # 0 en las zonas aplanadas, 1 lejos de ellas.
        w = suave(0.03, 0.065, dist_caminos(x, y, self.grafo))
        for zx, zy, r in self.zonas:
            w = min(w, suave(r * 0.6, r, math.hypot(x - zx, y - zy)))
        return w

    def altura(self, x, y):
        h = self.relieve(x, y) * RELIEVE
        for mx, my, a, s in self.montes:
            h += a * math.exp(-((x - mx) ** 2 + (y - my) ** 2) / (2 * s * s))
        w = self.libre(x, y)
        return SUELO_Z + h * w - 0.0008 * (1 - w)

    def altura_max(self, x, y, radio):
        """La altura más alta en un círculo: para asentar piezas planas (charcos) sin que asome el suelo."""
        return max(self.altura(x + math.cos(a) * r, y + math.sin(a) * r)
                   for r in (0, radio * 0.5, radio) for a in (i * math.pi / 4 for i in range(8)))


# ---------------------------------------------------------------------------------------------------------
# Peana y suelo
# ---------------------------------------------------------------------------------------------------------
def contorno_cuna(rng, paso=0.025, mordiscos=True):
    """Borde del Dominio en sentido antihorario visto desde arriba. Los lados rectos y el borde del Altar
    quedan limpios, porque encajan con los vecinos; el arco exterior puede llevar mordiscos de borde roto."""
    c, s = math.cos(MEDIO_ANGULO), math.sin(MEDIO_ANGULO)
    il = Vector((-R_INTERIOR * s / c, -R_INTERIOR, 0))
    ir = Vector((R_INTERIOR * s / c, -R_INTERIOR, 0))
    ol = Vector((-R_EXTERIOR * s, -R_EXTERIOR * c, 0))
    orr = Vector((R_EXTERIOR * s, -R_EXTERIOR * c, 0))
    puntos = []

    def recta(a, b):
        n = max(1, math.ceil((b - a).length / paso))
        puntos.extend(a.lerp(b, i / n) for i in range(n))

    recta(il, ol)
    a0 = math.radians(270) - MEDIO_ANGULO
    n = math.ceil(R_EXTERIOR * 2 * MEDIO_ANGULO / paso)
    for i in range(n):
        a = a0 + 2 * MEDIO_ANGULO * i / n
        r = R_EXTERIOR - (rng.uniform(0, 0.007) if i and mordiscos else 0)
        puntos.append(Vector((math.cos(a) * r, math.sin(a) * r, 0)))
    recta(orr, ir)
    recta(ir, il)
    return puntos


def desplazar_contorno(puntos, distancia):
    """Desplaza un polígono convexo en sentido antihorario (hacia fuera si distancia > 0), con inglete."""
    n = len(puntos)
    salida = []
    for i in range(n):
        a, p, b = puntos[i - 1], puntos[i], puntos[(i + 1) % n]
        n1 = Vector(((p - a).y, -(p - a).x, 0)).normalized()
        n2 = Vector(((b - p).y, -(b - p).x, 0)).normalized()
        m = (n1 + n2).normalized()
        salida.append(p + m * distancia / max(m.dot(n1), 0.3))
    return salida


def pieza_peana(rng):
    """Peana de miniatura: base negra bajo la tierra, con el canto visible y un chaflán facetado arriba.
    Sigue la cuña limpia, sin los mordiscos del borde de la tierra, que así asoman sobre ella. Sin tapas:
    la de arriba queda bajo la tierra y la de abajo sobre la mesa."""
    bm, capa = nuevo_bm()
    ideal = contorno_cuna(rng, paso=0.03, mordiscos=False)
    abajo = [bm.verts.new((p.x, p.y, 0.0)) for p in desplazar_contorno(ideal, PEANA_CANTO + 0.002)]
    canto = [bm.verts.new((p.x, p.y, PEANA_ALTO - 0.003)) for p in desplazar_contorno(ideal, PEANA_CANTO)]
    chaflan = [bm.verts.new((p.x, p.y, PEANA_ALTO)) for p in desplazar_contorno(ideal, PEANA_CANTO - 0.003)]
    dentro = [bm.verts.new((p.x, p.y, PEANA_ALTO)) for p in desplazar_contorno(ideal, -0.015)]
    anillos_a_caras(bm, capa, abajo, canto, NEGRO)
    anillos_a_caras(bm, capa, canto, chaflan, NEGRO)
    anillos_a_caras(bm, capa, chaflan, dentro, NEGRO)
    return bm


def lados_de_sendero(grafo, ancho, paso=0.006):
    """Puntos a lo largo de los dos bordes de cada sendero, dentro del Dominio, y las aristas que los unen.
    Se meten en la triangulación del suelo para que la franja de tierra clara tenga el borde limpio."""
    puntos, aristas = [], []
    for a, b in grafo:
        d = (b - a)
        if d.length < 1e-6:
            continue
        n = Vector((-d.y, d.x, 0)).normalized() * (ancho / 2)
        pasos = max(1, math.ceil(d.length / paso))
        for lado in (n, -n):
            anterior = None
            for i in range(pasos + 1):
                p = a.lerp(b, i / pasos) + lado
                if not dentro_cuna(p.x, p.y, 0.004):
                    anterior = None
                    continue
                puntos.append(Vector((p.x, p.y)))
                if anterior is not None:
                    aristas.append((anterior, len(puntos) - 1))
                anterior = len(puntos) - 1
    return puntos, aristas


def pieza_suelo(rng, terreno, grafo):
    """Tierra del Dominio: triangulación de Delaunay con el contorno y los bordes de los senderos como
    restricción. Los senderos del grafo son tierra clara; el resto, tierra oscura con alguna mancha clara
    lejos del grafo, para que ninguna mancha se lea como un sendero."""
    bm, capa = nuevo_bm()
    contorno = contorno_cuna(rng)
    bordes, aristas_sendero = lados_de_sendero(grafo, ANCHO_SENDERO)
    celda = SEPARACION_SUELO
    rejilla = {}

    def libre(x, y, d):
        i, j = int(x // celda), int(y // celda)
        return all((x - px) ** 2 + (y - py) ** 2 >= d * d
                   for di in (-1, 0, 1) for dj in (-1, 0, 1) for px, py in rejilla.get((i + di, j + dj), ()))

    def anotar(x, y):
        rejilla.setdefault((int(x // celda), int(y // celda)), []).append((x, y))

    for p in list(contorno) + bordes:
        anotar(p.x, p.y)
    interiores = []
    for _ in range(60000):
        x, y = rng.uniform(-0.38, 0.38), rng.uniform(-R_EXTERIOR, -R_INTERIOR)
        if dentro_cuna(x, y, celda * 0.45) and libre(x, y, celda):
            anotar(x, y)
            interiores.append(Vector((x, y)))

    n = len(contorno)
    coords = [p.to_2d() for p in contorno] + bordes + interiores
    aristas = [(i, (i + 1) % n) for i in range(n)] + [(a + n, b + n) for a, b in aristas_sendero]
    salida = delaunay_2d_cdt(coords, aristas, [list(range(n))], 1, 1e-7)
    vs, caras_cdt, originales = salida[0], salida[2], salida[3]
    verts = [bm.verts.new((v.x, v.y, terreno.altura(v.x, v.y))) for v in vs]
    de_original = {}
    for i, orig in enumerate(originales):
        for o in orig:
            de_original[o] = i
    for f in caras_cdt:
        a, b, c = (verts[i] for i in f)
        if ((b.co - a.co).cross(c.co - a.co)).z < 0:
            b, c = c, b
        cx, cy = (a.co.x + b.co.x + c.co.x) / 3, (a.co.y + b.co.y + c.co.y) / 3
        d = dist_caminos(cx, cy, grafo)
        if d < ANCHO_SENDERO / 2:
            color = TIERRA_CLARA
        elif d > 0.08 and terreno.tono(cx, cy) + rng.uniform(-0.2, 0.2) > 0.55:
            color = TIERRA_CLARA
        else:
            color = TIERRA_OSCURA
        cara(bm, capa, [a, b, c], color)
    # Faldón hasta la peana por todo el contorno (se mete un poco en ella para que no quede rendija).
    arriba = [verts[de_original[i]] for i in range(n)]
    abajo = [bm.verts.new((v.co.x, v.co.y, PEANA_ALTO - 0.002)) for v in arriba]
    for i in range(n):
        cara(bm, capa, [abajo[i], abajo[(i + 1) % n], arriba[(i + 1) % n], arriba[i]], TIERRA_OSCURA)
    return bm


# ---------------------------------------------------------------------------------------------------------
# Casillas. Todas comparten canto de piedra clara y reborde biselado de hueso, que es lo que las hace
# leerse como casilla; cada tipo cambia el contorno y la cara. Son lo más limpio del Dominio.
# ---------------------------------------------------------------------------------------------------------
def contorno_superelipse(rng, lados, semilargo, semiancho, exponente, muescas=()):
    """Contorno de losa: superelipse (exponente 2 es un círculo, 4 un cuadrado de esquinas romas)."""
    puntos = []
    for k in range(lados):
        a = 2 * math.pi * k / lados + math.pi / lados + rng.uniform(-0.06, 0.06)
        c, s = math.cos(a), math.sin(a)
        r = 1 / (abs(c) ** exponente + abs(s) ** exponente) ** (1 / exponente) * rng.uniform(0.96, 1.03)
        if k in muescas:
            r *= rng.uniform(0.82, 0.88)
        puntos.append(Vector((c * r * semilargo, s * r * semiancho, 0)))
    return puntos


def contorno_poligono(rng, esquinas, divisiones):
    """Contorno a partir de esquinas (antihorario), con cada lado partido en `divisiones` tramos."""
    puntos = []
    for i, e in enumerate(esquinas):
        a, b = Vector((*e, 0)), Vector((*esquinas[(i + 1) % len(esquinas)], 0))
        for k in range(divisiones):
            p = a.lerp(b, k / divisiones)
            puntos.append(p + Vector((rng.uniform(-0.0006, 0.0006), rng.uniform(-0.0006, 0.0006), 0)))
    return puntos


def anillo_de(bm, rng, contorno, muescas=()):
    """Devuelve una función que crea un anillo del contorno metido `entra` metros hacia el centro."""
    def anillo(entra, z, jit=0.0, baja=0.0):
        vs = []
        for k, p in enumerate(contorno):
            q = p * (1 - entra / p.length)
            vs.append(bm.verts.new((q.x, q.y, z + rng.uniform(-jit, jit) - (baja if k in muescas else 0))))
        return vs
    return anillo


def losa_canto(bm, capa, anillo, alto=0.0055, color_canto=PIEDRA_CLARA):
    """Parte común de toda casilla: canto, bisel y reborde de hueso, y la cara de abajo. Devuelve el anillo
    interior del reborde, desde donde cada tipo construye su cara."""
    r_base = anillo(0, -0.004)
    r_arista = anillo(0, alto - 0.0015, baja=0.0018)
    r_bisel = anillo(0.0016, alto, baja=0.0012)
    r_reborde = anillo(0.0056, alto)
    anillos_a_caras(bm, capa, r_base, r_arista, color_canto)
    anillos_a_caras(bm, capa, r_arista, r_bisel, HUESO)
    anillos_a_caras(bm, capa, r_bisel, r_reborde, HUESO)
    cara(bm, capa, list(reversed(r_base)), PIEDRA_MEDIA)
    return r_reborde


def cara_facetada(bm, capa, anillo, r_reborde, rng, alto=0.0055):
    """Cara interior de losa normal: escalón hacia dentro y dos anillos de facetas hasta el centro."""
    r_esc = anillo(0.0068, alto - 0.0021)
    r_f1 = anillo(0.012, alto - 0.0019, 0.0002)
    r_f2 = anillo(0.019, alto - 0.0017, 0.0002)
    centro = [bm.verts.new((rng.uniform(-0.002, 0.002), rng.uniform(-0.002, 0.002), alto - 0.0019))]
    anillos_a_caras(bm, capa, r_reborde, r_esc, PIEDRA_CLARA)
    anillos_a_caras(bm, capa, r_esc, r_f1, PIEDRA_CLARA)
    anillos_a_caras(bm, capa, r_f1, r_f2, PIEDRA_CLARA)
    anillos_a_caras(bm, capa, r_f2, centro, PIEDRA_CLARA)


def construir_losa(bm, capa, rng, con_grieta=True):
    """Losa normal dentro de un bmesh existente: cuadrada de esquinas romas, con dos muescas de borde roto
    y una grieta que nace en una de ellas (sin cruzar la losa: los símbolos son del Nodo)."""
    lados = 16
    muescas = set(rng.sample(range(lados), 2))
    contorno = contorno_superelipse(rng, lados, LOSA_MEDIO, LOSA_MEDIO, 4, muescas)
    anillo = anillo_de(bm, rng, contorno, muescas)
    cara_facetada(bm, capa, anillo, losa_canto(bm, capa, anillo), rng)
    if con_grieta:
        k = next(iter(muescas))
        a = math.atan2(contorno[k].y, contorno[k].x) + rng.uniform(-0.3, 0.3)
        dentro = Vector((math.cos(a), math.sin(a), 0))
        lado = Vector((-dentro.y, dentro.x, 0))
        pts = []
        for i, r in enumerate((0.019, 0.014, 0.0095, 0.006)):
            p = dentro * r + lado * (rng.uniform(-0.0025, 0.0025) if i else 0)
            pts.append((p.x, p.y, 0.0037))
        grieta(bm, capa, pts, 0.0006, rng)


def pieza_losa(rng):
    """Casilla normal. Una sola malla para todas las casillas normales enteras."""
    bm, capa = nuevo_bm()
    construir_losa(bm, capa, rng)
    return bm


def pieza_losa_partida(rng):
    """Casilla normal partida en dos por una línea que la cruza: las dos mitades se separan un milímetro,
    una se hunde y se ladea un poco, y el corte queda a la vista en piedra media. Misma silueta y mismo
    reborde que las enteras, para que se siga leyendo como casilla."""
    bm, capa = nuevo_bm()
    semilla = rng.random()
    a = rng.uniform(0, math.pi)
    normal = Vector((math.cos(a), math.sin(a), 0))
    co = normal * rng.uniform(-0.007, 0.007)
    for lado in (1, -1):
        antes = set(bm.verts)
        construir_losa(bm, capa, random.Random(semilla), con_grieta=False)
        nuevos = verts_nuevos(bm, antes)
        aristas = list({e for v in nuevos for e in v.link_edges})
        caras = list({f for v in nuevos for f in v.link_faces})
        bmesh.ops.bisect_plane(bm, geom=nuevos + aristas + caras, dist=1e-7, plane_co=co,
                               plane_no=normal * lado, clear_outer=True)
        bordes = [e for e in bm.edges if e.is_boundary]
        for f in bmesh.ops.holes_fill(bm, edges=bordes, sides=0)["faces"]:
            f[capa] = PIEDRA_MEDIA
        mitad = verts_nuevos(bm, antes)
        bmesh.ops.translate(bm, vec=-normal * lado * 0.0008, verts=mitad)
        if lado < 0:
            eje = normal.cross(Vector((0, 0, 1)))
            bmesh.ops.rotate(bm, verts=mitad, cent=co, matrix=Matrix.Rotation(-0.05, 3, eje))
            bmesh.ops.translate(bm, vec=(0, 0, -0.0007), verts=mitad)
    return bm


def pieza_losa_nicho(rng, con_respaldo=False):
    """Casilla del Nicho, al final del ramal: tarima con forma de ataúd, más alta que las demás, con un
    hueco negro en el centro. La cabecera (+x) apunta al trono. Con con_respaldo (ronda 6, "el Nicho es
    el trono"): un respaldo bajo de piedra pegado a la cabecera, con el hueco del emblema mirando a la
    losa y dos remates; la ficha se posa en la tarima y "se sienta" mirando al Altar."""
    bm, capa = nuevo_bm()
    if con_respaldo:
        pm = PIEDRA_MEDIA
        antes = set(bm.verts)
        # Bajo (2,6 cm de piedra más los remates): desde el asiento del dueño se sigue viendo la tarima.
        bloque_tosco(bm, capa, (0.0465, 0, 0.019), (0.012, 0.056, 0.026), rng, pm, (0, 0, 0), 0.2)
        bloque_tosco(bm, capa, (0.0455, 0, 0.034), (0.010, 0.036, 0.008), rng, pm, (0, 0, 0.03), 0.25)
        caja(bm, capa, (0.0398, 0, 0.021), (0.0015, 0.020, 0.014), NEGRO)
        esquirla(bm, capa, (0.046, -0.026, 0.028), 0.020, 0.0042, (0.03, -0.05), rng, pm)
        esquirla(bm, capa, (0.046, 0.026, 0.028), 0.014, 0.0042, (0.03, 0.05), rng, pm, rota=True)
        runa(bm, capa, Vector((0.0399, -0.0215, 0.011)), Vector((0, 1, 0)), Vector((0, 0, 1)), RUNAS[0], 0.006, rng)
        # Ligeramente echado hacia atrás, como el trono.
        bmesh.ops.rotate(bm, verts=verts_nuevos(bm, antes), cent=(0.041, 0, 0.006),
                         matrix=Euler((0, 0.10, 0)).to_matrix())
    esquinas = [(-0.036, 0.0), (-0.024, -0.025), (0.024, -0.031), (0.041, 0.0), (0.024, 0.031), (-0.024, 0.025)]
    contorno = contorno_poligono(rng, esquinas, 3)
    anillo = anillo_de(bm, rng, contorno)
    alto = 0.013
    r = losa_canto(bm, capa, anillo, alto, color_canto=PIEDRA_MEDIA)
    r_labio = anillo(0.0095, alto)
    r_pared = anillo(0.0105, alto - 0.007)
    r_fondo = anillo(0.0135, alto - 0.0085)
    fondo = [bm.verts.new((0.002, 0, alto - 0.009))]
    anillos_a_caras(bm, capa, r, r_labio, PIEDRA_CLARA)
    anillos_a_caras(bm, capa, r_labio, r_pared, PIEDRA_MEDIA)
    anillos_a_caras(bm, capa, r_pared, r_fondo, NEGRO)
    anillos_a_caras(bm, capa, r_fondo, fondo, NEGRO)
    for y0 in (-0.018, 0.02):
        pts = [(-0.028 + 0.008 * i, y0 + rng.uniform(-0.002, 0.002), alto + 0.0001) for i in range(3)]
        grieta(bm, capa, pts, 0.0006, rng)
    return bm


def pieza_losa_fuente(rng):
    """Casilla de la Fuente de Poder (ronda 6): redonda y algo mayor, con la cara libre para la ficha y un
    filo de cian tenue junto al reborde que dice de qué estructura es. El pilón va al lado (pieza_pilon),
    tocando su borde por el lado contrario al camino (GDD 01, decisión del 30/09/2026)."""
    bm, capa = nuevo_bm()
    contorno = contorno_superelipse(rng, 20, 0.034, 0.034, 2)
    anillo = anillo_de(bm, rng, contorno)
    cara_facetada(bm, capa, anillo, losa_canto(bm, capa, anillo), rng)
    filo_fuera = anillo(0.0058, 0.0059)
    filo_dentro = anillo(0.0076, 0.0059)
    anillos_a_caras(bm, capa, filo_fuera, filo_dentro, CIAN_TENUE)
    return bm


def pieza_pilon(rng):
    """Pilón de la Fuente de Poder (ronda 6): estructura aparte, sobre una base de piedra de 4,4 cm, con el
    agua negra y un toque de cian tenue en el centro, mucho más débil que el brasero. La runa que flota
    encima es otra pieza (pieza_runa_flotante), para poder quitarla cuando la reserva se agota."""
    bm, capa = nuevo_bm()
    contorno = contorno_superelipse(rng, 14, 0.022, 0.022, 2)
    anillo = anillo_de(bm, rng, contorno)
    r = losa_canto(bm, capa, anillo, alto=0.004, color_canto=PIEDRA_MEDIA)
    r_pie = anillo(0.0062, 0.0060)
    r_pared = anillo(0.0068, 0.0150)
    r_labio = anillo(0.0094, 0.0160)
    r_dentro = anillo(0.0106, 0.0118)
    r_agua = anillo(0.0160, 0.0119)
    brillo = [bm.verts.new((0, 0, 0.0120))]
    anillos_a_caras(bm, capa, r, r_pie, PIEDRA_CLARA)
    anillos_a_caras(bm, capa, r_pie, r_pared, PIEDRA_MEDIA)
    anillos_a_caras(bm, capa, r_pared, r_labio, PIEDRA_CLARA)
    anillos_a_caras(bm, capa, r_labio, r_dentro, PIEDRA_MEDIA)
    anillos_a_caras(bm, capa, r_dentro, r_agua, NEGRO)
    anillos_a_caras(bm, capa, r_agua, brillo, CIAN_TENUE)
    for i in range(3):
        a = 2 * math.pi * i / 3 + 0.4
        o = Vector((math.cos(a) * 0.0117, math.sin(a) * 0.0117, 0.006))
        runa(bm, capa, o, Vector((-math.sin(a), math.cos(a), 0)), Vector((0, 0, 1)), RUNAS[i], 0.0055, rng)
    return bm


def pieza_runa_flotante(rng):
    """Runa de la Fuente: un octaedro de cristal cian con el origen en su centro. La vista lo coloca
    flotando sobre el pilón mientras la Fuente tiene reserva, y lo quita cuando se agota."""
    bm, capa = nuevo_bm()
    puntos = [Vector(v) for v in ((0.0055, 0, 0), (-0.0055, 0, 0), (0, 0.0055, 0), (0, -0.0055, 0),
                                   (0, 0, 0.0095), (0, 0, -0.0095))]
    m = Euler((0, 0, rng.uniform(0, math.pi))).to_matrix()
    casco(bm, capa, [m @ p for p in puntos], CIAN)
    return bm


def pieza_losa_nodo(rng):
    """Casilla del Nodo de Poder: hexagonal, con las puntas en la dirección del camino, y una marca
    tallada en la cara (círculo con un triángulo dentro)."""
    bm, capa = nuevo_bm()
    esquinas = [(-0.038, 0.0), (-0.02, -0.029), (0.02, -0.029), (0.038, 0.0), (0.02, 0.029), (-0.02, 0.029)]
    contorno = contorno_poligono(rng, esquinas, 3)
    anillo = anillo_de(bm, rng, contorno)
    cara_facetada(bm, capa, anillo, losa_canto(bm, capa, anillo), rng)
    z = 0.0037
    circulo = [(math.cos(a) * 0.0125, math.sin(a) * 0.0125, z)
               for a in (2 * math.pi * i / 14 for i in range(15))]
    grieta(bm, capa, circulo, 0.0009, rng)
    triangulo = [(math.cos(a) * 0.0105, math.sin(a) * 0.0105, z)
                 for a in (2 * math.pi * i / 3 for i in range(4))]
    for p, q in zip(triangulo, triangulo[1:]):
        grieta(bm, capa, [p, q], 0.0009, rng)
    return bm


def pieza_sendero(rng):
    """Tramo de sendero de unos 2,4 cm a lo largo de x: dos hileras de piedrecillas que marcan los bordes
    de la franja de tierra clara. Se pone en el hueco entre dos losas conectadas y a lo largo de las
    conexiones que salen del Dominio."""
    bm, capa = nuevo_bm()
    for lado in (-1, 1):
        for x in (-0.0078, 0.0, 0.0078):
            c = Vector((x + rng.uniform(-0.0006, 0.0006), lado * (ANCHO_SENDERO / 2 + 0.0008), 0))
            e = rng.uniform(0.0026, 0.0034)
            puntos = []
            for k in range(8):
                a = 2 * math.pi * k / 8 + rng.uniform(-0.3, 0.3)
                r = e * rng.uniform(0.75, 1.0)
                puntos.append(c + Vector((math.cos(a) * r, math.sin(a) * r * 0.85, rng.uniform(0.0012, 0.002))))
                puntos.append(c + Vector((math.cos(a) * r * 0.9, math.sin(a) * r * 0.75, -0.001)))
            casco(bm, capa, puntos, PIEDRA_CLARA)
    return bm


# ---------------------------------------------------------------------------------------------------------
# Estructuras del kit (ronda 6): el Altar, las de los Nodos y el Sello. Cada una con el origen en su
# centro y la base en z = 0; la vista las coloca junto a la losa, por el lado contrario al camino.
# ---------------------------------------------------------------------------------------------------------
ALTAR_RADIO = R_INTERIOR / math.cos(MEDIO_ANGULO)  # vértices del hexágono: su apotema es R_INTERIOR


def pieza_altar(rng):
    """El Altar: tarima hexagonal de piedra en el centro de la mesa, que encaja con los seis Dominios por
    su apotema. Dos gradas, la de arriba con seis huecos tallados (los de las seis runas del desafío
    final), y en el centro el trono vacío del Warden: un sitial tosco con una hornacina negra y dos agujas
    rotas, lo más alto de la mesa. Mira hacia -y. La base va sobre la mesa (z = 0) y la primera grada
    queda a la altura del suelo de los Dominios."""
    bm, capa = nuevo_bm()
    pm = PIEDRA_MEDIA
    esquinas = [(math.cos(a) * ALTAR_RADIO, math.sin(a) * ALTAR_RADIO)
                for a in (math.pi / 6 + math.pi / 3 * i for i in range(6))]
    contorno = contorno_poligono(rng, esquinas, 5)
    anillo = anillo_de(bm, rng, contorno)
    z1 = SUELO_Z + 0.010
    r = losa_canto(bm, capa, anillo, alto=z1, color_canto=pm)
    r_grada = anillo(0.062, z1)
    anillos_a_caras(bm, capa, r, r_grada, PIEDRA_CLARA)
    z2 = z1 + 0.011
    r_sube = anillo(0.0625, z2 - 0.0015)
    r_bisel = anillo(0.0645, z2)
    r_alto = anillo(0.115, z2)
    anillos_a_caras(bm, capa, r_grada, r_sube, pm)
    anillos_a_caras(bm, capa, r_sube, r_bisel, HUESO)
    anillos_a_caras(bm, capa, r_bisel, r_alto, PIEDRA_CLARA)
    anillos_a_caras(bm, capa, r_alto, [bm.verts.new((0, 0, z2))], PIEDRA_CLARA)
    # Seis huecos para las runas en la grada de abajo, uno frente a cada Dominio, y grietas entre ellos.
    for i in range(6):
        a = math.pi / 3 * i
        c = Vector((math.cos(a) * 0.138, math.sin(a) * 0.138, z1))
        circulo = [(c.x + math.cos(t) * 0.008, c.y + math.sin(t) * 0.008, z1) for t in (2 * math.pi * k / 10 for k in range(11))]
        grieta(bm, capa, circulo, 0.0008, rng)
        a2 = a + math.pi / 6
        pts = [(math.cos(a2) * rr + rng.uniform(-0.004, 0.004), math.sin(a2) * rr + rng.uniform(-0.004, 0.004), z1)
               for rr in (0.095, 0.115, 0.135, 0.155)]
        grieta(bm, capa, pts, 0.0007, rng)
    # El trono del Warden, vacío: sitial, hornacina negra, respaldo alto y dos agujas rotas.
    zt = z2
    bloque_tosco(bm, capa, (0, 0.006, zt + 0.012), (0.056, 0.046, 0.024), rng, pm, (0, 0, 0.02), 0.25)
    bloque_tosco(bm, capa, (0, 0.024, zt + 0.062), (0.050, 0.012, 0.076), rng, pm, (0, 0, -0.02), 0.2)
    caja(bm, capa, (0, 0.0175, zt + 0.064), (0.026, 0.002, 0.056), NEGRO)
    bloque_tosco(bm, capa, (-0.0105, 0.024, zt + 0.108), (0.025, 0.012, 0.016), rng, pm, (0, -0.4, 0))
    # Las agujas: la izquierda entera, la más alta de la mesa (por encima de los tronos de los Elegidos).
    esquirla(bm, capa, (-0.029, 0.026, zt + 0.02), 0.185, 0.0075, (-0.03, 0.02), rng, pm)
    esquirla(bm, capa, (0.029, 0.026, zt + 0.02), 0.110, 0.0075, (0.05, 0.02), rng, pm, rota=True)
    esquirla(bm, capa, (0.004, 0.026, zt + 0.112), 0.050, 0.005, (0.1, 0.0), rng, pm)
    for i, x in enumerate((-0.018, -0.006, 0.006, 0.018)):
        runa(bm, capa, Vector((x, -0.0172, zt + 0.004)), Vector((1, 0, 0)), Vector((0, 0, 1)), RUNAS[i], 0.0065, rng)
    calavera(bm, capa, Matrix.Translation((0.002, 0.0168, zt + 0.098)) @ Matrix.Scale(0.42, 4), rng, pm)
    # Escombros de las agujas, caídos en la grada de arriba.
    for _ in range(4):
        a = rng.uniform(0, math.tau)
        c = (math.cos(a) * rng.uniform(0.05, 0.1), math.sin(a) * rng.uniform(0.05, 0.1), zt + 0.003)
        e = rng.uniform(0.005, 0.009)
        bloque_tosco(bm, capa, c, (e, e * 0.8, e * 0.6), rng, pm, (rng.uniform(0, 1), rng.uniform(0, 1), 0))
    return bm


def pieza_nodo_planar(rng):
    """Estructura del Nodo Planar: arco de piedra de dos pilares torcidos con un velo de luz cian tenue
    dentro (el velo se enciende cuando alguien puede saltar; la vista cambia su material). El arco se
    abre a lo largo de x, así que, puesto junto a la losa por el lado contrario al camino, el velo mira a
    la losa. Emerge del suelo en el Despertar: hasta entonces la vista no lo pone."""
    bm, capa = nuevo_bm()
    pm = PIEDRA_MEDIA
    semi, alto = 0.027, 0.052
    for lado in (-1, 1):
        base = Vector((lado * semi, 0, -0.006))
        puntos = [base, base + Vector((lado * 0.002, 0.001, alto * 0.5)), base + Vector((0, 0, alto))]
        tubo(bm, capa, puntos, [0.0075, 0.0068, 0.0062], 5, pm, rng, 0.14, tapa=True)
        bloque_tosco(bm, capa, (lado * semi, 0, 0.004), (0.02, 0.018, 0.012), rng, pm, (0, 0, rng.uniform(-0.3, 0.3)))
    arco = [Vector((semi * math.cos(math.pi - math.pi * i / 8), 0, alto + 0.012 + (semi - 0.004) * math.sin(math.pi * i / 8)))
            for i in range(9)]
    arco[0].x, arco[-1].x = -semi, semi
    arco[0].z = arco[-1].z = alto - 0.004
    tubo(bm, capa, arco, [0.0058] * 9, 5, pm, rng, 0.14)
    bloque_tosco(bm, capa, (0, 0, alto + 0.012 + semi - 0.004), (0.02, 0.012, 0.012), rng, pm, (0, 0, 0.1))
    # Velo: una lámina delgada dentro del arco, con las dos caras.
    interior = [Vector((-semi + 0.007, 0, 0.0)), Vector((-semi + 0.007, 0, alto - 0.004))]
    interior += [Vector(((semi - 0.009) * math.cos(math.pi - math.pi * i / 8), 0,
                         alto + 0.008 + (semi - 0.011) * math.sin(math.pi * i / 8))) for i in range(1, 8)]
    interior += [Vector((semi - 0.007, 0, alto - 0.004)), Vector((semi - 0.007, 0, 0.0))]
    for y in (-0.0006, 0.0006):
        vs = [bm.verts.new(p + Vector((0, y, 0))) for p in interior]
        cara(bm, capa, vs if y > 0 else list(reversed(vs)), CIAN_TENUE)
    return bm


def pieza_nodo_duelo(rng):
    """Estructura del Nodo de Duelo: dos espadas clavadas y cruzadas en un montículo de tierra removida,
    con las empuñaduras de madera y las guardas de hierro. Emerge en el Despertar."""
    bm, capa = nuevo_bm()
    elipsoide(bm, capa, (0, 0, -0.003), (0.03, 0.026, 0.013), 16, rng, TIERRA_CLARA, z_min=0.0)
    for lado in (-1, 1):
        m = Matrix.Translation((lado * 0.012, 0, 0.006)) @ Euler((0.12 * lado, lado * 0.55, lado * 0.25)).to_matrix().to_4x4()
        # Hoja (hierro claro), guarda y pomo (hierro negro), empuñadura (madera).
        casco(bm, capa, [m @ Vector(p) for p in ((-0.006, -0.0012, 0.0), (0.006, -0.0012, 0.0), (0.006, 0.0012, 0.0),
                                                 (-0.006, 0.0012, 0.0), (-0.0015, -0.0007, 0.052), (0.0015, -0.0007, 0.052),
                                                 (0.0015, 0.0007, 0.052), (-0.0015, 0.0007, 0.052), (0, 0, 0.06))],
              PIEDRA_CLARA)
        caja(bm, capa, (0, 0, 0.052), (0.024, 0.0045, 0.0045), NEGRO, m)
        tubo(bm, capa, [m @ Vector((0, 0, 0.054)), m @ Vector((0, 0, 0.070))], [0.0032, 0.003], 5, MADERA, rng, 0.1)
        octaedro(bm, capa, m @ Vector((0, 0, 0.073)), 0.0042, NEGRO)
    return bm


def pieza_nodo_oraculo(rng):
    """Estructura del Nodo del Oráculo: pedestal de piedra con tres garras que sujetan un orbe de cristal
    cian tenue (el Oráculo se consulta manteniéndolo pulsado; la vista anima el pulso). Emerge en el
    Despertar."""
    bm, capa = nuevo_bm()
    pm = PIEDRA_MEDIA
    torno(bm, capa, [(0.018, -0.004), (0.017, 0.006), (0.011, 0.012), (0.009, 0.040), (0.014, 0.048), (0.012, 0.052)],
          7, [pm, PIEDRA_CLARA, pm, pm, pm, PIEDRA_CLARA, pm], rng)
    for i in range(3):
        a = 2 * math.pi * i / 3 + rng.uniform(-0.2, 0.2)
        d = Vector((math.cos(a), math.sin(a), 0))
        puntos = [d * 0.010 + Vector((0, 0, 0.050)), d * 0.016 + Vector((0, 0, 0.062)), d * 0.012 + Vector((0, 0, 0.074))]
        tubo(bm, capa, puntos, [0.003, 0.0026, 0], 4, NEGRO, rng, 0.1, tapa=True)
    elipsoide(bm, capa, (0, 0, 0.065), (0.0125, 0.0125, 0.0125), 36, rng, CIAN_TENUE)
    for i in range(2):
        runa(bm, capa, Vector((0.0095 * math.cos(i * 2.1), 0.0095 * math.sin(i * 2.1), 0.016)),
             Vector((-math.sin(i * 2.1), math.cos(i * 2.1), 0)), Vector((0, 0, 1)), RUNAS[i + 1], 0.006, rng)
    return bm


def pieza_sello(rng):
    """Sello del Warden sobre una casilla: dos cadenas cruzadas en diagonal, tensas entre cuatro estacas de
    hierro clavadas en las esquinas de la losa. Con el origen en el centro de la losa y la base a la
    altura de su cara. La vista lo pone y lo quita con el Sello."""
    bm, capa = nuevo_bm()
    d = LOSA_MEDIO + 0.006
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        base = Vector((sx * d, sy * d, 0))
        tubo(bm, capa, [base + Vector((0, 0, 0.022)), base + Vector((sx * 0.0005, sy * 0.0005, 0.010)), base + Vector((0, 0, -0.006))],
             [0.0036, 0.0033, 0], 5, NEGRO, rng, 0.1, tapa=True)
    for sx in (-1, 1):
        curva = catmull_rom([(-d, -sx * d, 0.016), (-d * 0.5, -sx * d * 0.5, 0.012 + rng.uniform(-0.002, 0.002)),
                             (0, 0, 0.011), (d * 0.5, sx * d * 0.5, 0.012 + rng.uniform(-0.002, 0.002)), (d, sx * d, 0.016)], 16)
        paso = 0.0095
        n = int(longitudes(curva)[-1] / paso)
        for i, (c, t) in enumerate(puntos_a_distancia(curva, [paso * (i + 0.5) for i in range(n)])):
            lateral = t.cross(Vector((0, 0, 1)))
            lateral = lateral.normalized() if lateral.length > 1e-4 else Vector((0, 1, 0))
            eje_ancho = lateral if i % 2 == 0 else lateral.cross(t).normalized()
            eslabon(bm, capa, c, t, eje_ancho, 0.0062, 0.0040, 0.0013, PIEDRA_MEDIA)
    octaedro(bm, capa, (0, 0, 0.013), 0.006, NEGRO)
    return bm


# ---------------------------------------------------------------------------------------------------------
# Escenografía. Cada función devuelve un bmesh en coordenadas locales, con el origen en el centro de la base.
# ---------------------------------------------------------------------------------------------------------
INCLINACION_HORNACINA = Euler((-0.08, 0.05, 0.05))
CENTRO_HORNACINA = Vector((0, 0.02, 0.03))
PERCHA_CUERVO = Vector((0.019, 0.024, 0.0795))  # cima del pilar derecho partido, antes de inclinarlo


def runa(bm, capa, origen, eje_u, eje_v, trazos, tam, rng):
    """Runa tallada: trazos rectos en coordenadas (u, v) de 0 a 1 sobre un plano, como surcos negros."""
    for (u0, v0), (u1, v1) in trazos:
        p = origen + eje_u * (u0 * tam) + eje_v * (v0 * tam)
        q = origen + eje_u * (u1 * tam) + eje_v * (v1 * tam)
        grieta(bm, capa, [p, q], 0.0005, rng)


RUNAS = [  # cuatro glifos propios, angulosos, sin parecido con alfabetos reales
    [((0.5, 0), (0.5, 1)), ((0.5, 0.6), (0.1, 1)), ((0.5, 0.35), (0.9, 0.7))],
    [((0.2, 0), (0.2, 1)), ((0.2, 1), (0.8, 0.5)), ((0.8, 0.5), (0.2, 0.3))],
    [((0.1, 0), (0.5, 1)), ((0.5, 1), (0.9, 0)), ((0.3, 0.45), (0.7, 0.45))],
    [((0.5, 0), (0.5, 1)), ((0.15, 0.2), (0.85, 0.8)), ((0.15, 0.8), (0.5, 0.55))],
]


def pieza_trono(rng):
    """Trono de piedra tosco, mirando hacia -y: escalinata de sillares sueltos (uno partido) con runas
    talladas en el frente, asiento y una hornacina de pilares de tambores con el nicho negro al fondo y
    una calavera tallada en la clave. El respaldo está roto: el pilar derecho ha perdido su último tambor
    y el arco derecho, que están caídos junto a la escalinata; la losa trasera ha perdido la esquina de
    arriba a la derecha. Agujas a los lados y grietas en las piedras."""
    bm, capa = nuevo_bm()
    pm = PIEDRA_MEDIA
    # Escalinata: tres gradas de sillares; en la de abajo, el de la derecha partido en dos.
    bloque_tosco(bm, capa, (-0.026, 0.004, 0.005), (0.025, 0.074, 0.010), rng, pm, (0, 0, 0.03))
    bloque_tosco(bm, capa, (0.0, 0.003, 0.005), (0.026, 0.072, 0.0105), rng, pm, (0, 0, -0.02))
    bloque_tosco(bm, capa, (0.021, 0.004, 0.0045), (0.014, 0.072, 0.009), rng, pm, (0.02, -0.03, -0.05))
    bloque_tosco(bm, capa, (0.034, 0.006, 0.0042), (0.011, 0.068, 0.0085), rng, pm, (0.03, 0.05, 0.08))
    bloque_tosco(bm, capa, (-0.016, 0.009, 0.015), (0.032, 0.058, 0.010), rng, pm, (0, 0, -0.04))
    bloque_tosco(bm, capa, (0.017, 0.010, 0.015), (0.033, 0.056, 0.010), rng, pm, (0, 0, 0.05))
    bloque_tosco(bm, capa, (-0.013, 0.012, 0.025), (0.026, 0.046, 0.010), rng, pm, (0, 0, 0.03))
    bloque_tosco(bm, capa, (0.013, 0.012, 0.025), (0.026, 0.046, 0.010), rng, pm, (0, 0, -0.03))
    for pts in (((-0.034, -0.02, 0.0098), (-0.028, -0.01, 0.0099), (-0.030, 0.002, 0.0099)),
                ((0.006, -0.012, 0.0199), (0.014, -0.006, 0.02), (0.024, -0.009, 0.0199))):
        grieta(bm, capa, pts, 0.0006, rng)
    # Runas en el frente de la segunda grada, una hilera a lo ancho.
    for i, x in enumerate((-0.026, -0.012, 0.004, 0.018)):
        runa(bm, capa, Vector((x, -0.0203, 0.0118)), Vector((1, 0, 0)), Vector((0, 0, 1)), RUNAS[i], 0.0065, rng)
    # Asiento.
    bloque_tosco(bm, capa, (0, 0.004, 0.035), (0.036, 0.028, 0.012), rng, pm, (0.03, 0, 0.02))

    # Hornacina: se inclina hacia atrás y se tuerce un poco, como si el suelo hubiera cedido.
    antes = set(bm.verts)
    bloque_tosco(bm, capa, (0, 0.034, 0.060), (0.054, 0.010, 0.060), rng, pm, mella=0.2)
    bloque_tosco(bm, capa, (-0.013, 0.0345, 0.1035), (0.028, 0.010, 0.027), rng, pm, (0.02, 0.06, 0.03), 0.25)
    caja(bm, capa, (0, 0.028, 0.069), (0.027, 0.002, 0.078), NEGRO)
    for z0, z1 in ((0.030, 0.055), (0.056, 0.081), (0.082, 0.106)):
        bloque_tosco(bm, capa, (-0.019 + rng.uniform(-0.001, 0.001), 0.024, (z0 + z1) / 2),
                     (0.012, 0.014, z1 - z0), rng, pm, (0, 0, rng.uniform(-0.08, 0.08)))
    for z0, z1 in ((0.030, 0.055), (0.056, 0.079)):
        bloque_tosco(bm, capa, (0.019 + rng.uniform(-0.001, 0.001), 0.024, (z0 + z1) / 2),
                     (0.012, 0.014, z1 - z0), rng, pm, (0, 0, rng.uniform(-0.08, 0.08)))
    bloque_tosco(bm, capa, (-0.0095, 0.024, 0.1115), (0.027, 0.012, 0.009), rng, pm, (0, -0.55, 0))
    esquirla(bm, capa, (-0.001, 0.024, 0.116), 0.034, 0.0055, (0.08, 0.05), rng, pm)
    esquirla(bm, capa, (-0.021, 0.024, 0.105), 0.022, 0.005, (-0.35, 0.05), rng, pm)
    esquirla(bm, capa, (-0.034, 0.036, 0.028), 0.080, 0.0065, (-0.08, 0.03), rng, pm)
    esquirla(bm, capa, (0.034, 0.036, 0.028), 0.046, 0.0065, (0.1, 0.03), rng, pm, rota=True)
    # Calavera tallada en la piedra, sobre el nicho, y runas en columna en el tambor de abajo.
    calavera(bm, capa, Matrix.Translation((-0.003, 0.0185, 0.0985)) @ Matrix.Scale(0.42, 4), rng, pm)
    for i in range(3):
        runa(bm, capa, Vector((-0.0222, 0.0168, 0.034 + i * 0.0068)), Vector((1, 0, 0)), Vector((0, 0, 1)),
             RUNAS[(i + 2) % 4], 0.0058, rng)
    grieta(bm, capa, ((-0.021, 0.0168, 0.060), (-0.018, 0.0168, 0.067), (-0.020, 0.0168, 0.076)), 0.0006, rng)
    grieta(bm, capa, ((-0.024, 0.0293, 0.094), (-0.017, 0.0293, 0.098), (-0.010, 0.0293, 0.095)), 0.0006, rng)
    bmesh.ops.rotate(bm, verts=verts_nuevos(bm, antes), cent=CENTRO_HORNACINA,
                     matrix=INCLINACION_HORNACINA.to_matrix())

    # Brazos: el izquierdo entero con su apoyo; el derecho, partido, y el trozo caído en la escalinata.
    bloque_tosco(bm, capa, (-0.024, -0.002, 0.047), (0.009, 0.030, 0.009), rng, pm, (0.04, 0, 0))
    bloque_tosco(bm, capa, (-0.024, -0.013, 0.0385), (0.008, 0.008, 0.012), rng, pm)
    bloque_tosco(bm, capa, (0.024, 0.006, 0.045), (0.009, 0.016, 0.008), rng, pm, (0.25, -0.1, 0.1))
    bloque_tosco(bm, capa, (0.036, -0.026, 0.013), (0.011, 0.015, 0.008), rng, pm, (0.3, 0.5, 0.8))
    # Escombros del respaldo roto: el arco derecho y el tambor caídos, y cascotes.
    bloque_tosco(bm, capa, (0.054, -0.004, 0.005), (0.027, 0.012, 0.009), rng, pm, (0.2, 0.15, 1.0))
    bloque_tosco(bm, capa, (0.050, 0.030, 0.006), (0.012, 0.014, 0.024), rng, pm, (1.5, 0.2, 0.5))
    for _ in range(3):
        c = (rng.uniform(0.04, 0.066), rng.uniform(-0.03, 0.04), 0.003)
        e = rng.uniform(0.005, 0.008)
        bloque_tosco(bm, capa, c, (e, e * 0.8, e * 0.7), rng, pm, (rng.uniform(0, 1), rng.uniform(0, 1), 0))
    return bm


def percha_del_cuervo():
    """Punto del trono (en sus coordenadas locales, ya escalado) donde se posa el cuervo."""
    p = INCLINACION_HORNACINA.to_matrix() @ (PERCHA_CUERVO - CENTRO_HORNACINA) + CENTRO_HORNACINA
    return p * ESCALA_TRONO


def pieza_brasero(rng):
    """Cuenco de hierro sobre tres patas torcidas; dentro, llamas de cristal cian (la magia)."""
    bm, capa = nuevo_bm()
    perfil = [(0.006, 0.019), (0.014, 0.022), (0.020, 0.029), (0.021, 0.033), (0.017, 0.033), (0.012, 0.027)]
    torno(bm, capa, perfil, 7, [NEGRO] * 4 + [PIEDRA_MEDIA, NEGRO, NEGRO], rng)
    for k in range(3):
        a = 2 * math.pi * k / 3 + rng.uniform(-0.25, 0.25)
        d = Vector((math.cos(a), math.sin(a), 0))
        codo = d.cross(Vector((0, 0, 1))) * rng.uniform(-0.004, 0.004)
        puntos = [d * 0.009 + Vector((0, 0, 0.022)), d * 0.019 + codo + Vector((0, 0, 0.012)),
                  d * 0.024 + Vector((0, 0, -0.002))]
        tubo(bm, capa, puntos, [0.0034, 0.0028, 0.0036], 4, NEGRO, rng, 0.1, tapa=True)
    for k in range(5):
        a = 2 * math.pi * k / 5 + rng.uniform(-0.4, 0.4)
        r = rng.uniform(0.002, 0.008)
        base = Vector((math.cos(a) * r, math.sin(a) * r, 0.025))
        alto = rng.uniform(0.018, 0.036) if k else 0.04
        sesgo = Vector((math.cos(a), math.sin(a), 0)) * rng.uniform(0.1, 0.35)
        medio = base + (Vector((0, 0, 1)) + sesgo).normalized() * alto * 0.5 \
            + Vector((rng.uniform(-0.003, 0.003), rng.uniform(-0.003, 0.003), 0))
        punta = medio + (Vector((0, 0, 1)) - sesgo * 0.5).normalized() * alto * 0.5
        tubo(bm, capa, [base, medio, punta], [0.0045, 0.0032, 0], 4, CIAN, rng, 0.2, tapa=True)
    # Pavesas: tres octaedros sueltos por encima de las llamas.
    for _ in range(3):
        c = Vector((rng.uniform(-0.01, 0.01), rng.uniform(-0.01, 0.01), rng.uniform(0.062, 0.078)))
        e = rng.uniform(0.0015, 0.0025)
        casco(bm, capa, [c + Vector(v) * e for v in
                         ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1.6), (0, 0, -1.2))], CIAN)
    return bm


def garra(bm, capa, origen, direccion, largo, radio, tramos, lados, rng, doblez=(0.4, 0.7), color=MADERA):
    """Rama en tramos rectos: sale en la dirección dada y a partir de la mitad se dobla hacia abajo, en
    punta. Devuelve los puntos de su eje para colgar ramitas de ellos."""
    q, rd = origen.copy(), direccion.copy()
    puntos = [q.copy()]
    for t in range(tramos):
        if t >= tramos // 2:
            rd.z -= rng.uniform(*doblez)
        rd = (rd + Vector((rng.gauss(0, 0.2), rng.gauss(0, 0.2), 0))).normalized()
        q = q + rd * largo / tramos
        puntos.append(q.copy())
    tubo(bm, capa, puntos, [radio * (1 - t / tramos) for t in range(tramos)] + [0], lados, color, rng)
    return puntos


def raices(bm, capa, rng, base_z, r0, n, largo):
    """Raíces que salen del pie y reptan por el suelo antes de hundirse, en punta."""
    for k in range(n):
        a = 2 * math.pi * k / n + rng.uniform(-0.4, 0.4)
        out = Vector((math.cos(a), math.sin(a), 0))
        lado = Vector((-out.y, out.x, 0)) * rng.uniform(-0.25, 0.25)
        l = largo * rng.uniform(0.7, 1.2)
        base = Vector((0, 0, base_z))
        p1 = base + (out + lado) * l * 0.35 + Vector((0, 0, 0.001))
        p2 = base + (out - lado) * l * 0.7 + Vector((0, 0, -0.001))
        p3 = base + out * l + Vector((0, 0, -0.005))
        tubo(bm, capa, [base, p1, p2, p3], [r0 * 0.7, r0 * 0.5, r0 * 0.3, 0], 4, MADERA, rng)


def pieza_arbol(rng):
    """Árbol muerto: tronco grueso y quebrado que a media altura se abre en tres o cuatro brazos, como una
    mano; los brazos acaban en garra hacia abajo y echan ramitas con púas. Muñones de ramas partidas y
    raíces que reptan por el suelo. Todo facetado y en punta."""
    bm, capa = nuevo_bm()
    alto = rng.uniform(0.05, 0.065)           # altura de la horquilla; los brazos suben por encima
    segmentos = rng.randint(5, 6)
    azimut = rng.uniform(0, 2 * math.pi)
    inclinacion = rng.uniform(0.08, 0.22)
    hacia = Vector((math.cos(azimut) * inclinacion, math.sin(azimut) * inclinacion, 1)).normalized()
    p, d = Vector((0, 0, -0.004)), Vector((0, 0, 1))
    puntos = [p.copy()]
    for i in range(segmentos):
        quiebro = 0.45 if rng.random() < 0.35 else 0.15  # de vez en cuando, un codo seco
        d = (d + (hacia - d) * 0.5 + Vector((rng.gauss(0, quiebro), rng.gauss(0, quiebro), 0)))
        # El tronco se tuerce pero no se tumba: si no, desde el asiento parece un leño caído.
        horizontal = Vector((d.x, d.y, 0))
        if horizontal.length > 0.6 * d.z:
            horizontal *= 0.6 * d.z / horizontal.length
            d = Vector((horizontal.x, horizontal.y, d.z))
        d.normalize()
        p = p + d * (alto / segmentos)
        puntos.append(p.copy())
    r0 = rng.uniform(0.011, 0.014)
    radios = [r0 * (1 - 0.5 * i / segmentos) for i in range(segmentos + 1)]
    radios[0] *= 1.6
    # El tronco termina en una punta corta por encima de la horquilla, que queda dentro de los brazos.
    puntos.append(puntos[-1] + d * 0.007)
    radios.append(0)
    tubo(bm, capa, puntos, radios, 6, MADERA, rng)

    # Brazos de la horquilla, muy abiertos; cada uno con dos a cuatro ramitas, y cada ramita con una púa.
    horquilla, r_brazo = puntos[-2], radios[-2] * 0.8
    n_brazos = rng.randint(3, 4)
    a0 = rng.uniform(0, 2 * math.pi)
    for b in range(n_brazos):
        a = a0 + b * 2 * math.pi / n_brazos + rng.uniform(-0.4, 0.4)
        rd = Vector((math.cos(a), math.sin(a), rng.uniform(0.5, 1.1))).normalized()
        largo = rng.uniform(0.045, 0.068) * (1.2 if b == 0 else 1.0)
        rama = garra(bm, capa, horquilla, rd, largo, r_brazo * rng.uniform(0.75, 0.95), 4, 5, rng)
        for _ in range(rng.randint(2, 4)):
            base = rama[rng.randint(1, 3)]
            a2 = a + rng.choice((-1, 1)) * rng.uniform(0.5, 1.4)
            d2 = Vector((math.cos(a2), math.sin(a2), rng.uniform(0.1, 0.9))).normalized()
            ramita = garra(bm, capa, base, d2, rng.uniform(0.015, 0.026), r_brazo * 0.42, 3, 3, rng)
            pua = ramita[1]
            a3 = a2 + rng.choice((-1, 1)) * rng.uniform(0.6, 1.2)
            d3 = Vector((math.cos(a3), math.sin(a3), rng.uniform(0.2, 1.0))).normalized()
            tubo(bm, capa, [pua, pua + d3 * rng.uniform(0.006, 0.011)], [r_brazo * 0.22, 0], 3, MADERA, rng)

    # Muñones: ramas partidas a media altura del tronco, cortas y en punta.
    for _ in range(2):
        i = rng.randint(1, segmentos - 1)
        a = rng.uniform(0, 2 * math.pi)
        rd = Vector((math.cos(a), math.sin(a), rng.uniform(0.0, 0.5))).normalized()
        base = puntos[i]
        tubo(bm, capa, [base, base + rd * 0.010, base + rd * 0.014], [radios[i] * 0.5, radios[i] * 0.4, 0],
             4, MADERA, rng)
    raices(bm, capa, rng, 0.004, r0, rng.randint(5, 6), 0.03)
    return bm


def pieza_arbol_caido(rng):
    """Árbol caído: tronco tumbado a lo largo de x, con el cepellón de raíces y tierra arrancado en un
    extremo, el otro partido en astillas, y dos o tres ramas que apuntan al cielo."""
    bm, capa = nuevo_bm()
    eje = [Vector((-0.05, 0, 0.009)), Vector((-0.015, 0.004, 0.0095)), Vector((0.02, -0.003, 0.008)),
           Vector((0.052, 0.003, 0.0065))]
    tubo(bm, capa, eje, [0.0095, 0.0085, 0.0075, 0.006], 6, MADERA, rng, 0.15, tapa=True)
    for k in range(4):
        a = 2 * math.pi * k / 4 + rng.uniform(-0.4, 0.4)
        base = eje[-1] + Vector((0, math.cos(a), math.sin(a))) * 0.003
        esquirla(bm, capa, base, 0.012, 0.0025, (4.0, math.cos(a) * 0.6), rng, MADERA)
    # Cepellón: terrón de tierra y raíces en abanico, casi vertical.
    elipsoide(bm, capa, (-0.056, 0, 0.011), (0.006, 0.016, 0.014), 16, rng, TIERRA_OSCURA)
    for k in range(7):
        a = 2 * math.pi * k / 7 + rng.uniform(-0.3, 0.3)
        d = Vector((-0.35, math.cos(a), math.sin(a) * 0.8 + 0.2)).normalized()
        base = Vector((-0.056, 0, 0.011))
        l = rng.uniform(0.016, 0.026)
        tubo(bm, capa, [base, base + d * l * 0.6, base + (d + Vector((-0.2, 0, -0.3))).normalized() * l],
             [0.003, 0.0018, 0], 4, MADERA, rng)
    for x in (-0.02, 0.018, 0.04)[:rng.randint(2, 3)]:
        base = Vector((x, 0, 0.012))
        rd = Vector((rng.uniform(-0.3, 0.3), rng.uniform(-0.5, 0.5), 1)).normalized()
        garra(bm, capa, base, rd, rng.uniform(0.022, 0.034), 0.0035, 3, 4, rng, doblez=(0.2, 0.4))
    return bm


def pieza_roca(rng):
    """Roca de varios trozos angulosos medio enterrados."""
    bm, capa = nuevo_bm()
    rx, ry, rz = rng.uniform(0.016, 0.026), rng.uniform(0.013, 0.02), rng.uniform(0.010, 0.018)
    trozos = [(Vector((0, 0, rz * 0.35)), rx, ry, rz)]
    for _ in range(rng.randint(1, 2)):
        a = rng.uniform(0, 2 * math.pi)
        f = rng.uniform(0.45, 0.7)
        c = Vector((math.cos(a) * rx * 0.9, math.sin(a) * ry * 0.9, rz * 0.2))
        trozos.append((c, rx * f, ry * f, rz * f * rng.uniform(0.8, 1.3)))
    for c, ax, ay, az in trozos:
        puntos = []
        for _ in range(rng.randint(16, 22)):
            v = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))).normalized()
            v *= rng.uniform(0.75, 1.0)
            puntos.append(c + Vector((v.x * ax, v.y * ay, max(v.z * az, -0.004))))
        rot = Euler((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), rng.uniform(0, math.pi))).to_matrix()
        casco(bm, capa, [rot @ (p - c) + c for p in puntos], PIEDRA_MEDIA)
    return bm


def pieza_huesos(rng):
    """Tres huesos largos esparcidos y una calavera. Los huesos no se cruzan en el centro: cruzados bajo
    la calavera se leen como una bandera pirata, que es justo el tono infantil."""
    bm, capa = nuevo_bm()
    a0 = rng.uniform(0, 2 * math.pi)
    for k in range(3):
        a = rng.uniform(0, math.pi)
        largo, r = rng.uniform(0.032, 0.044), rng.uniform(0.0025, 0.003)
        sitio = a0 + k * 2 * math.pi / 3 + rng.uniform(-0.4, 0.4)
        c = Vector((math.cos(sitio) * 0.016, -0.006 + math.sin(sitio) * 0.013, r))
        eje = Vector((math.cos(a), math.sin(a), rng.uniform(-0.15, 0.15))).normalized()
        hueso_largo(bm, capa, c - eje * largo / 2, c + eje * largo / 2, r, rng)
    m = Matrix.Translation((rng.uniform(-0.005, 0.005), rng.uniform(0.016, 0.02), 0)) \
        @ Euler((rng.uniform(-0.2, 0.1), rng.uniform(-0.25, 0.25), rng.uniform(-0.8, 0.8))).to_matrix().to_4x4()
    calavera(bm, capa, m, rng)
    return bm


def pieza_cadena(rng):
    """Estaca de hierro clavada con una argolla, y una cadena que cae y se arrastra por el suelo hacia +x."""
    bm, capa = nuevo_bm()
    tubo(bm, capa, [Vector((0, 0, 0.034)), Vector((0.0005, 0, 0.018)), Vector((0, 0, -0.008))],
         [0.0042, 0.0038, 0], 5, NEGRO, rng, 0.1, tapa=True)
    eslabon(bm, capa, Vector((0, 0, 0.028)), Vector((1, 0, 0)), Vector((0, 1, 0)), 0.0068, 0.0068, 0.0014,
            PIEDRA_MEDIA)
    curva = catmull_rom([(0.006, 0, 0.027), (0.014, 0.002, 0.019), (0.02, 0.004, 0.009), (0.028, 0.006, 0.004),
                         (0.04, 0.004 + rng.uniform(-0.004, 0.004), 0.003), (0.052, -0.002, 0.003),
                         (0.062, -0.01 + rng.uniform(-0.006, 0.006), 0.003), (0.068, -0.02, 0.003)], 16)
    paso = 0.0095
    n = int(longitudes(curva)[-1] / paso)
    for i, (c, t) in enumerate(puntos_a_distancia(curva, [paso * (i + 0.5) for i in range(n)])):
        lateral = t.cross(Vector((0, 0, 1)))
        lateral = lateral.normalized() if lateral.length > 1e-4 else Vector((0, 1, 0))
        eje_ancho = lateral if i % 2 == 0 else lateral.cross(t).normalized()
        eslabon(bm, capa, c, t, eje_ancho, 0.0062, 0.0040, 0.0013, PIEDRA_MEDIA)
    return bm


def pieza_lapida(rng):
    """Lápida partida en dos, volcada hacia atrás y medio hundida, con tres rayas de inscripción y la
    tierra removida a sus pies."""
    bm, capa = nuevo_bm()
    grosor, bisel = 0.009, 0.0012

    def trozo(poligono, matriz, rayas=()):
        centro = Vector((sum(x for x, _ in poligono) / len(poligono), 0, sum(z for _, z in poligono) / len(poligono)))
        puntos = []
        for x, z in poligono:
            p = Vector((x, 0, z))
            dentro = p + (centro - p).normalized() * bisel
            for y in (-grosor / 2, grosor / 2):
                puntos.append(Vector((dentro.x, y, dentro.z)))
                puntos.append(Vector((x, y - math.copysign(bisel, y), z)))
        casco(bm, capa, [matriz @ p for p in puntos], PIEDRA_MEDIA)
        for x, z, w in rayas:
            caja(bm, capa, (x, -grosor / 2 - 0.0002, z), (w, 0.0008, 0.0022), NEGRO, matriz)

    trozo([(-0.022, 0.0), (0.022, 0.0), (0.0225, 0.029), (-0.022, 0.026)], Matrix.Identity(4))
    alta = Matrix.Translation((0.0015, 0.0005, 0.0025)) @ Euler((0.0, 0.06, 0.03)).to_matrix().to_4x4()
    trozo([(-0.022, 0.028), (0.0225, 0.031), (0.023, 0.046), (0.016, 0.056), (0, 0.061), (-0.016, 0.055),
           (-0.022, 0.046)], alta, [(0, 0.050, 0.014), (0, 0.043, 0.020), (-0.002, 0.036, 0.012)])
    # Volcada hacia atrás unos 45 grados y hundida: se ve la cara desde arriba y desde el asiento, y sigue
    # leyéndose como lápida (tumbada del todo pasaría por una losa).
    vuelco = Matrix.Translation((0, 0, -0.004)) @ Euler((-0.8, 0, rng.uniform(-0.2, 0.2))).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=vuelco, verts=list(bm.verts))
    elipsoide(bm, capa, (0, -0.004, -0.002), (0.03, 0.016, 0.006), 14, rng, TIERRA_CLARA, z_min=0.0)
    return bm


def pieza_estacas(rng):
    """Tres o cuatro estacas de madera afiladas, clavadas y torcidas; en una hay una calavera ensartada."""
    bm, capa = nuevo_bm()
    n = rng.randint(3, 4)
    for k in range(n):
        a = 2 * math.pi * k / n + rng.uniform(-0.4, 0.4)
        base = Vector((math.cos(a), math.sin(a), 0)) * rng.uniform(0.006, 0.014) + Vector((0, 0, -0.006))
        alto = rng.uniform(0.045, 0.07)
        inc = Vector((rng.uniform(-0.25, 0.25), rng.uniform(-0.25, 0.25), 1)).normalized()
        medio = base + inc * alto * 0.55 + Vector((rng.uniform(-0.002, 0.002), rng.uniform(-0.002, 0.002), 0))
        punta = base + inc * alto
        tubo(bm, capa, [base, medio, medio + inc * alto * 0.3, punta], [0.0036, 0.0033, 0.003, 0], 5, MADERA, rng)
        if k == 0:
            m = Matrix.Translation(base + inc * alto * 0.62 - Vector((0, 0, 0.009))) \
                @ Euler((0.2, 0, rng.uniform(-0.6, 0.6))).to_matrix().to_4x4() @ Matrix.Scale(0.8, 4)
            calavera(bm, capa, m, rng)
    return bm


def pieza_jaula(rng):
    """Jaula de hierro caída de lado y medio hundida: barrotes que se cierran en punta arriba, dos aros,
    un gancho y, dentro, una calavera y un hueso."""
    bm, capa = nuevo_bm()
    R, H, n = 0.022, 0.058, 8
    cima = Vector((0, 0, H))
    for k in range(n):
        a = 2 * math.pi * k / n
        c, s = math.cos(a), math.sin(a)
        barra = [Vector((c * R, s * R, 0.003)), Vector((c * R * 1.12, s * R * 1.12, H * 0.45)),
                 Vector((c * R * 0.85, s * R * 0.85, H * 0.8)), cima]
        tubo(bm, capa, barra, [0.0012, 0.0012, 0.0011, 0], 4, NEGRO, rng, 0.05)
    for z, f in ((0.004, 1.0), (H * 0.45, 1.12)):
        aro = [Vector((math.cos(a) * R * f, math.sin(a) * R * f, z)) for a in (2 * math.pi * i / 14 for i in range(15))]
        tubo(bm, capa, aro, [0.0014] * 15, 4, NEGRO, rng, 0.05)
    torno(bm, capa, [(R * 1.02, 0.0), (R * 1.02, 0.004)], 12, [NEGRO] * 3, rng, 0.02)
    eslabon(bm, capa, cima + Vector((0, 0, 0.005)), Vector((0, 0, 1)), Vector((1, 0, 0)), 0.006, 0.004, 0.0013,
            NEGRO)
    calavera(bm, capa, Matrix.Translation((0.004, 0.002, 0.004)) @ Euler((0, 0, 0.7)).to_matrix().to_4x4()
             @ Matrix.Scale(0.8, 4), rng)
    hueso_largo(bm, capa, Vector((-0.014, -0.006, 0.0065)), Vector((0.006, -0.014, 0.007)), 0.0022, rng)
    # Caída de lado y medio hundida.
    bmesh.ops.rotate(bm, verts=list(bm.verts), cent=(0, 0, 0), matrix=Euler((0.35, 0.12, 0)).to_matrix())
    bmesh.ops.translate(bm, vec=(0, 0, -0.005), verts=list(bm.verts))
    return bm


def pieza_velas(rng):
    """Velas derretidas y apagadas sobre un charco de cera: cabos de distinta altura con la boca hundida,
    goterones por los lados y la mecha negra."""
    bm, capa = nuevo_bm()
    elipsoide(bm, capa, (0, 0, 0.0), (0.016, 0.013, 0.0018), 14, rng, HUESO, z_min=0.0)
    n = rng.randint(3, 5)
    for k in range(n):
        a = 2 * math.pi * k / n + rng.uniform(-0.4, 0.4)
        cx, cy = math.cos(a) * rng.uniform(0.003, 0.009), math.sin(a) * rng.uniform(0.003, 0.009)
        h, r = rng.uniform(0.008, 0.024), rng.uniform(0.0024, 0.0034)
        torno(bm, capa, [(r * 1.15, 0.0), (r, h * 0.3), (r * 0.97, h), (r * 0.6, h - 0.0014)], 6,
              [HUESO] * 5, rng, 0.06, centro=(cx, cy))
        for _ in range(rng.randint(1, 2)):
            b = rng.uniform(0, 2 * math.pi)
            lado = Vector((math.cos(b), math.sin(b), 0))
            arriba = Vector((cx, cy, h * 0.95)) + lado * r
            abajo = Vector((cx, cy, h * rng.uniform(0.2, 0.5))) + lado * r * 1.2
            casco(bm, capa, [arriba + Vector((0, 0, 0.001)), arriba + lado.cross(Vector((0, 0, 1))) * 0.0012,
                             arriba - lado.cross(Vector((0, 0, 1))) * 0.0012, abajo + lado * 0.0009,
                             abajo - lado * 0.0005, abajo - Vector((0, 0, 0.0015))], HUESO)
        tubo(bm, capa, [Vector((cx, cy, h - 0.0015)), Vector((cx + 0.0005, cy, h + 0.0025))], [0.0005, 0.0003],
             3, NEGRO, rng, 0.0)
    return bm


def pieza_cuervo(rng):
    """Cuervo posado, mirando hacia +x: cuerpo y cabeza facetados, pico, cola y patas. Negro."""
    bm, capa = nuevo_bm()
    ladeo = Euler((0, -0.25, 0)).to_matrix().to_4x4()
    elipsoide(bm, capa, (0, 0, 0.012), (0.014, 0.0065, 0.0075), 18, rng, NEGRO, ladeo)
    elipsoide(bm, capa, (0.0145, 0, 0.0215), (0.0055, 0.0048, 0.005), 12, rng, NEGRO)
    casco(bm, capa, [Vector((0.0185, 0.0018, 0.0215)), Vector((0.0185, -0.0018, 0.0215)),
                     Vector((0.0185, 0, 0.0235)), Vector((0.0185, 0, 0.0195)), Vector((0.0285, 0, 0.0205))], PIEDRA_MEDIA)
    casco(bm, capa, [Vector((-0.011, 0.004, 0.012)), Vector((-0.011, -0.004, 0.012)), Vector((-0.011, 0, 0.015)),
                     Vector((-0.028, 0.006, 0.006)), Vector((-0.028, -0.006, 0.006)), Vector((-0.027, 0, 0.0075))], NEGRO)
    for y in (-0.003, 0.003):
        tubo(bm, capa, [Vector((0.002, y, 0.006)), Vector((0.003, y, 0.0))], [0.0008, 0.0006], 3, NEGRO, rng, 0.0)
        for dx in (0.004, -0.002):
            tubo(bm, capa, [Vector((0.003, y, 0.0005)), Vector((0.003 + dx, y, 0.0002))], [0.0005, 0], 3,
                 NEGRO, rng, 0.0)
    return bm


def pieza_charco(rng, objeto):
    """Charco de agua estancada: borde irregular de barro, agua casi negra facetada (material brillante,
    lo único que refleja la luz) y algo medio hundido: un hueso, una raíz o una cadena."""
    bm, capa = nuevo_bm()
    n = 14
    R = rng.uniform(0.017, 0.021)
    estira = rng.uniform(1.0, 1.25)
    radios = [R * rng.uniform(0.75, 1.15) for _ in range(n)]
    radios = [(radios[k - 1] + 2 * radios[k] + radios[(k + 1) % n]) / 4 for k in range(n)]
    contorno = [Vector((math.cos(2 * math.pi * k / n) * radios[k] * estira,
                        math.sin(2 * math.pi * k / n) * radios[k], 0)) for k in range(n)]

    def anillo(f, z, jit=0.0):
        return [bm.verts.new((p.x * f, p.y * f, z + rng.uniform(-jit, jit))) for p in contorno]

    fuera = anillo(1.18, -0.003)
    barro = anillo(1.0, 0.0022, 0.0008)
    orilla = anillo(0.86, 0.0012)
    medio = anillo(0.55, 0.0012, 0.00025)
    centro = [bm.verts.new((rng.uniform(-0.004, 0.004), rng.uniform(-0.004, 0.004), 0.0012))]
    anillos_a_caras(bm, capa, fuera, barro, TIERRA_OSCURA)
    anillos_a_caras(bm, capa, barro, orilla, TIERRA_OSCURA)
    anillos_a_caras(bm, capa, orilla, medio, AGUA)
    anillos_a_caras(bm, capa, medio, centro, AGUA)
    if objeto == "hueso":
        hueso_largo(bm, capa, Vector((-0.014, 0.004, -0.001)), Vector((0.01, -0.004, 0.0035)), 0.0026, rng)
    elif objeto == "raiz":
        tubo(bm, capa, [Vector((-0.02, 0.002, -0.003)), Vector((-0.009, 0.004, 0.006)), Vector((0.003, -0.001, 0.007)),
                        Vector((0.015, 0.002, -0.002))], [0.003, 0.0026, 0.0021, 0.0016], 5, MADERA, rng)
    else:
        for i in range(4):
            c = Vector((-0.012 + i * 0.0085, 0.001 * i, 0.0035 - i * 0.0015))
            t = Vector((1, 0.1, -0.18)).normalized()
            lateral = t.cross(Vector((0, 0, 1))).normalized()
            eslabon(bm, capa, c, t, lateral if i % 2 == 0 else lateral.cross(t).normalized(), 0.0058, 0.0038,
                    0.0012, PIEDRA_MEDIA)
    return bm


def pieza_ficha(rng):
    """Ficha de prueba: carta vertical de 6x9 cm, con una ventana gris donde iría la ilustración, sobre
    una peana circular. Mira hacia -y. La ventana no es negra para no confundirla con el nicho del trono."""
    bm, capa = nuevo_bm()
    r = PEANA_DIAMETRO / 2
    torno(bm, capa, [(r, 0.0), (r, 0.0055), (r - 0.002, 0.0075)], 16, [NEGRO] * 4, rng, irregular=0.0)
    z0 = 0.0065
    caja(bm, capa, (0, 0, z0 + FICHA_ALTO / 2), (FICHA_ANCHO, 0.0015, FICHA_ALTO), HUESO)
    caja(bm, capa, (0, -0.0008, z0 + FICHA_ALTO * 0.56), (FICHA_ANCHO - 0.01, 0.0004, FICHA_ALTO * 0.66),
         PIEDRA_MEDIA)
    return bm


# ---------------------------------------------------------------------------------------------------------
# Paso a Blender: mallas con color por vértice, materiales, instancias
# ---------------------------------------------------------------------------------------------------------
def srgb_a_lineal(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


PALETA_LINEAL = [tuple(srgb_a_lineal(v) for v in col) for col in PALETA]
MATERIALES = ("M_Paleta", "M_Paleta_Emisivo", "M_Paleta_Tenue", "M_Agua")


def malla(nombre, bm, escala=1.0):
    if escala != 1.0:
        bmesh.ops.scale(bm, vec=(escala, escala, escala), verts=list(bm.verts))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    me = bpy.data.meshes.new(nombre)
    bm.to_mesh(me)
    bm.free()
    indices = [0] * len(me.polygons)
    me.attributes["pal"].data.foreach_get("value", indices)
    colores = []
    for poly in me.polygons:
        c = PALETA_LINEAL[COLOR_DE.get(indices[poly.index], indices[poly.index])]
        colores.extend(list(c) + [1.0] for _ in range(poly.loop_total))
    col = me.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    col.data.foreach_set("color", [c for rgba in colores for c in rgba])
    me.color_attributes.active_color = col
    try:
        me.color_attributes.render_color_index = me.color_attributes.active_color_index
    except AttributeError:
        pass
    # Material por cara: cian (emisivo), cian tenue, agua brillante; el resto, la paleta mate.
    me.polygons.foreach_set("material_index", [MATERIAL_DE.get(i, 0) for i in indices])
    me.attributes.remove(me.attributes["pal"])
    for p in me.polygons:
        p.use_smooth = False
    for m in MATERIALES:
        me.materials.append(bpy.data.materials[m])
    me.update()
    return me


def material_paleta(nombre, emision=0.0, rugosidad=0.95, especular=0.15, tinte=None, barniz=False):
    m = bpy.data.materials.new(nombre)
    if m.node_tree is None:
        m.use_nodes = True
    nodos = m.node_tree.nodes
    bsdf = next((n for n in nodos if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is None:
        bsdf = nodos.new("ShaderNodeBsdfPrincipled")
        salida = next(n for n in nodos if n.type == "OUTPUT_MATERIAL")
        m.node_tree.links.new(bsdf.outputs[0], salida.inputs["Surface"])
    vc = nodos.new("ShaderNodeVertexColor")
    vc.layer_name = "Col"
    m.node_tree.links.new(vc.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = rugosidad
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = especular
    if tinte is not None and "Specular Tint" in bsdf.inputs:
        bsdf.inputs["Specular Tint"].default_value = (*tinte, 1.0)
    if barniz:
        # Agua estancada: capa brillante encima (refleja luces y cielo) y, en ángulo rasante, un velo
        # verdoso como el de una película de aceite. El color base sigue siendo el negro de la paleta.
        for entrada, valor in (("Coat Weight", 1.0), ("Coat Roughness", 0.04), ("Sheen Weight", 0.7),
                               ("Sheen Roughness", 0.35)):
            if entrada in bsdf.inputs:
                bsdf.inputs[entrada].default_value = valor
        if "Sheen Tint" in bsdf.inputs:
            bsdf.inputs["Sheen Tint"].default_value = (0.25, 0.55, 0.2, 1.0)
    if emision > 0:
        # La emisión es el cian de la paleta como constante: el GLB no admite emisión por vértice.
        bsdf.inputs["Emission Color"].default_value = (*PALETA_LINEAL[CIAN], 1.0)
        bsdf.inputs["Emission Strength"].default_value = emision
    return m


def crear_materiales():
    material_paleta("M_Paleta")
    material_paleta("M_Paleta_Emisivo", emision=6.0)
    material_paleta("M_Paleta_Tenue", emision=0.5)  # doce veces más débil que el brasero
    # Agua estancada: el negro de la paleta con poca rugosidad y reflejo verdoso, como una película de aceite.
    material_paleta("M_Agua", rugosidad=0.12, especular=0.9, tinte=(0.55, 0.85, 0.45), barniz=True)


def instanciar(nombre, me, pos, rot_z=0.0, escala=1.0, coleccion=None):
    ob = bpy.data.objects.new(nombre, me)
    ob.location = pos
    ob.rotation_euler = (0, 0, rot_z)
    ob.scale = (escala,) * 3
    (coleccion or bpy.context.scene.collection).objects.link(ob)
    return ob


def actualizar():
    """Recalcula las matrices de los objetos. Un objeto recién creado o movido no tiene matrix_world al día
    hasta que se actualiza la capa de vista; sin esto, los rayos verían todas las piezas en el origen."""
    bpy.context.view_layer.update()


def alto_max(ob):
    return max((ob.matrix_world @ Vector(c)).z for c in ob.bound_box)


# ---------------------------------------------------------------------------------------------------------
# Oclusión ambiental en el color por vértice
# ---------------------------------------------------------------------------------------------------------
def direcciones_hemisferio(n):
    """Direcciones del hemisferio +z repartidas en espiral de Fibonacci, con densidad de coseno: más
    rayos hacia la normal. Deterministas, así la oclusión no cambia entre ejecuciones."""
    salida = []
    for i in range(n):
        u = (i + 0.5) / n
        phi = i * 2.399963
        r = math.sqrt(u)
        salida.append(Vector((r * math.cos(phi), r * math.sin(phi), math.sqrt(1 - u))))
    return salida


def oclusion_esquinas(me, bvh, matriz, distancia, plano_z=None):
    """Oclusión de cada esquina de cara: fracción de rayos del hemisferio de la cara que chocan con algo
    antes de `distancia`, pesada por lo cerca que chocan. plano_z añade un suelo plano infinito a esa
    altura (para las piezas, que se calculan solas). Devuelve un valor en [0, 1] por esquina (1 = libre)."""
    dirs = direcciones_hemisferio(AO_RAYOS)
    rot = matriz.to_3x3()
    arriba = Vector((0, 0, 1))
    salida = [1.0] * len(me.loops)
    for poly in me.polygons:
        n = (rot @ poly.normal).normalized()
        giro = arriba.rotation_difference(n).to_matrix()
        rayos = [giro @ d for d in dirs]
        centro = matriz @ poly.center
        for li in poly.loop_indices:
            v = matriz @ me.vertices[me.loops[li].vertex_index].co
            p = v.lerp(centro, 0.15) + n * 0.0002
            tapado = 0.0
            for d in rayos:
                loc, _, _, dist = bvh.ray_cast(p, d, distancia)
                if loc is not None:
                    tapado += 1 - dist / distancia
                elif plano_z is not None and d.z < -1e-4:
                    t = (plano_z - p.z) / d.z
                    if 0 < t < distancia:
                        tapado += 1 - t / distancia
            salida[li] = 1 - tapado / len(rayos)
    return salida


def aplicar_oclusion(me, ao):
    col = me.color_attributes["Col"]
    valores = [0.0] * (len(me.loops) * 4)
    col.data.foreach_get("color", valores)
    for li, a in enumerate(ao):
        f = 1 - AO_FUERZA * (1 - a)
        valores[li * 4] *= f
        valores[li * 4 + 1] *= f
        valores[li * 4 + 2] *= f
    col.data.foreach_set("color", valores)


def bvh_de(objetos):
    verts, polys = [], []
    for ob in objetos:
        base = len(verts)
        m = ob.matrix_world
        verts.extend(m @ v.co for v in ob.data.vertices)
        polys.extend([base + i for i in p.vertices] for p in ob.data.polygons)
    return BVHTree.FromPolygons(verts, polys)


def hornear_oclusion(piezas, instancias, suelo, grafo, sin_oclusion=("peana",)):
    """Oclusión en el color por vértice. Las piezas se calculan una vez por malla, solas, contra sí
    mismas y un suelo plano bajo la base: sirve para todas sus instancias. El suelo se calcula con todas
    las instancias encima, así se oscurece bajo cada losa, tronco o roca; salvo la franja de los senderos,
    que queda entre dos losas y la oclusión la oscurecería justo donde tiene que leerse clara."""
    inicio = time.time()
    for nombre, me in piezas.items():
        if nombre in sin_oclusion or me is suelo.data:
            continue
        bvh = BVHTree.FromPolygons([v.co for v in me.vertices], [list(p.vertices) for p in me.polygons])
        aplicar_oclusion(me, oclusion_esquinas(me, bvh, Matrix.Identity(4), AO_DISTANCIA_PIEZA, plano_z=0.0))
    me = suelo.data
    ao = oclusion_esquinas(me, bvh_de(instancias), Matrix.Identity(4), AO_DISTANCIA_SUELO)
    for poly in me.polygons:
        if dist_caminos(poly.center.x, poly.center.y, grafo) < ANCHO_SENDERO / 2:
            for li in poly.loop_indices:
                ao[li] = 1.0
    aplicar_oclusion(me, ao)
    return time.time() - inicio


# ---------------------------------------------------------------------------------------------------------
# Visibilidad de las losas desde los cuatro asientos
# ---------------------------------------------------------------------------------------------------------
def crear_asientos(escena):
    """Cuatro cámaras sentadas alrededor de la mesa, a SENTADA_DISTANCIA del borde del tablero y a
    SENTADA_ALTURA sobre la mesa, cada una mirando al centro de este Dominio."""
    objetivo = Vector((0, -(R_INTERIOR + R_EXTERIOR) / 2, SUELO_Z))
    camaras = []
    for nombre, grados in ASIENTOS:
        a = math.radians(grados)
        d = R_EXTERIOR + SENTADA_DISTANCIA
        pos = Vector((math.cos(a) * d, math.sin(a) * d, SENTADA_ALTURA))
        cam = bpy.data.cameras.new(f"Cam_asiento_{nombre}")
        cam.lens = SENTADA_FOCAL
        cam.clip_start = 0.01
        ob = bpy.data.objects.new(f"Cam_asiento_{nombre}", cam)
        ob.location = pos
        ob.rotation_euler = (objetivo - pos).to_track_quat("-Z", "Y").to_euler()
        escena.collection.objects.link(ob)
        camaras.append(ob)
    return camaras


class Visibilidad:
    """Comprueba si cada losa se ve, al menos en parte, desde cada asiento: se lanzan rayos desde la cámara
    a puntos de la cara superior de la losa, y basta con que uno llegue sin chocar antes y caiga dentro
    del encuadre."""

    def __init__(self, escena, camaras, losas):
        self.escena, self.camaras, self.losas = escena, camaras, losas
        self.locales = {}
        for ob in losas:
            me = ob.data
            if me.name not in self.locales:
                # La cara de la losa está por debajo de 2 cm; lo que sobresalga (el respaldo del Nicho-trono,
                # ronda 6) es estructura, no casilla, y no cuenta como "ver la losa".
                cara = [v.co.copy() for v in me.vertices if v.co.z < 0.02 and v.co.to_2d().length < 0.038]
                z_max = max(v.z for v in cara)
                arriba = [v for v in cara if v.z >= z_max - 0.0035]
                paso = max(1, len(arriba) // 24)
                self.locales[me.name] = arriba[::paso]

    def informe(self, objetos):
        """{asiento: [nombres de losas ocultas del todo]} con los objetos dados como posibles obstáculos."""
        bvh = bvh_de(objetos)
        salida = {}
        for cam in self.camaras:
            o = cam.matrix_world.translation
            ocultas = []
            for ob in self.losas:
                visible = False
                for p in self.locales[ob.data.name]:
                    q = ob.matrix_world @ p + Vector((0, 0, 0.0006))
                    v = world_to_camera_view(self.escena, cam, q)
                    if not (0 <= v.x <= 1 and 0 <= v.y <= 1 and v.z > 0):
                        continue
                    d = q - o
                    loc, _, _, h = bvh.ray_cast(o, d.normalized(), d.length)
                    if loc is None or h >= d.length - 0.002:
                        visible = True
                        break
                if not visible:
                    ocultas.append(ob.name)
            salida[cam.name] = ocultas
        return salida


# ---------------------------------------------------------------------------------------------------------
# Colocación
# ---------------------------------------------------------------------------------------------------------
def buscar_sitio(rng, ocupados, grafo, radio, pie, hueco, margen, cerca=None, borde=None, aceptar=None,
                 candidatos=12, intentos=6000, motivos=None):
    """Reúne puntos válidos y los prueba de mejor a peor (el más alejado de lo ya colocado primero), para
    repartir las piezas sin racimos. Devuelve el primero que `aceptar(x, y)` da por bueno, o None.
    - radio: radio de la pieza entera (copa incluida). hueco: espacio libre exigido entre su borde y el de
      cada vecina, para que no formen racimos.
    - pie: radio de lo que la pieza ocupa en el suelo. Ese pie queda a más de media losa de margen de
      cualquier losa o sendero; la copa de un árbol sí puede asomar por encima.
    - cerca=(x, y, r): busca solo en ese círculo. borde: solo a menos de esto del perímetro."""
    sep_grafo = LOSA_MEDIO + MARGEN_LOSA + pie
    validos = []
    for _ in range(intentos):
        if cerca:
            a, r = rng.uniform(0, 2 * math.pi), cerca[2] * math.sqrt(rng.random())
            x, y = cerca[0] + math.cos(a) * r, cerca[1] + math.sin(a) * r
        else:
            x, y = rng.uniform(-0.38, 0.38), rng.uniform(-R_EXTERIOR, -R_INTERIOR)
        motivo = None
        db = dist_borde(x, y)
        if db < margen or (borde is not None and db > borde):
            motivo = "borde"
        elif any(math.hypot(x - ox, y - oy) < radio + r + hueco for ox, oy, r in ocupados):
            motivo = "vecinas"
        elif dist_caminos(x, y, grafo) < sep_grafo:
            motivo = "camino"
        if motivo:
            if motivos is not None:
                motivos[motivo] = motivos.get(motivo, 0) + 1
            continue
        holgura = min((math.hypot(x - ox, y - oy) - r for ox, oy, r in ocupados), default=1.0)
        validos.append((holgura, x, y))
        if len(validos) >= candidatos:
            break
    for _, x, y in sorted(validos, reverse=True):
        if aceptar is None or aceptar(x, y):
            return x, y
        if motivos is not None:
            motivos["oculta losa"] = motivos.get("oculta losa", 0) + 1
    return None


# ---------------------------------------------------------------------------------------------------------
# Escena de render: mesa, luces, cámaras
# ---------------------------------------------------------------------------------------------------------
def preparar_render(escena, brasero_pos):
    for motor in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            escena.render.engine = motor
            break
        except TypeError:
            continue
    ee = escena.eevee
    # Sin trazado de rayos ni oclusión en pantalla: lo que se ve tiene que poder precalcularse. La
    # oclusión que hay está horneada en el color por vértice.
    for attr, valor in (("use_raytracing", False), ("taa_render_samples", 32), ("use_shadows", True),
                        ("use_gtao", False), ("use_bloom", False), ("use_ssr", False)):
        if hasattr(ee, attr):
            setattr(ee, attr, valor)
    escena.view_settings.view_transform = "Standard"
    escena.view_settings.look = "None"

    # Relleno: cielo frío, un punto más alto que en la ronda 2 para que se lea en una pantalla de 7".
    mundo = bpy.data.worlds.new("Mundo")
    escena.world = mundo
    if mundo.node_tree is None:
        mundo.use_nodes = True
    nodos = mundo.node_tree.nodes
    fondo = next(n for n in nodos if n.type == "BACKGROUND")
    fondo.inputs["Color"].default_value = (0.065, 0.073, 0.1, 1)
    fondo.inputs["Strength"].default_value = 1.0
    # La cámara ve un gris neutro detrás de la mesa; la luz de relleno sigue siendo el cielo frío.
    neutro = nodos.new("ShaderNodeBackground")
    gris = srgb_a_lineal(62)
    neutro.inputs["Color"].default_value = (gris, gris, gris, 1)
    camino = nodos.new("ShaderNodeLightPath")
    mezcla = nodos.new("ShaderNodeMixShader")
    salida = next(n for n in nodos if n.type == "OUTPUT_WORLD")
    enlaces = mundo.node_tree.links
    enlaces.new(camino.outputs["Is Camera Ray"], mezcla.inputs[0])
    enlaces.new(fondo.outputs[0], mezcla.inputs[1])
    enlaces.new(neutro.outputs[0], mezcla.inputs[2])
    enlaces.new(mezcla.outputs[0], salida.inputs["Surface"])

    # Luz principal dura: sol casi puntual desde la izquierda y algo por delante, bajo, para sombras largas
    # que siguen dando tono y el frente del trono iluminado.
    sol = bpy.data.lights.new("Sol", "SUN")
    sol.energy = 4.8
    sol.angle = math.radians(0.5)
    sol.color = (1.0, 0.93, 0.82)
    if hasattr(sol, "shadow_maximum_resolution"):
        sol.shadow_maximum_resolution = 0.0002
    ob = bpy.data.objects.new("Sol", sol)
    ob.rotation_euler = Euler((math.radians(50), 0, math.radians(-70)))
    escena.collection.objects.link(ob)

    # El brasero ilumina un poco su entorno: en Unreal sería una luz estática, precalculada. Es la luz que
    # reflejan los charcos.
    punto = bpy.data.lights.new("Brasero_luz", "POINT")
    punto.energy = 0.6
    punto.color = tuple(PALETA_LINEAL[CIAN])
    punto.shadow_soft_size = 0.005
    ob = bpy.data.objects.new("Brasero_luz", punto)
    ob.location = (brasero_pos[0], brasero_pos[1], brasero_pos[2] + 0.07)
    escena.collection.objects.link(ob)

    # Mesa gris neutra: fondo de los renders, no se exporta.
    mesa_me = bpy.data.meshes.new("Mesa")
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=3.0)
    bm.to_mesh(mesa_me)
    bm.free()
    mat = bpy.data.materials.new("M_Mesa")
    if mat.node_tree is None:
        mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    gris = srgb_a_lineal(62)
    bsdf.inputs["Base Color"].default_value = (gris, gris, gris, 1)
    bsdf.inputs["Roughness"].default_value = 0.9
    mesa_me.materials.append(mat)
    escena.collection.objects.link(bpy.data.objects.new("Mesa", mesa_me))

    # Cámara sentada principal: a SENTADA_DISTANCIA del borde exterior, mirando hacia el centro del tablero.
    cam = bpy.data.cameras.new("Cam_sentada")
    cam.lens = SENTADA_FOCAL
    cam.clip_start = 0.01
    sentada = bpy.data.objects.new("Cam_sentada", cam)
    sentada.location = (0, -R_EXTERIOR - SENTADA_DISTANCIA, SENTADA_ALTURA)
    sentada.rotation_euler = (math.radians(90 - SENTADA_PICADO), 0, 0)
    escena.collection.objects.link(sentada)

    # Cámara cenital ortográfica con el Dominio entero y un margen.
    cam = bpy.data.cameras.new("Cam_cenital")
    cam.type = "ORTHO"
    alto = R_EXTERIOR - R_INTERIOR
    ancho = 2 * R_EXTERIOR * math.sin(MEDIO_ANGULO)
    cam.ortho_scale = max(ancho, alto * RES_X / RES_Y) * 1.08
    cam.clip_start = 0.01
    cenital = bpy.data.objects.new("Cam_cenital", cam)
    cenital.location = (0, -(R_INTERIOR + R_EXTERIOR) / 2, 1.2)
    escena.collection.objects.link(cenital)
    return sentada, cenital


def render(escena, camara, ruta):
    escena.camera = camara
    escena.render.filepath = ruta
    bpy.ops.render.render(write_still=True)


# ---------------------------------------------------------------------------------------------------------
# Exportación GLB
# ---------------------------------------------------------------------------------------------------------
def opciones_gltf():
    admitidas = bpy.ops.export_scene.gltf.get_rna_type().properties.keys()
    deseadas = {"export_format": "GLB", "use_selection": True, "export_apply": True, "export_lights": False,
                "export_cameras": False, "export_materials": "EXPORT", "export_vertex_color": "MATERIAL",
                "export_all_vertex_colors": False, "export_yup": True}
    return {k: v for k, v in deseadas.items() if k in admitidas}


def exportar(ruta, objetos):
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objetos:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objetos[0]
    bpy.ops.export_scene.gltf(filepath=ruta, **opciones_gltf())


# ---------------------------------------------------------------------------------------------------------
def argumentos():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser(prog="gen_bosque_lowpoly.py")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default=os.path.join("Saved", "Art", "prueba-c", "ronda6"))
    # Dónde va el trono respecto a la losa del Nicho (ronda 6, a decidir por el autor): "detras", al fondo
    # del ramal mirando hacia la losa (el jugador ve su respaldo), o "lateral", al lado de la losa y de cara
    # al asiento del jugador (ve su frente y la losa queda libre).
    # Tercera opción, "nicho" (propuesta del tutor del 19/10/2026): no hay trono aparte; la losa del Nicho
    # lleva un respaldo bajo y la ficha se sienta en ella mirando al Altar. El trono grande queda para el
    # Warden, en el Altar.
    ap.add_argument("--trono", choices=("detras", "lateral", "nicho"), default="detras")
    return ap.parse_args(argv)


def main():
    args = argumentos()
    salida = os.path.abspath(args.out)
    os.makedirs(os.path.join(salida, "glb"), exist_ok=True)
    rng = random.Random(args.seed)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    escena = bpy.context.scene
    escena.render.resolution_x, escena.render.resolution_y = RES_X, RES_Y
    escena.render.resolution_percentage = 100
    escena.render.image_settings.file_format = "PNG"
    crear_materiales()
    dominio = bpy.data.collections.new("Dominio_BosqueNegro")
    escena.collection.children.link(dominio)
    asientos = crear_asientos(escena)

    # Casillas: el paso entre losas sale de repartir las once del principal a lo largo de su curva; el ramal
    # usa el mismo paso.
    denso_p = catmull_rom(CAMINO_PRINCIPAL)
    paso = longitudes(denso_p)[-1] / (N_PRINCIPAL - 1)
    principal = puntos_a_distancia(denso_p, [i * paso for i in range(N_PRINCIPAL)])
    origen = principal[RAMAL_DESDE][0]
    denso_r = catmull_rom([(origen.x, origen.y)] + RAMAL_CONTROL)
    if longitudes(denso_r)[-1] < N_RAMAL * paso:
        print("AVISO: el ramal es más corto que sus losas; alarga RAMAL_CONTROL")
    ramal = puntos_a_distancia(denso_r, [(i + 1) * paso for i in range(N_RAMAL)])
    casillas = [dict(nombre=f"Casilla_{i + 1:02d}", pos=p, tan=t, tipo="normal")
                for i, (p, t) in enumerate(principal + ramal)]
    casillas[FUENTE_EN]["tipo"] = "fuente"
    casillas[NODO_EN]["tipo"] = "nodo"
    casillas[-1]["tipo"] = "nicho"
    normales = [i for i, c in enumerate(casillas) if c["tipo"] == "normal" and i != FICHA_EN]
    for i in rng.sample(normales, N_PARTIDAS):
        casillas[i]["tipo"] = "partida"
    print(f"Paso entre losas: {paso * 100:.1f} cm")

    # Grafo: conexiones entre casillas, y las dos que salen del Dominio (al Altar y al vecino).
    conexiones = [(i, i + 1) for i in range(N_PRINCIPAL - 1)]
    conexiones += [(RAMAL_DESDE, N_PRINCIPAL)] + [(N_PRINCIPAL + i, N_PRINCIPAL + i + 1) for i in range(N_RAMAL - 1)]
    p0, t0 = principal[0]
    altar = Vector((p0.x * 0.8, -R_INTERIOR + 0.01, 0))
    pf, tf = principal[-1]
    vecino = pf + tf * 0.12
    grafo = [[casillas[a]["pos"], casillas[b]["pos"]] for a, b in conexiones] + [[p0, altar], [pf, vecino]]

    # El Nicho es el final del ramal; el trono, detrás de él, mira hacia atrás por el ramal.
    fin, tangente_fin = ramal[-1]
    if args.trono == "lateral":
        # Al lado de la losa del Nicho, hacia el lado de la cuña que queda más lejos del principal, y de cara
        # al asiento del jugador (-y del tablero).
        lado = Vector((-tangente_fin.y, tangente_fin.x, 0))
        if lado.x > 0:
            lado = -lado
        trono_pos = fin + lado * 0.085
        trono_rot = 0.0
    else:
        trono_pos = fin + tangente_fin * 0.068  # ronda 6: más cerca, que el ramal acaba junto al borde exterior
        trono_rot = math.atan2(tangente_fin.y, tangente_fin.x) - math.pi / 2  # su frente (-y local) al ramal
    # El brasero, junto a la Fuente de Poder, hacia dentro de la curva y fuera del margen de las losas.
    pfu, tfu = principal[FUENTE_EN]
    centro_losas = sum((c["pos"] for c in casillas), Vector()) / len(casillas)
    hacia_dentro = centro_losas - pfu
    hacia_dentro = (hacia_dentro - tfu * hacia_dentro.dot(tfu)).normalized()
    # Ronda 6: el pilón es una estructura aparte, pegado a la losa de la Fuente por el lado contrario al
    # camino; el brasero, más allá. La estructura del Nodo, igual, por el lado de fuera de su losa.
    pilon_xy = pfu + hacia_dentro * 0.060
    brasero_xy = pfu + hacia_dentro * 0.128
    pno, tno = principal[NODO_EN]
    hacia_fuera_nodo = -(centro_losas - pno)
    hacia_fuera_nodo = (hacia_fuera_nodo - tno * hacia_fuera_nodo.dot(tno)).normalized()
    nodo_xy = pno + hacia_fuera_nodo * 0.058
    if not dentro_cuna(nodo_xy.x, nodo_xy.y, 0.03):
        nodo_xy = pno - hacia_fuera_nodo * 0.058
    # Muestra de la estructura de Duelo, por el lado de dentro de una losa normal del principal.
    pdu, tdu = principal[8]
    hacia_dentro_duelo = centro_losas - pdu
    hacia_dentro_duelo = (hacia_dentro_duelo - tdu * hacia_dentro_duelo.dot(tdu)).normalized()
    duelo_xy = pdu + hacia_dentro_duelo * 0.058
    # Muestra de la estructura del Oráculo, por el lado de dentro de otra losa normal.
    por, tor = principal[2]
    hacia_dentro_oraculo = centro_losas - por
    hacia_dentro_oraculo = (hacia_dentro_oraculo - tor * hacia_dentro_oraculo.dot(tor)).normalized()
    oraculo_xy = por + hacia_dentro_oraculo * 0.056
    terreno = Terreno(rng, grafo, [(trono_pos.x, trono_pos.y, 0.1), (pilon_xy.x, pilon_xy.y, 0.04),
                                  (brasero_xy.x, brasero_xy.y, 0.06), (nodo_xy.x, nodo_xy.y, 0.05),
                                  (duelo_xy.x, duelo_xy.y, 0.05), (oraculo_xy.x, oraculo_xy.y, 0.04)])

    piezas = {"peana": malla("peana", pieza_peana(rng)),
              "suelo": malla("suelo", pieza_suelo(rng, terreno, grafo))}
    instancias = [instanciar("Peana", piezas["peana"], (0, 0, 0), coleccion=dominio),
                  instanciar("Suelo", piezas["suelo"], (0, 0, 0), coleccion=dominio)]
    suelo = instancias[1]

    piezas["losa"] = malla("losa", pieza_losa(rng))
    piezas["losa_partida"] = malla("losa_partida", pieza_losa_partida(rng))
    piezas["losa_nicho"] = malla("losa_nicho", pieza_losa_nicho(rng, con_respaldo=args.trono == "nicho"))
    piezas["losa_fuente"] = malla("losa_fuente", pieza_losa_fuente(rng))
    piezas["losa_nodo"] = malla("losa_nodo", pieza_losa_nodo(rng))
    malla_de = {"normal": "losa", "partida": "losa_partida", "nicho": "losa_nicho", "fuente": "losa_fuente",
                "nodo": "losa_nodo"}
    losas = []
    for c in casillas:
        p, t = c["pos"], c["tan"]
        rot = math.atan2(t.y, t.x)
        if c["tipo"] in ("normal", "partida"):
            rot += rng.choice((0, math.pi / 2, math.pi, -math.pi / 2)) + rng.uniform(-0.06, 0.06)
        ob = instanciar(c["nombre"], piezas[malla_de[c["tipo"]]], (p.x, p.y, SUELO_Z - 0.0008), rot,
                        coleccion=dominio)
        instancias.append(ob)
        losas.append(ob)

    # Senderos: un tramo de piedrecillas en el hueco entre cada par de losas conectadas, y tramos seguidos a
    # lo largo de las dos conexiones que salen del Dominio.
    piezas["sendero"] = malla("sendero", pieza_sendero(rng))
    n_sendero = 0
    for a, b in grafo:
        d = b - a
        rot = math.atan2(d.y, d.x) + rng.choice((0, math.pi))
        if (a, b) in ((p0, altar), (pf, vecino)):
            puntos, s = [], LOSA_MEDIO + 0.014
            while s < d.length:
                puntos.append(a + d.normalized() * s)
                s += 0.024
        else:
            puntos = [(a + b) / 2]
        for p in puntos:
            if dentro_cuna(p.x, p.y, 0.012):
                n_sendero += 1
                instancias.append(instanciar(f"Sendero_{n_sendero:02d}", piezas["sendero"],
                                             (p.x, p.y, terreno.altura(p.x, p.y) - 0.0004), rot, coleccion=dominio))

    piezas["trono"] = malla("trono", pieza_trono(rng), ESCALA_TRONO)
    trono = None
    if args.trono != "nicho":
        trono = instanciar("Trono", piezas["trono"], (trono_pos.x, trono_pos.y, SUELO_Z - 0.001),
                           trono_rot, coleccion=dominio)
        instancias.append(trono)
    actualizar()
    piezas["brasero"] = malla("brasero", pieza_brasero(rng), ESCALA_BRASERO)
    bz = terreno.altura(brasero_xy.x, brasero_xy.y) - 0.001
    instancias.append(instanciar("Brasero", piezas["brasero"], (brasero_xy.x, brasero_xy.y, bz),
                                 rng.uniform(0, math.tau), coleccion=dominio))
    # Ronda 6: pilón con su runa flotante junto a la Fuente; arco Planar junto al Nodo; el Altar en el
    # centro de la mesa; un Sello de muestra sobre una losa normal.
    piezas["pilon"] = malla("pilon", pieza_pilon(rng))
    piezas["runa_flotante"] = malla("runa_flotante", pieza_runa_flotante(rng))
    pz = terreno.altura(pilon_xy.x, pilon_xy.y) - 0.001
    instancias.append(instanciar("Pilon", piezas["pilon"], (pilon_xy.x, pilon_xy.y, pz),
                                 math.atan2(-hacia_dentro.y, -hacia_dentro.x), coleccion=dominio))
    instancias.append(instanciar("Runa_flotante", piezas["runa_flotante"], (pilon_xy.x, pilon_xy.y, pz + 0.034),
                                 coleccion=dominio))
    piezas["nodo_planar"] = malla("nodo_planar", pieza_nodo_planar(rng))
    piezas["nodo_duelo"] = malla("nodo_duelo", pieza_nodo_duelo(rng))
    piezas["nodo_oraculo"] = malla("nodo_oraculo", pieza_nodo_oraculo(rng))
    instancias.append(instanciar("Nodo_planar", piezas["nodo_planar"],
                                 (nodo_xy.x, nodo_xy.y, terreno.altura(nodo_xy.x, nodo_xy.y) - 0.001),
                                 math.atan2(tno.y, tno.x), coleccion=dominio))
    instancias.append(instanciar("Nodo_duelo_muestra", piezas["nodo_duelo"],
                                 (duelo_xy.x, duelo_xy.y, terreno.altura(duelo_xy.x, duelo_xy.y) - 0.002),
                                 math.atan2(tdu.y, tdu.x), coleccion=dominio))
    instancias.append(instanciar("Nodo_oraculo_muestra", piezas["nodo_oraculo"],
                                 (oraculo_xy.x, oraculo_xy.y, terreno.altura(oraculo_xy.x, oraculo_xy.y) - 0.001),
                                 rng.uniform(0, math.tau), coleccion=dominio))
    piezas["altar"] = malla("altar", pieza_altar(rng))
    instancias.append(instanciar("Altar", piezas["altar"], (0, 0, 0), coleccion=dominio))
    piezas["sello"] = malla("sello", pieza_sello(rng))
    sello_en = next(i for i, c in enumerate(casillas) if c["tipo"] == "normal" and i != FICHA_EN and i > NODO_EN)
    ps = casillas[sello_en]["pos"]
    instancias.append(instanciar("Sello_muestra", piezas["sello"], (ps.x, ps.y, SUELO_Z - 0.0008 + 0.0055),
                                 losas[sello_en].rotation_euler.z, coleccion=dominio))
    actualizar()
    vis = Visibilidad(escena, asientos, losas)
    # El trono es lo más alto y está en la punta: desde el asiento de enfrente puede tapar entera una losa
    # del ramal. Se prueban pequeñas variantes (a un lado, más atrás, algo más bajo) hasta que no oculte
    # ninguna, sin acercarse a menos de media losa de margen de ninguna casilla.
    lateral = Vector((-tangente_fin.y, tangente_fin.x, 0))
    base_trono = trono.location.copy() if trono else trono_pos.copy()
    for dl, da, f in (() if trono is None else
                      ((0, 0, 1.0), (0.012, 0, 1.0), (-0.012, 0, 1.0), (0, 0.012, 1.0), (0.022, 0, 1.0),
                       (-0.022, 0, 1.0), (0.012, 0.012, 1.0), (-0.012, 0.012, 1.0), (0, 0, 0.92),
                       (0.012, 0, 0.92), (-0.012, 0, 0.92), (0, 0, 0.85))):
        p = base_trono + lateral * dl + tangente_fin * da
        if any((p.to_2d() - c["pos"].to_2d()).length < LOSA_MEDIO + 0.045 * f for c in casillas[:-1]):
            continue
        if dist_borde(p.x, p.y) < 0.04:
            continue
        trono.location, trono.scale = p, (f, f, f)
        actualizar()
        if not any(vis.informe(instancias).values()):
            break
    else:
        if trono:
            trono.location, trono.scale = base_trono, (1, 1, 1)
            actualizar()
    if trono and ((trono.location - base_trono).length > 1e-6 or trono.scale[0] != 1):
        print(f"Trono desplazado {(trono.location - base_trono).length * 100:.1f} cm y escalado "
              f"{trono.scale[0]:.2f} para no ocultar ninguna losa")
    # El cuervo, en la cima partida del pilar derecho del trono, mirando hacia el asiento principal. Sin
    # trono (variante "nicho"), en el remate del respaldo de la losa del Nicho.
    piezas["cuervo"] = malla("cuervo", pieza_cuervo(rng))
    if trono:
        trono_pos = trono.location.copy()
        percha = trono.matrix_world @ percha_del_cuervo()
    else:
        percha = losas[-1].matrix_world @ Vector((0.046, -0.026, 0.048))
    instancias.append(instanciar("Cuervo", piezas["cuervo"], percha,
                                 math.atan2(-1.54 - percha.y, -percha.x) + rng.uniform(-0.5, 0.5), 1.0, dominio))

    for v in "ABC":
        piezas[f"arbol_{v}"] = malla(f"arbol_{v}", pieza_arbol(rng), ESCALA_ARBOL)
        piezas[f"roca_{v}"] = malla(f"roca_{v}", pieza_roca(rng), ESCALA_ROCA)
    for n, f in (("arbol_caido", pieza_arbol_caido), ("huesos", pieza_huesos), ("cadena", pieza_cadena),
                 ("lapida_caida", pieza_lapida), ("estacas", pieza_estacas), ("jaula", pieza_jaula),
                 ("velas", pieza_velas)):
        piezas[n] = malla(n, f(rng))
    objetos_charco = ["hueso", "raiz", "cadena"]
    for i in range(N_CHARCOS):
        piezas[f"charco_{'ABC'[i]}"] = malla(f"charco_{'ABC'[i]}", pieza_charco(rng, objetos_charco[i % 3]))
    alto_trono = (alto_max(trono) - SUELO_Z) if trono else 0.20

    # Visibilidad de partida: con losas, trono y brasero. Ninguna pieza puede ocultar del todo una losa que
    # ahora se ve desde algún asiento.
    actualizar()
    base_ocultas = vis.informe(instancias)
    for cam, ocultas in base_ocultas.items():
        if ocultas:
            print(f"AVISO: desde {cam} ya están ocultas antes de la escenografía: {ocultas}")

    def no_oculta(me, escala, rot, hundir):
        def prueba(x, y):
            tmp = instanciar("_prueba", me, (x, y, terreno.altura(x, y) - hundir), rot, escala, dominio)
            actualizar()
            nuevas = vis.informe(instancias + [tmp])
            bpy.data.objects.remove(tmp)
            return all(set(nuevas[c]) <= set(base_ocultas[c]) for c in nuevas)
        return prueba

    def lejos_del_grafo(x, y):
        """Ángulo que apunta desde el punto del grafo más cercano hacia (x, y)."""
        mejor, punto = 1e9, None
        for a, b in grafo:
            ab = b - a
            t = max(0.0, min(1.0, (Vector((x, y, 0)) - a).dot(ab) / max(ab.length_squared, 1e-12)))
            q = a + ab * t
            dd = (q - Vector((x, y, 0))).length
            if dd < mejor:
                mejor, punto = dd, q
        return math.atan2(y - punto.y, x - punto.x)

    ocupados = [(trono_pos.x, trono_pos.y, 0.075), (brasero_xy.x, brasero_xy.y, 0.045),
                (pilon_xy.x, pilon_xy.y, 0.03), (nodo_xy.x, nodo_xy.y, 0.045), (duelo_xy.x, duelo_xy.y, 0.04),
                (oraculo_xy.x, oraculo_xy.y, 0.03)]
    omitidas = []

    def colocar(nombre, me, radio, pie=None, hundir=0.001, escala=1.0, rot=None, alto=False, cerca=None,
                borde=None, obligatoria=False, orientar=None):
        """Coloca una pieza de escenografía. Prueba de más a menos exigente: hueco entre vecinas de HUECOS,
        y para lo alto primero junto al perímetro. Las piezas altas no pueden ocultar ninguna losa. Si una
        pieza opcional no cabe, se omite y se avisa."""
        bordes = [borde, borde * 1.6, None] if borde else [None]
        motivos = {}
        for hueco in HUECOS:
            for b in bordes:
                r_giro = rng.uniform(0, math.tau) if rot is None else rot
                aceptar = no_oculta(me, escala, r_giro, hundir) if alto else None
                sitio = buscar_sitio(rng, ocupados, grafo, radio * escala, (pie or radio) * escala, hueco,
                                     max(0.02, (pie or radio) * escala), cerca, b, aceptar, motivos=motivos)
                if sitio:
                    x, y = sitio
                    if orientar:
                        r_giro = orientar(x, y)
                    ocupados.append((x, y, radio * escala))
                    ob = instanciar(nombre, me, (x, y, terreno.altura(x, y) - hundir), r_giro, escala, dominio)
                    instancias.append(ob)
                    return ob
        if obligatoria:
            raise RuntimeError(f"no hay sitio para {nombre} (rechazos: {motivos})")
        omitidas.append(nombre)
        print(f"AVISO: {nombre} no cabe con las reglas de separación; se omite (rechazos: {motivos})")
        return None

    # Orden de colocación: lo que va junto al trono, luego lo alto (obligatorio y al perímetro), luego lo
    # bajo de mayor a menor, para que lo pequeño rellene los huecos que dejan las piezas grandes.
    # Radio (para separarse de las vecinas) y pie (lo que ocupa en el suelo, para el margen del camino).
    # La cadena se arrastra en sentido contrario al camino, así que su pie es solo la estaca.
    colocar("Cadena", piezas["cadena"], 0.035, pie=0.012, cerca=(trono_pos.x, trono_pos.y, 0.18),
            orientar=lejos_del_grafo)
    # Lo alto: al borde, sin ocultar losas. La copa de un árbol puede asomar sobre una losa; el pie, no.
    variantes = list("ABCABC")
    rng.shuffle(variantes)
    for i, v in enumerate(variantes):
        me = piezas[f"arbol_{v}"]
        alto = max(c.co.z for c in me.vertices)
        # Ningún árbol llega al trono: el trono es lo más alto del Dominio.
        escala = min(rng.uniform(0.9, 1.1), 0.85 * alto_trono / alto)
        colocar(f"Arbol_{i + 1}", me, 0.033, pie=0.03, escala=escala, alto=True, borde=BORDE_ALTO,
                obligatoria=True)
    # Los charcos, antes que el resto de lo bajo: uno junto al brasero, para que refleje su luz.
    for i in range(N_CHARCOS):
        me = piezas[f"charco_{'ABC'[i]}"]
        cerca = (brasero_xy.x, brasero_xy.y, 0.22) if i == 0 else None
        ob = colocar(f"Charco_{i + 1}", me, 0.03, hundir=0.0, cerca=cerca)
        if ob:
            ob.location.z = terreno.altura_max(ob.location.x, ob.location.y, 0.028) - 0.0004
    # El árbol caído, tumbado junto al perímetro y en paralelo al camino más cercano.
    colocar("Arbol_caido", piezas["arbol_caido"], 0.05, pie=0.022, alto=True, borde=BORDE_ALTO,
            orientar=lambda x, y: lejos_del_grafo(x, y) + math.pi / 2)
    colocar("Jaula", piezas["jaula"], 0.03, alto=True, borde=BORDE_ALTO)
    colocar("Estacas", piezas["estacas"], 0.025, pie=0.02, alto=True, borde=BORDE_ALTO)
    colocar("Lapida_caida", piezas["lapida_caida"], 0.03, pie=0.026, alto=True, hundir=0.0,
            orientar=lambda x, y: math.atan2(-1.54 - y, -x) + math.pi / 2 + rng.uniform(-0.4, 0.4))
    colocar("Velas_1", piezas["velas"], 0.018, cerca=(trono_pos.x, trono_pos.y, 0.18))
    for i in range(N_HUESOS):
        colocar(f"Huesos_{i + 1}", piezas["huesos"], 0.034, pie=0.03)
    for i in range(N_ROCAS):
        colocar(f"Roca_{i + 1}", piezas[f"roca_{'ABC'[i % 3]}"], 0.03, hundir=0.002, alto=True,
                escala=rng.uniform(0.8, 1.1))
    colocar("Velas_2", piezas["velas"], 0.018)

    # Suelo liso del kit: la cuña sin senderos pintados, para que la vista coloque losas y senderos desde
    # los datos del tablero. Solo se exporta; el render de conjunto usa el suelo con senderos.
    piezas["suelo_liso"] = malla("suelo_liso", pieza_suelo(rng, Terreno(rng, [], []), []))

    # Ficha de prueba sobre una losa del principal. No es escenografía: fuera del recuento del Dominio.
    ficha_me = malla("ficha_prueba", pieza_ficha(rng))
    fp = principal[FICHA_EN][0]
    ficha = instanciar("Ficha_prueba", ficha_me, (fp.x, fp.y, SUELO_Z - 0.0008 + 0.0036))

    actualizar()
    segundos = hornear_oclusion(piezas, instancias, suelo, grafo, sin_oclusion=("peana", "suelo_liso", "runa_flotante"))
    print(f"Oclusión en el color por vértice: {segundos:.0f} s")

    # Visibilidad final desde los cuatro asientos (sin la ficha, que es de prueba).
    final = vis.informe(instancias)
    print("\n| Asiento | Losas visibles | Ocultas del todo |\n|---|---:|---|")
    for cam, ocultas in final.items():
        print(f"| {cam.replace('Cam_asiento_', '')} | {len(losas) - len(ocultas)} de {len(losas)} | "
              f"{', '.join(ocultas) or 'ninguna'} |")

    # Recuento de triángulos y comprobación del presupuesto.
    tris = {n: len(me.polygons) for n, me in piezas.items()}
    usos = {n: 0 for n in piezas}
    for ob in instancias:
        usos[ob.data.name] += 1
    total = sum(tris[n] * usos[n] for n in piezas)
    print("\n| Pieza | Triángulos | Instancias | Subtotal |\n|---|---:|---:|---:|")
    for n in piezas:
        tope = next((t for pref, t in TRIS_MAX.items() if n.startswith(pref)), None)
        aviso = f"  <-- SUPERA {tope}" if tope and tris[n] > tope else ""
        print(f"| {n} | {tris[n]} | {usos[n]} | {tris[n] * usos[n]} |{aviso}")
    print(f"| **Total del Dominio** | | {len(instancias)} | **{total}** |")
    print(f"| ficha_prueba (fuera del Dominio) | {len(ficha_me.polygons)} | 1 | |")
    if total >= TRIS_MAX_DOMINIO:
        print(f"AVISO: el Dominio supera {TRIS_MAX_DOMINIO} triángulos")
    if omitidas:
        print(f"AVISO: piezas omitidas por falta de sitio: {omitidas}")
    mas_alto = max(instancias, key=alto_max)
    print(f"Pieza más alta: {mas_alto.name} ({(alto_max(mas_alto) - SUELO_Z) * 100:.1f} cm sobre el suelo)")

    # Colocación, grafo y visibilidad en datos: lo que un cargador de Unreal necesitaría.
    def fila(ob):
        return {"objeto": ob.name, "pieza": ob.data.name, "posicion_m": [round(c, 5) for c in ob.location],
                "rotacion_z_rad": round(ob.rotation_euler.z, 4), "escala": round(ob.scale.x, 4)}

    nombres = [c["nombre"] for c in casillas]
    with open(os.path.join(salida, "colocacion.json"), "w", encoding="utf-8") as f:
        json.dump({"semilla": args.seed, "radio_interior_m": R_INTERIOR, "radio_exterior_m": R_EXTERIOR,
                   "angulo_grados": math.degrees(2 * MEDIO_ANGULO), "paso_losas_m": round(paso, 4),
                   "grafo": {"casillas": [{"nombre": c["nombre"], "tipo": c["tipo"]} for c in casillas],
                             "conexiones": [[nombres[a], nombres[b]] for a, b in conexiones]
                             + [[nombres[0], "Altar"], [nombres[N_PRINCIPAL - 1], "Dominio_vecino"]]},
                   "visibilidad": {c.replace("Cam_asiento_", ""): {"visibles": len(losas) - len(o), "ocultas": o}
                                   for c, o in final.items()},
                   "triangulos_total": total, "triangulos_por_pieza": tris, "omitidas": omitidas,
                   "instancias": [fila(ob) for ob in instancias], "ficha_prueba": fila(ficha)},
                  f, ensure_ascii=False, indent=2)

    # Manifiesto del kit (ronda 6): lo que necesita quien coloque las piezas desde los datos del tablero.
    def caja_de(me):
        xs, ys, zs = ([v.co[i] for v in me.vertices] for i in range(3))
        return {"min_m": [round(min(xs), 4), round(min(ys), 4), round(min(zs), 4)],
                "max_m": [round(max(xs), 4), round(max(ys), 4), round(max(zs), 4)]}

    notas = {
        "peana": "Cuña de un Dominio (60 grados, apotema interior 0,15 m, radio exterior 0,74 m), base negra sobre la mesa (z = 0).",
        "suelo": "Tierra de la cuña con los senderos de ESTE trazado pintados: solo para el render de conjunto.",
        "suelo_liso": "Tierra de la cuña sin senderos: la que coloca el juego, una por Dominio girada de 60 en 60 grados.",
        "losa": "Casilla normal; origen en su centro, cara superior en z = 0,0055. La ficha se posa encima.",
        "losa_partida": "Casilla normal partida, para variar; misma silueta.",
        "losa_nicho": "Casilla del Nicho: tarima con hueco; su cabecera (+x) apunta al trono.",
        "losa_fuente": "Casilla de la Fuente: cara libre, filo cian tenue. El pilón va al lado.",
        "losa_nodo": "Casilla de un Nodo: hexagonal con la marca; las puntas (±x) en la dirección del camino.",
        "sendero": "Tramo de 2,4 cm a lo largo de x: uno por hueco entre losas conectadas, varios seguidos en conexiones largas.",
        "trono": "Trono del Elegido junto al Nicho, por el lado contrario al camino; mira hacia -y (su frente, hacia la losa).",
        "pilon": "Pilón de la Fuente junto a su losa, por el lado contrario al camino.",
        "runa_flotante": "Runa de la Fuente: sobre el pilón, a unos 3,4 cm de su base, mientras haya reserva.",
        "nodo_planar": "Arco del Nodo Planar junto a su losa; el arco se abre a lo largo de x, el velo mira a ±y. Desde el Despertar.",
        "nodo_duelo": "Espadas cruzadas del Nodo de Duelo junto a su losa. Desde el Despertar.",
        "nodo_oraculo": "Pedestal con orbe del Nodo del Oráculo junto a su losa. Desde el Despertar.",
        "altar": "El Altar en el centro de la mesa (z = 0); encaja con los seis Dominios por su apotema de 0,15 m. Mira hacia -y.",
        "sello": "Sello sobre una casilla: origen en el centro de la losa, base a la altura de su cara (z de la losa + 0,0055).",
        "brasero": "Escenografía con luz: a mano, una por Dominio.",
        "ficha_prueba": "Carta de 6 x 9 cm con peana: referencia de escala de las fichas.",
    }
    with open(os.path.join(salida, "kit.json"), "w", encoding="utf-8") as f:
        json.dump({"ronda": 6, "semilla": args.seed,
                   "unidades": "metros en el GLB; Unreal los importa en centímetros (1 m = 100 cm), sin escalar",
                   "paleta_srgb": PALETA,
                   "materiales": list(MATERIALES),
                   "convenciones": ["Origen de cada pieza en su centro y base en z = 0 (las losas, con la cara en z = 0,0055).",
                                    "Las estructuras se colocan junto a su losa, tocando su borde por el lado contrario al camino.",
                                    "Cada Dominio es la misma cuña girada de 60 en 60 grados alrededor del Altar.",
                                    "El color va por vértice (atributo Col); el cian es el único acento: M_Paleta_Emisivo "
                                    "(brasero, runa) y M_Paleta_Tenue (filo, agua del pilón, velo, orbe)."],
                   "piezas": {n: {"triangulos": len(me.polygons), "caja_m": caja_de(me), "nota": notas.get(n, "")}
                              for n, me in list(piezas.items()) + [("ficha_prueba", ficha_me)]}},
                  f, ensure_ascii=False, indent=2)

    # GLB: uno por pieza (en el origen), la ficha aparte, y el conjunto del Dominio (colocado, sin ficha).
    for n, me in list(piezas.items()) + [("ficha_prueba", ficha_me)]:
        tmp = instanciar(f"_export_{n}", me, (0, 0, 0))
        exportar(os.path.join(salida, "glb", f"{n}.glb"), [tmp])
        bpy.data.objects.remove(tmp)
    exportar(os.path.join(salida, "glb", "bosque_negro_conjunto.glb"), instancias)

    sentada, cenital = preparar_render(escena, (brasero_xy.x, brasero_xy.y, bz))
    render(escena, sentada, os.path.join(salida, "vista_sentada.png"))
    render(escena, cenital, os.path.join(salida, "vista_cenital.png"))
    for cam in asientos:
        render(escena, cam, os.path.join(salida, f"vista_{cam.name.replace('Cam_', '').lower()}.png"))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(salida, "bosque_negro.blend"))
    print(f"Hecho: {salida} (semilla {args.seed})")


if __name__ == "__main__":
    main()
