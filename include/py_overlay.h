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
    Point2D() = default; // Default constructor
    Point2D(int x, int y) : x(x), y(y) {};
};

class Point3D {
public:
    float x = 0;
    float y = 0;
    float z = 0;
    Point3D() = default; // Default constructor
    Point3D(float x, float y, float z) : x(x), y(y), z(z) {};
};

struct D3DVertex {
    float x, y, z, rhw;
    D3DCOLOR color;
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)


class Overlay {
public:
    ImDrawList* drawList;

    XMMATRIX CreateViewMatrix(const XMFLOAT3& eye_pos, const XMFLOAT3& look_at_pos, const XMFLOAT3& up_direction);
    XMMATRIX CreateProjectionMatrix(float fov, float aspect_ratio, float near_plane, float far_plane);
    GW::Vec3f GetWorldToScreen(const GW::Vec3f& world_position, const XMMATRIX& mat_view, const XMMATRIX& mat_proj, float viewport_width, float viewport_height);
    void GetScreenToWorld();

    TextureManager texture_manager;

public:
    Overlay() : texture_manager(TextureManager::Instance()) { }
	
    void RefreshDrawList();
    Point2D GetMouseCoords();
    float findZ(float x, float y, uint32_t pz);
	uint32_t FindZPlane(float x, float y, uint32_t zplane);
    Point2D WorldToScreen(float x, float y, float z);
    Point3D GetMouseWorldPos();

    
    // Game <-> World
    Point2D GamePosToWorldMap(float x, float y);
    Point2D WorlMapToGamePos(float x, float y);
    
    // World <-> Screen
	Point2D WorldMapToScreen(float x, float y);
    Point2D ScreenToWorldMap(float screen_x, float screen_y);
    
    // Game <-> Screen (combined)
	Point2D GameMapToScreen(float x, float y);
    Point2D ScreenToGameMapPos(float screen_x, float screen_y);
    
    //NormalizedScreen <-> Screen
    Point2D NormalizedScreenToScreen(float norm_x, float norm_y);
    Point2D ScreenToNormalizedScreen(float screen_x, float screen_y);
    
    //NormalizedScreen <-> World / Game
    Point2D NormalizedScreenToWorldMap(float norm_x, float norm_y);
    Point2D NormalizedScreenToGameMap(float norm_x, float norm_y);
    Point2D GamePosToNormalizedScreen(float x, float y);
    

    void BeginDraw();
	void BeginDraw(std::string name);
	void BeginDraw(std::string name, float x, float y, float width, float height);
    void EndDraw();
    void DrawLine(Point2D from, Point2D to, ImU32 color, float thickness);
    void DrawLine3D(Point3D from, Point3D to, ImU32 color, float thickness);
	void DrawTriangle(Point2D p1, Point2D p2, Point2D p3, ImU32 color, float thickness = 1.0f);
	void DrawTriangle3D(Point3D p1, Point3D p2, Point3D p3, ImU32 color, float thickness = 1.0f);
    void DrawTriangleFilled(Point2D p1, Point2D p2, Point2D p3, ImU32 color);
    void DrawTriangleFilled3D(Point3D p1, Point3D p2, Point3D p3, ImU32 color);	
    void DrawQuad(Point2D p1, Point2D p2, Point2D p3, Point2D p4, ImU32 color, float thickness = 1.0f);
	void DrawQuad3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, ImU32 color, float thickness = 1.0f);
	void DrawQuadFilled(Point2D p1, Point2D p2, Point2D p3, Point2D p4, ImU32 color);
	void DrawQuadFilled3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, ImU32 color);
    void DrawPoly(Point2D center, float radius, ImU32 color = 0xFFFFFFFF, int numSegments = 32, float thickness = 1.0f);
	void DrawPolyFilled(Point2D center, float radius, ImU32 color = 0xFFFFFFFF, int numSegments = 32);
    void DrawPoly3D(Point3D center, float radius, ImU32 color = 0xFFFFFFFF, int numSegments = 32, float thickness = 1.0f, bool autoZ = true);
	void DrawPolyFilled3D(Point3D center, float radius, ImU32 color = 0xFFFFFFFF, int numSegments = 32, bool autoZ = true);
	void DrawCubeOutline(Point3D center, float size, ImU32 color, float thickness);
    void DrawCubeFilled(Point3D center, float size, ImU32 color);
    void DrawText2D(Point2D position, std::string text, ImU32 color, bool centered = true, float scale = 1.0f);
    void DrawText3D(Point3D position3D, std::string text, ImU32 color, bool autoZ = true, bool centered = true, float scale = 1.0f);
    void DrawTexture(const std::string& path, float width = 32.0f, float height = 32.0f);
    void DrawTexture(const std::string& path,
        std::tuple<float, float> size,
        std::tuple<float, float> uv0,
        std::tuple<float, float> uv1,
        std::tuple<int, int, int, int> tint,
        std::tuple<int, int, int, int> border_col);
    void DrawTexturedRect(float x, float y, float width, float height, const std::string& texture_path);
    void DrawTexturedRect(std::tuple<float, float> pos,
        std::tuple<float, float> size,
        const std::string& texture_path,
        std::tuple<float, float> uv0,
        std::tuple<float, float> uv1,
        std::tuple<int, int, int, int> tint);
	void UpkeepTextures(int timeout=30) {TextureManager::Instance().CleanupOldTextures(timeout);}
    bool ImageButton(const std::string& caption, const std::string& file_path, float width=32.0f, float height=32.0f, int frame_padding = 0);
    bool ImageButton(const std::string& caption, const std::string& file_path,
        std::tuple<float, float> size,
        std::tuple<float, float> uv0,
        std::tuple<float, float> uv1,
        std::tuple<int, int, int, int> bg_color,
        std::tuple<int, int, int, int> tint_color,
        int frame_padding);

	void DrawTextureInForegound(
		const std::tuple<float, float>& pos,
		const std::tuple<float, float>& size,
		const std::string& texture_path,
		const std::tuple<float, float>& uv0,
		const std::tuple<float, float>& uv1,
		const std::tuple<int, int, int, int>& tint);

    void DrawTextureInDrawlist(
        const std::tuple<float, float>& pos,
        const std::tuple<float, float>& size,
        const std::string& texture_path,
        const std::tuple<float, float>& uv0,
        const std::tuple<float, float>& uv1,
        const std::tuple<int, int, int, int>& tint);


	bool IsMouseClicked(int button);
    Point2D GetDisplaySize();
	void PushClipRect(float x, float y, float x2, float y2);
	void PopClipRect() {drawList->PopClipRect();}
};


