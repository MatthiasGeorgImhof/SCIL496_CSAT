/*
 * ServiceManager.hpp
 *
 *  Created on: Dec 31, 2024
 *      Author: mgi
 */

#ifndef INC_SERVICEMANAGER_HPP_
#define INC_SERVICEMANAGER_HPP_

#include <memory>
#include "stdint.h"

#include "canard.h"
#include "ArrayList.hpp"
#include "Allocator.hpp"
#include "Task.hpp"
#include "canard.h"
#include "o1heap.h"

typedef struct {
	int16_t port_id;
	size_t payload_size;
	std::shared_ptr<Task> task;
} TaskHandler;

typedef struct {
	int16_t port_id;
	CanardRxSubscription subscription;
} SubscriptionMap;

constexpr size_t NUM_TASKS=32;

class ServiceManager {
public:
	ServiceManager() = delete;
	ServiceManager(O1HeapInstance* heap);
	void add(std::shared_ptr<Task> task);
	void remove(std::shared_ptr<Task> task);

	void subscribe(int16_t port_id, size_t payload_size, std::shared_ptr<Task> task);

	void setFilters(CanardInstance *canard);
	void removeFilters(CanardInstance *canard);

	void initializeServices(uint32_t now);
	void handleMessage(std::shared_ptr<CanardRxTransfer> transfer);
	void handleProcesses();

private:
	O1HeapInstance* heap;
	ArrayList<TaskHandler, NUM_TASKS> services;
	ArrayList<SubscriptionMap, NUM_TASKS> subscriptions;
};

#endif /* INC_SERVICEMANAGER_HPP_ */
