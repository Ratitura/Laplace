#include <stdint.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/arena.h"
#include "laplace/proof.h"
#include "laplace/proof_search.h"
#include "laplace/derive.h"

#define BENCH_SEARCH_ARENA_SIZE (32u * 1024u * 1024u)

static _Alignas(64) uint8_t g_bench_search_buf[BENCH_SEARCH_ARENA_SIZE];

typedef struct bench_search_ctx {
    laplace_arena_t                   arena;
    laplace_proof_store_t             store;
    laplace_proof_search_index_t      index;
    laplace_proof_search_scratch_t    scratch;
    laplace_proof_search_context_t    search;
    laplace_derive_context_t          derive;

    laplace_proof_expr_id_t           e_ph;
    laplace_proof_expr_id_t           e_ps;
    laplace_proof_expr_id_t           e_ph_impl_ps_impl_ph;
    laplace_proof_assertion_id_t      ax_id;
    laplace_proof_assertion_id_t      ax_1;
    laplace_proof_assertion_id_t      ax_mp;

    volatile uint32_t sink_u32;
} bench_search_ctx_t;

static bench_search_ctx_t g_bench_search;

static void bench_search_setup(void) {
    bench_search_ctx_t* const ctx = &g_bench_search;
    memset(ctx, 0, sizeof(*ctx));

    (void)laplace_arena_init(&ctx->arena,
        g_bench_search_buf, sizeof(g_bench_search_buf));
    (void)laplace_proof_store_init(&ctx->store, &ctx->arena);
    (void)laplace_proof_search_init(&ctx->search, &ctx->store,
        &ctx->index, &ctx->scratch);

    laplace_proof_symbol_id_t c_wff, c_lparen, c_rparen, c_implies;
    laplace_proof_symbol_id_t v_ph, v_ps;
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 0, 0, &c_wff);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 1, 0, &c_lparen);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 2, 0, &c_rparen);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 3, 0, &c_implies);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_VARIABLE, 4, 0, &v_ph);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_VARIABLE, 5, 0, &v_ps);

    {
        laplace_proof_symbol_id_t t[] = {v_ph};
        (void)laplace_proof_import_expr(&ctx->store, c_wff, t, 1, &ctx->e_ph);
    }
    {
        laplace_proof_symbol_id_t t[] = {v_ps};
        (void)laplace_proof_import_expr(&ctx->store, c_wff, t, 1, &ctx->e_ps);
    }

    laplace_proof_expr_id_t e_ph_impl_ps;
    {
        laplace_proof_symbol_id_t t[] = {c_lparen, v_ph, c_implies, v_ps, c_rparen};
        (void)laplace_proof_import_expr(&ctx->store, c_wff, t, 5, &e_ph_impl_ps);
    }
    {
        laplace_proof_symbol_id_t t[] = {
            c_lparen, v_ph, c_implies,
            c_lparen, v_ps, c_implies, v_ph, c_rparen,
            c_rparen
        };
        (void)laplace_proof_import_expr(&ctx->store, c_wff, t, 9,
            &ctx->e_ph_impl_ps_impl_ph);
    }

    laplace_proof_frame_desc_t fdesc = {0};
    laplace_proof_symbol_id_t mvars[] = {v_ph, v_ps};
    fdesc.mandatory_vars = mvars;
    fdesc.mandatory_var_count = 2;
    laplace_proof_frame_id_t frame;
    (void)laplace_proof_import_frame(&ctx->store, &fdesc, &frame);

    laplace_proof_hyp_id_t hyp_ph, hyp_ps;
    (void)laplace_proof_import_float_hyp(&ctx->store, frame, ctx->e_ph, &hyp_ph);
    (void)laplace_proof_import_float_hyp(&ctx->store, frame, ctx->e_ps, &hyp_ps);

    laplace_proof_hyp_id_t ess_ph, ess_ph_impl_ps;
    (void)laplace_proof_import_essential_hyp(&ctx->store, frame, ctx->e_ph, &ess_ph);
    (void)laplace_proof_import_essential_hyp(&ctx->store, frame, e_ph_impl_ps, &ess_ph_impl_ps);

    {
        laplace_proof_assertion_desc_t d = {
            .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
            .frame_id = frame,
            .conclusion_id = ctx->e_ph,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 1,
        };
        (void)laplace_proof_import_assertion(&ctx->store, &d, &ctx->ax_id);
    }
    {
        laplace_proof_assertion_desc_t d = {
            .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
            .frame_id = frame,
            .conclusion_id = ctx->e_ph_impl_ps_impl_ph,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 2,
        };
        (void)laplace_proof_import_assertion(&ctx->store, &d, &ctx->ax_1);
    }
    {
        laplace_proof_assertion_desc_t d = {
            .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
            .frame_id = frame,
            .conclusion_id = ctx->e_ps,
            .mand_float_offset = hyp_ph,
            .mand_float_count = 2,
            .mand_ess_offset = ess_ph,
            .mand_ess_count = 2,
        };
        (void)laplace_proof_import_assertion(&ctx->store, &d, &ctx->ax_mp);
    }

    (void)laplace_proof_search_build_index(&ctx->search);

    laplace_derive_context_init(&ctx->derive,
        NULL, NULL, NULL, NULL,
        (struct laplace_proof_store*)&ctx->store, NULL,
        (struct laplace_proof_search_context*)&ctx->search);
}

static void bench_search_build_index(void* const context) {
    bench_search_ctx_t* const ctx = (bench_search_ctx_t*)context;
    laplace_proof_search_reset(&ctx->search);
    (void)laplace_proof_search_build_index(&ctx->search);
    ctx->sink_u32 = laplace_proof_search_index_count(&ctx->search);
}

static void bench_search_query_candidates(void* const context) {
    bench_search_ctx_t* const ctx = (bench_search_ctx_t*)context;
    laplace_proof_search_candidate_buf_t buf;
    (void)laplace_proof_search_query_candidates(
        &ctx->search, ctx->e_ph, &buf);
    ctx->sink_u32 = buf.count;
}

static void bench_search_try_identity(void* const context) {
    bench_search_ctx_t* const ctx = (bench_search_ctx_t*)context;
    laplace_proof_search_try_result_t tr;
    (void)laplace_proof_search_try_assertion(
        &ctx->search, ctx->e_ph, ctx->ax_id, &tr);
    ctx->sink_u32 = (uint32_t)tr.status;
}

static void bench_search_try_ax1(void* const context) {
    bench_search_ctx_t* const ctx = (bench_search_ctx_t*)context;
    laplace_proof_search_try_result_t tr;
    (void)laplace_proof_search_try_assertion(
        &ctx->search, ctx->e_ph_impl_ps_impl_ph, ctx->ax_1, &tr);
    ctx->sink_u32 = (uint32_t)tr.status;
}

static void bench_search_try_mp(void* const context) {
    bench_search_ctx_t* const ctx = (bench_search_ctx_t*)context;
    laplace_proof_search_try_result_t tr;
    (void)laplace_proof_search_try_assertion(
        &ctx->search, ctx->e_ps, ctx->ax_mp, &tr);
    ctx->sink_u32 = tr.subgoal_count;
}

static void bench_search_state_expand(void* const context) {
    bench_search_ctx_t* const ctx = (bench_search_ctx_t*)context;
    laplace_proof_search_state_t state;
    (void)laplace_proof_search_state_init(&state, ctx->e_ph);
    laplace_proof_search_try_result_t tr;
    (void)laplace_proof_search_state_expand(
        &ctx->search, &state, 0, ctx->ax_id, &tr);
    ctx->sink_u32 = (uint32_t)state.status;
}

static void bench_search_derive_try(void* const context) {
    bench_search_ctx_t* const ctx = (bench_search_ctx_t*)context;

    laplace_derive_action_t action;
    memset(&action, 0, sizeof(action));
    action.api_version = LAPLACE_DERIVE_API_VERSION;
    action.kernel      = LAPLACE_KERNEL_PROOF;
    action.action      = LAPLACE_DERIVE_ACTION_PROOF_TRY_ASSERTION;
    action.payload.proof_search_try.goal_expr_id = ctx->e_ph;
    action.payload.proof_search_try.assertion_id = ctx->ax_id;

    laplace_derive_result_t result;
    laplace_derive_dispatch(&ctx->derive, &action, &result);
    ctx->sink_u32 = result.payload.proof_search.search_status;
}

void laplace_bench_proof_search(void) {
    bench_search_setup();

    const laplace_bench_case_t benches[] = {
        {"proof_search_build_index", bench_search_build_index,
         &g_bench_search, 2000000u},
        {"proof_search_query_candidates", bench_search_query_candidates,
         &g_bench_search, 5000000u},
        {"proof_search_try_identity", bench_search_try_identity,
         &g_bench_search, 5000000u},
        {"proof_search_try_ax1", bench_search_try_ax1,
         &g_bench_search, 2000000u},
        {"proof_search_try_mp", bench_search_try_mp,
         &g_bench_search, 2000000u},
        {"proof_search_state_expand", bench_search_state_expand,
         &g_bench_search, 2000000u},
        {"proof_search_derive_try", bench_search_derive_try,
         &g_bench_search, 2000000u},
    };

    const size_t count = sizeof(benches) / sizeof(benches[0]);
    for (size_t i = 0u; i < count; ++i) {
        laplace_bench_run_case(&benches[i]);
    }
}
