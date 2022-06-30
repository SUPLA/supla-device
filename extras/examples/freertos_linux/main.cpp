/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/*
 This application starts two tasks which prints on stdout periodically.
 This is just draft. It still lacks many important functions, so it is not
 recommend t use it or rely on it in any way.
*/

#include <FreeRTOS.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <task.h>

#include <iostream>

class TaskWrap {
 public:
  virtual ~TaskWrap() {
  }
  void start(const char *name, const uint16_t stackSize, UBaseType_t priority) {
    xTaskCreate(TaskWrap::setupTask, name, stackSize, this, priority, &handle);
  }

 private:
  virtual void setup() {
  }
  virtual void loop() {
  }

  static void setupTask(void *ptr) {
    TaskWrap *taskPtr = reinterpret_cast<TaskWrap *>(ptr);
    taskPtr->setup();
    while (1) {
      taskPtr->loop();
    }
  }
  TaskHandle_t handle;
};

class TestTask1 : public TaskWrap {
 private:
  void loop() override {
    SUPLA_LOG_DEBUG("task1 test: %d", millis());
    vTaskDelay(100);
  }
};

class TestTask2 : public TaskWrap {
 private:
  void loop() override {
    SUPLA_LOG_DEBUG("task2 test: %d", millis());
    vTaskDelay(550);
  }
};

int main() {
  SUPLA_LOG_DEBUG("Hello FreeRTOS with Supla!");

  TestTask1 task1;
  TestTask2 task2;

  task1.start("testing_task_1", 1024, 1);
  task2.start("testing_task_2", 1024, 1);

  vTaskStartScheduler();

  return 0;
}

void vAssertCalled(uint64_t line, const char *const fileName) {
}

extern "C" void vApplicationMallocFailedHook(void);
void vApplicationMallocFailedHook(void) {
}
