// A deliberately small stand-in for the slice of Unreal that AHArena.cpp uses.
//
// The point is not to emulate the engine. It is to compile the real file with a
// compiler and then RUN it, because the two things that keep costing a whole
// round trip are (a) a type-deduction error MSVC only finds at build time and
// (b) a layout bug you can only see by looking at the map.
//
// So the pieces that bite are modelled faithfully rather than conveniently:
//   * FMath::Min/Max/Clamp/Abs deduce ONE type, so mixing float and double fails
//     here exactly as it fails in Unreal.
//   * Atan2/Sqrt/Cos/Sin are float and double overloads, so a mixed pair is
//     ambiguous here exactly as it is there.
//   * FloorToInt/RoundToInt give int32 for a float and int64 for a double.
//   * FVector holds doubles, as it has since UE5 went large-world.
//   * FRandomStream is Unreal's own generator, number for number, so the worlds
//     this prints are the worlds the game will build from the same seed.
#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <functional>
#include <type_traits>

using int32  = std::int32_t;
using int64  = std::int64_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using uint8  = std::uint8_t;
using TCHAR  = char;

#define TEXT(x) x
#define INDEX_NONE (-1)
static constexpr int32  MAX_int32 = 0x7fffffff;
static constexpr double UE_KINDA_SMALL_NUMBER = 1.e-4;
static constexpr double PI = 3.1415926535897932;

#define UE_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

// ── FString ─────────────────────────────────────────────────────────────────
/**
 * A colour, only so AHClassData.h compiles here.
 *
 * The class table carries one per archetype for the HUD. Nothing offline reads
 * it; it exists so the RULES in that table -- hit dice, saves, the primary
 * ability -- can be compiled and checked without the editor.
 */
struct FLinearColor
{
    float R = 0.f, G = 0.f, B = 0.f, A = 1.f;
    FLinearColor() = default;
    FLinearColor(float InR, float InG, float InB, float InA = 1.f)
        : R(InR), G(InG), B(InB), A(InA) {}
};

struct FString
{
    std::string S;
    FString() = default;
    FString(const TCHAR* In) : S(In ? In : "") {}
    FString(const std::string& In) : S(In) {}
    bool IsEmpty() const { return S.empty(); }
    bool Contains(const TCHAR* What) const { return S.find(What) != std::string::npos; }
    const TCHAR* operator*() const { return S.c_str(); }
    FString& operator+=(const FString& Other) { S += Other.S; return *this; }
    friend FString operator+(const FString& A, const FString& B) { return FString(A.S + B.S); }
    friend FString operator+(const FString& A, const TCHAR* B)   { return FString(A.S + B); }
    friend bool operator==(const FString& A, const FString& B)   { return A.S == B.S; }
    friend bool operator!=(const FString& A, const FString& B)   { return A.S != B.S; }

    template<typename... TArgs>
    static FString Printf(const TCHAR* Format, TArgs... Args)
    {
        char Buffer[1024];
        std::snprintf(Buffer, sizeof(Buffer), Format, Args...);
        return FString(Buffer);
    }
};

// ── TArray / TSet ───────────────────────────────────────────────────────────
template<typename T>
struct TArray
{
    std::vector<T> V;
    TArray() = default;
    TArray(std::initializer_list<T> In) : V(In) {}
    int32 Num() const { return static_cast<int32>(V.size()); }
    int32 Add(const T& Item) { V.push_back(Item); return Num() - 1; }
    void  Reset() { V.clear(); }
    void  Reserve(int32 N) { V.reserve(static_cast<size_t>(N)); }
    void  Init(const T& Item, int32 N) { V.assign(static_cast<size_t>(N), Item); }
    bool  IsValidIndex(int32 I) const { return I >= 0 && I < Num(); }
    T&       operator[](int32 I)       { return V[static_cast<size_t>(I)]; }
    const T& operator[](int32 I) const { return V[static_cast<size_t>(I)]; }
    void  Swap(int32 A, int32 B) { std::swap(V[static_cast<size_t>(A)], V[static_cast<size_t>(B)]); }
    T     Pop() { T Last = V.back(); V.pop_back(); return Last; }
    void  RemoveAt(int32 I) { V.erase(V.begin() + static_cast<size_t>(I)); }
    const T* GetData() const { return V.data(); }
    T*       GetData()       { return V.data(); }
    template<typename TLess> void StableSort(TLess Less) { std::stable_sort(V.begin(), V.end(), Less); }
    typename std::vector<T>::iterator begin() { return V.begin(); }
    typename std::vector<T>::iterator end()   { return V.end(); }
    typename std::vector<T>::const_iterator begin() const { return V.begin(); }
    typename std::vector<T>::const_iterator end()   const { return V.end(); }
    friend bool operator==(const TArray& A, const TArray& B) { return A.V == B.V; }
    friend bool operator!=(const TArray& A, const TArray& B) { return A.V != B.V; }
};

template<typename T>
struct TSet
{
    std::set<T> S;
    void Add(const T& Item) { S.insert(Item); }
    void Remove(const T& Item) { S.erase(Item); }
    int32 Num() const { return static_cast<int32>(S.size()); }
    typename std::set<T>::const_iterator begin() const { return S.begin(); }
    typename std::set<T>::const_iterator end()   const { return S.end(); }
};

template<typename T> using TFunctionRef = std::function<T>;

// ── Vectors ─────────────────────────────────────────────────────────────────
struct FVector2D
{
    double X = 0, Y = 0;
    FVector2D() = default;
    FVector2D(double InX, double InY) : X(InX), Y(InY) {}
    FVector2D operator-(const FVector2D& O) const { return FVector2D(X - O.X, Y - O.Y); }
    FVector2D operator+(const FVector2D& O) const { return FVector2D(X + O.X, Y + O.Y); }
    FVector2D operator*(double S) const { return FVector2D(X * S, Y * S); }
    double SizeSquared() const { return X * X + Y * Y; }
    static double DotProduct(const FVector2D& A, const FVector2D& B) { return A.X * B.X + A.Y * B.Y; }
    static double Distance(const FVector2D& A, const FVector2D& B)
    { const double Dx = A.X - B.X, Dy = A.Y - B.Y; return std::sqrt(Dx * Dx + Dy * Dy); }
};

struct FVector
{
    double X = 0, Y = 0, Z = 0;
    FVector() = default;
    FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
    explicit FVector(double InF) : X(InF), Y(InF), Z(InF) {}
    FVector operator+(const FVector& O) const { return FVector(X + O.X, Y + O.Y, Z + O.Z); }
    FVector operator-(const FVector& O) const { return FVector(X - O.X, Y - O.Y, Z - O.Z); }
    FVector operator-() const { return FVector(-X, -Y, -Z); }
    FVector operator*(double S) const { return FVector(X * S, Y * S, Z * S); }
    bool Equals(const FVector& O, double Tolerance = 1.e-4) const
    { return std::fabs(X - O.X) <= Tolerance && std::fabs(Y - O.Y) <= Tolerance && std::fabs(Z - O.Z) <= Tolerance; }
    FVector GetSafeNormal2D() const
    {
        const double Length = std::sqrt(X * X + Y * Y);
        return Length > 1.e-8 ? FVector(X / Length, Y / Length, 0.0) : FVector(0, 0, 0);
    }
    static double Dist2D(const FVector& A, const FVector& B)
    { const double Dx = A.X - B.X, Dy = A.Y - B.Y; return std::sqrt(Dx * Dx + Dy * Dy); }
    static const FVector ZeroVector;
    static const FVector OneVector;
};

struct FRotator
{
    double Pitch = 0, Yaw = 0, Roll = 0;
    FRotator() = default;
    FRotator(double InPitch, double InYaw, double InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}
    static const FRotator ZeroRotator;
};

// ── FMath ───────────────────────────────────────────────────────────────────
struct FMath
{
    template<class T> static T Min(const T A, const T B) { return A < B ? A : B; }
    template<class T> static T Max(const T A, const T B) { return A > B ? A : B; }
    template<class T> static T Abs(const T A) { return A < T(0) ? -A : A; }
    template<class T> static T Clamp(const T Value, const T Low, const T High)
    { return Value < Low ? Low : Value > High ? High : Value; }
    template<class T, class U> static T Lerp(const T& A, const T& B, const U& Alpha)
    { return T(A + (B - A) * Alpha); }

    static float  Sqrt(float V)  { return std::sqrt(V); }
    static double Sqrt(double V) { return std::sqrt(V); }
    static float  Cos(float V)   { return std::cos(V); }
    static double Cos(double V)  { return std::cos(V); }
    static float  Sin(float V)   { return std::sin(V); }
    static double Sin(double V)  { return std::sin(V); }
    static float  Atan2(float Y, float X)   { return std::atan2(Y, X); }
    static double Atan2(double Y, double X) { return std::atan2(Y, X); }

    static int32 FloorToInt(float V)  { return static_cast<int32>(std::floor(V)); }
    static int64 FloorToInt(double V) { return static_cast<int64>(std::floor(V)); }
    static int32 RoundToInt(float V)  { return static_cast<int32>(std::floor(V + 0.5f)); }
    static int64 RoundToInt(double V) { return static_cast<int64>(std::floor(V + 0.5)); }
    static int32 TruncToInt(float V)  { return static_cast<int32>(V); }

    static float  RadiansToDegrees(float V)  { return V * (180.f / 3.1415927f); }
    static double RadiansToDegrees(double V) { return V * (180.0 / PI); }
    static float  DegreesToRadians(float V)  { return V * (3.1415927f / 180.f); }
    static double DegreesToRadians(double V) { return V * (PI / 180.0); }
};

// ── FRandomStream ───────────────────────────────────────────────────────────
// Unreal's own, number for number, so a seed here is the same world there.
struct FRandomStream
{
    mutable int32 InitialSeed = 0;
    mutable int32 Seed = 0;
    FRandomStream() = default;
    explicit FRandomStream(int32 InSeed) : InitialSeed(InSeed), Seed(InSeed) {}
    int32 GetInitialSeed() const { return InitialSeed; }
    void MutateSeed() const
    { Seed = static_cast<int32>(static_cast<uint32>(Seed) * 196314165u + 907633515u); }
    float GetFraction() const
    {
        MutateSeed();
        const uint32 Temp = ((static_cast<uint32>(Seed) >> 9) | 0x3f800000u);
        float Result;
        std::memcpy(&Result, &Temp, sizeof(Result));
        return Result - 1.0f;
    }
    float FRand() const { return GetFraction(); }
    int32 RandHelper(int32 A) const
    { return A > 0 ? FMath::Min(FMath::TruncToInt(GetFraction() * static_cast<float>(A)), A - 1) : 0; }
    int32 RandRange(int32 Low, int32 High) const { return Low + RandHelper((High - Low) + 1); }
    float FRandRange(float Low, float High) const { return Low + (High - Low) * GetFraction(); }
};
