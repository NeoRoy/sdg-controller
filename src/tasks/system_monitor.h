#ifndef BERND_BOX_TASKS_SYSTEM_MONITOR_H
#define BERND_BOX_TASKS_SYSTEM_MONITOR_H

#include <chrono>

#include "TaskSchedulerDeclarations.h"
#include "config.h"
#include "managers/mqtt.h"

namespace bernd_box {
namespace tasks {

/**
 * Monitors the controller's state and informs the coordinator about it. Health
 * parameters include free memory and heap fragmentation.
 */
class SystemMonitor : public Task {
 public:
  SystemMonitor(Scheduler* scheduler, Mqtt& mqtt);
  virtual ~SystemMonitor();

  /**
   * Specifies how often the system monitor should check and send its state
   *
   * \param interval How often the check should be performed
   */
  void setInterval(std::chrono::milliseconds interval);

 private:
  /**
   * Resets the CPU load counters and delays the task for one iteration
   * 
   * \return true
   */
  bool OnEnable() final;

  /**
   * Measure the state of the heap.
   *
   * \return true
   */
  bool Callback() final;

  Scheduler* scheduler_;
  Mqtt& mqtt_;
  /// The suffix of the telemetry and action topic to publish on
  const __FlashStringHelper* name_;
};

}  // namespace tasks
}  // namespace bernd_box

#endif  // BERND_BOX_TASKS_SYSTEM_STATE_H
