#include "py_2d_renderer.h"


using namespace DirectX;

void Py2DRenderer::set_primitives(const std::vector<std::vector<Point2D>>& prims, D3DCOLOR draw_color) {
    primitives = prims;
    color = draw_color;
}

void Py2DRenderer::set_world_zoom_x(float zoom) { world_zoom_x = zoom; }
void Py2DRenderer::set_world_zoom_y(float zoom) { world_zoom_y = zoom; }
void Py2DRenderer::set_world_pan(float x, float y) { world_pan_x = x; world_pan_y = y; }
void Py2DRenderer::set_world_rotation(float r) { world_rotation = r; }
void Py2DRenderer::set_world_space(bool enabled) { world_space = enabled; }
void Py2DRenderer::set_world_scale(float x) { world_scale = x; }

void Py2DRenderer::set_screen_offset(float x, float y) { screen_offset_x = x; screen_offset_y = y; }
void Py2DRenderer::set_screen_zoom_x(float zoom) { screen_zoom_x = zoom; }
void Py2DRenderer::set_screen_zoom_y(float zoom) { screen_zoom_y = zoom; }
void Py2DRenderer::set_screen_rotation(float r) { screen_rotation = r; }

void Py2DRenderer::set_circular_mask(bool enabled) { use_circular_mask = enabled; }
void Py2DRenderer::set_circular_mask_radius(float radius) { mask_radius = radius; }
void Py2DRenderer::set_circular_mask_center(float x, float y) { mask_center_x = x; mask_center_y = y; }

void Py2DRenderer::set_rectangle_mask(bool enabled) { use_rectangle_mask = enabled; }
void Py2DRenderer::set_rectangle_mask_bounds(float x, float y, float width, float height) {
    mask_rect_x = x;
    mask_rect_y = y;
    mask_rect_width = width;
    mask_rect_height = height;
}

void Py2DRenderer::SetupProjection() {
    if (!g_d3d_device) return;

    D3DVIEWPORT9 vp;
    g_d3d_device->GetViewport(&vp);

    float w = 5000.0f * 2;

    XMMATRIX ortho(
        2.0f / w, 0, 0, 0,
        0, 2.0f / w, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    );

    //float xscale = 1.0f;
    //float yscale = static_cast<float>(vp.Width) / static_cast<float>(vp.Height);

    float xscale = world_scale;
    float yscale = (static_cast<float>(vp.Width) / static_cast<float>(vp.Height)) * world_scale;


    float xtrans = static_cast<float>(0); // optional pan offset
    float ytrans = static_cast<float>(0); // optional pan offset

    XMMATRIX vp_matrix(
        xscale, 0, 0, 0,
        0, yscale, 0, 0,
        0, 0, 1, 0,
        xtrans, ytrans, 0, 1
    );

    XMMATRIX proj = ortho * vp_matrix;

    g_d3d_device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(&proj));
}

void Py2DRenderer::ApplyStencilMask() {
    if (!g_d3d_device) return;

    auto FillRect = [](float x, float y, float w, float h, D3DCOLOR color) {
        D3DVertex vertices[4] = {
            {x,     y,     0.0f, 1.0f, color},
            {x + w, y,     0.0f, 1.0f, color},
            {x,     y + h, 0.0f, 1.0f, color},
            {x + w, y + h, 0.0f, 1.0f, color}
        };
        g_d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(D3DVertex));
        };

    auto FillCircle = [](float x, float y, float radius, D3DCOLOR color, unsigned resolution = 192u) {
        D3DVertex vertices[193];
        for (unsigned i = 0; i <= resolution; ++i) {
            float angle = static_cast<float>(i) / resolution * XM_2PI;
            vertices[i] = {
                x + radius * cosf(angle),
                y + radius * sinf(angle),
                0.0f,
                1.0f,
                color
            };
        }
        g_d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, resolution, vertices, sizeof(D3DVertex));
        };

    g_d3d_device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
    g_d3d_device->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
    g_d3d_device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
    g_d3d_device->Clear(0, nullptr, D3DCLEAR_STENCIL, 0x00000000, 1.0f, 0);

    g_d3d_device->SetRenderState(D3DRS_STENCILREF, 1);
    g_d3d_device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
    g_d3d_device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);

    if (use_circular_mask)
        FillCircle(mask_center_x, mask_center_y, mask_radius, D3DCOLOR_ARGB(0, 0, 0, 0));
    else if (use_rectangle_mask)
        FillRect(mask_rect_x, mask_rect_y, mask_rect_width, mask_rect_height, D3DCOLOR_ARGB(0, 0, 0, 0));
    else
        FillRect(-5000.0f, -5000.0f, 10000.0f, 10000.0f, D3DCOLOR_ARGB(0, 0, 0, 0));

    g_d3d_device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
    g_d3d_device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    g_d3d_device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
}

void Py2DRenderer::ResetStencilMask() {
    if (!g_d3d_device) return;
    g_d3d_device->SetRenderState(D3DRS_STENCILREF, 0);
    g_d3d_device->SetRenderState(D3DRS_STENCILWRITEMASK, 0x00000000);
    g_d3d_device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NEVER);
    g_d3d_device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    g_d3d_device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    g_d3d_device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
}


void Py2DRenderer::render() {
    if (!g_d3d_device || primitives.empty()) return;

    IDirect3DStateBlock9* state_block = nullptr;
    if (FAILED(g_d3d_device->CreateStateBlock(D3DSBT_ALL, &state_block))) return;

    D3DMATRIX old_world, old_view, old_proj;
    g_d3d_device->GetTransform(D3DTS_WORLD, &old_world);
    g_d3d_device->GetTransform(D3DTS_VIEW, &old_view);
    g_d3d_device->GetTransform(D3DTS_PROJECTION, &old_proj);

    if (world_space) {
        SetupProjection();
        D3DVIEWPORT9 vp;
        g_d3d_device->GetViewport(&vp);

        // World width used in projection
        float world_width = 5000.0f * 2.0f;

        auto translate = XMMatrixTranslation(-world_pan_x, -world_pan_y, 0);
        auto rotateZ = XMMatrixRotationZ(world_rotation);
        auto zoom = XMMatrixScaling(world_zoom_x, world_zoom_y, 1.0f);
        auto scale = XMMatrixScaling(world_scale, world_scale, 1.0f);
        auto flipY = XMMatrixScaling(1.0f, -1.0f, 1.0f); //flip happens HERE

        auto view = translate * rotateZ * scale * flipY * zoom;


        g_d3d_device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view));
        g_d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    }
    else {
        g_d3d_device->SetTransform(D3DTS_VIEW, &old_view);
        g_d3d_device->SetTransform(D3DTS_PROJECTION, &old_proj);
        g_d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    }

    // Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
    g_d3d_device->SetIndices(nullptr);
    g_d3d_device->SetFVF(D3DFVF_CUSTOMVERTEX);
    g_d3d_device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true);
    g_d3d_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    g_d3d_device->SetPixelShader(nullptr);
    g_d3d_device->SetVertexShader(nullptr);
    g_d3d_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_d3d_device->SetRenderState(D3DRS_LIGHTING, false);
    g_d3d_device->SetRenderState(D3DRS_ZENABLE, false);
    g_d3d_device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
    g_d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
    g_d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    g_d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_d3d_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    g_d3d_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_d3d_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    auto FillRect = [](float x, float y, float w, float h, D3DCOLOR color) {
        if (!g_d3d_device) return;
        D3DVertex vertices[4] = {
            {x,     y,     0.0f, 1.0f, color},
            {x + w, y,     0.0f, 1.0f, color},
            {x,     y + h, 0.0f, 1.0f, color},
            {x + w, y + h, 0.0f, 1.0f, color}
        };
        g_d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(D3DVertex));
        };

    auto FillCircle = [](float x, float y, float radius, D3DCOLOR color, unsigned resolution = 192u) {
        if (!g_d3d_device) return;
        D3DVertex vertices[193];
        for (unsigned i = 0; i <= resolution; ++i) {
            float angle = static_cast<float>(i) / resolution * XM_2PI;
            vertices[i] = {
                x + radius * cosf(angle),
                y + radius * sinf(angle),
                0.0f,
                1.0f,
                color
            };
        }
        g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, resolution, vertices, sizeof(D3DVertex));
        };

    g_d3d_device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);

    if (use_circular_mask) {
        g_d3d_device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        g_d3d_device->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
        g_d3d_device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
        g_d3d_device->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);
        g_d3d_device->SetRenderState(D3DRS_STENCILREF, 1);
        g_d3d_device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        g_d3d_device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);

        FillCircle(mask_center_x, mask_center_y, mask_radius, D3DCOLOR_ARGB(0, 0, 0, 0));

        g_d3d_device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
        g_d3d_device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        g_d3d_device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    }
    else if (use_rectangle_mask) {
        g_d3d_device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        g_d3d_device->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
        g_d3d_device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
        g_d3d_device->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);
        g_d3d_device->SetRenderState(D3DRS_STENCILREF, 1);
        g_d3d_device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        g_d3d_device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);

        FillRect(mask_rect_x, mask_rect_y, mask_rect_width, mask_rect_height, D3DCOLOR_ARGB(0, 0, 0, 0));

        // Set stencil test to only draw where the rect was filled
        g_d3d_device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
        g_d3d_device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        g_d3d_device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    }
    else {
        FillRect(-5000.0f, -5000.0f, 10000.0f, 10000.0f, D3DCOLOR_ARGB(0, 0, 0, 0));
    }

    float cos_r = cosf(world_rotation);
    float sin_r = sinf(world_rotation);

    for (const auto& shape : primitives) {
        if (shape.size() != 3 && shape.size() != 4) continue;

        D3DVertex verts[4];
        for (size_t i = 0; i < shape.size(); ++i) {
            float x = shape[i].x;
            float y = shape[i].y;

            float x_out = x, y_out = y;
            if (world_space) {
                x_out = x * cos_r - y * sin_r;
                y_out = x * sin_r + y * cos_r;

                x_out *= world_scale;
                y_out *= world_scale;

                x_out = x_out * world_zoom_x + world_pan_x + screen_offset_x;
                y_out = y_out * world_zoom_y + world_pan_y + screen_offset_y;
            }

            verts[i] = {
                x_out, y_out, 0.0f, 1.0f, color
            };
        }

        if (shape.size() == 4)
            g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(D3DVertex));
        else
            g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, verts, sizeof(D3DVertex));
    }

    if (use_circular_mask || use_rectangle_mask) {
        g_d3d_device->SetRenderState(D3DRS_STENCILREF, 0);
        g_d3d_device->SetRenderState(D3DRS_STENCILWRITEMASK, 0x00000000);
        g_d3d_device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NEVER);
        g_d3d_device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        g_d3d_device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
        g_d3d_device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    }

    g_d3d_device->SetTransform(D3DTS_WORLD, &old_world);
    g_d3d_device->SetTransform(D3DTS_VIEW, &old_view);
    g_d3d_device->SetTransform(D3DTS_PROJECTION, &old_proj);

    state_block->Apply();
    state_block->Release();
}

void Py2DRenderer::DrawLine(Point2D from, Point2D to, D3DCOLOR color, float thickness) {
    if (!g_d3d_device) return;

    // Ensure 2D screen-space rendering is not affected by Z-buffer
    g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_d3d_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    if (thickness <= 1.0f) {
        D3DVertex verts[] = {
            { float(from.x), float(from.y), 0.0f, 1.0f, color },
            { float(to.x),   float(to.y),   0.0f, 1.0f, color }
        };
        g_d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        g_d3d_device->DrawPrimitiveUP(D3DPT_LINELIST, 1, verts, sizeof(D3DVertex));
        return;
    }

    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len == 0.0f) return;

    float nx = -dy / len;
    float ny = dx / len;
    float half = thickness * 0.5f;

    D3DVertex quad[] = {
        { from.x - nx * half, from.y - ny * half, 0.0f, 1.0f, color },
        { from.x + nx * half, from.y + ny * half, 0.0f, 1.0f, color },
        { to.x - nx * half, to.y - ny * half, 0.0f, 1.0f, color },
        { to.x + nx * half, to.y + ny * half, 0.0f, 1.0f, color }
    };

    g_d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(D3DVertex));
}


void Py2DRenderer::DrawTriangle(Point2D p1, Point2D p2, Point2D p3, D3DCOLOR color, float thickness) {
    DrawLine(p1, p2, color, thickness);
    DrawLine(p2, p3, color, thickness);
    DrawLine(p3, p1, color, thickness);
}

void Py2DRenderer::DrawTriangleFilled(Point2D p1, Point2D p2, Point2D p3, D3DCOLOR color) {
    // Ensure 2D screen-space rendering is not affected by Z-buffer
    g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_d3d_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    D3DVertex verts[] = {
        { float(p1.x), float(p1.y), 0.0f, 1.0f, color },
        { float(p2.x), float(p2.y), 0.0f, 1.0f, color },
        { float(p3.x), float(p3.y), 0.0f, 1.0f, color }
    };
    g_d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, verts, sizeof(D3DVertex));
}


void Py2DRenderer::DrawQuad(Point2D p1, Point2D p2, Point2D p3, Point2D p4, D3DCOLOR color, float thickness) {
    DrawLine(p1, p2, color, thickness);
    DrawLine(p2, p3, color, thickness);
    DrawLine(p3, p4, color, thickness);
    DrawLine(p4, p1, color, thickness);
}


void Py2DRenderer::DrawQuadFilled(Point2D p1, Point2D p2, Point2D p3, Point2D p4, D3DCOLOR color) {
    // Ensure 2D screen-space rendering is not affected by Z-buffer
    g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_d3d_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    D3DVertex verts[6] = {
        { float(p1.x), float(p1.y), 0.0f, 1.0f, color },
        { float(p2.x), float(p2.y), 0.0f, 1.0f, color },
        { float(p3.x), float(p3.y), 0.0f, 1.0f, color },
        { float(p3.x), float(p3.y), 0.0f, 1.0f, color },
        { float(p4.x), float(p4.y), 0.0f, 1.0f, color },
        { float(p1.x), float(p1.y), 0.0f, 1.0f, color }
    };

    g_d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, verts, sizeof(D3DVertex));
}


void Py2DRenderer::DrawPoly(Point2D center, float radius, D3DCOLOR color, int segments, float thickness) {
    float step = 2.0f * 3.141592654 / segments;
    for (int i = 0; i < segments; ++i) {
        float a0 = step * i;
        float a1 = step * (i + 1);
        Point2D p1(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius);
        Point2D p2(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);
        DrawLine(p1, p2, color, thickness);
    }
}

void Py2DRenderer::DrawPolyFilled(Point2D center, float radius, D3DCOLOR color, int segments) {
    // Ensure 2D screen-space rendering is not affected by Z-buffer
    g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_d3d_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    std::vector<D3DVertex> verts;
    verts.emplace_back(D3DVertex{ static_cast<float>(center.x), static_cast<float>(center.y), 0.0f, 1.0f, color });

    float step = 2.0f * 3.141592654 / segments;
    for (int i = 0; i <= segments; ++i) {
        float angle = step * i;
        float x = center.x + cosf(angle) * radius;
        float y = center.y + sinf(angle) * radius;
        verts.emplace_back(D3DVertex{ x, y, 0.0f, 1.0f, color });
    }

    g_d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, segments, verts.data(), sizeof(D3DVertex));
}


void Py2DRenderer::Setup3DView() {
    if (!g_d3d_device) return;

    // Lighting and blending
    g_d3d_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_d3d_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_d3d_device->SetRenderState(D3DRS_AMBIENT, 0xFFFFFFFF);

    // Depth testing
    g_d3d_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    g_d3d_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

    auto* cam = GW::CameraMgr::GetCamera();
    if (!cam) return;

    // Right-handed camera vectors (flip Z)
    D3DXVECTOR3 eye(cam->position.x, cam->position.y, -cam->position.z);
    D3DXVECTOR3 at(cam->look_at_target.x, cam->look_at_target.y, -cam->look_at_target.z);
    D3DXVECTOR3 up(0.0f, 0.0f, 1.0f); // RH: Z+ is up

    D3DXMATRIX view;
    D3DXMatrixLookAtRH(&view, &eye, &at, &up);
    g_d3d_device->SetTransform(D3DTS_VIEW, &view);

    float fov = GW::Render::GetFieldOfView();
    float aspect = static_cast<float>(GW::Render::GetViewportWidth()) / static_cast<float>(GW::Render::GetViewportHeight());
    float near_plane = 48000.0f / 1024.0f;
    float far_plane = 48000.0f;

    D3DXMATRIX proj;
    D3DXMatrixPerspectiveFovRH(&proj, fov, aspect, near_plane, far_plane);
    g_d3d_device->SetTransform(D3DTS_PROJECTION, &proj);

    //Set identity world transform
    D3DXMATRIX identity;
    D3DXMatrixIdentity(&identity);
    g_d3d_device->SetTransform(D3DTS_WORLD, &identity);

}


void Py2DRenderer::DrawLine3D(Point3D from, Point3D to, D3DCOLOR color, bool use_occlusion, int segments, float floor_offset)
{
    if (!g_d3d_device) return;

    Setup3DView();

    if (!use_occlusion) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    struct D3DVertex3D { float x, y, z; D3DCOLOR color; };
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    // Single segment: original behavior
    if (segments <= 1) {
        D3DVertex3D v[2] = {
            { from.x, from.y, -from.z, color },
            { to.x,   to.y,   -to.z,   color }
        };
        g_d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        g_d3d_device->DrawPrimitiveUP(D3DPT_LINELIST, 1, v, sizeof(D3DVertex3D));
        return;
    }

    // N segments: snap each segment’s endpoints to ground using its own z-plane (midpoint z)
    g_d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    for (int i = 0; i < segments; ++i) {
        float t0 = float(i) / float(segments);
        float t1 = float(i + 1) / float(segments);
        float x0 = lerp(from.x, to.x, t0);
        float y0 = lerp(from.y, to.y, t0);
        float x1 = lerp(from.x, to.x, t1);
        float y1 = lerp(from.y, to.y, t1);

        float zplane = lerp(from.z, to.z, (t0 + t1) * 0.5f);
		float z0 = overlay.findZ(x0, y0, 0) - floor_offset;
		float z1 = overlay.findZ(x1, y1, 0) - floor_offset;

        D3DVertex3D seg[2] = {
            { x0, y0, -z0, color },
            { x1, y1, -z1, color }
        };
        g_d3d_device->DrawPrimitiveUP(D3DPT_LINELIST, 1, seg, sizeof(D3DVertex3D));
    }
}




// ---------- TRIANGLE (outline) ----------
void Py2DRenderer::DrawTriangle3D(Point3D p1, Point3D p2, Point3D p3, D3DCOLOR color, bool use_occlusion, int edge_segments, float floor_offset)
{
    if (!g_d3d_device) return;
    // Reuse the line routine you just approved (per-segment FindZ, RH flip, etc.)
	DrawLine3D(p1, p2, color, use_occlusion, edge_segments, floor_offset);
	DrawLine3D(p2, p3, color, use_occlusion, edge_segments, floor_offset);
	DrawLine3D(p3, p1, color, use_occlusion, edge_segments, floor_offset);
}

void Py2DRenderer::DrawTriangleFilled3D(Point3D p1, Point3D p2, Point3D p3, D3DCOLOR color, bool use_occlusion, int segments, float floor_offset)
{
    if (!g_d3d_device) return;
    Setup3DView();
    if (!use_occlusion) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    struct V { float x, y, z; D3DCOLOR c; };
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto LerpP = [&](const Point3D& A, const Point3D& B, float t) {
        return Point3D{ lerp(A.x,B.x,t), lerp(A.y,B.y,t), lerp(A.z,B.z,t) };
        };

    // Subdivide the triangle into 'segments' radial strips from p1
    g_d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    for (int i = 0; i < segments; ++i) {
        float t0 = float(i) / float(segments);
        float t1 = float(i + 1) / float(segments);

        // Two edges from p1: p1->p2 and p1->p3
        Point3D a0 = LerpP(p1, p2, t0);
        Point3D a1 = LerpP(p1, p2, t1);
        Point3D b0 = LerpP(p1, p3, t0);
        Point3D b1 = LerpP(p1, p3, t1);

        // First small triangle: (a0, b0, b1)
        {
            // Use this triangle's centroid z as z-plane selector
            float zplane = (a0.z + b0.z + b1.z) / 3.0f;
            float za0 = overlay.findZ(a0.x, a0.y, zplane);
            float zb0 = overlay.findZ(b0.x, b0.y, zplane);
            float zb1 = overlay.findZ(b1.x, b1.y, zplane);

            V tri[3] = {
                { a0.x, a0.y, -za0, color },
                { b0.x, b0.y, -zb0, color },
                { b1.x, b1.y, -zb1, color }
            };
            g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, tri, sizeof(V));
        }

        // Second small triangle: (a0, b1, a1)
        {
            float zplane = (a0.z + b1.z + a1.z) / 3.0f;
            float za0 = overlay.findZ(a0.x, a0.y, zplane);
            float zb1 = overlay.findZ(b1.x, b1.y, zplane);
            float za1 = overlay.findZ(a1.x, a1.y, zplane);

            V tri[3] = {
                { a0.x, a0.y, -za0, color },
                { b1.x, b1.y, -zb1, color },
                { a1.x, a1.y, -za1, color }
            };
            g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, tri, sizeof(V));
        }
    }
}

// ---------- QUAD (outline) ----------
void Py2DRenderer::DrawQuad3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, D3DCOLOR color, bool use_occlusion, int edge_segments, float floor_offset)
{
    if (!g_d3d_device) return;
	DrawLine3D(p1, p2, color, use_occlusion, edge_segments, floor_offset);
	DrawLine3D(p2, p3, color, use_occlusion, edge_segments, floor_offset);
	DrawLine3D(p3, p4, color, use_occlusion, edge_segments, floor_offset);
	DrawLine3D(p4, p1, color, use_occlusion, edge_segments, floor_offset);
}

void Py2DRenderer::DrawQuadFilled3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, D3DCOLOR color, bool use_occlusion, int segments, float floor_offset)
{
    if (!g_d3d_device) return;
    // Split into two triangles; reuse the draped triangle filler
	DrawTriangleFilled3D(p1, p2, p3, color, use_occlusion, segments, floor_offset);
	DrawTriangleFilled3D(p3, p4, p1, color, use_occlusion, segments, floor_offset);
}

// ---------- POLY (outline) ----------
// 'sides' = number of polygon sides (existing meaning).
// 'edge_segments' = per-edge snapping subdivisions (new).
void Py2DRenderer::DrawPoly3D(Point3D center, float radius, D3DCOLOR color, int numSegments, bool autoZ, bool use_occlusion, int segments, float floor_offset)
{
    if (!g_d3d_device) return;
    Setup3DView();
    if (!use_occlusion) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    struct V { float x, y, z; D3DCOLOR c; };
    const float step = XM_2PI / numSegments;

    // Precompute ring points (XY); Z will be sampled per sub-segment
    std::vector<Point3D> ring;
    ring.reserve(numSegments + 1);
    for (int i = 0; i <= numSegments; ++i) {
        float a = step * i;
        float x = center.x + cosf(a) * radius;
        float y = center.y + sinf(a) * radius;
        float z = autoZ ? overlay.findZ(x, y, center.z) : center.z; // base Z (not final)
        ring.push_back({ x, y, z });
    }

    g_d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);

    // For each edge, split into 'edge_segments' and snap each small segment
    for (int i = 0; i < numSegments; ++i) {
        Point3D A = ring[i];
        Point3D B = ring[i + 1];

        for (int s = 0; s < std::max(1, segments); ++s) {
            float t0 = float(s) / float(std::max(1, segments));
            float t1 = float(s + 1) / float(std::max(1, segments));
            float x0 = A.x + (B.x - A.x) * t0;
            float y0 = A.y + (B.y - A.y) * t0;
            float x1 = A.x + (B.x - A.x) * t1;
            float y1 = A.y + (B.y - A.y) * t1;

            // Use this sub-segment’s midpoint z as the z-plane selector
            float zplane = (A.z + B.z) * 0.5f;
			float z0 = overlay.findZ(x0, y0, zplane) - floor_offset;
			float z1 = overlay.findZ(x1, y1, zplane) - floor_offset;

            V seg[2] = {
                { x0, y0, -z0, color },
                { x1, y1, -z1, color }
            };
            g_d3d_device->DrawPrimitiveUP(D3DPT_LINELIST, 1, seg, sizeof(V));
        }
    }
}

// ---------- POLY (filled, draped by triangulating each fan triangle and snapping per sub-tri) ----------
// 'sides' = polygon sides (existing).
// 'edge_segments' = how many sub-tris along each outer edge.
void Py2DRenderer::DrawPolyFilled3D(Point3D center, float radius, D3DCOLOR color, int numSegments, bool autoZ, bool use_occlusion, int segments, float floor_offset)
{
    if (!g_d3d_device) return;
    Setup3DView();
    if (!use_occlusion) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    struct V { float x, y, z; D3DCOLOR c; };
    const float step = XM_2PI / numSegments;

    // Precompute center Z (if requested)
	float zc = autoZ ? overlay.findZ(center.x, center.y, center.z) : center.z - floor_offset;

    // Precompute ring points (XY only; Z is sampled per sub-tri)
    std::vector<Point3D> ring;
    ring.reserve(numSegments + 1);
    for (int i = 0; i <= numSegments; ++i) {
        float a = step * i;
        ring.push_back({ center.x + cosf(a) * radius,
                         center.y + sinf(a) * radius,
                         center.z });
    }

    g_d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);

    for (int i = 0; i < numSegments; ++i) {
        Point3D A = ring[i];
        Point3D B = ring[i + 1];

        // Split fan triangle (center, A, B) along edge A->B
        int n = std::max(1, segments);
        for (int s = 0; s < n; ++s) {
            float t0 = float(s) / float(n);
            float t1 = float(s + 1) / float(n);

            Point3D E0{ A.x + (B.x - A.x) * t0, A.y + (B.y - A.y) * t0, center.z };
            Point3D E1{ A.x + (B.x - A.x) * t1, A.y + (B.y - A.y) * t1, center.z };

            // Triangle (center, E0, E1): pick this tri’s centroid z as z-plane
            float zplane = (zc + E0.z + E1.z) / 3.0f;

            float zCenter = overlay.findZ(center.x, center.y, zplane);
			float zE0 = overlay.findZ(E0.x, E0.y, zplane) - floor_offset;
			float zE1 = overlay.findZ(E1.x, E1.y, zplane) - floor_offset;

            V tri[3] = {
                { center.x, center.y, -zCenter, color },
                { E0.x,     E0.y,     -zE0,     color },
                { E1.x,     E1.y,     -zE1,     color }
            };
            g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, tri, sizeof(V));
        }
    }
}
void Py2DRenderer::DrawCubeOutline(Point3D center, float size, D3DCOLOR color, bool use_occlusion) {
    if (!g_d3d_device) return;
    Setup3DView();

    if (!use_occlusion) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    float h = size * 0.5f;

    Point3D c[8] = {
        {center.x - h, center.y - h, center.z - h},
        {center.x + h, center.y - h, center.z - h},
        {center.x + h, center.y + h, center.z - h},
        {center.x - h, center.y + h, center.z - h},
        {center.x - h, center.y - h, center.z + h},
        {center.x + h, center.y - h, center.z + h},
        {center.x + h, center.y + h, center.z + h},
        {center.x - h, center.y + h, center.z + h}
    };

    const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (int i = 0; i < 12; ++i) {
        DrawLine3D(c[edges[i][0]], c[edges[i][1]], color, use_occlusion);
    }
}

void Py2DRenderer::DrawCubeFilled(Point3D center, float size, D3DCOLOR color, bool use_occlusion) {
    if (!g_d3d_device) return;
    Setup3DView();

    if (!use_occlusion) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    float h = size * 0.5f;

    Point3D c[8] = {
        {center.x - h, center.y - h, center.z - h},
        {center.x + h, center.y - h, center.z - h},
        {center.x + h, center.y + h, center.z - h},
        {center.x - h, center.y + h, center.z - h},
        {center.x - h, center.y - h, center.z + h},
        {center.x + h, center.y - h, center.z + h},
        {center.x + h, center.y + h, center.z + h},
        {center.x - h, center.y + h, center.z + h}
    };

    const int faces[6][4] = {
        {0,1,2,3}, // back
        {4,5,6,7}, // front
        {0,1,5,4}, // bottom
        {2,3,7,6}, // top
        {1,2,6,5}, // right
        {0,3,7,4}  // left
    };

    for (int i = 0; i < 6; ++i) {
        DrawQuadFilled3D(c[faces[i][0]], c[faces[i][1]], c[faces[i][2]], c[faces[i][3]], color, use_occlusion);
    }
}

void Py2DRenderer::DrawTexture(const std::string& file_path, float screen_pos_x, float screen_pos_y, float width, float height, uint32_t int_tint) {
    D3DCOLOR tint = D3DCOLOR_ARGB((int_tint >> 24) & 0xFF, (int_tint >> 16) & 0xFF, (int_tint >> 8) & 0xFF, int_tint & 0xFF);
    tint = 0xFFFFFFFF; // Force full opacity for 2D textures

    if (!g_d3d_device) return;

    std::wstring wpath(file_path.begin(), file_path.end());
    LPDIRECT3DTEXTURE9 texture = overlay.texture_manager.GetTexture(wpath);
    if (!texture) return;

    float w = width * 0.5f;
    float h = height * 0.5f;

    struct Vertex {
        float x, y, z, rhw;
        float u, v;
        D3DCOLOR color;
    };

    Vertex quad[] = {
        { screen_pos_x - w, screen_pos_y - h, 0.0f, 1.0f, 0.0f, 0.0f, tint },
        { screen_pos_x + w, screen_pos_y - h, 0.0f, 1.0f, 1.0f, 0.0f, tint },
        { screen_pos_x + w, screen_pos_y + h, 0.0f, 1.0f, 1.0f, 1.0f, tint },
        { screen_pos_x - w, screen_pos_y + h, 0.0f, 1.0f, 0.0f, 1.0f, tint },
    };

    // Set render states for textures
    g_d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
    g_d3d_device->SetTexture(0, texture);

    // Set texture blending
    g_d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

    g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, quad, sizeof(Vertex));
}


void Py2DRenderer::DrawTexture3D(const std::string& file_path, float world_pos_x, float world_pos_y, float world_pos_z, float width, float height, bool use_occlusion, uint32_t int_tint) {
    D3DCOLOR tint = D3DCOLOR_ARGB((int_tint >> 24) & 0xFF, (int_tint >> 16) & 0xFF, (int_tint >> 8) & 0xFF, int_tint & 0xFF);
    tint = 0xFFFFFFFF;

    if (!g_d3d_device) return;

    std::wstring wpath(file_path.begin(), file_path.end());
    LPDIRECT3DTEXTURE9 texture = overlay.texture_manager.GetTexture(wpath);
    if (!texture) return;

    Setup3DView();

    if (!use_occlusion) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    // Flip Z to match RH coordinate system
    world_pos_z = -world_pos_z;

    float w = width / 2.0f;
    float h = height / 2.0f;

    struct D3DTexturedVertex3D {
        float x, y, z;
        float u, v;
        D3DCOLOR color;
    };

    D3DTexturedVertex3D verts[] = {
        { world_pos_x - w, world_pos_y - h, world_pos_z, 0.0f, 0.0f, tint },
        { world_pos_x + w, world_pos_y - h, world_pos_z, 1.0f, 0.0f, tint },
        { world_pos_x + w, world_pos_y + h, world_pos_z, 1.0f, 1.0f, tint },
        { world_pos_x - w, world_pos_y + h, world_pos_z, 0.0f, 1.0f, tint }
    };

    g_d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
    g_d3d_device->SetTexture(0, texture);
    g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, verts, sizeof(D3DTexturedVertex3D));

    if (!use_occlusion) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, TRUE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    }
}

void Py2DRenderer::DrawQuadTextured3D(const std::string& file_path,
    Point3D p1, Point3D p2, Point3D p3, Point3D p4,
    bool use_occlusion,
    uint32_t int_tint) {
    D3DCOLOR tint = D3DCOLOR_ARGB((int_tint >> 24) & 0xFF, (int_tint >> 16) & 0xFF, (int_tint >> 8) & 0xFF, int_tint & 0xFF);
    tint = 0xFFFFFFFF;
    if (!g_d3d_device) return;

    std::wstring wpath(file_path.begin(), file_path.end());
    LPDIRECT3DTEXTURE9 texture = TextureManager::Instance().GetTexture(wpath);
    if (!texture) return;

    Setup3DView();

    if (!use_occlusion) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    // Flip Z to match RH system
    p1.z = -p1.z;
    p2.z = -p2.z;
    p3.z = -p3.z;
    p4.z = -p4.z;

    struct D3DTexturedVertex3D {
        float x, y, z;
        float u, v;
        D3DCOLOR color;
    };

    D3DTexturedVertex3D verts[] = {
        { p1.x, p1.y, p1.z, 0.0f, 0.0f, tint },
        { p2.x, p2.y, p2.z, 1.0f, 0.0f, tint },
        { p3.x, p3.y, p3.z, 1.0f, 1.0f, tint },
        { p4.x, p4.y, p4.z, 0.0f, 1.0f, tint }
    };

    g_d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
    g_d3d_device->SetTexture(0, texture);
    g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, verts, sizeof(D3DTexturedVertex3D));

    if (!use_occlusion) {
        g_d3d_device->SetRenderState(D3DRS_ZENABLE, TRUE);
        g_d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    }
}

enum SaveGeometryResult {
    SAVE_OK = 0,
    ERR_NO_DEVICE,
    ERR_NO_PRIMITIVES,
    ERR_INVALID_DIMENSIONS,
    ERR_CREATE_TEXTURE,
    ERR_GET_SURFACE,
    ERR_GET_OLD_RT,
    ERR_SET_RT,
    ERR_CREATE_STATEBLOCK,
    ERR_SET_VIEWPORT,
    ERR_SAVE_FILE,
    ERR_STATEBLOCK_APPLY,
    ERR_UNKNOWN
};

int Py2DRenderer::SaveGeometryToFile(
    const std::wstring& filename,
    float min_x, float min_y,
    float max_x, float max_y
) {
    if (!g_d3d_device) return ERR_NO_DEVICE;
    if (primitives.empty()) return ERR_NO_PRIMITIVES;

    float src_width = max_x - min_x;
    float src_height = max_y - min_y;
    if (src_width <= 0 || src_height <= 0) return ERR_INVALID_DIMENSIONS;

    // --- Scale to fit max 2048 while preserving aspect ratio
    float scale = 2048.0f / std::max(src_width, src_height);
    int out_width = static_cast<int>(src_width * scale);
    int out_height = static_cast<int>(src_height * scale);

    // --- Create render target texture
    IDirect3DTexture9* rtTexture = nullptr;
    HRESULT hr = g_d3d_device->CreateTexture(
        out_width, out_height, 1,
        D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        &rtTexture, nullptr
    );
    if (FAILED(hr) || !rtTexture) return ERR_CREATE_TEXTURE;

    IDirect3DSurface9* rtSurface = nullptr;
    hr = rtTexture->GetSurfaceLevel(0, &rtSurface);
    if (FAILED(hr) || !rtSurface) { rtTexture->Release(); return ERR_GET_SURFACE; }

    IDirect3DSurface9* oldSurface = nullptr;
    hr = g_d3d_device->GetRenderTarget(0, &oldSurface);
    if (FAILED(hr)) { rtSurface->Release(); rtTexture->Release(); return ERR_GET_OLD_RT; }

    hr = g_d3d_device->SetRenderTarget(0, rtSurface);
    if (FAILED(hr)) { oldSurface->Release(); rtSurface->Release(); rtTexture->Release(); return ERR_SET_RT; }

    // --- State block save
    IDirect3DStateBlock9* state_block = nullptr;
    hr = g_d3d_device->CreateStateBlock(D3DSBT_ALL, &state_block);
    if (FAILED(hr)) { g_d3d_device->SetRenderTarget(0, oldSurface); oldSurface->Release(); rtSurface->Release(); rtTexture->Release(); return ERR_CREATE_STATEBLOCK; }

    D3DMATRIX old_world, old_view, old_proj;
    g_d3d_device->GetTransform(D3DTS_WORLD, &old_world);
    g_d3d_device->GetTransform(D3DTS_VIEW, &old_view);
    g_d3d_device->GetTransform(D3DTS_PROJECTION, &old_proj);

    // --- Viewport
    D3DVIEWPORT9 vp = { 0, 0, (DWORD)out_width, (DWORD)out_height, 0.0f, 1.0f };
    hr = g_d3d_device->SetViewport(&vp);
    if (FAILED(hr)) {
        state_block->Release();
        g_d3d_device->SetRenderTarget(0, oldSurface);
        oldSurface->Release(); rtSurface->Release(); rtTexture->Release();
        return ERR_SET_VIEWPORT;
    }

    // --- Clear target
    g_d3d_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

    // === COPY YOUR render() PIPELINE EXACTLY HERE ===
    // including SetupProjection, render states, FillRect/Circle, primitive loop etc.
    // The ONLY difference is: coordinates should be transformed with `scale` and offset `-min_x, -min_y`
    // to fit inside the output texture.

    float cos_r = cosf(world_rotation);
    float sin_r = sinf(world_rotation);

    for (const auto& shape : primitives) {
        if (shape.size() != 3 && shape.size() != 4) continue;

        D3DVertex verts[4];
        for (size_t i = 0; i < shape.size(); ++i) {
            float x = (shape[i].x - min_x) * scale;
            float y = (shape[i].y - min_y) * scale;

            verts[i] = { x, y, 0.0f, 1.0f, color };
        }

        if (shape.size() == 4)
            g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(D3DVertex));
        else
            g_d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, verts, sizeof(D3DVertex));
    }

    // --- Save to file
    hr = D3DXSaveSurfaceToFileW(filename.c_str(), D3DXIFF_PNG, rtSurface, nullptr, nullptr);

    // --- Restore state
    g_d3d_device->SetRenderTarget(0, oldSurface);
    g_d3d_device->SetTransform(D3DTS_WORLD, &old_world);
    g_d3d_device->SetTransform(D3DTS_VIEW, &old_view);
    g_d3d_device->SetTransform(D3DTS_PROJECTION, &old_proj);

    if (state_block) {
        hr = state_block->Apply();
        state_block->Release();
        if (FAILED(hr)) { oldSurface->Release(); rtSurface->Release(); rtTexture->Release(); return ERR_STATEBLOCK_APPLY; }
    }

    // Cleanup
    if (oldSurface) oldSurface->Release();
    if (rtSurface) rtSurface->Release();
    if (rtTexture) rtTexture->Release();

    if (FAILED(hr)) return ERR_SAVE_FILE;
    return SAVE_OK;
}




void bind_2drenderer(py::module_& m) {
    py::class_<Py2DRenderer>(m, "Py2DRenderer")
        .def(py::init<>())
        .def("set_primitives", &Py2DRenderer::set_primitives, py::arg("primitives"), py::arg("draw_color") = 0xFFFFFFFF)
        .def("set_world_zoom_x", &Py2DRenderer::set_world_zoom_x, py::arg("zoom"))
        .def("set_world_zoom_y", &Py2DRenderer::set_world_zoom_y, py::arg("zoom"))
        .def("set_world_pan", &Py2DRenderer::set_world_pan, py::arg("x"), py::arg("y"))
        .def("set_world_rotation", &Py2DRenderer::set_world_rotation, py::arg("r"))
        .def("set_world_space", &Py2DRenderer::set_world_space, py::arg("enabled"))
        .def("set_world_scale", &Py2DRenderer::set_world_scale, py::arg("scale"))

        .def("set_screen_offset", &Py2DRenderer::set_screen_offset, py::arg("x"), py::arg("y"))
        .def("set_screen_zoom_x", &Py2DRenderer::set_screen_zoom_x, py::arg("zoom"))
        .def("set_screen_zoom_y", &Py2DRenderer::set_screen_zoom_y, py::arg("zoom"))
        .def("set_screen_rotation", &Py2DRenderer::set_screen_rotation, py::arg("r"))

        .def("set_circular_mask", &Py2DRenderer::set_circular_mask, py::arg("enabled"))
        .def("set_circular_mask_radius", &Py2DRenderer::set_circular_mask_radius, py::arg("radius"))
        .def("set_circular_mask_center", &Py2DRenderer::set_circular_mask_center, py::arg("x"), py::arg("y"))

        .def("set_rectangle_mask", &Py2DRenderer::set_rectangle_mask, py::arg("enabled"))
        .def("set_rectangle_mask_bounds", &Py2DRenderer::set_rectangle_mask_bounds, py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"))

        .def("render", &Py2DRenderer::render)


        .def("DrawLine", &Py2DRenderer::DrawLine, py::arg("from"), py::arg("to"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawTriangle", &Py2DRenderer::DrawTriangle, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawTriangleFilled", &Py2DRenderer::DrawTriangleFilled, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawQuad", &Py2DRenderer::DrawQuad, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawQuadFilled", &Py2DRenderer::DrawQuadFilled, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawPoly", &Py2DRenderer::DrawPoly, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("segments") = 3, py::arg("thickness") = 1.0f)
        .def("DrawPolyFilled", &Py2DRenderer::DrawPolyFilled, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("segments") = 3)
        .def("DrawCubeOutline", &Py2DRenderer::DrawCubeOutline, py::arg("center"), py::arg("size"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true)
        .def("DrawCubeFilled", &Py2DRenderer::DrawCubeFilled, py::arg("center"), py::arg("size"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true)

		.def("DrawLine3D", &Py2DRenderer::DrawLine3D, py::arg("from"), py::arg("to"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true, py::arg("segments") = 16, py::arg("floor_offset") = 0.0f)
		.def("DrawTriangle3D", &Py2DRenderer::DrawTriangle3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true, py::arg("edge_segments") = 16, py::arg("floor_offset") = 0.0f)
		.def("DrawTriangleFilled3D", &Py2DRenderer::DrawTriangleFilled3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true, py::arg("edge_segments") = 16, py::arg("floor_offset") = 0.0f)
		.def("DrawQuad3D", &Py2DRenderer::DrawQuad3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true, py::arg("edge_segments") = 16, py::arg("floor_offset") = 0.0f)
		.def("DrawQuadFilled3D", &Py2DRenderer::DrawQuadFilled3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("use_occlusion") = true, py::arg("segments") = 16, py::arg("floor_offset") = 0.0f)
		.def("DrawPoly3D", &Py2DRenderer::DrawPoly3D, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 3, py::arg("autoZ") = true, py::arg("use_occlusion") = true, py::arg("segments") = 16, py::arg("floor_offset") = 0.0f)
		.def("DrawPolyFilled3D", &Py2DRenderer::DrawPolyFilled3D, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 3, py::arg("autoZ") = true, py::arg("use_occlusion") = true, py::arg("segments") = 16, py::arg("floor_offset") = 0.0f)

        .def("Setup3DView", &Py2DRenderer::Setup3DView)
        .def("ApplyStencilMask", &Py2DRenderer::ApplyStencilMask)
        .def("ResetStencilMask", &Py2DRenderer::ResetStencilMask)
        .def("DrawTexture", &Py2DRenderer::DrawTexture, py::arg("file_path"), py::arg("screen_pos_x"), py::arg("screen_pos_y"), py::arg("width") = 100.0f, py::arg("height") = 100.0f, py::arg("int_tint") = 0xFFFFFFFF)
        .def("DrawTexture3D", &Py2DRenderer::DrawTexture3D, py::arg("file_path"), py::arg("world_pos_x"), py::arg("world_pos_y"), py::arg("world_pos_z"), py::arg("width") = 100.0f, py::arg("height") = 100.0f, py::arg("use_occlusion") = true, py::arg("int_tint") = 0xFFFFFFFF)
        .def("DrawQuadTextured3D", &Py2DRenderer::DrawQuadTextured3D, py::arg("file_path"),
            py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"),
            py::arg("use_occlusion") = true, py::arg("int_tint") = 0xFFFFFFFF)
        .def("SaveGeometryToFile", &Py2DRenderer::SaveGeometryToFile, py::arg("filename"), py::arg("min_x"), py::arg("min_y"), py::arg("max_x"), py::arg("max_y"));
}

PYBIND11_EMBEDDED_MODULE(Py2DRenderer, m) {
    bind_2drenderer(m);
}
