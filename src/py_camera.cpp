#include "py_camera.h"

PyCamera::PyCamera() {
	GetContext();
}

void PyCamera::ResetContext() {

	look_at_agent_id = 0;
	h0004 = 0;
	h0008 = 0;
	h000C = 0;
	max_distance = 0;
	h0014 = 0;
	yaw = 0;
	current_yaw = 0;
	pitch = 0;
	camera_zoom = 0;
	h0024.clear();
	h0024.resize(4, 0);
	yaw_right_click = 0;
	yaw_right_click2 = 0;
	pitch_right_click = 0;
	distance2 = 0;
	acceleration_constant = 0;
	time_since_last_keyboard_rotation = 0;
	time_since_last_mouse_rotation = 0;
	time_since_last_mouse_move = 0;
	time_since_last_agent_selection = 0;
	time_in_the_map = 0;
	time_in_the_district = 0;
	yaw_to_go = 0;
	pitch_to_go = 0;
	dist_to_go = 0;
	max_distance2 = 0;
	h0070.clear();
	h0070.resize(2, 0);
	position.x = position.y = position.z = camera_pos_to_go.x =
		camera_pos_to_go.y =
		camera_pos_to_go.z =
		cam_pos_inverted.x =
		cam_pos_inverted.y =
		cam_pos_inverted.z =
		cam_pos_inverted_to_go.x =
		cam_pos_inverted_to_go.y =
		cam_pos_inverted_to_go.z =
		look_at_target.x =
		look_at_target.y =
		look_at_target.z =
		look_at_to_go.x =
		look_at_to_go.y =
		look_at_to_go.z =
		field_of_view =
		field_of_view2 = -1.0f;
}

void PyCamera::GetContext() {
	auto instance_type = GW::Map::GetInstanceType();
	bool is_map_ready = (GW::Map::GetIsMapLoaded()) && (!GW::Map::GetIsObserving()) && (instance_type != GW::Constants::InstanceType::Loading);

	if (!is_map_ready) {
		ResetContext();
		return;
	}

	const auto camera = GW::CameraMgr::GetCamera();
	if (!camera) { ResetContext(); return; }
	look_at_agent_id = camera->look_at_agent_id;
	h0004 = camera->h0004;
	h0008 = camera->h0008;
	h000C = camera->h000C;
	max_distance = camera->max_distance;
	h0014 = camera->h0014;
	yaw = camera->yaw;
	current_yaw = camera->GetCurrentYaw();
	pitch = camera->pitch;
	camera_zoom = camera->distance;
	h0024.push_back(camera->h0024[0]);
	h0024.push_back(camera->h0024[1]);
	h0024.push_back(camera->h0024[2]);
	h0024.push_back(camera->h0024[3]);
	yaw_right_click = camera->yaw_right_click;
	yaw_right_click2 = camera->yaw_right_click2;
	pitch_right_click = camera->pitch_right_click;
	distance2 = camera->distance2;
	acceleration_constant = camera->acceleration_constant;
	time_since_last_keyboard_rotation = camera->time_since_last_keyboard_rotation;
	time_since_last_mouse_rotation = camera->time_since_last_mouse_rotation;
	time_since_last_mouse_move = camera->time_since_last_mouse_move;
	time_since_last_agent_selection = camera->time_since_last_agent_selection;
	time_in_the_map = camera->time_in_the_map;
	time_in_the_district = camera->time_in_the_district;
	yaw_to_go = camera->yaw_to_go;
	pitch_to_go = camera->pitch_to_go;
	dist_to_go = camera->dist_to_go;
	max_distance2 = camera->max_distance2;
	h0070.push_back(camera->h0070[0]);
	h0070.push_back(camera->h0070[1]);
	position = Point3D(camera->position.x, camera->position.y, camera->position.z);
	camera_pos_to_go = Point3D(camera->camera_pos_to_go.x, camera->camera_pos_to_go.y, camera->camera_pos_to_go.z);
	cam_pos_inverted = Point3D(camera->cam_pos_inverted.x, camera->cam_pos_inverted.y, camera->cam_pos_inverted.z);
	cam_pos_inverted_to_go = Point3D(camera->cam_pos_inverted_to_go.x, camera->cam_pos_inverted_to_go.y, camera->cam_pos_inverted_to_go.z);
	look_at_target = Point3D(camera->look_at_target.x, camera->look_at_target.y, camera->look_at_target.z);
	look_at_to_go = Point3D(camera->look_at_to_go.x, camera->look_at_to_go.y, camera->look_at_to_go.z);
	field_of_view = camera->field_of_view;
	field_of_view2 = camera->field_of_view2;
}

void PyCamera::SetYaw(float _yaw) {
	GW::GameThread::Enqueue([_yaw] {
		const auto camera = GW::CameraMgr::GetCamera();
		camera->SetYaw(_yaw);
		});

}

void PyCamera::SetPitch(float _pitch) {
	GW::GameThread::Enqueue([_pitch] {
		const auto camera = GW::CameraMgr::GetCamera();
		camera->SetPitch(_pitch);
		});
}

void PyCamera::SetCameraPos(float x, float y, float z) {
	GW::GameThread::Enqueue([x, y, z] {
		const auto camera = GW::CameraMgr::GetCamera();
		camera->position = GW::Vec3f(x, y, z);
		});
}

void PyCamera::SetLookAtTarget(float x, float y, float z) {
	GW::GameThread::Enqueue([x, y, z] {
		const auto camera = GW::CameraMgr::GetCamera();
		camera->look_at_target = GW::Vec3f(x, y, z);
		});
}

void PyCamera::SetMaxDist(float dist) {
	GW::GameThread::Enqueue([dist] {
		GW::CameraMgr::SetMaxDist(dist);
		});
}

void PyCamera::SetFieldOfView(float fov) {
	GW::GameThread::Enqueue([fov] {
		GW::CameraMgr::SetFieldOfView(fov);

		});
}

void PyCamera::UnlockCam(bool unlock) {
	GW::GameThread::Enqueue([unlock] {
		GW::CameraMgr::UnlockCam(unlock);
		});
}

bool PyCamera::GetCameraUnlock() {
	return GW::CameraMgr::GetCameraUnlock();
}

void PyCamera::SetFog(bool fog) {
	GW::GameThread::Enqueue([fog] {
		GW::CameraMgr::SetFog(fog);
		});
}

void PyCamera::ForwardMovement(float amount, bool true_fordward) {
	GW::GameThread::Enqueue([amount, true_fordward] {
		GW::CameraMgr::ForwardMovement(amount, true_fordward);
		});
}

void PyCamera::VerticalMovement(float amount) {
	GW::GameThread::Enqueue([amount] {
		GW::CameraMgr::VerticalMovement(amount);
		});
}

void PyCamera::SideMovement(float amount) {
	GW::GameThread::Enqueue([amount] {
		GW::CameraMgr::SideMovement(amount);
		});
}

void PyCamera::RotateMovement(float angle) {
	GW::GameThread::Enqueue([angle] {
		GW::CameraMgr::RotateMovement(angle);
		});
}

Point3D PyCamera::ComputeCameraPos() {
	auto  camera_pos = GW::CameraMgr::ComputeCamPos();
	return Point3D(camera_pos.x, camera_pos.y, camera_pos.z);
}

void PyCamera::UpdateCameraPos() {
	GW::GameThread::Enqueue([] {
		GW::CameraMgr::UpdateCameraPos();
		});
}

void bind_camera(py::module_& m) {
	py::class_<PyCamera>(m, "PyCamera")
		.def(py::init<>())  // default constructor

		// Read/write fields
		.def_readwrite("look_at_agent_id", &PyCamera::look_at_agent_id)
		.def_readwrite("yaw", &PyCamera::yaw)
		.def_readwrite("pitch", &PyCamera::pitch)
		.def_readwrite("camera_zoom", &PyCamera::camera_zoom)
		.def_readwrite("max_distance", &PyCamera::max_distance)
		.def_readwrite("yaw_right_click", &PyCamera::yaw_right_click)
		.def_readwrite("yaw_right_click2", &PyCamera::yaw_right_click2)
		.def_readwrite("pitch_right_click", &PyCamera::pitch_right_click)
		.def_readwrite("distance2", &PyCamera::distance2)
		.def_readwrite("acceleration_constant", &PyCamera::acceleration_constant)
		.def_readwrite("time_since_last_keyboard_rotation", &PyCamera::time_since_last_keyboard_rotation)
		.def_readwrite("time_since_last_mouse_rotation", &PyCamera::time_since_last_mouse_rotation)
		.def_readwrite("time_since_last_mouse_move", &PyCamera::time_since_last_mouse_move)
		.def_readwrite("time_since_last_agent_selection", &PyCamera::time_since_last_agent_selection)
		.def_readwrite("time_in_the_map", &PyCamera::time_in_the_map)
		.def_readwrite("time_in_the_district", &PyCamera::time_in_the_district)
		.def_readwrite("yaw_to_go", &PyCamera::yaw_to_go)
		.def_readwrite("pitch_to_go", &PyCamera::pitch_to_go)
		.def_readwrite("dist_to_go", &PyCamera::dist_to_go)
		.def_readwrite("max_distance2", &PyCamera::max_distance2)
		.def_readwrite("field_of_view", &PyCamera::field_of_view)
		.def_readwrite("field_of_view2", &PyCamera::field_of_view2)
		.def_readwrite("h0024", &PyCamera::h0024)
		.def_readwrite("h0070", &PyCamera::h0070)
		.def_readwrite("position", &PyCamera::position)
		.def_readwrite("camera_pos_to_go", &PyCamera::camera_pos_to_go)
		.def_readwrite("cam_pos_inverted", &PyCamera::cam_pos_inverted)
		.def_readwrite("cam_pos_inverted_to_go", &PyCamera::cam_pos_inverted_to_go)
		.def_readwrite("look_at_target", &PyCamera::look_at_target)
		.def_readwrite("look_at_to_go", &PyCamera::look_at_to_go)

		// Methods (bound instance)
		.def("GetContext", &PyCamera::GetContext)
		.def("SetYaw", &PyCamera::SetYaw, py::arg("_yaw"))
		.def("SetPitch", &PyCamera::SetPitch, py::arg("_pitch"))
		.def("SetMaxDist", &PyCamera::SetMaxDist, py::arg("dist"))
		.def("SetFieldOfView", &PyCamera::SetFieldOfView, py::arg("fov"))
		.def("UnlockCam", &PyCamera::UnlockCam, py::arg("unlock"))
		.def("GetCameraUnlock", &PyCamera::GetCameraUnlock)
		.def("ForwardMovement", &PyCamera::ForwardMovement, py::arg("amount"), py::arg("true_forward"))
		.def("VerticalMovement", &PyCamera::VerticalMovement, py::arg("amount"))
		.def("SideMovement", &PyCamera::SideMovement, py::arg("amount"))
		.def("RotateMovement", &PyCamera::RotateMovement, py::arg("angle"))
		.def("ComputeCameraPos", &PyCamera::ComputeCameraPos)
		.def("UpdateCameraPos", &PyCamera::UpdateCameraPos)
		.def("SetCameraPos", &PyCamera::SetCameraPos, py::arg("x"), py::arg("y"), py::arg("z"))
		.def("SetLookAtTarget", &PyCamera::SetLookAtTarget, py::arg("x"), py::arg("y"), py::arg("z"))
		.def("SetFog", &PyCamera::SetFog, py::arg("fog"));
}


PYBIND11_EMBEDDED_MODULE(PyCamera, m) {
	bind_camera(m);
}
