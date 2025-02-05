/*
 * ServiceManager.cpp
 *
 *  Created on: Dec 31, 2024
 *      Author: mgi
 */

#include "ServiceManager.hpp"
#include <algorithm>
#include "Allocator.hpp"
#include "usb_device.h"
#include "usbd_cdc_if.h"

ServiceManager::ServiceManager(O1HeapInstance* heap) : heap(heap) {}


void ServiceManager::add(std::shared_ptr<Task> task)
{
	task->registerTask(this, task);
}

void ServiceManager::remove(std::shared_ptr<Task> task)
{

}

void ServiceManager::subscribe(int16_t port_id, size_t payload_size, std::shared_ptr<Task> task)
{
	services.push(TaskHandler {.port_id = port_id, .payload_size = payload_size, .task = task} );
}

void ServiceManager::setFilters(CanardInstance *canard)
{
	std::sort(services.begin(), services.end(), [](const TaskHandler& a, const TaskHandler& b) {
		return a.port_id > b.port_id; }
	);
	for(auto service : services)
	{
		int16_t port_id = service.port_id;
		size_t payload_size = service.payload_size;
		if (port_id < 1) continue;
		size_t index = subscriptions.find(
				SubscriptionMap {.port_id = port_id, .subscription = {}},
				[port_id](const SubscriptionMap& map, const SubscriptionMap& other) { return map.port_id == port_id; }
		);
		canardRxSubscribe(canard, CanardTransferKindMessage, port_id, payload_size, CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC, &subscriptions[index].subscription);
	}
}

void ServiceManager::removeFilters(CanardInstance *canard)
{

}

void ServiceManager::initializeServices(uint32_t now)
{
	for(auto service : services)
	{
		service.task->initialize(now);
	}
}

void ServiceManager::handleMessage(std::shared_ptr<CanardRxTransfer> transfer)
{
	for(auto service : services)
	{
		if (service.port_id < 1) break;
		if (service.port_id == transfer->metadata.port_id)
		{
			service.task->handleMessage(transfer);
//			{
//				char message[256];
//				sprintf(message, "ServiceManager::handleMessage %4d %8p %8p %ld %ld\r\n",
//						service.port_id, &service.task, service.task.get(), service.task.use_count(), transfer.use_count());
//				CDC_Transmit_FS((uint8_t *) message, strlen(message));
//			}
		}
	}
}

void ServiceManager::handleProcesses()
{
	for(auto service : services)
	{
		service.task->handleTask();
	}
}

