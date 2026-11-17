#ifndef XLIONCORE_PHYSICS_TASKS_H
#define XLIONCORE_PHYSICS_TASKS_H
#pragma once

// Box3D's solver on xscheduler's workers (plan 13.5): the world definition's task hooks hand each Box3D task to this module's scheduler (the
// same one the rest of the module uses; never a private pool). Box3D splits its parallel stages into one task per worker; finishTask joins,
// and xscheduler's join runs other pending jobs on the waiting thread (no parking: b3World_Step may run inside a job).
// XLION_PHYSICS_WORKERS=n overrides the worker count (1: serial, as before 13.5; the A/B switch).
#include "dependencies/xscheduler/source/xscheduler.h"
#include <box3d/box3d.h>
#include <algorithm>
#include <cstdlib>

namespace xlioncore::physics::tasks
{
    inline xscheduler::system& Scheduler() noexcept
    {
        auto& S = xscheduler::g_System;
        if (S.getWorkerCount() == 0) S.Init();
        return S;
    }

    // ponytail: one task_group allocated per Box3D task (a few per sub-step); a pool of groups if the allocation ever shows in a profile
    inline void* Enqueue(b3TaskCallback* pTask, void* pTaskContext, void* /*pUser*/, const char* /*pName*/) noexcept
    {
        auto* pGroup = new xscheduler::task_group({ "Physics" }, Scheduler());
        pGroup->Submit([pTask, pTaskContext] { pTask(pTaskContext); });
        return pGroup;
    }

    inline void Finish(void* pUserTask, void* /*pUser*/) noexcept
    {
        auto* pGroup = static_cast<xscheduler::task_group*>(pUserTask);
        pGroup->join();
        delete pGroup;
    }

    inline int WorkerCount() noexcept
    {
#if defined(_MSC_VER)
#pragma warning(suppress: 4996)
#endif
        if (const char* p = std::getenv("XLION_PHYSICS_WORKERS"); p && *p) return std::clamp(std::atoi(p), 1, B3_MAX_WORKERS);
        // (the stepping thread works too). ponytail: capped at 8 - measured 2026-11-18 (3000 boxes + 300 chain links, 60 Hz, 4 sub-steps, 28
        // logical cores): 1 worker 1.36 ms mean / 4.29 p95, 4: 0.47 / 1.31, 8: 0.35 / 0.86, 16: 0.37 / 0.82 (Box3D: past the performance
        // cores it gains nothing); a per-machine count from the core topology when another machine matters
        return std::clamp(Scheduler().getWorkerCount() + 1, 1, 8);
    }

    // The world definition with the hooks (serial when one worker)
    inline void Attach(b3WorldDef& Def) noexcept
    {
        Def.workerCount = static_cast<uint32_t>(WorkerCount());
        if (Def.workerCount > 1) { Def.enqueueTask = &Enqueue; Def.finishTask = &Finish; }
    }
}

#endif
