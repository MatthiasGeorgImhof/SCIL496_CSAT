/*
 * ServiceManager.hpp
 *
 *  Created on: Dec 31, 2024
 *      Author: mgi
 */

#ifndef INC_SERVICEMANAGER_HPP_
#define INC_SERVICEMANAGER_HPP_

#include <memory>
#include "cstdint"

#include "ArrayList.hpp"
#include "Task.hpp"

typedef struct {
	int16_t port_id;
	std::shared_ptr<Task> task;
} TaskHandler;
constexpr size_t NUM_TASKS=32;

class ServiceManager {
public:
	ServiceManager () = delete;
	ServiceManager(const ArrayList<TaskHandler, NUM_TASKS> &handlers): handlers_(handlers) {};
//	void add(std::shared_ptr<Task> task);
//	void remove(std::shared_ptr<Task> task);

	void initializeServices(uint32_t now) const;
	void handleMessage(std::shared_ptr<CyphalTransfer> transfer) const;
	void handleServices() const;

private:
	const ArrayList<TaskHandler, NUM_TASKS> &handlers_;
};

#endif /* INC_SERVICEMANAGER_HPP_ */
