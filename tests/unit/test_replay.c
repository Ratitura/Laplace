#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "laplace/config.h"
#include "laplace/hv.h"
#include "laplace/replay.h"
#include "laplace/version.h"
#include "test_harness.h"

static int test_replay_init(void) {
    laplace_replay_metadata_t meta;
    const laplace_error_t rc = laplace_replay_init(&meta, 12345u);
    LAPLACE_TEST_ASSERT(rc == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(meta.session_id == 12345u);
    LAPLACE_TEST_ASSERT(meta.version_major == laplace_version_major());
    LAPLACE_TEST_ASSERT(meta.version_minor == laplace_version_minor());
    LAPLACE_TEST_ASSERT(meta.version_patch == laplace_version_patch());
    LAPLACE_TEST_ASSERT(meta.hv_dimension == LAPLACE_HV_DIM);
    LAPLACE_TEST_ASSERT(meta.hv_words == LAPLACE_HV_WORDS);
    LAPLACE_TEST_ASSERT(meta.is_open == true);
    LAPLACE_TEST_ASSERT(meta.is_closed == false);
    LAPLACE_TEST_ASSERT(meta.seed_root == 0u);
    LAPLACE_TEST_ASSERT(meta.start_sequence == 0u);
    LAPLACE_TEST_ASSERT(meta.end_sequence == 0u);
    LAPLACE_TEST_ASSERT(meta.first_transport_correlation == 0u);
    LAPLACE_TEST_ASSERT(meta.last_transport_correlation == 0u);

    LAPLACE_TEST_ASSERT(meta.backend_name[0] != '\0');
    return 0;
}

static int test_replay_init_null(void) {
    LAPLACE_TEST_ASSERT(laplace_replay_init(NULL, 0u) == LAPLACE_ERR_INVALID_ARGUMENT);
    return 0;
}

static int test_replay_exec_config(void) {
    laplace_replay_metadata_t meta;
    laplace_replay_init(&meta, 1u);

    laplace_replay_capture_exec_config(&meta, 2u, 1000u, 500u, true);
    LAPLACE_TEST_ASSERT(meta.exec_mode == 2u);
    LAPLACE_TEST_ASSERT(meta.max_steps == 1000u);
    LAPLACE_TEST_ASSERT(meta.max_derivations == 500u);
    LAPLACE_TEST_ASSERT(meta.semi_naive_enabled == true);
    return 0;
}

static int test_replay_seed(void) {
    laplace_replay_metadata_t meta;
    laplace_replay_init(&meta, 2u);

    laplace_replay_set_seed(&meta, 0xDEADBEEFCAFEu);
    LAPLACE_TEST_ASSERT(meta.seed_root == 0xDEADBEEFCAFEu);
    return 0;
}

static int test_replay_session_lifecycle(void) {
    laplace_replay_metadata_t meta;
    laplace_replay_init(&meta, 3u);

    LAPLACE_TEST_ASSERT(laplace_replay_is_open(&meta) == true);

    laplace_replay_mark_start(&meta, 100u);
    LAPLACE_TEST_ASSERT(meta.start_sequence == 100u);
    LAPLACE_TEST_ASSERT(meta.wall_start_ns != 0u);

    laplace_replay_mark_end(&meta, 200u);
    LAPLACE_TEST_ASSERT(meta.end_sequence == 200u);
    LAPLACE_TEST_ASSERT(meta.wall_end_ns != 0u);
    LAPLACE_TEST_ASSERT(meta.is_open == false);
    LAPLACE_TEST_ASSERT(meta.is_closed == true);
    LAPLACE_TEST_ASSERT(laplace_replay_is_open(&meta) == false);
    return 0;
}

static int test_replay_transport_correlation(void) {
    laplace_replay_metadata_t meta;
    laplace_replay_init(&meta, 4u);

    laplace_replay_update_transport_correlation(&meta, 500u);
    LAPLACE_TEST_ASSERT(meta.first_transport_correlation == 500u);
    LAPLACE_TEST_ASSERT(meta.last_transport_correlation == 500u);

    laplace_replay_update_transport_correlation(&meta, 600u);
    LAPLACE_TEST_ASSERT(meta.first_transport_correlation == 500u);
    LAPLACE_TEST_ASSERT(meta.last_transport_correlation == 600u);

    laplace_replay_update_transport_correlation(&meta, 700u);
    LAPLACE_TEST_ASSERT(meta.first_transport_correlation == 500u);
    LAPLACE_TEST_ASSERT(meta.last_transport_correlation == 700u);
    return 0;
}

static int test_replay_get_session_id(void) {
    laplace_replay_metadata_t meta;
    laplace_replay_init(&meta, 42u);

    LAPLACE_TEST_ASSERT(laplace_replay_get_session_id(&meta) == 42u);
    return 0;
}

int laplace_test_replay(void) {
    typedef struct { const char* name; int (*fn)(void); } sub_t;
    const sub_t subs[] = {
        {"replay_init",                  test_replay_init},
        {"replay_init_null",             test_replay_init_null},
        {"replay_exec_config",           test_replay_exec_config},
        {"replay_seed",                  test_replay_seed},
        {"replay_session_lifecycle",     test_replay_session_lifecycle},
        {"replay_transport_correlation", test_replay_transport_correlation},
        {"replay_get_session_id",        test_replay_get_session_id},
    };
    for (size_t i = 0; i < sizeof(subs) / sizeof(subs[0]); ++i) {
        const int r = subs[i].fn();
        if (r != 0) {
            fprintf(stderr, "  SUBTEST FAIL: %s\n", subs[i].name);
            return 1;
        }
    }
    return 0;
}
