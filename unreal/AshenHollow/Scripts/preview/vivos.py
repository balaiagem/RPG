"""Da pra chegar em cada pessoa do vale?

A mesma pergunta que entrar.py faz das masmorras, feita agora de cada aldeao:
pinta as caixas de colisao de verdade numa grade de 20 cm, engorda tudo pelo
raio do agente do navmesh (42 cm), inunda a partir da borda de uma caixa de
doze metros em volta da pessoa e ve se o chao debaixo dela foi alcancado.

Existe porque um NPC cercado de casas e cercas e invisivel em qualquer log: o
jogo nao tem como saber que ninguem consegue falar com ele. Um flood fill sabe.
"""
import sys

import numpy as np

from entrar import AGENT, CELL, blocked_grid, flood_from_border, load

OFICIO = ["aldeao", "mercador", "lenhador", "pedreiro", "guarda", "carroceiro"]
ESPECIE = ["galinha", "veado", "corvo"]


def reachable(pieces, x, y, half=1200.0):
    grid = blocked_grid(pieces, x, y, half)
    free = ~grid
    seen = flood_from_border(free)
    mid = grid.shape[0] // 2
    # A janela e de um metro: a pessoa tem 84 cm de largura, entao "o chao
    # debaixo dela" sao umas cinco celulas da grade, nao uma.
    janela = seen[mid - 2:mid + 3, mid - 2:mid + 3]
    aberto = free[mid - 2:mid + 3, mid - 2:mid + 3]
    return int(janela.sum()), int(aberto.sum())


def main(paths):
    total = presos = entulhados = 0
    total_b = presos_b = 0
    for path in paths:
        spawn, marks, camps, pieces = load(path)
        folk, beasts = [], []
        for line in open(path):
            bit = line.split()
            if bit[0] == "GENTE":
                folk.append((float(bit[1]), float(bit[2]), float(bit[3]),
                             int(bit[4]), float(bit[5]), bit[6] == "1"))
            elif bit[0] == "BICHO":
                beasts.append((float(bit[1]), float(bit[2]), int(bit[3])))
        for x, y, _yaw, trade, _range, giver in folk:
            total += 1
            alcancado, aberto = reachable(pieces, x, y)
            if aberto == 0:
                entulhados += 1
                print("  ENTULHADO %-11s em (%.0f, %.0f)%s"
                      % (OFICIO[trade], x, y, "  <- o do recado" if giver else ""))
            elif alcancado == 0:
                presos += 1
                print("  PRESO     %-11s em (%.0f, %.0f)%s"
                      % (OFICIO[trade], x, y, "  <- o do recado" if giver else ""))
        for x, y, kind in beasts:
            if kind == 2:
                continue                      # corvo voa
            total_b += 1
            alcancado, aberto = reachable(pieces, x, y, 900.0)
            if alcancado == 0:
                # Uma galinha dentro de um quintal cercado esta onde deveria
                # estar: ela nao usa navmesh e nao tem pra onde ir. Um veado
                # cercado e um erro -- ele existe pra correr.
                cercada = kind == 0
                if not cercada:
                    presos_b += 1
                print("  %-9s %-11s em (%.0f, %.0f)"
                      % ("cercada" if cercada else "PRESO", ESPECIE[kind], x, y))
    print("%d pessoas: %d entulhadas, %d presas (%.1f%% ok)"
          % (total, entulhados, presos,
             100.0 * (total - entulhados - presos) / max(1, total)))
    print("%d bichos de chao: %d presos (%.1f%% ok)"
          % (total_b, presos_b, 100.0 * (total_b - presos_b) / max(1, total_b)))
    return 1 if (presos or entulhados or presos_b) else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
