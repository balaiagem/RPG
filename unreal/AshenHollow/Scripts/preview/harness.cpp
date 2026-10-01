// Builds valleys with the real AHArena::Build and checks what came out.
//
// Two jobs. It prints a map so a layout can be judged by eye before anyone
// spends an editor run on it, and it asserts the same invariants the automation
// test asserts, over far more seeds than a test run wants to sit through.
#include "AHArena.h"
#include <cstdio>
#include <cmath>
#include <fstream>
#include <map>
#include <vector>
#include <string>
/**
 * Which two meshes, when the spacing invariant fires.
 *
 * Added the day the kit arrived and the count went from zero to fifteen
 * thousand: knowing the number is useless, knowing it was "cerca + cerca"
 * pointed straight at the one line that was wrong. A failing invariant that
 * cannot say what failed costs an hour every time it fires.
 */
static std::map<std::string, int> GBlame;

static const char* Glyph(EAHCell Cell)
{
    switch (Cell)
    {
        case EAHCell::Field:   return "\"";
        case EAHCell::Wood:    return "T";
        case EAHCell::Village: return "H";
        case EAHCell::Ruins:   return "r";
        case EAHCell::Quarry:  return "q";
        case EAHCell::Road:    return "=";
        case EAHCell::Plaza:   return "O";
        case EAHCell::Water:   return "~";
        case EAHCell::Bridge:  return "#";
        default:               return ".";
    }
}

/**
 * A bridge IS road, and forgetting that is how the connectivity check starts
 * reporting a river as a broken road. Whatever the generator calls walkable,
 * this has to call walkable too.
 */
static bool Paved(const FAHArenaPlan& Plan, int32 Cx, int32 Cy)
{
    const EAHCell Kind = Plan.At(Cx, Cy);
    return Kind == EAHCell::Road || Kind == EAHCell::Plaza || Kind == EAHCell::Bridge;
}

static int32 WalkableFrom(const FAHArenaPlan& Plan, int32 StartX, int32 StartY)
{
    const int32 Side = AHArena::GridSide;
    std::vector<bool> Seen(static_cast<size_t>(Side * Side), false);
    if (!Paved(Plan, StartX, StartY)) return 0;
    std::vector<int32> Edge{ StartX * Side + StartY };
    Seen[static_cast<size_t>(Edge[0])] = true;
    int32 Reached = 0;
    const int32 StepX[4] = { 1, 0, -1, 0 };
    const int32 StepY[4] = { 0, 1, 0, -1 };
    while (!Edge.empty())
    {
        const int32 Key = Edge.back(); Edge.pop_back();
        ++Reached;
        for (int32 Dir = 0; Dir < 4; ++Dir)
        {
            const int32 Nx = Key / Side + StepX[Dir];
            const int32 Ny = Key % Side + StepY[Dir];
            if (!AHArena::InGrid(Nx, Ny) || !Paved(Plan, Nx, Ny)) continue;
            if (Seen[static_cast<size_t>(Nx * Side + Ny)]) continue;
            Seen[static_cast<size_t>(Nx * Side + Ny)] = true;
            Edge.push_back(Nx * Side + Ny);
        }
    }
    return Reached;
}

static void Show(const FAHArenaPlan& Plan)
{
    const int32 Side = AHArena::GridSide;
    for (int32 Cy = Side - 1; Cy >= 0; --Cy)
    {
        std::string Row;
        for (int32 Cx = 0; Cx < Side; ++Cx)
        {
            const FVector Here = AHArena::CellCentre(Cx, Cy);
            const char* Mark = Glyph(Plan.At(Cx, Cy));
            for (const FAHCampSpot& Camp : Plan.Camps)
                if (FVector::Dist2D(Here, Camp.Where) < 10.0) Mark = "x";
            if (Cx == AHArena::SpawnCellX && Cy == AHArena::SpawnCellY) Mark = "@";
            Row += Mark; Row += ' ';
        }
        std::printf("%s\n", Row.c_str());
    }
}

/** The ground as one digit per cell: 0 is the valley floor, 9 the hilltops. */
static void Relief(const FAHArenaPlan& Plan)
{
    const int32 Side = AHArena::GridSide;
    double Highest = 1.0;
    for (const float H : Plan.Heights) if (H > Highest) Highest = H;
    for (int32 Cy = Side - 1; Cy >= 0; --Cy)
    {
        std::string Row;
        for (int32 Cx = 0; Cx < Side; ++Cx)
        {
            const double H = Plan.HeightAt(Cx * AHArena::SubSteps, Cy * AHArena::SubSteps);
            if (Plan.At(Cx, Cy) == EAHCell::Water) { Row += "~ "; continue; }
            const int32 Step = static_cast<int32>(9.0 * H / Highest);
            Row += static_cast<char>('0' + (Step < 0 ? 0 : Step > 9 ? 9 : Step));
            Row += ' ';
        }
        std::printf("%s\n", Row.c_str());
    }
    std::printf("  (0 = fundo do vale, 9 = %.0f cm acima)\n", Highest);
}

/**
 * The real terrain, read from the file the heightmap script wrote.
 *
 * This is the point of the whole exercise: what the harness checks is the
 * ground the player actually walks on, not an imitation of it. terrain.bin is
 * a 257 x 257 grid of metres sampled from heightmap_ashen_1km.png, indexed
 * [x][y], covering the Landscape's full 100 800 uu.
 */
struct FTerrain
{
    int32 N = 0;
    std::vector<float> Metres;
    double HalfUU = 0.0;

    bool Load(const char* Path)
    {
        std::ifstream File(Path, std::ios::binary);
        if (!File) return false;
        File.read(reinterpret_cast<char*>(&N), sizeof(N));
        if (N <= 1) return false;
        Metres.resize(static_cast<size_t>(N) * N);
        File.read(reinterpret_cast<char*>(Metres.data()),
                  static_cast<std::streamsize>(Metres.size() * sizeof(float)));
        HalfUU = 100800.0 * 0.5;
        return static_cast<bool>(File);
    }

    float At(float X, float Y) const
    {
        const double Step = (2.0 * HalfUU) / (N - 1);
        double Fx = (X + HalfUU) / Step;
        double Fy = (Y + HalfUU) / Step;
        Fx = Fx < 0 ? 0 : (Fx > N - 1.001 ? N - 1.001 : Fx);
        Fy = Fy < 0 ? 0 : (Fy > N - 1.001 ? N - 1.001 : Fy);
        const int32 X0 = static_cast<int32>(Fx), Y0 = static_cast<int32>(Fy);
        const double Tx = Fx - X0, Ty = Fy - Y0;
        const float A = Metres[static_cast<size_t>(X0) * N + Y0];
        const float B = Metres[static_cast<size_t>(X0 + 1) * N + Y0];
        const float C = Metres[static_cast<size_t>(X0) * N + Y0 + 1];
        const float D = Metres[static_cast<size_t>(X0 + 1) * N + Y0 + 1];
        const double Low  = A + (B - A) * Tx;
        const double High = C + (D - C) * Tx;
        return static_cast<float>((Low + (High - Low) * Ty) * 100.0);   // metres -> uu
    }
};

static FTerrain GTerrain;

int main(int argc, char** argv)
{
    if (!GTerrain.Load("Scripts/preview/terrain.bin")
        && !GTerrain.Load("terrain.bin"))
    {
        std::printf("nao achei terrain.bin -- rode a partir da raiz do projeto\n");
        return 2;
    }
    auto Ground = [](float X, float Y) { return GTerrain.At(X, Y); };

    const int32 Worlds = argc > 1 ? std::atoi(argv[1]) : 300;
    const FVector Home(AHArena::SpawnX, AHArena::SpawnY, 110.0);
    auto Measure = [](const FString& Path) -> FVector
    {
        // Rough stand-ins for the real bounds: a fence panel is short, a wall is
        // longer, everything else is two metres of everything. Only the runs care.
        if (FString(Path).Contains("fence")) return FVector(110.0, 22.0, 90.0);
        if (FString(Path).Contains("wall_stone")) return FVector(190.0, 30.0, 120.0);
        return FVector(100.0, 100.0, 100.0);
    };

    int32 Broken = 0, NoCamps = 0, OffStreet = 0, Biggest = 0, OnSpawn = 0, Cramped = 0;
    int32 GateNotRoad = 0, GateUnreachable = 0, ArrivalNotRoad = 0;
    // The river's four silent failures. Every one of them is invisible from a
    // screenshot and ruins the map the moment you try to walk it.
    int32 CampInWater = 0, DryBlocker = 0, OpenWater = 0, BlockedBridge = 0, ThingInWater = 0;
    // The terrain, and the one rule the whole thing hangs on.
    int32 SpawnNotFlat = 0, Stacked = 0, InTheRoad = 0, OnASlope = 0;
    // The landmarks are what fills the ninety per cent of the map that has no
    // village on it, so they get invariants of their own.
    int32 MarksCrowded = 0, MarksOnRoad = 0, MarksNoFight = 0;
    // The two reserved places near the arrival shelf. A reservation that
    // quietly fails on one seed in fifty is the same as no reservation, and
    // the only way to know is to count it.
    int32 NoEarlyFight = 0, NoEarlyFort = 0;
    // A dungeon with one room is a shed, and a dungeon whose fights all sit
    // in one place is a fort with extra walls.
    int32 ThinDungeons = 0, TotalDungeons = 0;
    /**
     * The errand. Six ways for it to be broken, and every one of them looks
     * from inside the game like "the quest does not work" with no clue which
     * part failed -- which is exactly the class of bug that costs an evening
     * of Lucas's time to report and five minutes to fix once it is named.
     */
    /**
     * Anything standing where there is no Landscape under it.
     *
     * The playable grid ends at 50 000 uu and the Landscape ends at 50 400.
     * A piece past 50 400 gets a ground trace that hits nothing, keeps the
     * generator's estimated height, and hangs in the air -- which is what
     * "arvores spawnando nos ceus" was: a decorative treeline planted six to
     * fourteen metres beyond the edge ON PURPOSE, two hundred of them every
     * single world. The game had been counting them for days (`AH_GROUND 184
     * pecas sem chao sob elas`) and nobody read the number as a location.
     */
    int32 OffTheMap = 0;
    /**
     * The people and the animals.
     *
     * Same discipline as everything else in this file: a villager standing in
     * a wall, off the Landscape, in the river or on a cliff is invisible in
     * every log the game writes and obvious the moment somebody walks past
     * him. Counted over hundreds of worlds so it cannot be "rare".
     */
    int32 FolkOffMap = 0, FolkInWater = 0, FolkBuried = 0, FolkStacked = 0;
    int32 FolkOnRoad = 0, NoFolk = 0, NoTrades = 0, NoMarket = 0;
    int32 BeastOffMap = 0, BeastBuried = 0, NoBeasts = 0;
    long long TotalFolk = 0, TotalBeasts = 0;
    int32 NoErrand = 0, GiverFar = 0, GiverOnRoad = 0, GiverBuried = 0;
    int32 PrizeMissing = 0, PrizeAstray = 0, DoorAstray = 0, ErrandTooFar = 0;
    double GiverAwaySum = 0.0, ErrandWalkSum = 0.0;
    long long TotalMarks = 0;
    
    double TallestSeen = 0.0, ReliefSum = 0.0;
    long long TotalPieces = 0, TotalRoads = 0, TotalCamps = 0;
    std::map<std::string, long long> Tally;
    std::vector<int32> Sizes;

    for (int32 Seed = 0; Seed < Worlds; ++Seed)
    {
        // Every combination of gates, and arrival through each of them in turn:
        // a comarca on a corner of the world has two ways out, one in the middle
        // has four, and the player can walk in by any of them.
        // No gates on a fixed island: there is no neighbouring comarca to walk
        // into, so the player always arrives at the same place, which is the
        // one patch of ground the heightmap guarantees is level.
        const int32   Mask  = 0;
        const FVector Where = Home;
        FRandomStream Dice(Seed * 7919 + 13);
        const FAHArenaPlan World = AHArena::Build(Dice, Where, Measure, Ground, Mask);

        // Every gate has to be road, and road you can walk to from where you
        // came in -- otherwise the way out of the comarca is through a field.
        for (int32 Gate : World.Gates)
        {
            int32 Gx = 0, Gy = 0;
            AHArena::GateCellOf(Gate, Gx, Gy);
            if (!Paved(World, Gx, Gy)) ++GateNotRoad;
        }
        {
            int32 Ax = 0, Ay = 0;
            AHArena::CellOf(Where, Ax, Ay);
            if (!Paved(World, Ax, Ay)) ++ArrivalNotRoad;
            const int32 Joined = WalkableFrom(World, Ax, Ay);
            int32 Paved2 = 0;
            for (const EAHCell Cell : World.Cells)
                if (Cell == EAHCell::Road || Cell == EAHCell::Plaza
                    || Cell == EAHCell::Bridge) ++Paved2;
            if (Joined != Paved2) ++GateUnreachable;
        }

        // -- The ground -------------------------------------------------
        // The step limit that used to be checked here is gone with the ground
        // it measured: that was a height field the generator invented and had
        // to keep walkable by construction. The Landscape has cliffs on
        // purpose. What matters now is that nothing is BUILT on one.
        {
            double Lowest = 1e9, Highest = -1e9;
            for (const float H : World.Heights)
            {
                if (H < Lowest)  Lowest  = H;
                if (H > Highest) Highest = H;
            }
            ReliefSum += Highest - Lowest;
            if (Highest - Lowest > TallestSeen) TallestSeen = Highest - Lowest;

            // Where the player lands has to be gentle, or the first thing the
            // game does is drop him onto a slope he slides down.
            const float Flat = World.GroundAt(Where);
            double Tilt = 0.0;
            for (int32 Dir = 0; Dir < 4; ++Dir)
            {
                const double Dx = (Dir == 0) - (Dir == 2), Dy = (Dir == 1) - (Dir == 3);
                Tilt = std::max<double>(Tilt, std::fabs(
                    World.GroundAt(Where + FVector(Dx * 500.0, Dy * 500.0, 0.0)) - Flat));
            }
            if (Tilt > AHArena::BuildableRise * 0.5) ++SpawnNotFlat;
        }

        // -- Nothing stacked, nothing in the carriageway ------------------
        {
            std::vector<const FAHArenaPiece*> Solid;
            for (const FAHArenaPiece& Piece : World.Pieces)
            {
                if (Piece.bFlat || Piece.bInvisible || Piece.bSmoke) continue;
                if (Piece.Radius <= 0.f) continue;
                if (!Piece.MaterialPath.IsEmpty()) continue;          // a ground tile
                const std::string& Path = Piece.MeshPath.S;
                // Fences, ruin walls and bridge spans tile against themselves
                // by design; everything else must keep its distance.
                if (Path.find("fence") != std::string::npos) continue;
                if (Path.find("wall_stone") != std::string::npos) continue;
                if (Path.find("bridge") != std::string::npos) continue;
                Solid.push_back(&Piece);
                // On a hillside. The one failure the player reads as a bug
                // without knowing why: a cart with two wheels in the air, a
                // market stall sunk to its counter in a bank.
                {
                    const float Under = World.GroundAt(Piece.Location);
                    double Tilt = 0.0;
                    for (int32 Dir = 0; Dir < 4; ++Dir)
                    {
                        const double Dx = (Dir == 0) - (Dir == 2), Dy = (Dir == 1) - (Dir == 3);
                        Tilt = std::max<double>(Tilt, std::fabs(World.GroundAt(
                            Piece.Location + FVector(Dx * 260.0, Dy * 260.0, 0.0)) - Under));
                    }
                    if (Tilt > AHArena::BuildableRise) ++OnASlope;
                }
                // In the road: the carriageway is RoadWidth wide and is the one
                // place nothing may stand, which is what "casas aparecendo no
                // meio do caminho" was.
                int32 Px = 0, Py = 0;
                AHArena::CellOf(Piece.Location, Px, Py);
                for (int32 Ox = Px - 1; Ox <= Px + 1; ++Ox)
                    for (int32 Oy = Py - 1; Oy <= Py + 1; ++Oy)
                    {
                        if (!AHArena::InGrid(Ox, Oy)) continue;
                        // Only the carriageway. A square is a place to put a
                        // well and four market stalls, and always was.
                        if (World.At(Ox, Oy) != EAHCell::Road) continue;
                        const double Reach2 = AHArena::RoadWidth * .5
                                            + std::min<double>(Piece.Radius, 400.0);
                        if (FVector::Dist2D(Piece.Location, AHArena::CellCentre(Ox, Oy)) < Reach2)
                        { ++InTheRoad; Ox = Px + 2; break; }
                    }
            }
            // Swept along X: two things that cannot touch in one axis cannot
            // touch at all, and the all-pairs version made twenty thousand
            // worlds too slow to sit through -- which is the same as not
            // running it.
            std::sort(Solid.begin(), Solid.end(),
                      [](const FAHArenaPiece* A, const FAHArenaPiece* B)
                      { return A->Location.X < B->Location.X; });
            for (size_t I = 0; I < Solid.size(); ++I)
                for (size_t J = I + 1; J < Solid.size(); ++J)
                {
                    if (Solid[J]->Location.X - Solid[I]->Location.X > 260.0) break;
                    const double Want = std::min<double>(Solid[I]->Radius, 130.0)
                                      + std::min<double>(Solid[J]->Radius, 130.0) - 30.0;
                    if (Want <= 0.0) continue;
                    if (FVector::Dist2D(Solid[I]->Location, Solid[J]->Location) < Want)
                    {
                        ++Stacked;
                        GBlame[Solid[I]->MeshPath.S + " + " + Solid[J]->MeshPath.S]++;
                    }
                }
        }

        int32 Roads = 0;
        for (const EAHCell Cell : World.Cells)
        {
            Tally[Glyph(Cell)]++;
            if (Cell == EAHCell::Road || Cell == EAHCell::Plaza
                || Cell == EAHCell::Bridge) ++Roads;
        }

        // ── The river ───────────────────────────────────────────────────
        for (const FAHCampSpot& Camp : World.Camps)
        {
            int32 Kx = 0, Ky = 0;
            AHArena::CellOf(Camp.Where, Kx, Ky);
            const EAHCell Under = World.At(Kx, Ky);
            if (Under == EAHCell::Water || Under == EAHCell::Bridge) ++CampInWater;
        }
        {
            // Which cells got a blocker, and which cells should have had one.
            const int32 Grid = AHArena::GridSide;
            std::vector<bool> Walled(static_cast<size_t>(Grid * Grid), false);
            for (const FAHArenaPiece& Piece : World.Pieces)
            {
                int32 Px = 0, Py = 0;
                AHArena::CellOf(Piece.Location, Px, Py);
                if (!AHArena::InGrid(Px, Py)) continue;
                const EAHCell Under = World.At(Px, Py);
                if (Piece.bInvisible)
                {
                    Walled[static_cast<size_t>(Px * Grid + Py)] = true;
                    // A blocker anywhere but in the river is an invisible wall,
                    // and an invisible wall reads to a player as a broken game.
                    if (Under != EAHCell::Water) ++DryBlocker;
                    if (Under == EAHCell::Bridge) ++BlockedBridge;
                }
                // Nothing you can bump into stands in the stream. Water is not
                // where props go; the bank is.
                else if (Piece.bCover && Under == EAHCell::Water) ++ThingInWater;
            }
            for (int32 Cx = 0; Cx < Grid; ++Cx)
                for (int32 Cy = 0; Cy < Grid; ++Cy)
                    if (World.At(Cx, Cy) == EAHCell::Water
                        && !Walled[static_cast<size_t>(Cx * Grid + Cy)]) ++OpenWater;
        }
        if (WalkableFrom(World, AHArena::SpawnCellX, AHArena::SpawnCellY) != Roads && Mask == 0) ++Broken;
        if (World.Camps.Num() == 0) ++NoCamps;

        for (int32 Cx = 0; Cx < AHArena::GridSide; ++Cx)
            for (int32 Cy = 0; Cy < AHArena::GridSide; ++Cy)
                if (World.At(Cx, Cy) == EAHCell::Village
                    && !Paved(World, Cx + 1, Cy) && !Paved(World, Cx - 1, Cy)
                    && !Paved(World, Cx, Cy + 1) && !Paved(World, Cx, Cy - 1)) ++OffStreet;

        for (const FAHArenaPiece& Piece : World.Pieces)
        {
            if (!Piece.bCover) continue;
            const FVector Flat(Piece.Location.X, Piece.Location.Y, 0.0);
            // Against WHERE YOU CAME IN, not the old fixed spawn: the arrival
            // point moves with the gate you walked through.
            if (FVector::Dist2D(Flat, Where) < AHArena::SpawnClearance) ++OnSpawn;
            for (const FAHCampSpot& Camp : World.Camps)
            {
                // Indoors the walls are supposed to be close. Outdoors they
                // are not, and that is the whole difference between a room
                // and a camp buried in scenery.
                const double Keep = Camp.bIndoor ? 150.0 : AHArena::CampClearance;
                if (FVector::Dist2D(Flat, Camp.Where) < Keep) ++Cramped;
            }
        }

        // ── The landmarks ───────────────────────────────────────────────
        {
            bool bEarlyFight = false, bEarlyFort = false;
            for (const FAHLandmark& Mark : World.Landmarks)
            {
                const double Away = FVector::Dist2D(Mark.Where, Where);
                if (Mark.Foes > 0 && Away < 16000.0) bEarlyFight = true;
                if ((Mark.Kind == EAHSite::Masmorra || Mark.Kind == EAHSite::Forte)
                    && Away < 29000.0) bEarlyFort = true;
            }
            if (!bEarlyFight) ++NoEarlyFight;
            if (!bEarlyFort)  ++NoEarlyFort;

            for (const FAHLandmark& Mark : World.Landmarks)
            {
                if (Mark.Kind != EAHSite::Masmorra) continue;
                ++TotalDungeons;
                int32 Rooms = 0;
                for (const FAHCampSpot& Camp : World.Camps)
                    if (Camp.bIndoor
                        && FVector::Dist2D(Camp.Where, Mark.Where) < 2500.0) ++Rooms;
                if (Rooms < 2) ++ThinDungeons;
            }
        }

        for (const FAHArenaPiece& Piece : World.Pieces)
            if (std::fabs(Piece.Location.X) > AHArena::LandscapeEdge
             || std::fabs(Piece.Location.Y) > AHArena::LandscapeEdge) ++OffTheMap;

        // ── The errand ──────────────────────────────────────────────────
        /**
         * Everything the quest promises, asked of the plan rather than of the
         * screenshot. A marker pointing at a wall, a pack that was never
         * placed, a carter standing in the carriageway: all three are
         * invisible until somebody plays the whole thing through, and all
         * three are one line to check here.
         */
        if (!World.Errand.bValid) ++NoErrand;
        else
        {
            const FAHErrand& Job = World.Errand;
            const double Away = FVector::Dist2D(Job.Giver, Where);
            GiverAwaySum += Away;
            // Close enough to meet on the way out, far enough not to be
            // standing on the arrival mark.
            if (Away < AHArena::SpawnClearance || Away > 9500.0) ++GiverFar;

            int32 Gx = 0, Gy = 0;
            AHArena::CellOf(Job.Giver, Gx, Gy);
            if (Paved(World, Gx, Gy)) ++GiverOnRoad;
            // Nothing standing where the conversation happens.
            for (const FAHArenaPiece& Piece : World.Pieces)
            {
                if (Piece.bFlat || Piece.bInvisible) continue;
                if (FVector::Dist2D(Piece.Location, Job.Giver) < 240.0) { ++GiverBuried; break; }
            }

            if (!World.Landmarks.IsValidIndex(Job.Dungeon)
                || World.Landmarks[Job.Dungeon].Kind != EAHSite::Masmorra) ++PrizeAstray;
            else
            {
                const FVector& Den = World.Landmarks[Job.Dungeon].Where;
                // The pack has to BE somewhere, and that somewhere has to be
                // inside the dungeon it was promised to be in. Half a cell is
                // the whole footprint of a seven-by-seven plan of 420 cm
                // room sits up to 18 m from the middle; past that it is in the grass.
                if (Job.Prize.Equals(FVector::ZeroVector)) ++PrizeMissing;
                else if (FVector::Dist2D(Job.Prize, Den) > 2900.0) ++PrizeAstray;
                if (FVector::Dist2D(Job.Door, Den) > 3300.0) ++DoorAstray;
                // And the walk has to be a walk, not an expedition.
                const double Walk = FVector::Dist2D(Job.Giver, Den);
                ErrandWalkSum += Walk;
                if (Walk > 46000.0) ++ErrandTooFar;
                // The pack itself has to exist as a placed piece, or nothing
                // in the world can be picked up.
                bool bBag = false;
                for (const FAHArenaPiece& Piece : World.Pieces)
                    if (Piece.MeshPath.Contains("SM_Kit_mochila")) { bBag = true; break; }
                if (!bBag) ++PrizeMissing;
            }
        }
        // ── The living ───────────────────────────────────────────────────
        {
            TotalFolk   += World.Folk.Num();
            TotalBeasts += World.Beasts.Num();
            if (World.Folk.Num()   < 6) ++NoFolk;
            if (World.Beasts.Num() < 8) ++NoBeasts;
            int32 Trades = 0, Market = 0;
            for (int32 A = 0; A < World.Folk.Num(); ++A)
            {
                const FAHFolkSpot& Who = World.Folk[A];
                if (Who.Trade == EAHFolk::Lenhador || Who.Trade == EAHFolk::Pedreiro) ++Trades;
                if (Who.Trade == EAHFolk::Mercador) ++Market;
                if (std::fabs(Who.Where.X) > AHArena::LandscapeEdge
                 || std::fabs(Who.Where.Y) > AHArena::LandscapeEdge) ++FolkOffMap;
                int32 Fx = 0, Fy = 0;
                AHArena::CellOf(Who.Where, Fx, Fy);
                if (AHArena::InGrid(Fx, Fy))
                {
                    const EAHCell Under = World.Cells[Fx * AHArena::GridSide + Fy];
                    if (Under == EAHCell::Water) ++FolkInWater;
                    // On the verge, never ON the carriageway. A person standing
                    // in the middle of the road is the same bug the houses had.
                    if (Under == EAHCell::Road) ++FolkOnRoad;
                }
                // Nothing solid may be standing on top of him, or there is no
                // way to walk up and talk to him.
                for (const FAHArenaPiece& Piece : World.Pieces)
                {
                    if (Piece.bFlat || Piece.bSmoke) continue;
                    if (FVector::Dist2D(Piece.Location, Who.Where) < 150.0) { ++FolkBuried; break; }
                }
                // And they must not be standing in each other.
                for (int32 B = A + 1; B < World.Folk.Num(); ++B)
                    if (FVector::Dist2D(Who.Where, World.Folk[B].Where) < 180.0) ++FolkStacked;
            }
            // Every valley has somebody working and somebody selling. Both
            // were silently absent -- the trades lost a one-in-six roll in a
            // ring that was already full, and the single market square is the
            // cell the player lands on, where the spawn clearance forbids
            // everything. Neither showed up anywhere but in this count.
            if (Trades < 2) ++NoTrades;
            if (Market < 1) ++NoMarket;

            for (const FAHBeastSpot& Beast : World.Beasts)
            {
                if (std::fabs(Beast.Where.X) > AHArena::LandscapeEdge
                 || std::fabs(Beast.Where.Y) > AHArena::LandscapeEdge) ++BeastOffMap;
                if (Beast.Kind == EAHBeast::Corvo) continue;   // a crow is in the air
                for (const FAHArenaPiece& Piece : World.Pieces)
                {
                    if (Piece.bFlat || Piece.bSmoke) continue;
                    if (FVector::Dist2D(Piece.Location, Beast.Where) < 90.0) { ++BeastBuried; break; }
                }
            }
        }

        TotalMarks += World.Landmarks.Num();
        for (int32 A = 0; A < World.Landmarks.Num(); ++A)
        {
            const FAHLandmark& Mark = World.Landmarks[A];
            int32 Mx = 0, My = 0;
            AHArena::CellOf(Mark.Where, Mx, My);
            // On a road is the oldest bug in this project wearing a new hat.
            if (AHArena::InGrid(Mx, My) && Paved(World, Mx, My)) ++MarksOnRoad;
            // Ninety metres apart, or two places read as one messy place.
            for (int32 B = A + 1; B < World.Landmarks.Num(); ++B)
                if (FVector::Dist2D(Mark.Where, World.Landmarks[B].Where)
                    < 9.0 * AHArena::CellSize - 1.0) ++MarksCrowded;
            // Every hostile landmark must have handed the game mode a camp,
            // or it is a fort with nobody in it.
            if (Mark.Foes > 0)
            {
                bool bHasCamp = false;
                // A dungeon's fights are in its ROOMS, which are up to
                // fifteen metres from the middle of it. Everything else puts
                // its camp exactly where the landmark is.
                const double Reach = Mark.Kind == EAHSite::Masmorra ? 2500.0 : 1.0;
                for (const FAHCampSpot& Camp : World.Camps)
                    if (FVector::Dist2D(Camp.Where, Mark.Where) < Reach) { bHasCamp = true; break; }
                if (!bHasCamp) ++MarksNoFight;
            }
        }

        TotalPieces += World.Pieces.Num();
        TotalRoads  += Roads;
        TotalCamps  += World.Camps.Num();
        Sizes.push_back(World.Pieces.Num());
        if (World.Pieces.Num() > Biggest) Biggest = World.Pieces.Num();
    }

    std::sort(Sizes.begin(), Sizes.end());
    {
        // How many DISTINCT meshes a comarca uses, against how many actors it
        // spawns. The ratio is the whole question: if a few hundred actors are
        // really twenty-odd meshes repeated, they can be instanced, and a
        // seamless world stops being a performance argument.
        FRandomStream Sample(4242);
        const FAHArenaPlan One = AHArena::Build(Sample, Home, Measure, Ground, 15);
        std::map<std::string, int> MeshHistogram;
        int Blueprints = 0, Flats = 0;
        for (const FAHArenaPiece& Piece : One.Pieces)
        {
            if (Piece.MeshPath.S.find("/blueprints/") != std::string::npos) { ++Blueprints; continue; }
            if (Piece.bFlat) { ++Flats; }
            MeshHistogram[Piece.MeshPath.S]++;
        }
        int Repeated = 0;
        for (const auto& Row : MeshHistogram) if (Row.second > 1) Repeated += Row.second;
        std::printf("\numa comarca: %d pecas = %d malhas distintas + %d blueprints\n",
                    One.Pieces.Num(), (int)MeshHistogram.size(), Blueprints);
        std::printf("  dessas, %d sao repeticoes de uma malha ja usada (%.0f%%)\n",
                    Repeated, 100.0 * Repeated / std::max(1, One.Pieces.Num()));
        std::printf("  planos de estrada (sem colisao): %d\n", Flats);
    }
    std::printf("%d worlds\n", Worlds);
    std::printf("  roads not joined to the arrival cell : %d\n", Broken);
    std::printf("  worlds with no camp                  : %d\n", NoCamps);
    std::printf("  houses with no street                : %d\n", OffStreet);
    std::printf("  solid things on the arrival point    : %d\n", OnSpawn);
    std::printf("  solid things inside a camp's ring    : %d\n", Cramped);
    std::printf("  gates that are not road              : %d\n", GateNotRoad);
    std::printf("  gates you cannot walk to             : %d\n", GateUnreachable);
    std::printf("  arrival points that are not road     : %d\n", ArrivalNotRoad);
    std::printf("  camps standing in the river          : %d\n", CampInWater);
    std::printf("  invisible walls on dry land          : %d\n", DryBlocker);
    std::printf("  river cells you can wade into        : %d\n", OpenWater);
    std::printf("  bridges you cannot cross             : %d\n", BlockedBridge);
    std::printf("  solid things standing in the river   : %d\n", ThingInWater);
    std::printf("  solid things standing on a slope    : %d\n", OnASlope);
    std::printf("  arrival points not on flat ground    : %d\n", SpawnNotFlat);
    std::printf("  things standing inside each other    : %d\n", Stacked);
    {
        std::vector<std::pair<int,std::string>> Worst;
        for (const auto& Row : GBlame)
            Worst.push_back(std::make_pair(Row.second, Row.first));
        std::sort(Worst.rbegin(), Worst.rend());
        for (size_t I = 0; I < Worst.size() && I < 8; ++I)
            std::printf("      %6d  %s\n", Worst[I].first, Worst[I].second.c_str());
    }
    std::printf("  things standing in the carriageway   : %d\n", InTheRoad);
    std::printf("  landmarks standing on a road         : %d\n", MarksOnRoad);
    std::printf("  landmarks crowding each other        : %d\n", MarksCrowded);
    std::printf("  hostile landmarks with nobody home   : %d\n", MarksNoFight);
    std::printf("  worlds with no fight near the start  : %d\n", NoEarlyFight);
    std::printf("  worlds with no dungeon near the start: %d\n", NoEarlyFort);
    std::printf("  dungeons with fewer than two fights  : %d  (de %d)\n",
                ThinDungeons, TotalDungeons);
    std::printf("  things standing off the Landscape    : %d\n", OffTheMap);
    std::printf("  worlds with no errand                : %d\n", NoErrand);
    std::printf("  carter too near / too far from spawn : %d\n", GiverFar);
    std::printf("  carter standing in the carriageway   : %d\n", GiverOnRoad);
    std::printf("  carter with a prop on top of him     : %d\n", GiverBuried);
    std::printf("  backpack never placed                : %d\n", PrizeMissing);
    std::printf("  backpack outside its dungeon         : %d\n", PrizeAstray);
    std::printf("  dungeon door marker astray           : %d\n", DoorAstray);
    std::printf("  errand walk over 460 m               : %d\n", ErrandTooFar);
    std::printf("  worlds with almost nobody in them    : %d\n", NoFolk);
    std::printf("  worlds with nobody working           : %d\n", NoTrades);
    std::printf("  worlds with nobody at the market     : %d\n", NoMarket);
    std::printf("  people standing off the Landscape    : %d\n", FolkOffMap);
    std::printf("  people standing in the river         : %d\n", FolkInWater);
    std::printf("  people standing in the carriageway   : %d\n", FolkOnRoad);
    std::printf("  people with a prop on top of them    : %d\n", FolkBuried);
    std::printf("  people standing inside each other    : %d\n", FolkStacked);
    std::printf("  worlds with almost no animals        : %d\n", NoBeasts);
    std::printf("  animals off the Landscape            : %d\n", BeastOffMap);
    std::printf("  animals inside the scenery           : %d\n", BeastBuried);
    std::printf("  vivos: %.1f pessoas e %.1f bichos por mundo\n",
                static_cast<double>(TotalFolk) / Worlds,
                static_cast<double>(TotalBeasts) / Worlds);
    if (Worlds > NoErrand)
        std::printf("  errand: carter %.0f m out, walk %.0f m\n",
                    GiverAwaySum / (Worlds - NoErrand) / 100.0,
                    ErrandWalkSum / (Worlds - NoErrand) / 100.0);
    std::printf("  relief: media %.0f cm, maior %.0f cm\n", ReliefSum / Worlds, TallestSeen);
    std::printf("  pieces  min %d  median %d  p95 %d  max %d\n",
                Sizes.front(), Sizes[Sizes.size() / 2], Sizes[Sizes.size() * 95 / 100], Biggest);
    std::printf("  avg road cells %.1f, avg camps %.2f, avg marcos %.2f\n",
                static_cast<double>(TotalRoads) / Worlds,
                static_cast<double>(TotalCamps) / Worlds,
                static_cast<double>(TotalMarks) / Worlds);
    long long Cells = 0;
    for (const auto& Pair : Tally) Cells += Pair.second;
    for (const auto& Pair : Tally)
        std::printf("  %s %5.1f%%\n", Pair.first.c_str(), 100.0 * Pair.second / Cells);

    for (int32 Seed : { 0, 13, 99 })
    {
        FRandomStream Dice(Seed * 7919 + 13);
        const FAHArenaPlan World = AHArena::Build(Dice, Home, Measure, Ground);
        std::printf("\n--- seed %d : %s : %d pecas ---\n", Seed, *World.Name, World.Pieces.Num());
        Show(World);
        std::printf("\nrelevo:\n");
        Relief(World);
    }

    // Determinism, the same way the automation test asks it.
    FRandomStream One(1234), Two(1234);
    const FAHArenaPlan A = AHArena::Build(One, Home, Measure, Ground);
    const FAHArenaPlan B = AHArena::Build(Two, Home, Measure, Ground);
    bool bSame = A.Pieces.Num() == B.Pieces.Num() && A.Cells == B.Cells;
    for (int32 I = 0; bSame && I < A.Pieces.Num(); ++I)
        bSame = A.Pieces[I].Location.Equals(B.Pieces[I].Location)
             && A.Pieces[I].MeshPath == B.Pieces[I].MeshPath;
    std::printf("\nsame seed, same world: %s\n", bSame ? "sim" : "NAO");
    return (Broken || NoCamps || OffStreet || OnSpawn || Cramped || !bSame
            || CampInWater || DryBlocker || OpenWater || BlockedBridge || ThingInWater
            || OnASlope || SpawnNotFlat || Stacked || InTheRoad
            || MarksOnRoad || MarksCrowded || MarksNoFight
            || NoEarlyFight || NoEarlyFort || ThinDungeons
            || OffTheMap
            || NoErrand || GiverFar || GiverOnRoad || GiverBuried
            || PrizeMissing || PrizeAstray || DoorAstray || ErrandTooFar
         || NoFolk || NoTrades || NoMarket
         || FolkOffMap || FolkInWater || FolkOnRoad || FolkBuried || FolkStacked
         || NoBeasts || BeastOffMap || BeastBuried
         || GateNotRoad || GateUnreachable || ArrivalNotRoad) ? 1 : 0;
}
