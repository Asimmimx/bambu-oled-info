#include <assert.h>
#include <stdio.h>
#include "../src/PollSchedule.h"

int main() {
  using namespace pollSchedule;
  assert(!due(299999, 0, 300000));
  assert(due(300000, 0, 300000));
  assert(!due(19, UINT32_MAX - 10, 31));
  assert(due(20, UINT32_MAX - 10, 31));
  assert(!nearFinish(true, -1, 0, 600000, 10));
  assert(!nearFinish(false, 1, 0, 600000, 10)); // idle / paused ETA cannot count down
  assert(!nearFinish(true, 15, 0, 299999, 10));
  assert(nearFinish(true, 15, 0, 300000, 10)); // no new reports needed to enter fast mode
  assert(nearFinish(true, 0, 0, 600000, 10)); // overdue jobs stay fast until FINISH arrives
  assert(nearFinish(true, 10, UINT32_MAX - 10, 20, 10));
  assert(!nearFinish(true, INT32_MAX, 0, 600000, 10)); // no multiplication overflow
  assert(interval(true, false, 300000, 60000, 30000, 10000) == 300000);
  assert(interval(true, true, 300000, 60000, 30000, 10000) == 60000);
  assert(interval(false, false, 300000, 60000, 30000, 10000) == 30000);
  assert(interval(false, true, 300000, 60000, 30000, 10000) == 10000);
  assert(fullReportInterval(true, 0, 0, 1000, 330000, 60000) == 60000);
  assert(fullReportInterval(true, 1, 0, 59999, 330000, 60000) == 330000);
  assert(fullReportInterval(true, 1, 0, 60000, 330000, 60000) == 60000);
  assert(fullReportInterval(false, 0, 0, 60000, 330000, 60000) == 330000);
  assert(fullReportInterval(true, -1, 0, 60000, 330000, 60000) == 330000);
  assert(!due(59999, 0, fullReportInterval(true, 0, 0, 59999, 330000, 60000)));
  assert(due(60000, 0, fullReportInterval(true, 0, 0, 60000, 330000, 60000)));

  Next next;
  next.consider(0, true, 60000, 50000, 30000, 0); // selected printer is not due
  next.consider(1, true, 60000, 0, 30000, 0);
  assert(next.index == 1); // focus cannot starve another printer
  next.consider(0, true, 60000, 20000, 30000, 0);
  assert(next.index == 1); // even a due focused printer cannot displace a more overdue one
  next.consider(2, false, 60000, 0, 300000, 0);
  assert(next.index == 2); // unknown printers are read first
  Next backoff;
  backoff.consider(0, true, 59999, 0, 60000, 0);
  assert(backoff.index == -1); // focus must respect authentication cooldown
  backoff.consider(0, true, 60000, 0, 60000, 0);
  assert(backoff.index == 0);
  Next wrap;
  wrap.consider(1, true, 20, UINT32_MAX - 10, 31, -1);
  assert(wrap.index == 1);
  puts("Polling regression tests passed");
}
