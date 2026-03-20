#include <string.h>

#include "test_harness.h"
#include "laplace/proof.h"
#include "laplace/proof_search.h"
#include "laplace/derive.h"

#define TEST_SEARCH_ARENA_SIZE (32u * 1024u * 1024u)

static _Alignas(64) uint8_t g_arena_buf[TEST_SEARCH_ARENA_SIZE];

static laplace_arena_t                g_arena;
static laplace_proof_store_t          g_store;
static laplace_proof_search_index_t   g_index;
static laplace_proof_search_scratch_t g_scratch;
static laplace_proof_search_context_t g_search;

typedef struct search_corpus {
    laplace_proof_symbol_id_t c_wff;
    laplace_proof_symbol_id_t c_turnstile;
    laplace_proof_symbol_id_t c_implies;
    laplace_proof_symbol_id_t c_not;
    laplace_proof_symbol_id_t c_lparen;
    laplace_proof_symbol_id_t c_rparen;

    laplace_proof_symbol_id_t v_ph;
    laplace_proof_symbol_id_t v_ps;
    laplace_proof_symbol_id_t v_ch;

    laplace_proof_expr_id_t e_ph;
    laplace_proof_expr_id_t e_ps;
    laplace_proof_expr_id_t e_ch;
    laplace_proof_expr_id_t e_ph_impl_ps;
    laplace_proof_expr_id_t e_ps_impl_ph;
    laplace_proof_expr_id_t e_ph_impl_ps_impl_ph;

    laplace_proof_frame_id_t frame;

    laplace_proof_hyp_id_t hyp_ph;
    laplace_proof_hyp_id_t hyp_ps;
    laplace_proof_hyp_id_t hyp_ch;

    laplace_proof_hyp_id_t ess_ph;
    laplace_proof_hyp_id_t ess_ph_impl_ps;

    laplace_proof_dv_id_t dv_ph_ps;

    laplace_proof_assertion_id_t ax_id;
    laplace_proof_assertion_id_t ax_1;
    laplace_proof_assertion_id_t ax_mp;
    laplace_proof_assertion_id_t ax_id_dv;
} search_corpus_t;

static search_corpus_t g_mc;

static laplace_error_t fixture_init(void) {
    memset(&g_arena, 0, sizeof(g_arena));
    memset(&g_store, 0, sizeof(g_store));
    memset(&g_index, 0, sizeof(g_index));
    memset(&g_scratch, 0, sizeof(g_scratch));
    memset(&g_search, 0, sizeof(g_search));
    laplace_error_t err = laplace_arena_init(&g_arena,
        g_arena_buf, sizeof(g_arena_buf));
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_store_init(&g_store, &g_arena);
    if (err != LAPLACE_OK) return err;
    return laplace_proof_search_init(&g_search, &g_store,
        &g_index, &g_scratch);
}

static laplace_error_t build_corpus(void) {
    memset(&g_mc, 0, sizeof(g_mc));
    laplace_error_t err;

    err = laplace_proof_import_symbol(&g_store, LAPLACE_PROOF_SYMBOL_CONSTANT, 0, 0, &g_mc.c_wff);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(&g_store, LAPLACE_PROOF_SYMBOL_CONSTANT, 1, 0, &g_mc.c_turnstile);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(&g_store, LAPLACE_PROOF_SYMBOL_CONSTANT, 2, 0, &g_mc.c_implies);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(&g_store, LAPLACE_PROOF_SYMBOL_CONSTANT, 3, 0, &g_mc.c_not);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(&g_store, LAPLACE_PROOF_SYMBOL_CONSTANT, 4, 0, &g_mc.c_lparen);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(&g_store, LAPLACE_PROOF_SYMBOL_CONSTANT, 5, 0, &g_mc.c_rparen);
    if (err != LAPLACE_OK) return err;

    err = laplace_proof_import_symbol(&g_store, LAPLACE_PROOF_SYMBOL_VARIABLE, 6, 0, &g_mc.v_ph);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(&g_store, LAPLACE_PROOF_SYMBOL_VARIABLE, 7, 0, &g_mc.v_ps);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_symbol(&g_store, LAPLACE_PROOF_SYMBOL_VARIABLE, 8, 0, &g_mc.v_ch);
    if (err != LAPLACE_OK) return err;

    {
        laplace_proof_symbol_id_t tokens[] = {g_mc.v_ph};
        err = laplace_proof_import_expr(&g_store, g_mc.c_wff, tokens, 1, &g_mc.e_ph);
        if (err != LAPLACE_OK) return err;
    }
    {
        laplace_proof_symbol_id_t tokens[] = {g_mc.v_ps};
        err = laplace_proof_import_expr(&g_store, g_mc.c_wff, tokens, 1, &g_mc.e_ps);
        if (err != LAPLACE_OK) return err;
    }
    {
        laplace_proof_symbol_id_t tokens[] = {g_mc.v_ch};
        err = laplace_proof_import_expr(&g_store, g_mc.c_wff, tokens, 1, &g_mc.e_ch);
        if (err != LAPLACE_OK) return err;
    }
    {
        laplace_proof_symbol_id_t tokens[] = {
            g_mc.c_lparen, g_mc.v_ph, g_mc.c_implies, g_mc.v_ps, g_mc.c_rparen
        };
        err = laplace_proof_import_expr(&g_store, g_mc.c_wff, tokens, 5, &g_mc.e_ph_impl_ps);
        if (err != LAPLACE_OK) return err;
    }
    {
        laplace_proof_symbol_id_t tokens[] = {
            g_mc.c_lparen, g_mc.v_ps, g_mc.c_implies, g_mc.v_ph, g_mc.c_rparen
        };
        err = laplace_proof_import_expr(&g_store, g_mc.c_wff, tokens, 5, &g_mc.e_ps_impl_ph);
        if (err != LAPLACE_OK) return err;
    }
    {
        laplace_proof_symbol_id_t tokens[] = {
            g_mc.c_lparen, g_mc.v_ph, g_mc.c_implies,
            g_mc.c_lparen, g_mc.v_ps, g_mc.c_implies, g_mc.v_ph, g_mc.c_rparen,
            g_mc.c_rparen
        };
        err = laplace_proof_import_expr(&g_store, g_mc.c_wff, tokens, 9, &g_mc.e_ph_impl_ps_impl_ph);
        if (err != LAPLACE_OK) return err;
    }

    {
        laplace_proof_frame_desc_t fdesc;
        memset(&fdesc, 0, sizeof(fdesc));
        laplace_proof_symbol_id_t mvars[] = {g_mc.v_ph, g_mc.v_ps, g_mc.v_ch};
        fdesc.mandatory_vars = mvars;
        fdesc.mandatory_var_count = 3;
        err = laplace_proof_import_frame(&g_store, &fdesc, &g_mc.frame);
        if (err != LAPLACE_OK) return err;
    }

    err = laplace_proof_import_float_hyp(&g_store, g_mc.frame, g_mc.e_ph, &g_mc.hyp_ph);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_float_hyp(&g_store, g_mc.frame, g_mc.e_ps, &g_mc.hyp_ps);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_float_hyp(&g_store, g_mc.frame, g_mc.e_ch, &g_mc.hyp_ch);
    if (err != LAPLACE_OK) return err;

    err = laplace_proof_import_essential_hyp(&g_store, g_mc.frame, g_mc.e_ph, &g_mc.ess_ph);
    if (err != LAPLACE_OK) return err;
    err = laplace_proof_import_essential_hyp(&g_store, g_mc.frame, g_mc.e_ph_impl_ps, &g_mc.ess_ph_impl_ps);
    if (err != LAPLACE_OK) return err;

    err = laplace_proof_import_dv_pair(&g_store, g_mc.frame, g_mc.v_ph, g_mc.v_ps, &g_mc.dv_ph_ps);
    if (err != LAPLACE_OK) return err;

    {
        laplace_proof_assertion_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.kind           = LAPLACE_PROOF_ASSERTION_AXIOM;
        desc.frame_id       = g_mc.frame;
        desc.conclusion_id  = g_mc.e_ph;
        desc.mand_float_offset = g_mc.hyp_ph;
        desc.mand_float_count  = 1;
        err = laplace_proof_import_assertion(&g_store, &desc, &g_mc.ax_id);
        if (err != LAPLACE_OK) return err;
    }

    {
        laplace_proof_assertion_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.kind           = LAPLACE_PROOF_ASSERTION_AXIOM;
        desc.frame_id       = g_mc.frame;
        desc.conclusion_id  = g_mc.e_ph_impl_ps_impl_ph;
        desc.mand_float_offset = g_mc.hyp_ph;
        desc.mand_float_count  = 2;
        err = laplace_proof_import_assertion(&g_store, &desc, &g_mc.ax_1);
        if (err != LAPLACE_OK) return err;
    }

    {
        laplace_proof_assertion_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.kind           = LAPLACE_PROOF_ASSERTION_AXIOM;
        desc.frame_id       = g_mc.frame;
        desc.conclusion_id  = g_mc.e_ps;
        desc.mand_float_offset = g_mc.hyp_ph;
        desc.mand_float_count  = 2;
        desc.mand_ess_offset   = g_mc.ess_ph;
        desc.mand_ess_count    = 2;
        err = laplace_proof_import_assertion(&g_store, &desc, &g_mc.ax_mp);
        if (err != LAPLACE_OK) return err;
    }

    {
        laplace_proof_assertion_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.kind           = LAPLACE_PROOF_ASSERTION_AXIOM;
        desc.frame_id       = g_mc.frame;
        desc.conclusion_id  = g_mc.e_ph;
        desc.mand_float_offset = g_mc.hyp_ph;
        desc.mand_float_count  = 1;
        desc.dv_offset         = g_mc.dv_ph_ps;
        desc.dv_count          = 1;
        err = laplace_proof_import_assertion(&g_store, &desc, &g_mc.ax_id_dv);
        if (err != LAPLACE_OK) return err;
    }

    return LAPLACE_OK;
}

static int test_search_init(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(g_search.initialized == true);
    LAPLACE_TEST_ASSERT(g_search.store == &g_store);
    LAPLACE_TEST_ASSERT(g_search.index == &g_index);
    LAPLACE_TEST_ASSERT(g_search.scratch == &g_scratch);
    return 0;
}

static int test_search_init_null(void) {
    laplace_proof_search_context_t ctx;
    laplace_proof_store_t store;
    memset(&store, 0, sizeof(store));

    LAPLACE_TEST_ASSERT(
        laplace_proof_search_init(NULL, &store, &g_index, &g_scratch)
        == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(
        laplace_proof_search_init(&ctx, NULL, &g_index, &g_scratch)
        == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(
        laplace_proof_search_init(&ctx, &store, NULL, &g_scratch)
        == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(
        laplace_proof_search_init(&ctx, &store, &g_index, NULL)
        == LAPLACE_ERR_INVALID_ARGUMENT);
    return 0;
}

static int test_search_build_index(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(g_index.built == true);
    LAPLACE_TEST_ASSERT(g_index.count == 4u);
    LAPLACE_TEST_ASSERT(laplace_proof_search_index_count(&g_search) == 4u);
    return 0;
}

static int test_search_build_index_empty(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(g_index.built == true);
    LAPLACE_TEST_ASSERT(g_index.count == 0u);
    return 0;
}

static int test_search_query_candidates_by_expr(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_proof_search_candidate_buf_t buf;
    LAPLACE_TEST_ASSERT(laplace_proof_search_query_candidates(
        &g_search, g_mc.e_ph, &buf) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(buf.count >= 2u);
    LAPLACE_TEST_ASSERT(buf.total_matches >= 2u);
    LAPLACE_TEST_ASSERT(buf.truncated == false);
    return 0;
}

static int test_search_query_candidates_typecode(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_proof_search_candidate_buf_t buf;
    LAPLACE_TEST_ASSERT(laplace_proof_search_query_candidates_typecode(
        &g_search, g_mc.c_wff, &buf) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(buf.count == 4u);
    LAPLACE_TEST_ASSERT(buf.total_matches == 4u);
    return 0;
}

static int test_search_query_no_index(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);

    laplace_proof_search_candidate_buf_t buf;
    LAPLACE_TEST_ASSERT(laplace_proof_search_query_candidates(
        &g_search, g_mc.e_ph, &buf) == LAPLACE_ERR_INVALID_STATE);
    return 0;
}

static int test_search_try_identity(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, g_mc.e_ph, g_mc.ax_id, &tr) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_SUCCESS);
    LAPLACE_TEST_ASSERT(tr.subgoal_count == 0u);
    LAPLACE_TEST_ASSERT(tr.assertion_id == g_mc.ax_id);
    return 0;
}

static int test_search_try_ax1(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, g_mc.e_ph_impl_ps_impl_ph, g_mc.ax_1, &tr) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_SUCCESS);
    LAPLACE_TEST_ASSERT(tr.subgoal_count == 0u);
    return 0;
}

static int test_search_try_mp_subgoals(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, g_mc.e_ps, g_mc.ax_mp, &tr) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_SUCCESS);
    LAPLACE_TEST_ASSERT(tr.subgoal_count == 2u);
    LAPLACE_TEST_ASSERT(tr.subgoals[0].expr_id == g_mc.e_ph);
    LAPLACE_TEST_ASSERT(tr.subgoals[1].expr_id == g_mc.e_ph_impl_ps);
    return 0;
}

static int test_search_try_typecode_mismatch(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);

    laplace_proof_symbol_id_t c_set;
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &g_store, LAPLACE_PROOF_SYMBOL_CONSTANT, 100, 0, &c_set) == LAPLACE_OK);

    laplace_proof_expr_id_t e_set_ph;
    {
        laplace_proof_symbol_id_t tokens[] = {g_mc.v_ph};
        LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
            &g_store, c_set, tokens, 1, &e_set_ph) == LAPLACE_OK);
    }

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, e_set_ph, g_mc.ax_id, &tr) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_TYPECODE_MISMATCH);
    return 0;
}

static int test_search_try_unification_conflict(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, g_mc.e_ps_impl_ph, g_mc.ax_1, NULL) == LAPLACE_ERR_INVALID_ARGUMENT);

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, g_mc.e_ps_impl_ph, g_mc.ax_1, &tr) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_UNIFICATION_CONFLICT);
    return 0;
}

static int test_search_try_invalid_goal(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, 9999u, g_mc.ax_id, &tr) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_INVALID_GOAL);
    return 0;
}

static int test_search_try_invalid_assertion(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, g_mc.e_ph, 9999u, &tr) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_INVALID_ASSERTION);
    return 0;
}

static int test_search_state_init(void) {
    laplace_proof_search_state_t state;
    LAPLACE_TEST_ASSERT(laplace_proof_search_state_init(&state, 42u) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(state.root_goal.expr_id == 42u);
    LAPLACE_TEST_ASSERT(state.obligation_count == 1u);
    LAPLACE_TEST_ASSERT(state.obligations[0].goal.expr_id == 42u);
    LAPLACE_TEST_ASSERT(state.obligations[0].depth == 0u);
    LAPLACE_TEST_ASSERT(state.status == LAPLACE_PROOF_SEARCH_STATE_OPEN);
    return 0;
}

static int test_search_state_expand_identity(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_proof_search_state_t state;
    LAPLACE_TEST_ASSERT(laplace_proof_search_state_init(&state, g_mc.e_ph) == LAPLACE_OK);

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_state_expand(
        &g_search, &state, 0, g_mc.ax_id, &tr) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_SUCCESS);
    LAPLACE_TEST_ASSERT(state.obligation_count == 0u);
    LAPLACE_TEST_ASSERT(state.status == LAPLACE_PROOF_SEARCH_STATE_PROVED);
    LAPLACE_TEST_ASSERT(state.step_count == 1u);
    return 0;
}

static int test_search_state_expand_mp(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_proof_search_state_t state;
    LAPLACE_TEST_ASSERT(laplace_proof_search_state_init(&state, g_mc.e_ps) == LAPLACE_OK);

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_state_expand(
        &g_search, &state, 0, g_mc.ax_mp, &tr) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_SUCCESS);
    LAPLACE_TEST_ASSERT(state.obligation_count == 2u);
    LAPLACE_TEST_ASSERT(state.obligations[0].goal.expr_id == g_mc.e_ph);
    LAPLACE_TEST_ASSERT(state.obligations[1].goal.expr_id == g_mc.e_ph_impl_ps);
    LAPLACE_TEST_ASSERT(state.status == LAPLACE_PROOF_SEARCH_STATE_OPEN);
    LAPLACE_TEST_ASSERT(state.depth == 1u);
    return 0;
}

static int test_search_state_expand_invalid_index(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_proof_search_state_t state;
    LAPLACE_TEST_ASSERT(laplace_proof_search_state_init(&state, g_mc.e_ph) == LAPLACE_OK);

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_state_expand(
        &g_search, &state, 99u, g_mc.ax_id, &tr) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_INVALID_GOAL);
    return 0;
}

static int test_search_reset(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(g_index.count == 4u);
    LAPLACE_TEST_ASSERT(g_index.built == true);

    laplace_proof_search_reset(&g_search);
    LAPLACE_TEST_ASSERT(g_index.count == 0u);
    LAPLACE_TEST_ASSERT(g_index.built == false);
    return 0;
}

static int test_search_status_strings(void) {
    LAPLACE_TEST_ASSERT(
        laplace_proof_search_status_string(LAPLACE_PROOF_SEARCH_SUCCESS) != NULL);
    LAPLACE_TEST_ASSERT(
        laplace_proof_search_status_string(LAPLACE_PROOF_SEARCH_DV_VIOLATION) != NULL);
    LAPLACE_TEST_ASSERT(
        laplace_proof_search_status_string(LAPLACE_PROOF_SEARCH_STATUS_COUNT_) != NULL);

    const char* s = laplace_proof_search_status_string(LAPLACE_PROOF_SEARCH_SUCCESS);
    LAPLACE_TEST_ASSERT(s[0] == 's');

    const char* u = laplace_proof_search_status_string(LAPLACE_PROOF_SEARCH_STATUS_COUNT_);
    LAPLACE_TEST_ASSERT(u[0] == 'u');
    return 0;
}

static int test_search_type_sizes(void) {
    LAPLACE_TEST_ASSERT(sizeof(laplace_proof_search_goal_t) == 4u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_proof_search_obligation_t) == 16u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_proof_search_subst_entry_t) == 16u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_proof_search_index_key_t) == 16u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_proof_search_index_entry_t) == 20u);
    return 0;
}

static int test_search_derive_build_index(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);

    laplace_derive_context_t dctx;
    laplace_derive_context_init(&dctx,
        NULL, NULL, NULL, NULL,
        &g_store, NULL,
        (struct laplace_proof_search_context*)&g_search);

    laplace_derive_action_t action;
    memset(&action, 0, sizeof(action));
    action.api_version = LAPLACE_DERIVE_API_VERSION;
    action.kernel      = LAPLACE_KERNEL_PROOF;
    action.action      = LAPLACE_DERIVE_ACTION_PROOF_BUILD_INDEX;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&dctx, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_ACK);
    LAPLACE_TEST_ASSERT(g_index.built == true);
    LAPLACE_TEST_ASSERT(g_index.count == 4u);
    return 0;
}

static int test_search_derive_query_candidates(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_derive_context_t dctx;
    laplace_derive_context_init(&dctx,
        NULL, NULL, NULL, NULL,
        &g_store, NULL,
        (struct laplace_proof_search_context*)&g_search);

    laplace_derive_action_t action;
    memset(&action, 0, sizeof(action));
    action.api_version = LAPLACE_DERIVE_API_VERSION;
    action.kernel      = LAPLACE_KERNEL_PROOF;
    action.action      = LAPLACE_DERIVE_ACTION_PROOF_QUERY_CANDIDATES;
    action.payload.proof_search_candidates.goal_expr_id = g_mc.e_ph;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&dctx, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_PROOF_CANDIDATES);
    LAPLACE_TEST_ASSERT(result.payload.proof_candidates.candidate_count >= 2u);
    return 0;
}

static int test_search_derive_try_assertion(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_derive_context_t dctx;
    laplace_derive_context_init(&dctx,
        NULL, NULL, NULL, NULL,
        &g_store, NULL,
        (struct laplace_proof_search_context*)&g_search);

    laplace_derive_action_t action;
    memset(&action, 0, sizeof(action));
    action.api_version = LAPLACE_DERIVE_API_VERSION;
    action.kernel      = LAPLACE_KERNEL_PROOF;
    action.action      = LAPLACE_DERIVE_ACTION_PROOF_TRY_ASSERTION;
    action.payload.proof_search_try.goal_expr_id = g_mc.e_ph;
    action.payload.proof_search_try.assertion_id = g_mc.ax_id;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&dctx, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_OK);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_PROOF_SEARCH);
    LAPLACE_TEST_ASSERT(result.payload.proof_search.search_status ==
                         (uint32_t)LAPLACE_PROOF_SEARCH_SUCCESS);
    LAPLACE_TEST_ASSERT(result.payload.proof_search.subgoal_count == 0u);
    return 0;
}

static int test_search_derive_no_context(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);

    laplace_derive_context_t dctx;
    laplace_derive_context_init(&dctx,
        NULL, NULL, NULL, NULL,
        &g_store, NULL, NULL);

    laplace_derive_action_t action;
    memset(&action, 0, sizeof(action));
    action.api_version = LAPLACE_DERIVE_API_VERSION;
    action.kernel      = LAPLACE_KERNEL_PROOF;
    action.action      = LAPLACE_DERIVE_ACTION_PROOF_BUILD_INDEX;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&dctx, &action, &result);

    LAPLACE_TEST_ASSERT(result.status == LAPLACE_DERIVE_STATUS_UNSUPPORTED_ACTION);
    LAPLACE_TEST_ASSERT(result.result_kind == LAPLACE_DERIVE_RESULT_UNSUPPORTED_ACTION);
    return 0;
}

static int test_search_deterministic(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_build_index(&g_search) == LAPLACE_OK);

    laplace_proof_search_try_result_t tr1, tr2;
    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, g_mc.e_ps, g_mc.ax_mp, &tr1) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, g_mc.e_ps, g_mc.ax_mp, &tr2) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(tr1.status == tr2.status);
    LAPLACE_TEST_ASSERT(tr1.subgoal_count == tr2.subgoal_count);
    for (uint32_t i = 0u; i < tr1.subgoal_count; ++i) {
        LAPLACE_TEST_ASSERT(tr1.subgoals[i].expr_id == tr2.subgoals[i].expr_id);
    }
    return 0;
}

static int test_search_dv_success(void) {
    LAPLACE_TEST_ASSERT(fixture_init() == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(build_corpus() == LAPLACE_OK);

    laplace_proof_search_try_result_t tr;
    LAPLACE_TEST_ASSERT(laplace_proof_search_try_assertion(
        &g_search, g_mc.e_ph, g_mc.ax_id_dv, &tr) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(tr.status == LAPLACE_PROOF_SEARCH_SUCCESS);
    return 0;
}

int laplace_test_proof_search(void) {
    typedef struct {
        const char* name;
        int (*fn)(void);
    } subtest_t;

    const subtest_t subtests[] = {
        {"init",                       test_search_init},
        {"init_null",                  test_search_init_null},
        {"build_index",               test_search_build_index},
        {"build_index_empty",         test_search_build_index_empty},
        {"query_candidates_by_expr",  test_search_query_candidates_by_expr},
        {"query_candidates_typecode", test_search_query_candidates_typecode},
        {"query_no_index",            test_search_query_no_index},
        {"try_identity",              test_search_try_identity},
        {"try_ax1",                   test_search_try_ax1},
        {"try_mp_subgoals",           test_search_try_mp_subgoals},
        {"try_typecode_mismatch",     test_search_try_typecode_mismatch},
        {"try_unification_conflict",  test_search_try_unification_conflict},
        {"try_invalid_goal",          test_search_try_invalid_goal},
        {"try_invalid_assertion",     test_search_try_invalid_assertion},
        {"state_init",                test_search_state_init},
        {"state_expand_identity",     test_search_state_expand_identity},
        {"state_expand_mp",           test_search_state_expand_mp},
        {"state_expand_invalid_idx",  test_search_state_expand_invalid_index},
        {"reset",                     test_search_reset},
        {"status_strings",            test_search_status_strings},
        {"type_sizes",                test_search_type_sizes},
        {"derive_build_index",        test_search_derive_build_index},
        {"derive_query_candidates",   test_search_derive_query_candidates},
        {"derive_try_assertion",      test_search_derive_try_assertion},
        {"derive_no_context",         test_search_derive_no_context},
        {"deterministic",             test_search_deterministic},
        {"dv_success",                test_search_dv_success},
    };

    const size_t count = sizeof(subtests) / sizeof(subtests[0]);
    int failures = 0;

    for (size_t i = 0u; i < count; ++i) {
        const int r = subtests[i].fn();
        if (r != 0) {
            fprintf(stderr, "    FAIL proof_search/%s\n", subtests[i].name);
            ++failures;
        }
    }

    return failures;
}
