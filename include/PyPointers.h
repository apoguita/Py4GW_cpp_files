
class PyPointers {
public:
	static uintptr_t GetMissionMapContextPtr();
	static uintptr_t GetWorldMapContextPtr();
	static uintptr_t GetGameplayContextPtr();
	static uintptr_t GetAreaInfoPtr();
	static uintptr_t GetMapContextPtr();
};

uintptr_t  PyPointers::GetWorldMapContextPtr(){
	return reinterpret_cast<uintptr_t>(GW::Map::GetWorldMapContext());
}

uintptr_t  PyPointers::GetMissionMapContextPtr(){
	return reinterpret_cast<uintptr_t>(GW::Map::GetMissionMapContext());
}

uintptr_t PyPointers::GetGameplayContextPtr(){
	return reinterpret_cast<uintptr_t>(GW::GetGameplayContext());
}

uintptr_t PyPointers::GetAreaInfoPtr(){
	return reinterpret_cast<uintptr_t>(GW::Map::GetMapInfo(GW::Map::GetMapID()));
}

uintptr_t PyPointers::GetMapContextPtr(){
	return reinterpret_cast<uintptr_t>(GW::GetMapContext());
}


PYBIND11_EMBEDDED_MODULE(PyPointers, m) {

	py::class_<PyPointers>(m, "PyPointers")
		.def_static("GetMissionMapContextPtr", &PyPointers::GetMissionMapContextPtr)
		.def_static("GetWorldMapContextPtr", &PyPointers::GetWorldMapContextPtr)
		.def_static("GetGameplayContextPtr", &PyPointers::GetGameplayContextPtr)
		.def_static("GetMapContextPtr", &PyPointers::GetMapContextPtr)
		.def_static("GetAreaInfoPtr", &PyPointers::GetAreaInfoPtr)
		;

}

