#ifndef LAPLACE_EPOCH_H
#define LAPLACE_EPOCH_H

#include "laplace/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct laplace_epoch_snapshot {
    laplace_epoch_id_t current_epoch;
} laplace_epoch_snapshot_t;

#ifdef __cplusplus
}
#endif

#endif