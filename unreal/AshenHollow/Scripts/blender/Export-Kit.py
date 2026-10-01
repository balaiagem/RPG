"""Exports every piece of the kit as its own FBX, plus the manifest.

One file per asset, named exactly as the asset will be named in Unreal, each
one built at the origin with no rotation and its pivot on the ground. Blender
metres become Unreal units at 100:1, and Blender +X stays Unreal +X -- which is
what makes "the door faces +X" mean the same thing in both programs.
"""
import json
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_kit as kit                                            # noqa: E402

PROLOGUE = '// GENERATED FILE -- do not edit by hand.\n//\n// Written by Scripts/blender/Export-Kit.py from Art/Kit/kit.json, which the\n// same script writes while it models the kit. Regenerate both together.\n//\n// WHY THIS FILE EXISTS\n// --------------------\n// For three weeks the generator reasoned about a bounding box it measured off\n// the asset at runtime, and lost, over and over, to geometry that lived\n// outside that box: a banner\'s mast, a tent\'s guy rope, the newel posts at the\n// mouth of a bridge. And nothing anywhere recorded which face of a house was\n// its front, so "face the street" was a guess that was wrong half the time.\n//\n// Here the footprint is DECLARED. It is authored in Blender alongside the mesh\n// -- the piece is modelled INSIDE it -- exported with it, and compiled in. A\n// piece cannot disagree with its own footprint, and the offline harness sees\n// exactly the numbers the game will.\n//\n// Sizes are full width, depth and height in centimetres. The pivot is on the\n// ground at the centre of the footprint; a piece with a door has that door\n// facing +X before rotation.\n#pragma once\n\n#include "CoreMinimal.h"\n\nnamespace AHKit\n{\n    /** Where the kit lands once Importar-Kit.cmd has run. */\n    inline const TCHAR* const Folder = TEXT("/Game/AshenHollow/Kit/Meshes/");\n\n    struct FKitPiece\n    {\n        const TCHAR* Name;\n        float Wide;      // along X, centimetres\n        float Deep;      // along Y, centimetres\n        float Tall;      // centimetres\n        bool  bHasDoor;  // the door faces +X before rotation\n    };\n\n    inline const FKitPiece Pieces[] =\n    {\n'

EPILOGUE = '    };\n\n    /**\n     * The declared size of a kit piece, or nullptr when the path is not one.\n     *\n     * Compares whole paths rather than picking the name off the end, because\n     * the offline harness\'s stand-in FString has no FindLastChar and a harness\n     * that compiles against a different string type is a harness that lies.\n     * The Contains guard keeps the thirty-odd comparisons off the hot path for\n     * everything that is not a kit piece.\n     */\n    inline const FKitPiece* Find(const FString& Path)\n    {\n        if (!Path.Contains(TEXT("SM_Kit_"))) return nullptr;\n        for (const FKitPiece& Piece : Pieces)\n            if (Path == FString(Folder) + Piece.Name) return &Piece;\n        return nullptr;\n    }\n\n    /** Full size in centimetres, or all zeroes when the path is not a kit piece. */\n    inline FVector SizeOf(const FString& Path)\n    {\n        const FKitPiece* Found = Find(Path);\n        return Found ? FVector(Found->Wide, Found->Deep, Found->Tall)\n                     : FVector(0.0, 0.0, 0.0);\n    }\n\n    /** True when this piece was modelled with a front door on +X. */\n    inline bool HasDoor(const FString& Path)\n    {\n        const FKitPiece* Found = Find(Path);\n        return Found && Found->bHasDoor;\n    }\n}\n'

FBX = os.path.join(kit.OUT, "fbx")
os.makedirs(FBX, exist_ok=True)



def write_table(manifest):
    """Writes Public/AHKitTable.h from the manifest we have just built.

    The table used to be maintained by hand beside the kit, which meant the
    generator could be reasoning about a footprint the mesh no longer had --
    the exact class of silent disagreement this whole kit exists to remove.
    One script models the pieces, exports them, and writes the numbers the
    game compiles against, so they cannot drift.
    """
    rows = []
    for entry in manifest["pieces"]:
        head = '        {{ TEXT("{0}"),'.format(entry["name"])
        # The f suffixes are not decoration: the fields are floats and MSVC
        # warns C4305 on every one of forty-two rows without them.
        rows.append("{0}{1:7.1f}f, {2:7.1f}f, {3:7.1f}f, {4:<5} }},".format(
            head.ljust(38),
            entry["footprint_x_m"] * 100.0,
            entry["footprint_y_m"] * 100.0,
            entry["height_m"] * 100.0,
            "true" if entry["door_yaw_deg"] is not None else "false"))
    text = PROLOGUE + "\n".join(rows) + "\n" + EPILOGUE
    path = os.path.join(kit.OUT, "AHKitTable.h")
    with open(path, "w", encoding="utf-8", newline="\r\n") as out:
        out.write(text)
    return path


def main():
    kit.clear()
    catalogue = kit.build_all()

    for entry in catalogue:
        obj = bpy.data.objects[entry["name"]]
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        # The UCX objects go in the same file as the mesh they belong to, or
        # Unreal builds a convex hull round the whole asset instead -- which
        # for a tree is a five-metre invisible cylinder.
        for shape in entry.get("ucx", []):
            if shape in bpy.data.objects:
                bpy.data.objects[shape].select_set(True)
        bpy.context.view_layer.objects.active = obj
        path = os.path.join(FBX, entry["name"] + ".fbx")
        bpy.ops.export_scene.fbx(
            filepath=path,
            use_selection=True,
            object_types={"MESH"},
            apply_unit_scale=True,
            global_scale=1.0,
            apply_scale_options="FBX_SCALE_NONE",
            bake_space_transform=False,
            axis_forward="-Z",
            axis_up="Y",
            mesh_smooth_type="FACE",
            use_mesh_modifiers=True,
            use_triangles=False,
            path_mode="COPY",
        )
        entry["fbx"] = entry["name"] + ".fbx"
        entry["materials"] = [slot.material.name for slot in obj.material_slots
                              if slot.material]

    manifest = {
        "unit": "metre",
        # What finish() baked the per-face shading around. Import-Kit.py
        # multiplies the vertex colour back up by 1 / this, so the average
        # colour on screen is exactly the palette's and the variation rides
        # on top of it. The two numbers have to travel together or the whole
        # kit comes out darker than it was authored.
        "grain_centre": round(sum(kit.SHADES) / max(1, len(kit.SHADES)), 5),
        "pivot": "base centre",
        "door_axis": "+X",
        "unreal_scale": 100,
        "palette": {("M_Kit_" + k): {"rgb": [round(c, 4) for c in v[0]],
                                     "roughness": v[1],
                                     "emissive": k in kit.EMISSIVE}
                    for k, v in kit.PALETTE.items()},
        "pieces": catalogue,
    }
    with open(os.path.join(kit.OUT, "kit.json"), "w", encoding="utf-8") as out:
        json.dump(manifest, out, indent=2, ensure_ascii=False)

    write_table(manifest)

    print(json.dumps({
        "fbx": len(catalogue),
        "faces": sum(e["faces"] for e in catalogue),
        "materiais": len(manifest["palette"]),
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
