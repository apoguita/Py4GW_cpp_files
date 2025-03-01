#include "py_overlay.h"

GW::Vec3f MouseWorldPos;
GW::Vec2f MouseScreenPos;

void GlobalMouseClass::SetMouseWorldPos(float x, float y, float z) {
    MouseWorldPos.x = x;
    MouseWorldPos.y = y;
    MouseWorldPos.z = z;
}

GW::Vec3f GlobalMouseClass::GetMouseWorldPos() {
    return MouseWorldPos;
}

void GlobalMouseClass::SetMouseCoords(float x, float y) {
    MouseScreenPos.x = x;
    MouseScreenPos.y = y;
}

GW::Vec2f GlobalMouseClass::GetMouseCoords() {
    return MouseScreenPos;
}

// Overlay methods
Overlay::Overlay() {
    drawList = ImGui::GetWindowDrawList();
}

void Overlay::RefreshDrawList() {
	drawList = ImGui::GetWindowDrawList();
}
Point2D Overlay::GetMouseCoords() {
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    return Point2D(mouse_pos.x, mouse_pos.y);
}


XMMATRIX Overlay::CreateViewMatrix(const XMFLOAT3& eye_pos, const XMFLOAT3& look_at_pos, const XMFLOAT3& up_direction) {
    XMMATRIX viewMatrix = XMMatrixLookAtLH(XMLoadFloat3(&eye_pos), XMLoadFloat3(&look_at_pos), XMLoadFloat3(&up_direction));
    return viewMatrix;
}

XMMATRIX Overlay::CreateProjectionMatrix(float fov, float aspect_ratio, float near_plane, float far_plane) {
    XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(fov, aspect_ratio, near_plane, far_plane);
    return projectionMatrix;
}

// Function to convert world coordinates to screen coordinates
GW::Vec3f Overlay::GetWorldToScreen(const GW::Vec3f& world_position, const XMMATRIX& mat_view, const XMMATRIX& mat_proj, float viewport_width, float viewport_height) {
    GW::Vec3f res;

    // Transform the point into camera space
    XMVECTOR pos = XMVectorSet(world_position.x, world_position.y, world_position.z, 1.0f);
    pos = XMVector3TransformCoord(pos, mat_view);

    // Transform the point into projection space
    pos = XMVector3TransformCoord(pos, mat_proj);

    // Perform perspective division
    XMFLOAT4 projected;
    XMStoreFloat4(&projected, pos);
    if (projected.w < 0.1f) return res; // Point is behind the camera

    projected.x /= projected.w;
    projected.y /= projected.w;
    projected.z /= projected.w;

    // Transform to screen space
    res.x = ((projected.x + 1.0f) / 2.0f) * viewport_width;
    res.y = ((-projected.y + 1.0f) / 2.0f) * viewport_height;
    res.z = projected.z;

    return res;
}

void Overlay::GetScreenToWorld() {

    GW::GameThread::Enqueue([]() {


        GlobalMouseClass setmousepos;
        GW::Vec2f* screen_coord = 0;
        uintptr_t address = GW::Scanner::Find("\x8B\xE5\x5D\xC3\x8B\x4F\x10\x83\xC7\x08", "xxxxxxxxxx", 0xC);
        //uintptr_t address = ptrGetter.GetNdcScreenCoordsPtr();
        if (Verify(address))
        {
            screen_coord = *reinterpret_cast<GW::Vec2f**>(address);
        }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); return; }

        address = GW::Scanner::Find("\xD9\x5D\x14\xD9\x45\x14\x83\xEC\x0C", "xxxxxxxx", -0x2F);
        if (address)
        {
            ScreenToWorldPoint_Func = (ScreenToWorldPoint_pt)address;
        }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); return; }

        address = GW::Scanner::Find("\xff\x75\x08\xd9\x5d\xfc\xff\x77\x7c", "xxxxxxxxx", -0x27);
        if (address) {
            MapIntersect_Func = (MapIntersect_pt)address;
        }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); return; }

        GW::Vec3f origin;
        GW::Vec3f p2;
        float unk1 = ScreenToWorldPoint_Func(&origin, screen_coord->x, screen_coord->y, 0);
        float unk2 = ScreenToWorldPoint_Func(&p2, screen_coord->x, screen_coord->y, 1);
        GW::Vec3f dir = p2 - origin;
        GW::Vec3f ray_dir = GW::Normalize(dir);
        GW::Vec3f hit_point;
        int layer = 0;
        uint32_t ret = MapIntersect_Func(&origin, &ray_dir, &hit_point, &layer); //needs to run in game thread

        if (ret) { setmousepos.SetMouseWorldPos(hit_point.x, hit_point.y, hit_point.z); }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); }

        });

}

float Overlay::findZ(float x, float y, float pz) {

    float altitude = ALTITUDE_UNKNOWN;
    unsigned int cur_altitude = 0u;
    float z = pz - (pz * 0.05);// adds an offset more of z to look for unleveled planes

    // in order to properly query altitudes, we have to use the pathing map
    // to determine the number of Z planes in the current map.
    const GW::PathingMapArray* pathing_map = GW::Map::GetPathingMap();
    if (pathing_map != nullptr) {
        const size_t pmap_size = pathing_map->size();
        if (pmap_size > 0) {

            GW::Map::QueryAltitude({ x, y, pmap_size - 1 }, 5.f, altitude);
            if (altitude < z) {
                // recall that the Up camera component is inverted
                z = altitude;
            }

        }
    }
    return z;
}

Point2D Overlay::WorldToScreen(float x, float y, float z) {
    GW::Vec3f world_position = { x, y, z };

    static auto cam = GW::CameraMgr::GetCamera();
    float camX = GW::CameraMgr::GetCamera()->position.x;
    float camY = GW::CameraMgr::GetCamera()->position.y;
    float camZ = GW::CameraMgr::GetCamera()->position.z;


    XMFLOAT3 eyePos = { camX, camY, camZ };
    XMFLOAT3 lookAtPos = { cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z };
    XMFLOAT3 upDir = { 0.0f, 0.0f, -1.0f };  // Up Vector

    // Create view matrix
    XMMATRIX viewMatrix = CreateViewMatrix(eyePos, lookAtPos, upDir);

    // Define projection parameters
    float fov = GW::Render::GetFieldOfView();
    float aspectRatio = static_cast<float>(GW::Render::GetViewportWidth()) / static_cast<float>(GW::Render::GetViewportHeight());
    float nearPlane = 0.1f;
    float farPlane = 100000.0f;

    // Create projection matrix
    XMMATRIX projectionMatrix = CreateProjectionMatrix(fov, aspectRatio, nearPlane, farPlane);
    GW::Vec3f res = GetWorldToScreen(world_position, viewMatrix, projectionMatrix, static_cast<float>(GW::Render::GetViewportWidth()), static_cast<float>(GW::Render::GetViewportHeight()));
    Point2D ret = Point2D(res.x, res.y);
    return ret;

}

Point3D Overlay::GetMouseWorldPos() {
    GetScreenToWorld();
    GlobalMouseClass mousepos;
    GW::Vec3f ret_mouse_coords = mousepos.GetMouseWorldPos();
    Point3D ret = Point3D(ret_mouse_coords.x, ret_mouse_coords.y, ret_mouse_coords.z);
    return ret;
}

void Overlay::BeginDraw() {
    ImGuiIO& io = ImGui::GetIO();

    // Make the panel cover the whole screen
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    // Create a transparent and click-through panel
    ImGui::Begin("HeroOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoInputs);
}

void Overlay::EndDraw() {
    ImGui::End();
}


void Overlay::DrawLine(Point2D from, Point2D to, ImU32 color = 0xFF00FF00, float thickness = 1.0) {
    drawList = ImGui::GetWindowDrawList();
    ImVec2 from_imvec = ImVec2(from.x, from.y);
    ImVec2 to_imvec = ImVec2(to.x, to.y);
    drawList->AddLine(from_imvec, to_imvec, color, thickness);
}

void Overlay::DrawLine3D(Point3D from, Point3D to, ImU32 color = 0xFF00FF00, float thickness = 1.0) {
    drawList = ImGui::GetWindowDrawList();
    Point2D from3D = WorldToScreen(from.x, from.y, from.z);
    Point2D to3D = WorldToScreen(to.x, to.y, to.z);

    ImVec2 fromProjected;
    ImVec2 toProjected;

    fromProjected.x = from3D.x;
    fromProjected.y = from3D.y;

    toProjected.x = to3D.x;
    toProjected.y = to3D.y;

    drawList->AddLine(fromProjected, toProjected, color, thickness);
}

void Overlay::DrawPoly(Point2D center, float radius, ImU32 color, int numSegments, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    const float segmentRadian = 2 * 3.14159265358979323846f / numSegments; // Calculate radians per segment
    ImVec2 prevPoint(center.x + radius, center.y); // Starting point for the first segment

    // Draw the circle using line segments
    for (int i = 1; i <= numSegments; ++i) {
        float angle = segmentRadian * i;
        ImVec2 currentPoint(center.x + radius * cos(angle), center.y + radius * sin(angle));

        // Draw the line from previous to current point
        drawList->AddLine(prevPoint, currentPoint, color, thickness);

        // Update the previous point to current
        prevPoint = currentPoint;
    }

    // Connect the last point to the starting point to close the polygon
    drawList->AddLine(prevPoint, ImVec2(center.x + radius, center.y), color, thickness);
}


void Overlay::DrawPoly3D(Point3D center, float radius, ImU32 color, int numSegments, float thickness, bool autoZ) {
    drawList = ImGui::GetWindowDrawList();
    const float segmentRadian = 2 * 3.14159265358979323846f / numSegments; // Calculate radians per segment
    ImVec2 prevPoint(center.x + radius, center.y); // Starting point for the first segment
    GW::Vec3f prevPoint3D;
    prevPoint3D.x = prevPoint.x;
    prevPoint3D.y = prevPoint.y;
    prevPoint3D.z = autoZ ? findZ(center.x, center.y, center.z) : center.z;

    Point2D prevPointtransformed3D = WorldToScreen(prevPoint3D.x, prevPoint3D.y, prevPoint3D.z);
    ImVec2 prevPointTransformed(prevPointtransformed3D.x, prevPointtransformed3D.y);

    // Draw the circle using line segments
    for (int i = 1; i <= numSegments; ++i) {
        float angle = segmentRadian * i;
        ImVec2 currentPoint(center.x + radius * cos(angle), center.y + radius * sin(angle));
        GW::Vec3f currentPoint3D;
        currentPoint3D.x = currentPoint.x;
        currentPoint3D.y = currentPoint.y;
        currentPoint3D.z = autoZ ? findZ(currentPoint3D.x, currentPoint3D.y, center.z) : center.z;

        Point2D currentPointtransformed3D = WorldToScreen(currentPoint3D.x, currentPoint3D.y, currentPoint3D.z);
        ImVec2 currentPointTransformed(currentPointtransformed3D.x, currentPointtransformed3D.y);


        drawList->AddLine(prevPointTransformed, currentPointTransformed, color, thickness);
        prevPoint = currentPoint;

        prevPoint3D.x = prevPoint.x;
        prevPoint3D.y = prevPoint.y;
        prevPoint3D.z = autoZ ? findZ(prevPoint3D.x, prevPoint3D.y, center.z) : center.z;

        prevPointtransformed3D = WorldToScreen(prevPoint3D.x, prevPoint3D.y, prevPoint3D.z);
        prevPointTransformed.x = prevPointtransformed3D.x;
        prevPointTransformed.y = prevPointtransformed3D.y;
    }
}

void Overlay::DrawText2D(Point2D position, std::string text, ImU32 color, bool centered, float scale) {
    drawList = ImGui::GetWindowDrawList();
    ImVec2 screenPos(position.x, position.y);
    ImFont* currentFont = ImGui::GetFont();   // Get the current font
    float originalFontSize = currentFont->Scale;  // Save the original font scale
    currentFont->Scale = scale;  // Set the new scale for the font

    ImGui::PushFont(currentFont);  // Apply the new font scale

    // Check if the text should be centered
    if (centered) {
        // Get the size of the text with the current font scale
        ImVec2 textSizeVec = ImGui::CalcTextSize(text.c_str());

        // Adjust the x coordinate to center the text
        screenPos.x -= textSizeVec.x / 2;
    }

    // Convert std::string to const char* for ImGui::AddText
    drawList->AddText(screenPos, color, text.c_str());

    // Restore the original font size
    currentFont->Scale = originalFontSize;  // Restore original font scale
    ImGui::PopFont();  // Pop the font after rendering
}

void Overlay::DrawText3D(Point3D position3D, std::string text, ImU32 color, bool autoZ, bool centered, float scale) {
    drawList = ImGui::GetWindowDrawList();
    // Project the 3D position to 2D screen coordinates
    Point2D screenPosition = { 0, 0 };

    if (autoZ) {
        // Optionally adjust Z if autoZ is true
        float z = findZ(position3D.x, position3D.y, position3D.z);
        screenPosition = WorldToScreen(position3D.x, position3D.y, z);
    }
    else {
        screenPosition = WorldToScreen(position3D.x, position3D.y, position3D.z);
    }

    // Convert the screen position to ImVec2 for ImGui
    ImVec2 screenPos(screenPosition.x, screenPosition.y);

    // Save the current font size and set the new one
    ImFont* currentFont = ImGui::GetFont();   // Get the current font
    float originalFontSize = currentFont->Scale;  // Save the original font scale
    currentFont->Scale = scale;  // Set the new scale for the font

    ImGui::PushFont(currentFont);  // Apply the new font scale

    // Check if the text should be centered
    if (centered) {
        // Get the size of the text with the current font scale
        ImVec2 textSizeVec = ImGui::CalcTextSize(text.c_str());

        // Adjust the x coordinate to center the text
        screenPos.x -= textSizeVec.x / 2;
    }

    // Convert std::string to const char* for ImGui::AddText
    drawList->AddText(screenPos, color, text.c_str());

    // Restore the original font size
    currentFont->Scale = originalFontSize;  // Restore original font scale
    ImGui::PopFont();  // Pop the font after rendering
}

void Overlay::DrawFilledTriangle3D(Point3D p1, Point3D p2, Point3D p3, ImU32 color) {
	drawList = ImGui::GetWindowDrawList();
	Point2D p1Screen = WorldToScreen(p1.x, p1.y, p1.z);
	Point2D p2Screen = WorldToScreen(p2.x, p2.y, p2.z);
	Point2D p3Screen = WorldToScreen(p3.x, p3.y, p3.z);
	ImVec2 p1ImVec(p1Screen.x, p1Screen.y);
	ImVec2 p2ImVec(p2Screen.x, p2Screen.y);
	ImVec2 p3ImVec(p3Screen.x, p3Screen.y);
	drawList->AddTriangleFilled(p1ImVec, p2ImVec, p3ImVec, color);
}

void Overlay::DrawQuad(Point2D p1, Point2D p2, Point2D p3, Point2D p4, ImU32 color, float thickness) {
	drawList = ImGui::GetWindowDrawList();
	ImVec2 p1ImVec(p1.x, p1.y);
	ImVec2 p2ImVec(p2.x, p2.y);
	ImVec2 p3ImVec(p3.x, p3.y);
	ImVec2 p4ImVec(p4.x, p4.y);
	drawList->AddLine(p1ImVec, p2ImVec, color, thickness);
	drawList->AddLine(p2ImVec, p3ImVec, color, thickness);
	drawList->AddLine(p3ImVec, p4ImVec, color, thickness);
	drawList->AddLine(p4ImVec, p1ImVec, color, thickness);
}

void Overlay::DrawQuadFilled(Point2D p1, Point2D p2, Point2D p3, Point2D p4, ImU32 color) {
	drawList = ImGui::GetWindowDrawList();
	ImVec2 p1ImVec(p1.x, p1.y);
	ImVec2 p2ImVec(p2.x, p2.y);
	ImVec2 p3ImVec(p3.x, p3.y);
	ImVec2 p4ImVec(p4.x, p4.y);
	drawList->AddQuadFilled(p1ImVec, p2ImVec, p3ImVec, p4ImVec, color);
}

void Overlay::DrawQuad3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, ImU32 color, float thickness) {
	drawList = ImGui::GetWindowDrawList();
	Point2D p1Screen = WorldToScreen(p1.x, p1.y, p1.z);
	Point2D p2Screen = WorldToScreen(p2.x, p2.y, p2.z);
	Point2D p3Screen = WorldToScreen(p3.x, p3.y, p3.z);
	Point2D p4Screen = WorldToScreen(p4.x, p4.y, p4.z);
	ImVec2 p1ImVec(p1Screen.x, p1Screen.y);
	ImVec2 p2ImVec(p2Screen.x, p2Screen.y);
	ImVec2 p3ImVec(p3Screen.x, p3Screen.y);
	ImVec2 p4ImVec(p4Screen.x, p4Screen.y);
	drawList->AddLine(p1ImVec, p2ImVec, color, thickness);
	drawList->AddLine(p2ImVec, p3ImVec, color, thickness);
	drawList->AddLine(p3ImVec, p4ImVec, color, thickness);
	drawList->AddLine(p4ImVec, p1ImVec, color, thickness);
}

void Overlay::DrawQuadFilled3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, ImU32 color) {
	drawList = ImGui::GetWindowDrawList();
	Point2D p1Screen = WorldToScreen(p1.x, p1.y, p1.z);
	Point2D p2Screen = WorldToScreen(p2.x, p2.y, p2.z);
	Point2D p3Screen = WorldToScreen(p3.x, p3.y, p3.z);
	Point2D p4Screen = WorldToScreen(p4.x, p4.y, p4.z);
	ImVec2 p1ImVec(p1Screen.x, p1Screen.y);
	ImVec2 p2ImVec(p2Screen.x, p2Screen.y);
	ImVec2 p3ImVec(p3Screen.x, p3Screen.y);
	ImVec2 p4ImVec(p4Screen.x, p4Screen.y);
	drawList->AddQuadFilled(p1ImVec, p2ImVec, p3ImVec, p4ImVec, color);
}

Point2D Overlay::GetDisplaySize() {
    drawList = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    return Point2D(io.DisplaySize.x, io.DisplaySize.y);
}

bool Overlay::IsMouseClicked(int button) {
    drawList = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
	return io.MouseDown[button];
}


std::vector<float> GetMapBoundaries() {
    GW::MapContext* m = GW::GetMapContext();
    std::vector<float> boundaries;
    for (int i = 0; i < 5; i++) {
        boundaries.push_back(m->map_boundaries[i]);
    }
    return boundaries;
};

struct PathingTrapezoid {
    uint32_t id;
    std::vector<uint32_t> neighbor_ids; // Indices of adjacent trapezoids
    uint16_t portal_left;
    uint16_t portal_right;
    float XTL;
    float XTR;
    float YT;
    float XBL;
    float XBR;
    float YB;

    PathingTrapezoid()
        : id(0), portal_left(0xFFFF), portal_right(0xFFFF),
        XTL(0), XTR(0), YT(0), XBL(0), XBR(0), YB(0) {}
};

struct Node {
    uint32_t type; // XNode = 0, YNode = 1, SinkNode = 2
    uint32_t id;

    Node() : type(0), id(0) {}
};

struct XNode : Node {
    std::pair<float, float> pos; // Position (x, y)
    std::pair<float, float> dir; // Direction vector (x, y)
    uint32_t left_id;  // Left node ID
    uint32_t right_id; // Right node ID

    XNode() : pos({ 0.0f, 0.0f }), dir({ 0.0f, 0.0f }), left_id(UINT32_MAX), right_id(UINT32_MAX) {
        type = 0; // Set type to XNode
    }
};

struct YNode : Node {
    std::pair<float, float> pos; // Position (x, y)
    uint32_t left_id;  // Left node ID
    uint32_t right_id; // Right node ID

    YNode() : pos({ 0.0f, 0.0f }), left_id(UINT32_MAX), right_id(UINT32_MAX) {
        type = 1; // Set type to YNode
    }
};

struct SinkNode : Node {
    std::vector<uint32_t> trapezoid_ids; // IDs of associated trapezoids

    SinkNode() {
        type = 2; // Set type to SinkNode
    }
};

struct Portal {
    uint16_t left_layer_id;
    uint16_t right_layer_id;
    uint32_t h0004;
    uint32_t pair_index;  // Index of the paired portal
    std::vector<uint32_t> trapezoid_indices; // IDs of associated trapezoids

    Portal() : left_layer_id(0xFFFF), right_layer_id(0xFFFF), h0004(0), pair_index(UINT32_MAX) {}
};

struct PathingMap {
    uint32_t zplane;
    uint32_t h0004;
    uint32_t h0008;
    uint32_t h000C;
    uint32_t h0010;
    std::vector<PathingTrapezoid> trapezoids;
    std::vector<SinkNode> sink_nodes;
    std::vector<XNode> x_nodes;
    std::vector<YNode> y_nodes;
    uint32_t h0034;
    uint32_t h0038;
    std::vector<Portal> portals;
    uint32_t root_node_id;

    PathingMap()
        : zplane(UINT32_MAX), h0004(0), h0008(0), h000C(0), h0010(0), h0034(0), h0038(0), root_node_id(UINT32_MAX) {}
};

PathingTrapezoid ConvertTrapezoid(const GW::PathingTrapezoid& original) {
    PathingTrapezoid trapezoid;
    trapezoid.id = original.id;
    trapezoid.XTL = original.XTL;
    trapezoid.XTR = original.XTR;
    trapezoid.YT = original.YT;
    trapezoid.XBL = original.XBL;
    trapezoid.XBR = original.XBR;
    trapezoid.YB = original.YB;
    trapezoid.portal_left = original.portal_left;
    trapezoid.portal_right = original.portal_right;

    // Map adjacent trapezoids by their IDs
    for (int i = 0; i < 4; ++i) {
        if (original.adjacent[i]) {
            trapezoid.neighbor_ids.push_back(original.adjacent[i]->id);
        }
    }

    return trapezoid;
}

PathingMap ConvertPathingMap(const GW::PathingMap& original) {
    PathingMap pathing_map;
    pathing_map.zplane = original.zplane;

    // Convert trapezoids
    for (uint32_t i = 0; i < original.trapezoid_count; ++i) {
        pathing_map.trapezoids.push_back(ConvertTrapezoid(original.trapezoids[i]));
    }

    // Convert SinkNodes
    /*
    for (uint32_t i = 0; i < original.sink_node_count; ++i) {
        SinkNode sink_node;
        sink_node.id = original.sink_nodes[i].id;

        // Safely iterate through the null-terminated trapezoid list
        if (original.sink_nodes[i].trapezoid) { // Ensure the trapezoid list exists
            const GW::PathingTrapezoid* const* trapezoid_ptr = original.sink_nodes[i].trapezoid;

            // Iterate until the null-terminator is reached
            while (*trapezoid_ptr != nullptr) {
                sink_node.trapezoid_ids.push_back((*trapezoid_ptr)->id);
                ++trapezoid_ptr; // Move to the next pointer in the array
            }
        }

        // Add the converted SinkNode to the destination map
        pathing_map.sink_nodes.push_back(sink_node);
    }
    */

    // Convert XNodes
    for (uint32_t i = 0; i < original.x_node_count; ++i) {
        XNode x_node;
        x_node.id = original.x_nodes[i].id;
        x_node.pos = { original.x_nodes[i].pos.x, original.x_nodes[i].pos.y };
        x_node.dir = { original.x_nodes[i].dir.x, original.x_nodes[i].dir.y };
        if (original.x_nodes[i].left) x_node.left_id = original.x_nodes[i].left->id;
        if (original.x_nodes[i].right) x_node.right_id = original.x_nodes[i].right->id;

        pathing_map.x_nodes.push_back(x_node);
    }

    // Convert YNodes
    for (uint32_t i = 0; i < original.y_node_count; ++i) {
        YNode y_node;
        y_node.id = original.y_nodes[i].id;
        y_node.pos = { original.y_nodes[i].pos.x, original.y_nodes[i].pos.y };
        if (original.y_nodes[i].left) y_node.left_id = original.y_nodes[i].left->id;
        if (original.y_nodes[i].right) y_node.right_id = original.y_nodes[i].right->id;

        pathing_map.y_nodes.push_back(y_node);
    }

    // Convert Portals
    for (uint32_t i = 0; i < original.portal_count; ++i) {
        Portal portal;
        portal.left_layer_id = original.portals[i].left_layer_id;
        portal.right_layer_id = original.portals[i].right_layer_id;
        portal.h0004 = original.portals[i].h0004;

        // Map trapezoids connected by the portal
        for (uint32_t j = 0; j < original.portals[i].count; ++j) {
            portal.trapezoid_indices.push_back(original.portals[i].trapezoids[j]->id);
        }

        // Determine the pair index manually
        portal.pair_index = UINT32_MAX; // Default to invalid
        if (original.portals[i].pair) {
            for (uint32_t k = 0; k < original.portal_count; ++k) {
                if (&original.portals[k] == original.portals[i].pair) {
                    portal.pair_index = k;
                    break;
                }
            }
        }

        pathing_map.portals.push_back(portal);
    }

    return pathing_map;
}



std::vector<PathingMap> GetPathingMaps() {
    const auto* pathing_map_array = GW::Map::GetPathingMap();

    std::vector<PathingMap> converted_maps;

    if (pathing_map_array) {
        for (size_t i = 0; i < pathing_map_array->size(); ++i) {
            converted_maps.push_back(ConvertPathingMap((*pathing_map_array)[i]));
        }
    }

    return converted_maps;
}



namespace py = pybind11;

void bind_Point2D(py::module_& m) {
    py::class_<Point2D>(m, "Point2D")
        .def(py::init<int, int>()) // Bind the constructor
        .def_readonly("x", &Point2D::x, "X coordinate")  // Expose x as a property
        .def_readonly("y", &Point2D::y, "Y coordinate");  // Expose y as a property
}

void bind_Point3D(py::module_& m) {
    py::class_<Point3D>(m, "Point3D")
        .def(py::init<float, float, float>()) // Bind the constructor
        .def_readonly("x", &Point3D::x, "X coordinate")  // Expose x as a property
        .def_readonly("y", &Point3D::y, "Y coordinate")  // Expose y as a property
        .def_readonly("z", &Point3D::z, "Z coordinate");  // Expose z as a property
}

void bind_overlay(py::module_& m) {
    py::class_<Overlay>(m, "Overlay")
        .def(py::init<>())  // Constructor binding
		.def("RefreshDrawList", &Overlay::RefreshDrawList)  // Expose RefreshDrawList method
        .def("GetMouseCoords", &Overlay::GetMouseCoords)  // Expose GetMouseCoords method
        .def("FindZ", &Overlay::findZ, py::arg("x"), py::arg("y"), py::arg("pz"))  // Expose findZ method
        .def("WorldToScreen", &Overlay::WorldToScreen, py::arg("x"), py::arg("y"), py::arg("z"))  // Expose WorldToScreen method
        .def("GetMouseWorldPos", &Overlay::GetMouseWorldPos)  // Expose GetMouseWorldPos method
        .def("BeginDraw", &Overlay::BeginDraw)  // Expose BeginDraw method
        .def("EndDraw", &Overlay::EndDraw)
        .def("DrawLine", &Overlay::DrawLine, py::arg("from"), py::arg("to"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawLine3D", &Overlay::DrawLine3D, py::arg("from"), py::arg("to"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawPoly", &Overlay::DrawPoly, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 32, py::arg("thickness") = 1.0f)
        .def("DrawPoly3D", &Overlay::DrawPoly3D, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 32, py::arg("thickness") = 1.0f, py::arg("autoZ") = true)
		.def("DrawFilledTriangle3D", &Overlay::DrawFilledTriangle3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawText", &Overlay::DrawText2D, py::arg("position"), py::arg("text"), py::arg("color") = 0xFFFFFFFF, py::arg("centered") = true, py::arg("scale") = 1.0)
        .def("DrawText3D", &Overlay::DrawText3D, py::arg("position3D"), py::arg("text"), py::arg("color") = 0xFFFFFFFF, py::arg("autoZ") = true, py::arg("centered") = true, py::arg("scale") = 1.0)
        .def("GetDisplaySize", &Overlay::GetDisplaySize)
	    .def("IsMouseClicked", &Overlay::IsMouseClicked, py::arg("button") = 0)
		.def("DrawQuad", &Overlay::DrawQuad, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
		.def("DrawQuadFilled", &Overlay::DrawQuadFilled, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF)
		.def("DrawQuad3D", &Overlay::DrawQuad3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
		.def("DrawQuadFilled3D", &Overlay::DrawQuadFilled3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF);

}

PYBIND11_EMBEDDED_MODULE(PyOverlay, m) {
    bind_Point2D(m);
    bind_Point3D(m);
    bind_overlay(m);
}



PYBIND11_EMBEDDED_MODULE(PyPathing, m) {
    // Bind PathingTrapezoid
    py::class_<PathingTrapezoid>(m, "PathingTrapezoid")
        .def(py::init<>())  // Default constructor
        .def_readwrite("id", &PathingTrapezoid::id)
        .def_readwrite("XTL", &PathingTrapezoid::XTL)
        .def_readwrite("XTR", &PathingTrapezoid::XTR)
        .def_readwrite("YT", &PathingTrapezoid::YT)
        .def_readwrite("XBL", &PathingTrapezoid::XBL)
        .def_readwrite("XBR", &PathingTrapezoid::XBR)
        .def_readwrite("YB", &PathingTrapezoid::YB)
        .def_readwrite("portal_left", &PathingTrapezoid::portal_left)
        .def_readwrite("portal_right", &PathingTrapezoid::portal_right)
        .def_readwrite("neighbor_ids", &PathingTrapezoid::neighbor_ids);  // List of adjacent trapezoid IDs

    // Bind Portal
    py::class_<Portal>(m, "Portal")
        .def(py::init<>())
        .def_readwrite("left_layer_id", &Portal::left_layer_id)
        .def_readwrite("right_layer_id", &Portal::right_layer_id)
        .def_readwrite("h0004", &Portal::h0004)
        .def_readwrite("pair_index", &Portal::pair_index)  // Paired portal index
        .def_readwrite("trapezoid_indices", &Portal::trapezoid_indices);  // Vector of trapezoid IDs


    // Bind Node as base class
    py::class_<Node>(m, "Node")
        .def(py::init<>())
        .def_readwrite("type", &Node::type)
        .def_readwrite("id", &Node::id);

    // Bind XNode, YNode, and SinkNode as derived classes
    py::class_<XNode, Node>(m, "XNode")
        .def(py::init<>())
        .def_readwrite("pos", &XNode::pos)   // Python-safe 2D position
        .def_readwrite("dir", &XNode::dir)   // Direction vector
        .def_readwrite("left_id", &XNode::left_id)
        .def_readwrite("right_id", &XNode::right_id);

    py::class_<YNode, Node>(m, "YNode")
        .def(py::init<>())
        .def_readwrite("pos", &YNode::pos)
        .def_readwrite("left_id", &YNode::left_id)
        .def_readwrite("right_id", &YNode::right_id);

    py::class_<SinkNode, Node>(m, "SinkNode")
        .def(py::init<>())
        .def_readwrite("trapezoid_ids", &SinkNode::trapezoid_ids);  // List of trapezoid IDs

    // Bind PathingMap
    py::class_<PathingMap>(m, "PathingMap")
        .def(py::init<>())  // Default constructor
        .def_readwrite("zplane", &PathingMap::zplane)
        .def_readwrite("h0004", &PathingMap::h0004)
        .def_readwrite("h0008", &PathingMap::h0008)
        .def_readwrite("h000C", &PathingMap::h000C)
        .def_readwrite("h0010", &PathingMap::h0010)
        .def_readwrite("trapezoids", &PathingMap::trapezoids)  // Vector of PathingTrapezoid
        .def_readwrite("sink_nodes", &PathingMap::sink_nodes)  // Vector of SinkNode
        .def_readwrite("x_nodes", &PathingMap::x_nodes)        // Vector of XNode
        .def_readwrite("y_nodes", &PathingMap::y_nodes)        // Vector of YNode
        .def_readwrite("h0034", &PathingMap::h0034)
        .def_readwrite("h0038", &PathingMap::h0038)
        .def_readwrite("portals", &PathingMap::portals)        // Vector of Portal
        .def_readwrite("root_node_id", &PathingMap::root_node_id);

    // Bind the GetPathingMaps function
    m.def("get_map_boundaries", &GetMapBoundaries, "Retrieve map boundaries");
    m.def("get_pathing_maps", &GetPathingMaps, "Retrieve and convert all pathing maps");
}

