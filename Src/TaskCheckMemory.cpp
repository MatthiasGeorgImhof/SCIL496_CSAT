/*
 * TaskSendHeartBeat.cpp
 *
 *  Created on: Dec 4, 2024
 *      Author: mgi
 */

#include "TaskCheckMemory.hpp"
#include "RegistrationManager.hpp"
#include "Logger.hpp"
#include "cyphal.hpp"

void TaskCheckMemory::registerTask(RegistrationManager *manager, std::shared_ptr<Task> task)
{
	CyphalSubscription subscription = { 0, 0, CyphalTransferKindMessage};
	std::tuple<> adapters;
	manager->subscribe(subscription, task, adapters);
}

void TaskCheckMemory::unregisterTask(RegistrationManager *manager, std::shared_ptr<Task> task)
{
	CyphalSubscription subscription = { 0, 0, CyphalTransferKindMessage};
	std::tuple<> adapters;
	manager->unsubscribe(subscription, task, adapters);
}

void TaskCheckMemory::handleTaskImpl()
{
	bool heap_health = o1heapDoInvariantsHold(o1heap);
	O1HeapDiagnostics diagnostic = o1heapGetDiagnostics(o1heap);
	{
		log(LOG_LEVEL_INFO, "Memory: %d, (%4d %4d %4d)\r\n", heap_health, diagnostic.capacity, diagnostic.peak_allocated, diagnostic.allocated);
	}
}


