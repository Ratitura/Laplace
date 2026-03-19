#include "laplace/state.h"

/*
 * Transition table encoded as a 2D boolean matrix.
 * Row = from state, Column = to state.
 * Entry is true if transition is legal.
 *
 * Valid transitions:
 *   FREE     -> PENDING
 *   PENDING  -> READY
 *   PENDING  -> DEAD
 *   READY    -> ACTIVE
 *   READY    -> MASKED
 *   READY    -> DEAD
 *   ACTIVE   -> MASKED
 *   ACTIVE   -> DEAD
 *   MASKED   -> ACTIVE
 *   MASKED   -> DEAD
 *   DEAD     -> RETIRED
 *   RETIRED  -> FREE
 */
static const bool laplace__state_transitions[LAPLACE_STATE_COUNT_][LAPLACE_STATE_COUNT_] = {
    /* from FREE    */ { false, true,  false, false, false, false, false },
    /* from PENDING */ { false, false, true,  false, false, true,  false },
    /* from READY   */ { false, false, false, true,  true,  true,  false },
    /* from ACTIVE  */ { false, false, false, false, true,  true,  false },
    /* from MASKED  */ { false, false, false, true,  false, true,  false },
    /* from DEAD    */ { false, false, false, false, false, false, true  },
    /* from RETIRED */ { true,  false, false, false, false, false, false },
};

bool laplace_state_transition_valid(const laplace_entity_state_t from,
                                     const laplace_entity_state_t to) {
    if ((uint32_t)from >= LAPLACE_STATE_COUNT_ || (uint32_t)to >= LAPLACE_STATE_COUNT_) {
        return false;
    }
    return laplace__state_transitions[(uint32_t)from][(uint32_t)to];
}

const char* laplace_state_name(const laplace_entity_state_t state) {
    switch (state) {
        case LAPLACE_STATE_FREE:    return "FREE";
        case LAPLACE_STATE_PENDING: return "PENDING";
        case LAPLACE_STATE_READY:   return "READY";
        case LAPLACE_STATE_ACTIVE:  return "ACTIVE";
        case LAPLACE_STATE_MASKED:  return "MASKED";
        case LAPLACE_STATE_DEAD:    return "DEAD";
        case LAPLACE_STATE_RETIRED: return "RETIRED";
        default:                    return "UNKNOWN";
    }
}
