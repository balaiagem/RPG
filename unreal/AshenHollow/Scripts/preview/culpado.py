"""Which piece is sealing the dungeon?

Removes one piece at a time and re-floods. Any piece whose absence lets the
outside reach the inside is a piece standing in the only doorway. This is the
question "nao consigo entrar" has really been asking for four rounds, and it
takes forty seconds to answer instead of a ninety-five minute build.
"""
import sys

import numpy as np

sys.path.insert(0, "/tmp/claude-0/-home-claude/b4287b6f-d956-5930-9ad1-6953d7fffa21/scratchpad")
from entrar import CELL, blocked_grid, flood_from_border, load


def inside_reached(pieces, mx, my, half, drop=None):
    keep = [p for i, p in enumerate(pieces) if i != drop]
    grid = blocked_grid(keep, mx, my, half)
    seen = flood_from_border(~grid)
    mid = grid.shape[0] // 2
    return int(seen[mid - 6:mid + 6, mid - 6:mid + 6].sum())


def main(path, mark_index):
    spawn, marks, camps, pieces = load(path)
    mx = my = None
    for index, x, y, kind in marks:
        if index == mark_index:
            mx, my, half = x, y, (2600.0 if kind == 8 else 1800.0)
    if mx is None:
        print("marco nao encontrado"); return

    base = inside_reached(pieces, mx, my, half)
    print("com tudo no lugar, miolo alcancavel: %d" % base)

    near = [i for i, p in enumerate(pieces)
            if abs(p[1] - mx) <= half + 600 and abs(p[2] - my) <= half + 600]
    print("%d pecas na vizinhanca; testando uma a uma..." % len(near))

    guilty = []
    for i in near:
        got = inside_reached(pieces, mx, my, half, drop=i)
        if got > base + 20:
            name, px, py = pieces[i][0], pieces[i][1], pieces[i][2]
            guilty.append((got, name, px, py, pieces[i][4]))
    guilty.sort(reverse=True)
    if not guilty:
        print("NENHUMA peca sozinha abre: o vao nao existe na planta.")
    for got, name, px, py, yaw in guilty[:10]:
        print("  tirando %-18s em (%.0f, %.0f) yaw %.0f  ->  miolo %d"
              % (name, px, py, yaw, got))


if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]))
