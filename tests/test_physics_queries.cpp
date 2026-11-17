// Plan 13.2: shape casts and overlaps (xlioncore_physics_queries.h) on a plain Box3D world: a floor, a dynamic ball at (0,1,0) r 0.5, a
// static wall at x 5 (half 0.5), a static mesh box (13.3's kind of shape) at (-5,1,0). Gates: a sphere dropped from above hits the ball's top
// at the right fraction; a capsule cast hits the wall's face, and passes when the wall is skipped; a box cast hits the mesh; a shape that
// starts inside something hits at fraction 0; overlaps find exactly the bodies in the volume. Build: this file alone, the xLION root and box3d's include on the path, linked with box3d.
#include "../src/physics/xlioncore_physics_queries.h"
#include <cmath>
#include <cstdio>

using namespace xlioncore::physics;
static int g_Fails = 0;
static void Check(const char* What, bool Ok, float A = 0, float B = 0) { std::printf("%s %s (%.4f %.4f)\n", Ok ? "ok  " : "FAIL", What, A, B); g_Fails += !Ok; }

int main()
{
    b3WorldDef WD = b3DefaultWorldDef();
    const auto World = b3CreateWorld(&WD);
    b3ShapeDef SD = b3DefaultShapeDef();

    b3BodyDef FD = b3DefaultBodyDef(); FD.position = { 0, -0.5f, 0 };
    const auto Floor = b3CreateBody(World, &FD); const auto FloorBox = b3MakeBoxHull(20, 0.5f, 20); b3CreateHullShape(Floor, &SD, &FloorBox.base);

    b3BodyDef BD = b3DefaultBodyDef(); BD.type = b3_dynamicBody; BD.position = { 0, 1, 0 };
    const auto Ball = b3CreateBody(World, &BD); const b3Sphere Sp{ { 0, 0, 0 }, 0.5f }; b3CreateSphereShape(Ball, &SD, &Sp);

    b3BodyDef WDf = b3DefaultBodyDef(); WDf.position = { 5, 1, 0 };
    const auto Wall = b3CreateBody(World, &WDf); const auto WallBox = b3MakeBoxHull(0.5f, 1, 2); const auto WallShape = b3CreateHullShape(Wall, &SD, &WallBox.base);

    b3BodyDef MD = b3DefaultBodyDef(); MD.position = { -5, 1, 0 };
    const auto MeshBody = b3CreateBody(World, &MD); b3MeshData* pMesh = b3CreateBoxMesh({ 0, 0, 0 }, { 0.5f, 0.5f, 0.5f }, true);
    b3CreateMeshShape(MeshBody, &SD, pMesh, { 1, 1, 1 });

    // 1. a sphere (r 0.25) dropped from y 5 straight down 10: touches the ball's top (y 1.5) when its centre is at 1.75 -> fraction 0.325
    auto H = CastShapeClosest(World, { 0, 5, 0 }, query_shape::Sphere(0.25f), { 0, -10, 0 });
    Check("sphere cast hits the ball's top", H.m_bHit && std::abs(H.m_Fraction - 0.325f) < 0.01f && std::abs(H.m_Point.m_Y - 1.5f) < 0.01f && H.m_Normal.m_Y > 0.99f, H.m_Fraction, H.m_Point.m_Y);
    Check("the hit names the ball's body", H.m_bHit && b3Shape_GetBody(H.m_Shape).index1 == Ball.index1);

    // 2. a capsule (y 1.5..2.5 above the ball, r 0.2) along +x from x 1: the wall's face (x 4.5) when its centre is at 4.3 -> fraction 3.3/10
    const auto Cap = query_shape::Capsule({ 0, -0.5f, 0 }, { 0, 0.5f, 0 }, 0.2f);
    H = CastShapeClosest(World, { 1, 1.4f, 0 }, Cap, { 10, 0, 0 });
    Check("capsule cast hits the wall's face", H.m_bHit && std::abs(H.m_Fraction - 0.33f) < 0.01f && H.m_Normal.m_X < -0.99f, H.m_Fraction, H.m_Point.m_X);
    H = CastShapeClosest(World, { 1, 1.4f, 0 }, Cap, { 10, 0, 0 }, b3DefaultQueryFilter(), [&](b3ShapeId S) { return S.index1 == WallShape.index1; });
    Check("skipping the wall, the capsule passes", !H.m_bHit, H.m_Fraction);

    // 3. a box (half 0.3) along -x from x -1 at the mesh box's height: the mesh's face (x -4.5) when its centre is at -4.2 -> fraction 3.2/10
    H = CastShapeClosest(World, { -1, 1, 0 }, query_shape::Box({ 0.3f, 0.3f, 0.3f }), { -10, 0, 0 });
    Check("box cast hits the mesh collider", H.m_bHit && std::abs(H.m_Fraction - 0.32f) < 0.01f && b3Shape_GetBody(H.m_Shape).index1 == MeshBody.index1, H.m_Fraction, H.m_Point.m_X);

    // 4. a shape that starts inside the ball: a hit at once
    H = CastShapeClosest(World, { 0, 1, 0 }, query_shape::Sphere(0.1f), { 0, 0, 5 });
    Check("a cast that starts inside a body hits at fraction 0", H.m_bHit && H.m_Fraction == 0.0f, H.m_Fraction);

    // 5. overlaps: a sphere at the ball finds the ball (and the floor it rests just above? the ball's bottom is at y 0.5: no), a wide box finds all
    std::vector<b3BodyId> Out;
    OverlapShape(World, { 0, 1, 0 }, query_shape::Sphere(0.3f), Out);
    Check("a sphere overlap at the ball finds only the ball", Out.size() == 1 && Out[0].index1 == Ball.index1, float(Out.size()));
    OverlapShape(World, { 0, 1, 0 }, query_shape::Box({ 6, 1.05f, 1 }), Out);     // (y -0.05..2.05: into the floor too)
    Check("a wide box overlap finds the floor, the ball, the wall and the mesh box", Out.size() == 4, float(Out.size()));
    OverlapShape(World, { 0, 3, 0 }, query_shape::Capsule({ -1, 0, 0 }, { 1, 0, 0 }, 0.2f), Out);
    Check("an overlap in empty air finds nothing", Out.empty(), float(Out.size()));

    b3DestroyWorld(World); b3DestroyMesh(pMesh);
    std::printf(g_Fails ? "%d FAILED\n" : "ALL PASSED\n", g_Fails);
    return g_Fails ? 1 : 0;
}
