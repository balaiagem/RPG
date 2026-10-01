// Despeja um mundo inteiro em texto, para o Python olhar com as caixas de
// colisao de verdade. O gerador aqui e o do jogo, linha por linha.
#include "AHArena.h"
#include <cstdio>
#include <fstream>
#include <vector>
#include <cmath>
struct FTerrain {
    int Side = 0; std::vector<float> H; double Step = 0.0;
    bool Load(const char* P){ std::ifstream In(P, std::ios::binary); if(!In) return false;
        int32 S=0; In.read((char*)&S,4); Side=S; H.resize((size_t)S*S);
        In.read((char*)H.data(),(std::streamsize)H.size()*4); Step=100800.0/(S-1); return true; }
    float At(float X,float Y) const { double U=(X+50400.0)/Step, V=(Y+50400.0)/Step;
        int i=(int)std::floor(U), j=(int)std::floor(V);
        if(i<0)i=0; if(j<0)j=0; if(i>Side-1)i=Side-1; if(j>Side-1)j=Side-1;
        return H[(size_t)i*Side+j]; }
};
static FTerrain GT;
int main(int argc,char** argv){
    if(!GT.Load("Scripts/preview/terrain.bin") && !GT.Load("terrain.bin")){ std::printf("sem terrain.bin\n"); return 2; }
    auto Ground=[](float X,float Y){ return GT.At(X,Y); };
    auto Measure=[](const FString&)->FVector{ return FVector(100.0,100.0,100.0); };
    const int Seed = argc>1?std::atoi(argv[1]):0;
    const FVector Home(AHArena::SpawnX, AHArena::SpawnY, 110.0);
    FRandomStream Dice(Seed*7919+13);
    const FAHArenaPlan W = AHArena::Build(Dice, Home, Measure, Ground, 0);
    std::printf("SPAWN %.1f %.1f\n", Home.X, Home.Y);
    for(int i=0;i<W.Landmarks.Num();++i)
        std::printf("MARCO %d %.1f %.1f %d\n", i, W.Landmarks[i].Where.X,
                    W.Landmarks[i].Where.Y, (int)W.Landmarks[i].Kind);
    for(const FAHCampSpot& C : W.Camps)
        std::printf("CAMPO %.1f %.1f %d %d\n", C.Where.X, C.Where.Y, C.Foes, C.bIndoor?1:0);
    for(const FAHArenaPiece& P : W.Pieces){
        const char* n = *P.MeshPath;
        const char* slash = n; for(const char* c=n; *c; ++c) if(*c=='/') slash=c+1;
        std::printf("PECA %s %.1f %.1f %.1f %.2f %.3f %.3f %.3f %d%d\n",
                    slash, P.Location.X, P.Location.Y, P.Location.Z,
                    P.Rotation.Yaw, P.Scale.X, P.Scale.Y, P.Scale.Z,
                    P.bFlat?1:0, P.bInvisible?1:0);
    }
    // As pessoas e os bichos. GENTE tem oficio, alcance e se e o do recado;
    // BICHO tem a especie e o alcance.
    for(const FAHFolkSpot& F : W.Folk)
        std::printf("GENTE %.1f %.1f %.1f %d %.0f %d\n", F.Where.X, F.Where.Y,
                    F.Yaw, (int)F.Trade, F.Range, F.bGiver?1:0);
    for(const FAHBeastSpot& B : W.Beasts)
        std::printf("BICHO %.1f %.1f %d %.0f\n", B.Where.X, B.Where.Y,
                    (int)B.Kind, B.Range);
    if(W.Errand.bValid)
        std::printf("RECADO %.1f %.1f %.1f %.1f %d\n", W.Errand.Giver.X, W.Errand.Giver.Y,
                    W.Errand.Prize.X, W.Errand.Prize.Y, W.Errand.Dungeon);
    return 0;
}
