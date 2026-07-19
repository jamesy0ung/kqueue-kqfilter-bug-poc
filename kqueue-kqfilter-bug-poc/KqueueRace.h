#ifndef KqueueRace_h
#define KqueueRace_h

#include <stdint.h>

typedef struct {
    int64_t iterations;
    int won;
    int kqa;
    int kqb;
    int trigger_result;
    int trigger_errno;
} kq_race_result_t;

kq_race_result_t kq_race_run(long limit);

#endif /* KqueueRace_h */
