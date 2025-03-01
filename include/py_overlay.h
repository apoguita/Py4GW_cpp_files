#pragma once
#include "Headers.h"
using namespace DirectX;


constexpr auto ALTITUDE_UNKNOWN = std::numeric_limits<float>::max();

extern GW::Vec3f MouseWorldPos;
extern GW::Vec2f MouseScreenPos;

struct GlobalMouseClass {
    void SetMouseWorldPos(float x, float y, float z);
    GW::Vec3f GetMouseWorldPos();

    void SetMouseCoords(float x, float y);
    GW::Vec2f GetMouseCoords();
};

class Point2D{
public:
    int x = 0;
    int y = 0;
    Point2D(int x, int y) : x(x), y(y) {};
};

class Point3D {
public:
    float x = 0;
    float y = 0;
    float z = 0;
    Point3D(float x, float y, float z) : x(x), y(y), z(z) {};
};

class Overlay {
private:
    ImDrawList* drawList;

    XMMATRIX CreateViewMatrix(const XMFLOAT3& eye_pos, const XMFLOAT3& look_at_pos, const XMFLOAT3& up_direction);
    XMMATRIX CreateProjectionMatrix(float fov, float aspect_ratio, float near_plane, float far_plane);
    GW::Vec3f GetWorldToScreen(const GW::Vec3f& world_position, const XMMATRIX& mat_view, const XMMATRIX& mat_proj, float viewport_width, float viewport_height);
    void GetScreenToWorld();

public:
    Overlay();
    void RefreshDrawList();
    Point2D GetMouseCoords();
    float findZ(float x, float y, float pz);
    Point2D WorldToScreen(float x, float y, float z);
    Point3D GetMouseWorldPos();
    void BeginDraw();
    void EndDraw();
    void DrawLine(Point2D from, Point2D to, ImU32 color, float thickness);
    void DrawLine3D(Point3D from, Point3D to, ImU32 color, float thickness);
    void DrawPoly(Point2D center, float radius, ImU32 color = 0xFFFFFFFF, int numSegments = 32, float thickness = 1.0f);
    void DrawPoly3D(Point3D center, float radius, ImU32 color = 0xFFFFFFFF, int numSegments = 32, float thickness = 1.0f, bool autoZ = true);
	void DrawText2D(Point2D position, std::string text, ImU32 color, bool centered = true, float scale = 1.0f);
    void DrawText3D(Point3D position3D, std::string text, ImU32 color, bool autoZ = true, bool centered = true, float scale = 1.0f);
	void DrawFilledTriangle3D(Point3D p1, Point3D p2, Point3D p3, ImU32 color);
	void DrawQuad(Point2D p1, Point2D p2, Point2D p3, Point2D p4, ImU32 color, float thickness = 1.0f);
	void DrawQuad3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, ImU32 color, float thickness = 1.0f);
	void DrawQuadFilled(Point2D p1, Point2D p2, Point2D p3, Point2D p4, ImU32 color);
	void DrawQuadFilled3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, ImU32 color);
	bool IsMouseClicked(int button);
    Point2D GetDisplaySize();
};

