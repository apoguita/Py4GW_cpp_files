#pragma once
#include "Headers.h"

using namespace DirectX;


PYBIND11_EMBEDDED_MODULE(PyQuest, m) {
    py::class_<Quest>(m, "PyQuest")
        .def(py::init<>())
        .def("set_active_quest_id", &Quest::SetActiveQuestId, py::arg("quest_id"))
        .def("get_active_quest_id", &Quest::GetActiveQuestId)
        .def("abandon_quest_id", &Quest::AbandonQuestId, py::arg("quest_id"))
		.def("is_quest_completed", &Quest::IsQuestCompleted, py::arg("quest_id"))
		.def("is_quest_primary", &Quest::IsQuestPrimary, py::arg("quest_id"));
}



