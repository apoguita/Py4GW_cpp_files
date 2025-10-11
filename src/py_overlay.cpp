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
        //uintptr_t address = GW::Scanner::Find("\x8B\xE5\x5D\xC3\x8B\x4F\x10\x83\xC7\x08", "xxxxxxxxxx", 0xC);
        uintptr_t address = GW::Scanner::Find("\x83\x3d\x00\x00\x00\x00\x00\x0f\x00\x00\x00\x00\x00\x8b\x4f\x10", "xx????xx?????xxx", 0x15);
		Logger::AssertAddress("ptrNdcScreenCoords", (uintptr_t)address);
        if (Verify(address))
        {
            screen_coord = *reinterpret_cast<GW::Vec2f**>(address);
        }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); return; }

        //address = GW::Scanner::Find("\xD9\x5D\x14\xD9\x45\x14\x83\xEC\x0C", "xxxxxxxx", -0x2F);
        address = GW::Scanner::ToFunctionStart(GW::Scanner::Find("\xD9\x5D\x14\xD9\x45\x14\x83\xEC\x0C", "xxxxxxxx", 0));
		Logger::AssertAddress("ScreenToWorldPoint_Func", (uintptr_t)address);
        if (address)
        {
            ScreenToWorldPoint_Func = (ScreenToWorldPoint_pt)address;
        }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); return; }

        //address = GW::Scanner::Find("\xff\x75\x08\xd9\x5d\xfc\xff\x77\x7c", "xxxxxxxxx", -0x27);
        address = GW::Scanner::ToFunctionStart(GW::Scanner::Find("\xff\x75\x08\xd9\x5d\xfc\xff\x77\x7c", "xxxxxxx", 0));
		Logger::AssertAddress("MapIntersect_Func", (uintptr_t)address);
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


GW::Vec3f GetVec3f(const GW::GamePos& gp) {
    auto map = GW::GetMapContext();
    if (!map || map->sub1->sub2->pmaps.size() <= gp.zplane) [[unlikely]] {
        auto player = GW::Agents::GetControlledCharacter();
        if (player) return GW::Vec3f(gp.x, gp.y, player->z);
        else return GW::Vec3f(gp.x, gp.y, 0.0f);
    }

    GW::Vec3f pos{ gp.x, gp.y, 0.0f };
    GW::Map::QueryAltitude(gp, 5.0f, pos.z);
    return pos;
}


struct ZProbeResult {
    float z;
    uint32_t zplane;
};

// cache: (map_id, x, y) -> result
struct ZKeyHash {
    std::size_t operator()(const std::tuple<uint32_t, int, int>& k) const noexcept {
        auto [map_id, x, y] = k;
        return std::hash<uint32_t>()(map_id)
            ^ (std::hash<int>()(x) << 1)
            ^ (std::hash<int>()(y) << 2);
    }
};

static std::unordered_map<std::tuple<uint32_t, int, int>, ZProbeResult, ZKeyHash> point_cache;
static std::unordered_map<uint32_t, std::unordered_set<uint32_t>> valid_planes;

static void UpdateValidPlanes() {
    uint32_t map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    auto& set = valid_planes[map_id];

    // Initialize with pmaps indices only once
    if (set.empty()) {
        auto map = GW::GetMapContext();
        if (map && map->sub1 && map->sub1->sub2) {
            for (size_t i = 0; i < map->sub1->sub2->pmaps.size(); ++i) {
                set.insert(static_cast<uint32_t>(i)); // index is the actual plane
            }
        }
    }

    // Always add player's current plane (already an index)
    if (auto player = GW::Agents::GetControlledCharacter()) {
        set.insert(static_cast<uint32_t>(player->plane));
    }
}


// ---------------- INTERNAL SHARED RESOLVER ----------------
static ZProbeResult ProbeZ(float x, float y, uint32_t zplane) {
    uint32_t map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    auto key = std::make_tuple(map_id, (int)x, (int)y);

    // cache lookup
    auto it = point_cache.find(key);
    if (it != point_cache.end()) {
        return it->second;
    }

    auto map = GW::GetMapContext();
    if (!map || !map->sub1 || !map->sub1->sub2) {
        auto player = GW::Agents::GetControlledCharacter();
        float z = player ? player->z : 0.0f;
        ZProbeResult res{ z, 0 };
        point_cache[key] = res;
        return res;
    }

    // explicit zplane query ? use index directly
    if (zplane != 0) {
        float z = GetVec3f(GW::GamePos(x, y, zplane)).z;

        // log plane if not in valid list
        auto& set = valid_planes[map_id];
        set.insert(zplane);

        ZProbeResult res{ z, zplane };
        point_cache[key] = res;
        return res;
    }

    // ensure valid planes are populated
    UpdateValidPlanes();

    // base reference
    float base_z = GetVec3f(GW::GamePos(x, y, 0)).z;
    std::vector<std::pair<uint32_t, float>> entries;
    entries.emplace_back(0, base_z);

    // probe all cached valid planes (indices)
    for (uint32_t zp : valid_planes[map_id]) {
        if (zp == 0) continue; // base already included
        float z = GetVec3f(GW::GamePos(x, y, zp)).z;
        entries.emplace_back(zp, z);
    }

    // deduplicate by Z
    std::unordered_map<float, int> counts;
    for (auto& [zp, z] : entries) counts[z]++;

    std::vector<std::pair<uint32_t, float>> unique;
    for (auto& [zp, z] : entries) {
        if (counts[z] == 1)
            unique.emplace_back(zp, z);
    }

    // fallback to base if nothing unique
    if (unique.empty()) {
        ZProbeResult res{ base_z, 0 };
        point_cache[key] = res;
        return res;
    }

    // pick layer with maximum delta from base
    auto top = std::max_element(unique.begin(), unique.end(),
        [base_z](auto& a, auto& b) {
            return std::abs(a.second - base_z) < std::abs(b.second - base_z);
        });

    ZProbeResult res{ top->second, top->first };
    point_cache[key] = res;
    return res;
}


// ---------------- PUBLIC API ----------------
float Overlay::findZ(float x, float y, uint32_t zplane) {
    return ProbeZ(x, y, zplane).z;
}

uint32_t Overlay::FindZPlane(float x, float y, uint32_t zplane) {
    return ProbeZ(x, y, zplane).zplane;
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

struct ImRect {
    ImVec2 Min;
    ImVec2 Max;

    ImRect() : Min(), Max() {}
    ImRect(const ImVec2& min, const ImVec2& max) : Min(min), Max(max) {}
    ImRect(float x1, float y1, float x2, float y2) : Min(x1, y1), Max(x2, y2) {}

    ImVec2 GetCenter() const { return ImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f); }
    ImVec2 GetSize()   const { return ImVec2(Max.x - Min.x, Max.y - Min.y); }
    bool Contains(const ImVec2& p) const { return p.x >= Min.x && p.y >= Min.y && p.x < Max.x && p.y < Max.y; }
};

bool GetMapWorldMapBounds(GW::AreaInfo* map, ImRect* out) {
    if (!map) return false;
    auto bounds = &map->icon_start_x;
    if (*bounds == 0)
        bounds = &map->icon_start_x_dupe;

    // NB: Even though area info holds map bounds as uints, the world map uses signed floats anyway - a cast should be fine here.
    *out = {
        { static_cast<float> (bounds[0]), static_cast<float>(bounds[1]) },
        { static_cast<float> (bounds[2]), static_cast<float>(bounds[3]) }
    };
    return true;
}
GW::Array<GW::MapProp*>* GetMapProps() {
    const auto m = GW::GetMapContext();
    const auto p = m ? m->props : nullptr;
    return p ? &p->propArray : nullptr;
}

Point2D Overlay::GamePosToWorldMap(float x, float y) {
    ImRect map_bounds;
    if (!GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds))
        return Point2D(0, 0);
    const auto current_map_context = GW::GetMapContext();
    if (!current_map_context)
        return Point2D(0, 0);

    const auto game_map_rect = ImRect({
        current_map_context->map_boundaries[1], current_map_context->map_boundaries[2],
        current_map_context->map_boundaries[3], current_map_context->map_boundaries[4],
        });

    // NB: World map is 96 gwinches per unit, this is hard coded in the GW source
    const auto gwinches_per_unit = 96.f;
    GW::Vec2f map_mid_world_point = {
        map_bounds.Min.x + (abs(game_map_rect.Min.x) / gwinches_per_unit),
        map_bounds.Min.y + (abs(game_map_rect.Max.y) / gwinches_per_unit),
    };

    float result_x = 0.f;
    float result_y = 0.f;
    // Convert the game coordinates to world map coordinates

    result_x = (x / gwinches_per_unit) + map_mid_world_point.x;
    result_y = ((y * -1.f) / gwinches_per_unit) + map_mid_world_point.y; // Inverted Y Axis
    return Point2D(result_x, result_y);

}

Point2D Overlay::WorlMapToGamePos(float x, float y) {
    ImRect map_bounds;
    if (!GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds))
        return Point2D(0, 0);
    if (!map_bounds.Contains({ x, y }))
        return Point2D(0, 0); // Current map doesn't contain these coords; we can't plot a position

    const auto current_map_context = GW::GetMapContext();
    if (!current_map_context)
        return Point2D(0, 0);

    const auto game_map_rect = ImRect({
        current_map_context->map_boundaries[1], current_map_context->map_boundaries[2],
        current_map_context->map_boundaries[3], current_map_context->map_boundaries[4],
        });

    // NB: World map is 96 gwinches per unit, this is hard coded in the GW source
    constexpr auto gwinches_per_unit = 96.f;
    GW::Vec2f map_mid_world_point = {
        map_bounds.Min.x + (abs(game_map_rect.Min.x) / gwinches_per_unit),
        map_bounds.Min.y + (abs(game_map_rect.Max.y) / gwinches_per_unit),
    };

    float result_x = 0;
    float result_y = 0;
    result_x = (x - map_mid_world_point.x) * gwinches_per_unit;
    result_y = ((y - map_mid_world_point.y) * gwinches_per_unit) * -1.f; // Inverted Y Axis
    return Point2D(result_x, result_y);
}

Point2D Overlay::WorldMapToScreen(float x, float y) {
    GW::Vec2f position(x, y);
    const auto gameplay_context = GW::GetGameplayContext();
    const auto mission_map_context = GW::Map::GetMissionMapContext();
    const auto mission_map_frame = mission_map_context ? GW::UI::GetFrameById(mission_map_context->frame_id) : nullptr;
    if (!(mission_map_frame && mission_map_frame->IsVisible())) return Point2D(0, 0);

    const auto root = GW::UI::GetRootFrame();
    auto mission_map_top_left = mission_map_frame->position.GetContentTopLeft(root);
    auto mission_map_bottom_right = mission_map_frame->position.GetContentBottomRight(root);
    auto mission_map_scale = mission_map_frame->position.GetViewportScale(root);
    auto mission_map_zoom = gameplay_context->mission_map_zoom;
    auto mission_map_center_pos = mission_map_context->player_mission_map_pos;
    auto mission_map_last_click_location = mission_map_context->last_mouse_location;
    auto current_pan_offset = mission_map_context->h003c->mission_map_pan_offset;
    auto mission_map_screen_pos = mission_map_top_left + (mission_map_bottom_right - mission_map_top_left) / 2;

    const auto offset = (position - current_pan_offset);
    const auto scaled_offset = GW::Vec2f(offset.x * mission_map_scale.x, offset.y * mission_map_scale.y);
    auto result(scaled_offset * mission_map_zoom + mission_map_screen_pos);
    return Point2D(result.x, result.y);
}

Point2D Overlay::ScreenToWorldMap(float screen_x, float screen_y) {
    const auto gameplay = GW::GetGameplayContext();
    const auto mission_map_ctx = GW::Map::GetMissionMapContext();
    const auto frame = mission_map_ctx ? GW::UI::GetFrameById(mission_map_ctx->frame_id) : nullptr;
    if (!(frame && frame->IsVisible())) return Point2D(0, 0);

    const auto root = GW::UI::GetRootFrame();
    auto top_left = frame->position.GetContentTopLeft(root);
    auto bottom_right = frame->position.GetContentBottomRight(root);
    auto center = top_left + (bottom_right - top_left) / 2;
    auto scale = frame->position.GetViewportScale(root);
    auto zoom = gameplay->mission_map_zoom;
    auto pan_offset = mission_map_ctx->h003c->mission_map_pan_offset;

    GW::Vec2f offset = {
        (screen_x - center.x) / (zoom * scale.x),
        (screen_y - center.y) / (zoom * scale.y)
    };

    auto result = pan_offset + offset;
    return Point2D(result.x, result.y);
}

Point2D Overlay::GameMapToScreen(float x, float y)
{
    Point2D world_map_pos = GamePosToWorldMap(x, y);
    return WorldMapToScreen(world_map_pos.x, world_map_pos.y);
}

Point2D Overlay::ScreenToGameMapPos(float screen_x, float screen_y) {
    Point2D world = ScreenToWorldMap(screen_x, screen_y);
    return WorlMapToGamePos(world.x, world.y);
}

Point2D Overlay::NormalizedScreenToScreen(float norm_x, float norm_y)
{
    const auto mission_map_context = GW::Map::GetMissionMapContext();
    const auto mission_map_frame = mission_map_context ? GW::UI::GetFrameById(mission_map_context->frame_id) : nullptr;
    if (!(mission_map_frame && mission_map_frame->IsVisible()))
        return Point2D(0.f, 0.f);

    const auto root = GW::UI::GetRootFrame();

    GW::Vec2f top_left = mission_map_frame->position.GetContentTopLeft(root);
    GW::Vec2f bottom_right = mission_map_frame->position.GetContentBottomRight(root);
    GW::Vec2f size = bottom_right - top_left;

    // Convert from [-1, 1] to [0, 1], and invert Y
    float adjusted_x = (norm_x + 1.0f) / 2.0f;
    float adjusted_y = (1.0f - norm_y) / 2.0f;

    float screen_x = top_left.x + adjusted_x * size.x;
    float screen_y = top_left.y + adjusted_y * size.y;

    return Point2D(screen_x, screen_y);
}



Point2D Overlay::ScreenToNormalizedScreen(float screen_x, float screen_y)
{
    const auto mission_map_context = GW::Map::GetMissionMapContext();
    const auto mission_map_frame = mission_map_context ? GW::UI::GetFrameById(mission_map_context->frame_id) : nullptr;
    if (!(mission_map_frame && mission_map_frame->IsVisible()))
        return Point2D(0.f, 0.f);

    const auto root = GW::UI::GetRootFrame();

    GW::Vec2f top_left = mission_map_frame->position.GetContentTopLeft(root);
    GW::Vec2f bottom_right = mission_map_frame->position.GetContentBottomRight(root);
    GW::Vec2f size = bottom_right - top_left;

    float rel_x = (screen_x - top_left.x) / size.x;
    float rel_y = (screen_y - top_left.y) / size.y;

    // Convert to normalized range [-1, 1], with Y inverted
    float norm_x = rel_x * 2.0f - 1.0f;
    float norm_y = (1.0f - rel_y) * 2.0f - 1.0f;

    return Point2D(norm_x, norm_y);
}


Point2D Overlay::NormalizedScreenToWorldMap(float norm_x, float norm_y) {
    Point2D screen = NormalizedScreenToScreen(norm_x, norm_y);
    return ScreenToWorldMap(screen.x, screen.y);
}


Point2D Overlay::NormalizedScreenToGameMap(float norm_x, float norm_y) {
    Point2D world = NormalizedScreenToWorldMap(norm_x, norm_y);
    return WorlMapToGamePos(world.x, world.y);
}


Point2D Overlay::GamePosToNormalizedScreen(float x, float y) {
    Point2D screen = GameMapToScreen(x, y);
    return Point2D(
        ScreenToNormalizedScreen(screen.x, screen.y)
    );
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
        //ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoInputs);
}

void Overlay::BeginDraw(std::string name) {

    ImGuiIO& io = ImGui::GetIO();

    // Make the panel cover the whole screen
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    // Create a transparent and click-through panel
    std::string internal_name = "##" + name;
    ImGui::Begin(internal_name.c_str(), nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground |
        //ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoInputs);

}

void Overlay::BeginDraw(std::string name, float x, float y, float width, float height) {
	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(x, y));
	// Create a transparent and click-through panel
    std::string internal_name = "##" + name;
	ImGui::Begin(internal_name.c_str(), nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBackground |
		//ImGuiWindowFlags_NoBringToFrontOnFocus |
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

void Overlay::DrawTriangle(Point2D p1, Point2D p2, Point2D p3, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    ImVec2 v1(p1.x, p1.y);
    ImVec2 v2(p2.x, p2.y);
    ImVec2 v3(p3.x, p3.y);
    drawList->AddTriangle(v1, v2, v3, color, thickness);
}


void Overlay::DrawTriangle3D(Point3D p1, Point3D p2, Point3D p3, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    Point2D a = WorldToScreen(p1.x, p1.y, p1.z);
    Point2D b = WorldToScreen(p2.x, p2.y, p2.z);
    Point2D c = WorldToScreen(p3.x, p3.y, p3.z);
    drawList->AddTriangle(ImVec2(a.x, a.y), ImVec2(b.x, b.y), ImVec2(c.x, c.y), color, thickness);
}

void Overlay::DrawTriangleFilled(Point2D p1, Point2D p2, Point2D p3, ImU32 color) {
    drawList = ImGui::GetWindowDrawList();
    drawList->AddTriangleFilled(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), ImVec2(p3.x, p3.y), color);
}

void Overlay::DrawTriangleFilled3D(Point3D p1, Point3D p2, Point3D p3, ImU32 color) {
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
    ImVec2 points[4] = {
        ImVec2(p1.x, p1.y),
        ImVec2(p2.x, p2.y),
        ImVec2(p3.x, p3.y),
        ImVec2(p4.x, p4.y)
    };
    drawList->AddPolyline(points, 4, color, true /* closed */, thickness);
}


void Overlay::DrawQuad3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();

    Point2D s1 = WorldToScreen(p1.x, p1.y, p1.z);
    Point2D s2 = WorldToScreen(p2.x, p2.y, p2.z);
    Point2D s3 = WorldToScreen(p3.x, p3.y, p3.z);
    Point2D s4 = WorldToScreen(p4.x, p4.y, p4.z);

    ImVec2 screenPoints[4] = {
        ImVec2(s1.x, s1.y),
        ImVec2(s2.x, s2.y),
        ImVec2(s3.x, s3.y),
        ImVec2(s4.x, s4.y)
    };

    drawList->AddPolyline(screenPoints, 4, color, true /* closed */, thickness);
}


void Overlay::DrawQuadFilled(Point2D p1, Point2D p2, Point2D p3, Point2D p4, ImU32 color) {
    drawList = ImGui::GetWindowDrawList();
    ImVec2 points[4] = {
        ImVec2(p1.x, p1.y),
        ImVec2(p2.x, p2.y),
        ImVec2(p3.x, p3.y),
        ImVec2(p4.x, p4.y)
    };
    drawList->AddConvexPolyFilled(points, 4, color);
}


void Overlay::DrawQuadFilled3D(Point3D p1, Point3D p2, Point3D p3, Point3D p4, ImU32 color) {
    drawList = ImGui::GetWindowDrawList();

    Point2D s1 = WorldToScreen(p1.x, p1.y, p1.z);
    Point2D s2 = WorldToScreen(p2.x, p2.y, p2.z);
    Point2D s3 = WorldToScreen(p3.x, p3.y, p3.z);
    Point2D s4 = WorldToScreen(p4.x, p4.y, p4.z);

    ImVec2 screenPoints[4] = {
        ImVec2(s1.x, s1.y),
        ImVec2(s2.x, s2.y),
        ImVec2(s3.x, s3.y),
        ImVec2(s4.x, s4.y)
    };

    drawList->AddConvexPolyFilled(screenPoints, 4, color);
}


void Overlay::DrawPoly(Point2D center, float radius, ImU32 color, int numSegments, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    std::vector<ImVec2> points;
    points.reserve(numSegments);

    const float segmentRadian = 2.0f * 3.14159265358979323846f / numSegments;

    for (int i = 0; i < numSegments; ++i) {
        float angle = segmentRadian * i;
        float x = center.x + cos(angle) * radius;
        float y = center.y + sin(angle) * radius;
        points.emplace_back(ImVec2(x, y));
    }

    drawList->AddPolyline(points.data(), points.size(), color, true /* closed */, thickness);
}


void Overlay::DrawPolyFilled(Point2D center, float radius, ImU32 color, int numSegments) {
    drawList = ImGui::GetWindowDrawList();
    std::vector<ImVec2> points;
    points.reserve(numSegments);

    const float segmentRadian = 2.0f * 3.14159265358979323846f / numSegments;

    for (int i = 0; i < numSegments; ++i) {
        float angle = segmentRadian * i;
        float x = center.x + cos(angle) * radius;
        float y = center.y + sin(angle) * radius;
        points.emplace_back(ImVec2(x, y));
    }

    drawList->AddConvexPolyFilled(points.data(), points.size(), color);
}



void Overlay::DrawPoly3D(Point3D center, float radius, ImU32 color, int numSegments, float thickness, bool autoZ) {
    drawList = ImGui::GetWindowDrawList();
    const float segmentRadian = 2.0f * 3.14159265358979323846f / numSegments;

    std::vector<ImVec2> points;
    points.reserve(numSegments);

    for (int i = 0; i < numSegments; ++i) {
        float angle = segmentRadian * i;
        float x = center.x + radius * std::cos(angle);
        float y = center.y + radius * std::sin(angle);
        float z = autoZ ? findZ(x, y, center.z) : center.z;

        Point2D screen = WorldToScreen(x, y, z);
        points.emplace_back(ImVec2(screen.x, screen.y));
    }

    drawList->AddPolyline(points.data(), points.size(), color, true /* closed */, thickness);
}



void Overlay::DrawPolyFilled3D(Point3D center, float radius, ImU32 color, int numSegments, bool autoZ) {
    drawList = ImGui::GetWindowDrawList();
    std::vector<ImVec2> points;
    points.reserve(numSegments);

    const float segmentRadian = 2.0f * 3.14159265358979323846f / numSegments;

    for (int i = 0; i < numSegments; ++i) {
        float angle = segmentRadian * i;
        float x = center.x + cos(angle) * radius;
        float y = center.y + sin(angle) * radius;
        float z = autoZ ? findZ(x, y, center.z) : center.z;

        Point2D screen = WorldToScreen(x, y, z);
        points.emplace_back(ImVec2(screen.x, screen.y));
    }

    drawList->AddConvexPolyFilled(points.data(), points.size(), color);
}


void Overlay::DrawCubeOutline(Point3D center, float size, ImU32 color, float thickness) {
    drawList = ImGui::GetWindowDrawList();
    float h = size / 2.0f;

    Point3D corners[8] = {
        {center.x - h, center.y - h, center.z - h},
        {center.x + h, center.y - h, center.z - h},
        {center.x + h, center.y + h, center.z - h},
        {center.x - h, center.y + h, center.z - h},
        {center.x - h, center.y - h, center.z + h},
        {center.x + h, center.y - h, center.z + h},
        {center.x + h, center.y + h, center.z + h},
        {center.x - h, center.y + h, center.z + h},
    };

    int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (auto& edge : edges) {
        Point2D p1 = WorldToScreen(corners[edge[0]].x, corners[edge[0]].y, corners[edge[0]].z);
        Point2D p2 = WorldToScreen(corners[edge[1]].x, corners[edge[1]].y, corners[edge[1]].z);
        drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, thickness);
    }
}


void Overlay::DrawCubeFilled(Point3D center, float size, ImU32 color) {
    drawList = ImGui::GetWindowDrawList();
    float h = size / 2.0f;

    Point3D corners[8] = {
        {center.x - h, center.y - h, center.z - h},
        {center.x + h, center.y - h, center.z - h},
        {center.x + h, center.y + h, center.z - h},
        {center.x - h, center.y + h, center.z - h},
        {center.x - h, center.y - h, center.z + h},
        {center.x + h, center.y - h, center.z + h},
        {center.x + h, center.y + h, center.z + h},
        {center.x - h, center.y + h, center.z + h},
    };

    int faces[6][4] = {
        {0,1,2,3}, {4,5,6,7}, {0,1,5,4},
        {2,3,7,6}, {0,3,7,4}, {1,2,6,5}
    };

    for (auto& face : faces) {
        ImVec2 screenPts[4];
        for (int i = 0; i < 4; ++i) {
            Point2D screen = WorldToScreen(corners[face[i]].x, corners[face[i]].y, corners[face[i]].z);
            screenPts[i] = ImVec2(screen.x, screen.y);
        }
        drawList->AddConvexPolyFilled(screenPts, 4, color);
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

void Overlay::PushClipRect(float x, float y, float x2, float y2) {
	drawList = ImGui::GetWindowDrawList();
	ImVec2 min(x, y);
	ImVec2 max(x2, y2);
	drawList->PushClipRect(min, max, true);
}

void Overlay::DrawTexture(const std::string& path, float width, float height)
{
    std::wstring wpath(path.begin(), path.end());
    IDirect3DTexture9* tex = TextureManager::Instance().GetTexture(wpath);
    if (!tex)
        return;


    ImTextureID tex_id = reinterpret_cast<ImTextureID>(tex);
    ImGui::Image(tex_id, ImVec2(width, height));
}

void Overlay::DrawTexture(const std::string& path,
    std::tuple<float, float> size,
    std::tuple<float, float> uv0,
    std::tuple<float, float> uv1,
    std::tuple<int, int, int, int> tint,
    std::tuple<int, int, int, int> border_col) {
    std::wstring wpath(path.begin(), path.end());
    IDirect3DTexture9* tex = TextureManager::Instance().GetTexture(wpath);
    if (!tex)
        return;

    ImTextureID tex_id = reinterpret_cast<ImTextureID>(tex);
    ImVec2 im_size(std::get<0>(size), std::get<1>(size));
    //ImVec2 im_pos(std::get<0>(pos), std::get<1>(pos));
    ImVec2 im_uv0(std::get<0>(uv0), std::get<1>(uv0));
    ImVec2 im_uv1(std::get<0>(uv1), std::get<1>(uv1));

    ImVec4 color = ImVec4(
        std::get<0>(tint) / 255.0f,
        std::get<1>(tint) / 255.0f,
        std::get<2>(tint) / 255.0f,
        std::get<3>(tint) / 255.0f
    );

	ImVec4 border_color = ImVec4(
		std::get<0>(border_col) / 255.0f,
		std::get<1>(border_col) / 255.0f,
		std::get<2>(border_col) / 255.0f,
		std::get<3>(border_col) / 255.0f
	);

    //ImGui::SetCursorPos(im_pos);
	ImGui::Image(tex_id, im_size, im_uv0, im_uv1, color, border_color);
}

void Overlay::DrawTexturedRect(float x, float y, float width, float height, const std::string& texture_path) {
    std::wstring wpath(texture_path.begin(), texture_path.end());
    IDirect3DTexture9* tex = TextureManager::Instance().GetTexture(wpath);
    if (!tex)
        return;
    ImTextureID tex_id = reinterpret_cast<ImTextureID>(tex);
    ImGui::GetWindowDrawList()->AddImage(tex_id, ImVec2(x, y), ImVec2(x + width, y + height));
}

void Overlay::DrawTexturedRect(std::tuple<float, float> pos,
    std::tuple<float, float> size,
    const std::string& texture_path,
    std::tuple<float, float> uv0,
    std::tuple<float, float> uv1,
    std::tuple<int, int, int, int> tint)
{
    std::wstring wpath(texture_path.begin(), texture_path.end());
    IDirect3DTexture9* tex = TextureManager::Instance().GetTexture(wpath);
    if (!tex) return;

    ImTextureID tex_id = reinterpret_cast<ImTextureID>(tex);

    const float x = std::get<0>(pos), y = std::get<1>(pos);
    const float w = std::get<0>(size), h = std::get<1>(size);
    ImVec2 uv_start(std::get<0>(uv0), std::get<1>(uv0));
    ImVec2 uv_end(std::get<0>(uv1), std::get<1>(uv1));
    ImVec4 tint_color(
        std::get<0>(tint) / 255.0f,
        std::get<1>(tint) / 255.0f,
        std::get<2>(tint) / 255.0f,
        std::get<3>(tint) / 255.0f
    );

    ImGui::GetWindowDrawList()->AddImage(tex_id, ImVec2(x, y), ImVec2(x + w, y + h), uv_start, uv_end, ImGui::ColorConvertFloat4ToU32(tint_color));
}


bool Overlay::ImageButton(const std::string& caption, const std::string& file_path, float width, float height, int frame_padding) {
    std::wstring wpath(file_path.begin(), file_path.end());
    auto* tex = TextureManager::Instance().GetTexture(wpath);
    if (!tex)
        return false;

    ImVec2 size(width, height);
    ImTextureID tex_id = reinterpret_cast<ImTextureID>(tex);

    std::string id_str = caption;

    // Optional: allow frame padding override
    if (frame_padding >= 0)
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2((float)frame_padding, (float)frame_padding));

    bool result = ImGui::ImageButton(
        id_str.c_str(),      // required unique ID
        tex_id,
        size,
        ImVec2(0, 0), ImVec2(1, 1),
        ImVec4(0, 0, 0, 0),  // bg color
        ImVec4(1, 1, 1, 1)   // tint color
    );

    if (frame_padding >= 0)
        ImGui::PopStyleVar();

    return result;
}

bool Overlay::ImageButton(const std::string& caption, const std::string& file_path,
    std::tuple<float, float> size,
    std::tuple<float, float> uv0,
    std::tuple<float, float> uv1,
    std::tuple<int, int, int, int> bg_color,
    std::tuple<int, int, int, int> tint_color,
    int frame_padding)
{
    std::wstring wpath(file_path.begin(), file_path.end());
    auto* tex = TextureManager::Instance().GetTexture(wpath);
    if (!tex)
        return false;

    ImTextureID tex_id = reinterpret_cast<ImTextureID>(tex);
    ImVec2 sz(std::get<0>(size), std::get<1>(size));
    ImVec2 uv0f(std::get<0>(uv0), std::get<1>(uv0));
    ImVec2 uv1f(std::get<0>(uv1), std::get<1>(uv1));

    ImVec4 bg(
        std::get<0>(bg_color) / 255.0f,
        std::get<1>(bg_color) / 255.0f,
        std::get<2>(bg_color) / 255.0f,
        std::get<3>(bg_color) / 255.0f
    );

    ImVec4 tint(
        std::get<0>(tint_color) / 255.0f,
        std::get<1>(tint_color) / 255.0f,
        std::get<2>(tint_color) / 255.0f,
        std::get<3>(tint_color) / 255.0f
    );

    if (frame_padding >= 0)
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2((float)frame_padding, (float)frame_padding));

    bool result = ImGui::ImageButton(
        caption.c_str(),
        tex_id,
        sz,
        uv0f,
        uv1f,
        bg,
        tint
    );

    if (frame_padding >= 0)
        ImGui::PopStyleVar();

    return result;
}

void Overlay::DrawTextureInForegound(
    const std::tuple<float, float>& pos,
    const std::tuple<float, float>& size,
    const std::string& texture_path,
    const std::tuple<float, float>& uv0,
    const std::tuple<float, float>& uv1,
    const std::tuple<int, int, int, int>& tint)
{
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();  // THIS is the correct call

    std::wstring wpath(texture_path.begin(), texture_path.end());
    IDirect3DTexture9* tex = TextureManager::Instance().GetTexture(wpath);
    if (!tex || !draw_list)
        return;

    ImTextureID tex_id = reinterpret_cast<ImTextureID>(tex);

    float x = std::get<0>(pos), y = std::get<1>(pos);
    float w = std::get<0>(size), h = std::get<1>(size);
    ImVec2 uv_start(std::get<0>(uv0), std::get<1>(uv0));
    ImVec2 uv_end(std::get<0>(uv1), std::get<1>(uv1));
    ImVec4 tint_color(
        std::get<0>(tint) / 255.0f,
        std::get<1>(tint) / 255.0f,
        std::get<2>(tint) / 255.0f,
        std::get<3>(tint) / 255.0f
    );

    draw_list->AddImage(
        tex_id,
        ImVec2(x, y),
        ImVec2(x + w, y + h),
        uv_start,
        uv_end,
        ImGui::ColorConvertFloat4ToU32(tint_color)
    );
}

void Overlay::DrawTextureInDrawlist(
    const std::tuple<float, float>& pos,
    const std::tuple<float, float>& size,
    const std::string& texture_path,
    const std::tuple<float, float>& uv0,
    const std::tuple<float, float>& uv1,
    const std::tuple<int, int, int, int>& tint)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();  // Draw in current ImGui window

    std::wstring wpath(texture_path.begin(), texture_path.end());
    IDirect3DTexture9* tex = TextureManager::Instance().GetTexture(wpath);
    if (!tex || !draw_list)
        return;

    ImTextureID tex_id = reinterpret_cast<ImTextureID>(tex);

    float x = std::get<0>(pos), y = std::get<1>(pos);
    float w = std::get<0>(size), h = std::get<1>(size);
    ImVec2 uv_start(std::get<0>(uv0), std::get<1>(uv0));
    ImVec2 uv_end(std::get<0>(uv1), std::get<1>(uv1));
    ImVec4 tint_color(
        std::get<0>(tint) / 255.0f,
        std::get<1>(tint) / 255.0f,
        std::get<2>(tint) / 255.0f,
        std::get<3>(tint) / 255.0f
    );

    draw_list->AddImage(
        tex_id,
        ImVec2(x, y),
        ImVec2(x + w, y + h),
        uv_start,
        uv_end,
        ImGui::ColorConvertFloat4ToU32(tint_color)
    );
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
        .def("FindZ", &Overlay::findZ, py::arg("x"), py::arg("y"), py::arg("pz") = 0)  // Expose findZ method
		.def("FindZPlane", &Overlay::FindZPlane, py::arg("x"), py::arg("y"), py::arg("z") = 0)  // Expose findZPlane method
        .def("WorldToScreen", &Overlay::WorldToScreen, py::arg("x"), py::arg("y"), py::arg("z"))  // Expose WorldToScreen method
        .def("GetMouseWorldPos", &Overlay::GetMouseWorldPos)  // Expose GetMouseWorldPos method
        // Game <-> World
        .def("GamePosToWorldMap", &Overlay::GamePosToWorldMap, py::arg("x"), py::arg("y"), "Convert game position to world map coordinates")
        .def("WorldMapToGamePos", &Overlay::WorlMapToGamePos, py::arg("x"), py::arg("y"), "Convert world map position to game coordinates")
        // World <-> Screen
        .def("WorldMapToScreen", &Overlay::WorldMapToScreen, py::arg("x"), py::arg("y"), "Convert world map position to screen coordinates")
        .def("ScreenToWorldMap", &Overlay::ScreenToWorldMap, py::arg("x"), py::arg("y"), "Convert screen position to world map coordinates")
        // Game <-> Screen (combined)
        .def("GameMapToScreen", &Overlay::GameMapToScreen, py::arg("x"), py::arg("y"), "Convert game map position to screen coordinates")
        .def("ScreenToGameMapPos", &Overlay::ScreenToGameMapPos, py::arg("x"), py::arg("y"), "Convert screen position to game map coordinates")
        //NormalizedScreen <-> Screen
        .def("NormalizedScreenToScreen", &Overlay::NormalizedScreenToScreen, py::arg("norm_x"), py::arg("norm_y"), "Convert normalized screen coordinates to screen coordinates")
        .def("ScreenToNormalizedScreen", &Overlay::ScreenToNormalizedScreen, py::arg("screen_x"), py::arg("screen_y"), "Convert screen coordinates to normalized screen coordinates")
        //NormalizedScreen <-> World / Game
        .def("NormalizedScreenToWorldMap", &Overlay::NormalizedScreenToWorldMap, py::arg("norm_x"), py::arg("norm_y"), "Convert normalized screen coordinates to world map coordinates")
        .def("NormalizedScreenToGameMap", &Overlay::NormalizedScreenToGameMap, py::arg("norm_x"), py::arg("norm_y"), "Convert normalized screen coordinates to game map coordinates")
        .def("GamePosToNormalizedScreen", &Overlay::GamePosToNormalizedScreen, py::arg("x"), py::arg("y"), "Convert game position to normalized screen coordinates")

        .def("BeginDraw", py::overload_cast<>(&Overlay::BeginDraw), "BeginDraw with no arguments")
        .def("BeginDraw", py::overload_cast<std::string>(&Overlay::BeginDraw), py::arg("name"), "BeginDraw with name")
        .def("BeginDraw", py::overload_cast<std::string, float, float, float, float>(&Overlay::BeginDraw),
            py::arg("name"), py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"), "BeginDraw with name and bounds")
        .def("EndDraw", &Overlay::EndDraw)
        .def("DrawLine", &Overlay::DrawLine, py::arg("from"), py::arg("to"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawLine3D", &Overlay::DrawLine3D, py::arg("from"), py::arg("to"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawTriangle", &Overlay::DrawTriangle, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawTriangle3D", &Overlay::DrawTriangle3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawTriangleFilled", &Overlay::DrawTriangleFilled, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawTriangleFilled3D", &Overlay::DrawTriangleFilled3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawQuad", &Overlay::DrawQuad, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawQuad3D", &Overlay::DrawQuad3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawQuadFilled", &Overlay::DrawQuadFilled, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawQuadFilled3D", &Overlay::DrawQuadFilled3D, py::arg("p1"), py::arg("p2"), py::arg("p3"), py::arg("p4"), py::arg("color") = 0xFFFFFFFF)
        .def("DrawPoly", &Overlay::DrawPoly, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 12, py::arg("thickness") = 1.0f)
        .def("DrawPoly3D", &Overlay::DrawPoly3D, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 12, py::arg("thickness") = 1.0f, py::arg("autoZ") = true)
        .def("DrawPolyFilled", &Overlay::DrawPolyFilled, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 12)
        .def("DrawPolyFilled3D", &Overlay::DrawPolyFilled3D, py::arg("center"), py::arg("radius"), py::arg("color") = 0xFFFFFFFF, py::arg("numSegments") = 12, py::arg("autoZ") = true)
        .def("DrawCubeOutline", &Overlay::DrawCubeOutline, py::arg("center"), py::arg("size"), py::arg("color") = 0xFFFFFFFF, py::arg("thickness") = 1.0f)
        .def("DrawCubeFilled", &Overlay::DrawCubeFilled, py::arg("center"), py::arg("size"), py::arg("color") = 0xFFFFFFFF)

        .def("DrawText", &Overlay::DrawText2D, py::arg("position"), py::arg("text"), py::arg("color") = 0xFFFFFFFF, py::arg("centered") = true, py::arg("scale") = 1.0)
        .def("DrawText3D", &Overlay::DrawText3D, py::arg("position3D"), py::arg("text"), py::arg("color") = 0xFFFFFFFF, py::arg("autoZ") = true, py::arg("centered") = true, py::arg("scale") = 1.0)
        .def("GetDisplaySize", &Overlay::GetDisplaySize)
        .def("IsMouseClicked", &Overlay::IsMouseClicked, py::arg("button") = 0)
        .def("PushClipRect", &Overlay::PushClipRect, py::arg("x"), py::arg("y"), py::arg("x2"), py::arg("y2"))
        .def("PopClipRect", &Overlay::PopClipRect)
        // ==== DrawTexture overloads ====
        .def("DrawTexture",
            static_cast<void(Overlay::*)(const std::string&, float, float)>(&Overlay::DrawTexture),
            py::arg("path"),
            py::arg("width") = 32.0f,
            py::arg("height") = 32.0f)

        .def("DrawTexture",
            static_cast<void(Overlay::*)(const std::string&,
                std::tuple<float, float>,
                std::tuple<float, float>,
                std::tuple<float, float>,
                std::tuple<int, int, int, int>,
                std::tuple<int, int, int, int>)>(&Overlay::DrawTexture),
            py::arg("path"),
            py::arg("size") = std::make_tuple(32.0f, 32.0f),
            py::arg("uv0") = std::make_tuple(0.0f, 0.0f),
            py::arg("uv1") = std::make_tuple(1.0f, 1.0f),
            py::arg("tint") = std::make_tuple(255, 255, 255, 255),
            py::arg("border_col") = std::make_tuple(0, 0, 0, 0))

        // ==== DrawTexturedRect overloads ====
        .def("DrawTexturedRect",
            static_cast<void(Overlay::*)(float, float, float, float, const std::string&)>(&Overlay::DrawTexturedRect),
            py::arg("x"),
            py::arg("y"),
            py::arg("width"),
            py::arg("height"),
            py::arg("texture_path"))

        .def("DrawTexturedRect",
            static_cast<void(Overlay::*)(std::tuple<float, float>,
                std::tuple<float, float>,
                const std::string&,
                std::tuple<float, float>,
                std::tuple<float, float>,
                std::tuple<int, int, int, int>)>(&Overlay::DrawTexturedRect),
            py::arg("pos"),
            py::arg("size"),
            py::arg("texture_path"),
            py::arg("uv0") = std::make_tuple(0.0f, 0.0f),
            py::arg("uv1") = std::make_tuple(1.0f, 1.0f),
            py::arg("tint") = std::make_tuple(255, 255, 255, 255))

        .def("UpkeepTextures", &Overlay::UpkeepTextures)
        .def("ImageButton",
            static_cast<bool(Overlay::*)(const std::string&, const std::string&, float, float, int)>(&Overlay::ImageButton),
            py::arg("caption"),
            py::arg("file_path"),
            py::arg("width") = 32.0f,
            py::arg("height") = 32.0f,
            py::arg("frame_padding") = 0)

        .def("ImageButton",
            static_cast<bool(Overlay::*)(const std::string&, const std::string&,
                std::tuple<float, float>,
                std::tuple<float, float>,
                std::tuple<float, float>,
                std::tuple<int, int, int, int>,
                std::tuple<int, int, int, int>,
                int)>(&Overlay::ImageButton),
            py::arg("caption"),
            py::arg("file_path"),
            py::arg("size") = std::make_tuple(32.0f, 32.0f),
            py::arg("uv0") = std::make_tuple(0.0f, 0.0f),
            py::arg("uv1") = std::make_tuple(1.0f, 1.0f),
            py::arg("bg_color") = std::make_tuple(0, 0, 0, 0),
            py::arg("tint_color") = std::make_tuple(255, 255, 255, 255),
            py::arg("frame_padding") = 0)

		.def("DrawTextureInForegound",
			&Overlay::DrawTextureInForegound,
			py::arg("pos") = std::make_tuple(0.0f, 0.0f),
			py::arg("size") = std::make_tuple(100.0f, 100.0f),
			py::arg("texture_path") = "",
			py::arg("uv0") = std::make_tuple(0.0f, 0.0f),
			py::arg("uv1") = std::make_tuple(1.0f, 1.0f),
			py::arg("tint") = std::make_tuple(255, 255, 255, 255))

        .def("DrawTextureInDrawlist",
            &Overlay::DrawTextureInDrawlist,
            py::arg("pos") = std::make_tuple(0.0f, 0.0f),
            py::arg("size") = std::make_tuple(100.0f, 100.0f),
            py::arg("texture_path") = "",
            py::arg("uv0") = std::make_tuple(0.0f, 0.0f),
            py::arg("uv1") = std::make_tuple(1.0f, 1.0f),
            py::arg("tint") = std::make_tuple(255, 255, 255, 255));


}

PYBIND11_EMBEDDED_MODULE(PyOverlay, m) {
    bind_Point2D(m);
    bind_Point3D(m);
    bind_overlay(m);
}

