#pragma once
#include "Headers.h"


class Py2DRenderer {
private:
    Overlay overlay;
public:

    void set_primitives(const std::vector<std::vector<Point2D>>& prims, D3DCOLOR color);
    void set_world_zoom_x(float zoom);
    void set_world_zoom_y(float zoom);
    void set_world_pan(float x, float y);
    void set_world_rotation(float r);
    void set_world_space(bool enabled);
    void set_world_scale(float x);

    // Screen-space offset (pixel units)
    void set_screen_offset(float x, float y);
    void set_screen_zoom_x(float zoom);
    void set_screen_zoom_y(float zoom);
    void set_screen_rotation(float r);

    void set_circular_mask(bool enabled);
    void set_circular_mask_radius(float radius);
    void set_circular_mask_center(float x, float y);

    void set_rectangle_mask(bool enabled);
    void set_rectangle_mask_bounds(float x, float y, float width, float height);

    void render();

    void Setup3DView();

    void DrawLine(Point2D from, Point2D to, D3DCOLOR color, float thickness);
	void DrawLine3D(Point3D from, Point3D to, D3DCOLOR color, bool use_occlusion = true, int segments = 1,float floor_offset = 0.0f);
    void DrawTriangle(Point2D p1, Point2D p2, Point2D p3, D3DCOLOR color, float thickness = 1.0f);
    void DrawTriangle3D(Point3D p1, Point3D p2, Point3D p3, D3DCOLOR color, bool use_occlusion = true, int edge_segments=1, float floor_offset = 0.0f);
    void DrawTriangleFilled(Point2D p1, Point2D p2, Point2D p3, D3DCOLOR color);
	void DrawTriangleFilled3D(Point3D p1, Point3D p2, Point3D p3, D3DCOLOR color, bool use_occlusion = true, int edge_segments = 1, float floor_offset = 0.0f);
    void DrawQuad(Point2D p1, Point2D p2, Point2D p3, Point2D p4, D3DCOLOR color, float thickness = 1.0f);
	void DrawQuad3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, D3DCOLOR color, bool use_occlusion = true, int edge_segments = 1, float floor_offset = 0.0f);
    void DrawQuadFilled(Point2D p1, Point2D p2, Point2D p3, Point2D p4, D3DCOLOR color);
	void DrawQuadFilled3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, D3DCOLOR color, bool use_occlusion = true, int edge_segments = 1, float floor_offset = 0.0f);
    void DrawPoly(Point2D center, float radius, D3DCOLOR color = 0xFFFFFFFF, int numSegments = 32, float thickness = 1.0f);
    void DrawPolyFilled(Point2D center, float radius, D3DCOLOR color = 0xFFFFFFFF, int numSegments = 32);
	void DrawPoly3D(Point3D center, float radius, D3DCOLOR color = 0xFFFFFFFF, int numSegments = 32, bool autoZ = true, bool use_occlusion = true, int segments = 1, float floor_offset = 0.0f);
	void DrawPolyFilled3D(Point3D center, float radius, D3DCOLOR color = 0xFFFFFFFF, int numSegments = 32, bool autoZ = true, bool use_occlusion = true, int segments = 1, float floor_offset = 0.0f);
    void DrawCubeOutline(Point3D center, float size, D3DCOLOR color, bool use_occlusion = true);
    void DrawCubeFilled(Point3D center, float size, D3DCOLOR color, bool use_occlusion = true);
    void DrawTexture(const std::string& file_path, float screen_pos_x, float screen_pos_y, float width, float height, uint32_t int_tint);
    void DrawTexture3D(const std::string& file_path, float world_pos_x, float world_pos_y, float world_pos_z, float width, float height, bool use_occlusion, uint32_t int_tint);
    void DrawQuadTextured3D(const std::string& file_path, Point3D p1, Point3D p2, Point3D p3, Point3D p4, bool use_occlusion, uint32_t int_tint);
    int Py2DRenderer::SaveGeometryToFile(
        const std::wstring& filename,
        float min_x, float min_y,
        float max_x, float max_y
    );
    void ApplyStencilMask();
    void ResetStencilMask();


private:
    void SetupProjection();

    struct D3DVertex {
        float x, y, z, rhw;
        D3DCOLOR color;
    };

    struct D3DVertex3D {
        float x, y, z;           // No RHW
        D3DCOLOR color;
    };


    std::vector<std::vector<Point2D>> primitives;
    D3DCOLOR color;
    float world_zoom_x = 1.0f;
    float world_zoom_y = 1.0f;
    float world_pan_x = 0.0f;
    float world_pan_y = 0.0f;
    float world_scale = 1.0f;
    float world_rotation = 0.0f;
    bool world_space = true;

    // Screen transform (pixels)
    float screen_offset_x = 0.0f;
    float screen_offset_y = 0.0f;
    float screen_zoom_x = 1.0f;
    float screen_zoom_y = 1.0f;
    float screen_rotation = 0.0f;

    bool use_circular_mask = false;
    float mask_radius = 5000.0f;
    float mask_center_x = 0.0f;
    float mask_center_y = 0.0f;

    bool use_rectangle_mask = false;
    float mask_rect_x = 0.0f;
    float mask_rect_y = 0.0f;
    float mask_rect_width = 0.0f;
    float mask_rect_height = 0.0f;





    void Py2DRenderer::EnableDepthBuffer(bool enable) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, enable ? D3DZB_TRUE : D3DZB_FALSE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, enable ? TRUE : FALSE);
        g_d3d_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    }


};


