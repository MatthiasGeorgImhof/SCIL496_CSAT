/*
 * ServiceManager.cpp
 *
 *  Created on: Dec 31, 2024
 *      Author: mgi
 */

#include "ServiceManager.hpp"
#include <algorithm>
#include "Allocator.hpp"

//void ServiceManager::add(std::shared_ptr<Task> task)
//{
//	task->registerTask(this, task);
//}
//
//void ServiceManager::remove(std::shared_ptr<Task> task)
//{
//	task->unregisterTask(this, task);
//}
//
//void ServiceManager::subscribe(int16_t port_id, std::shared_ptr<Task> task)
//{
//	services.push(TaskHandler {.port_id = port_id, .task = task} );
//}
//
//void ServiceManager::unsubscribe(int16_t port_id, std::shared_ptr<Task> task)
//{
//	size_t index = services.find(TaskHandler {.port_id = port_id, .task = task},
//			[](TaskHandler task, TaskHandler other) { return task.port_id == other.port_id && task.task == other.task; });
//	services.remove(index);
//}
//
//
void ServiceManager::initializeServices(uint32_t now) const
{
	for(auto handler : handlers_)
	{
		handler.task->initialize(now);
	}
}

void ServiceManager::handleMessage(std::shared_ptr<CyphalTransfer> transfer) const
{
	for(auto handler : handlers_)
	{
		if (handler.port_id == transfer->metadata.port_id)
		{
			handler.task->handleMessage(transfer);
		}
	}
}

void ServiceManager::handleServices() const
{
	for(auto handler : handlers_)
	{
		handler.task->handleTask();
	}
}

