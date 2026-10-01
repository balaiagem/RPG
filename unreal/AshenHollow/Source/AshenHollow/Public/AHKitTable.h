// GENERATED FILE -- do not edit by hand.
//
// Written by Scripts/blender/Export-Kit.py from Art/Kit/kit.json, which the
// same script writes while it models the kit. Regenerate both together.
//
// WHY THIS FILE EXISTS
// --------------------
// For three weeks the generator reasoned about a bounding box it measured off
// the asset at runtime, and lost, over and over, to geometry that lived
// outside that box: a banner's mast, a tent's guy rope, the newel posts at the
// mouth of a bridge. And nothing anywhere recorded which face of a house was
// its front, so "face the street" was a guess that was wrong half the time.
//
// Here the footprint is DECLARED. It is authored in Blender alongside the mesh
// -- the piece is modelled INSIDE it -- exported with it, and compiled in. A
// piece cannot disagree with its own footprint, and the offline harness sees
// exactly the numbers the game will.
//
// Sizes are full width, depth and height in centimetres. The pivot is on the
// ground at the centre of the footprint; a piece with a door has that door
// facing +X before rotation.
#pragma once

#include "CoreMinimal.h"

namespace AHKit
{
    /** Where the kit lands once Importar-Kit.cmd has run. */
    inline const TCHAR* const Folder = TEXT("/Game/AshenHollow/Kit/Meshes/");

    struct FKitPiece
    {
        const TCHAR* Name;
        float Wide;      // along X, centimetres
        float Deep;      // along Y, centimetres
        float Tall;      // centimetres
        bool  bHasDoor;  // the door faces +X before rotation
    };

    inline const FKitPiece Pieces[] =
    {
        { TEXT("SM_Kit_casa_a"),        520.0f,   440.0f,   585.9f, true  },
        { TEXT("SM_Kit_casa_b"),        660.0f,   520.0f,   896.7f, true  },
        { TEXT("SM_Kit_casa_c"),        800.0f,   600.0f,   995.1f, true  },
        { TEXT("SM_Kit_casa_d"),        460.0f,   400.0f,   546.6f, true  },
        { TEXT("SM_Kit_casa_e"),        720.0f,   480.0f,   595.2f, true  },
        { TEXT("SM_Kit_casa_f"),        580.0f,   580.0f,   970.3f, true  },
        { TEXT("SM_Kit_casa_g"),        900.0f,   540.0f,   921.5f, true  },
        { TEXT("SM_Kit_casa_h"),        420.0f,   360.0f,   517.4f, true  },
        { TEXT("SM_Kit_celeiro"),      1000.0f,   700.0f,   809.5f, true  },
        { TEXT("SM_Kit_torre"),         380.0f,   380.0f,  1116.5f, true  },
        { TEXT("SM_Kit_poco"),          220.0f,   220.0f,   336.5f, false },
        { TEXT("SM_Kit_banca"),         280.0f,   360.0f,   243.6f, true  },
        { TEXT("SM_Kit_ponte"),         800.0f,   440.0f,   131.0f, false },
        { TEXT("SM_Kit_muro"),          400.0f,    70.0f,   221.0f, false },
        { TEXT("SM_Kit_cerca"),         400.0f,    30.0f,   116.0f, false },
        { TEXT("SM_Kit_portao"),        120.0f,   560.0f,   487.5f, false },
        { TEXT("SM_Kit_placa"),         100.0f,   220.0f,   277.0f, false },
        { TEXT("SM_Kit_tenda"),         300.0f,   260.0f,   186.0f, false },
        { TEXT("SM_Kit_altar"),         200.0f,   140.0f,   133.8f, false },
        { TEXT("SM_Kit_arco"),          120.0f,   420.0f,   473.0f, false },
        { TEXT("SM_Kit_lapide"),        110.0f,    90.0f,   102.0f, false },
        { TEXT("SM_Kit_torre_ruina"),   440.0f,   440.0f,   587.0f, false },
        { TEXT("SM_Kit_palicada"),      360.0f,    50.0f,   253.0f, false },
        { TEXT("SM_Kit_lampiao"),        70.0f,    70.0f,   344.9f, false },
        { TEXT("SM_Kit_menir"),         140.0f,   110.0f,   240.7f, false },
        { TEXT("SM_Kit_piso"),          400.0f,   400.0f,    13.4f, false },
        { TEXT("SM_Kit_arvore_a"),      320.0f,   320.0f,   583.2f, false },
        { TEXT("SM_Kit_arvore_b"),      380.0f,   380.0f,   843.8f, false },
        { TEXT("SM_Kit_arvore_c"),      260.0f,   260.0f,   423.0f, false },
        { TEXT("SM_Kit_grama"),          50.0f,    50.0f,    33.5f, false },
        { TEXT("SM_Kit_arbusto"),       180.0f,   180.0f,   112.0f, false },
        { TEXT("SM_Kit_pedra_a"),       240.0f,   200.0f,   133.4f, false },
        { TEXT("SM_Kit_pedra_b"),       130.0f,   110.0f,    72.9f, false },
        { TEXT("SM_Kit_pedra_c"),       360.0f,   280.0f,   220.2f, false },
        { TEXT("SM_Kit_toco"),          120.0f,   120.0f,    66.0f, false },
        { TEXT("SM_Kit_barril"),         80.0f,    80.0f,    90.0f, false },
        { TEXT("SM_Kit_caixa"),          90.0f,    90.0f,    87.0f, false },
        { TEXT("SM_Kit_mochila"),        52.0f,    48.0f,    69.5f, false },
        { TEXT("SM_Kit_galinha"),        34.0f,    46.0f,    33.6f, false },
        { TEXT("SM_Kit_veado"),          66.0f,   170.0f,   117.8f, false },
        { TEXT("SM_Kit_corvo"),          92.0f,    46.0f,    10.2f, false },
        { TEXT("SM_Kit_fardo"),         150.0f,   110.0f,   112.2f, false },
        { TEXT("SM_Kit_fogueira"),      180.0f,   180.0f,    54.0f, false },
        { TEXT("SM_Kit_carroca"),       280.0f,   170.0f,   133.0f, false },
        { TEXT("SM_Kit_bandeira"),       60.0f,   160.0f,   309.0f, false },
    };

    /**
     * The declared size of a kit piece, or nullptr when the path is not one.
     *
     * Compares whole paths rather than picking the name off the end, because
     * the offline harness's stand-in FString has no FindLastChar and a harness
     * that compiles against a different string type is a harness that lies.
     * The Contains guard keeps the thirty-odd comparisons off the hot path for
     * everything that is not a kit piece.
     */
    inline const FKitPiece* Find(const FString& Path)
    {
        if (!Path.Contains(TEXT("SM_Kit_"))) return nullptr;
        for (const FKitPiece& Piece : Pieces)
            if (Path == FString(Folder) + Piece.Name) return &Piece;
        return nullptr;
    }

    /** Full size in centimetres, or all zeroes when the path is not a kit piece. */
    inline FVector SizeOf(const FString& Path)
    {
        const FKitPiece* Found = Find(Path);
        return Found ? FVector(Found->Wide, Found->Deep, Found->Tall)
                     : FVector(0.0, 0.0, 0.0);
    }

    /** True when this piece was modelled with a front door on +X. */
    inline bool HasDoor(const FString& Path)
    {
        const FKitPiece* Found = Find(Path);
        return Found && Found->bHasDoor;
    }
}
