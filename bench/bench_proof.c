#include <stdint.h>
#include <string.h>

#include "bench_harness.h"
#include "laplace/arena.h"
#include "laplace/proof.h"

#define BENCH_PROOF_ARENA_SIZE (32u * 1024u * 1024u) /* 32 MB */

static _Alignas(64) uint8_t g_bench_proof_buf[BENCH_PROOF_ARENA_SIZE];

typedef struct bench_proof_ctx {
    laplace_arena_t       arena;
    laplace_proof_store_t store;

    laplace_proof_symbol_id_t    const_id;
    laplace_proof_symbol_id_t    var_ids[4];
    laplace_proof_expr_id_t      expr_ids[4];
    laplace_proof_frame_id_t     frame_id;
    laplace_proof_hyp_id_t       hyp_ids[2];
    laplace_proof_assertion_id_t assertion_id;
    laplace_proof_theorem_id_t   theorem_id;

    volatile uint32_t sink_u32;
    volatile uint8_t  sink_u8;
} bench_proof_ctx_t;

static bench_proof_ctx_t g_bench_proof;

static void bench_proof_setup(void) {
    bench_proof_ctx_t* const ctx = &g_bench_proof;
    memset(ctx, 0, sizeof(*ctx));

    (void)laplace_arena_init(&ctx->arena,
        g_bench_proof_buf, sizeof(g_bench_proof_buf));
    (void)laplace_proof_store_init(&ctx->store, &ctx->arena);

    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &ctx->const_id);
    for (uint32_t i = 0u; i < 4u; ++i) {
        (void)laplace_proof_import_symbol(&ctx->store,
            LAPLACE_PROOF_SYMBOL_VARIABLE, i + 1u, 0u, &ctx->var_ids[i]);
    }

    for (uint32_t i = 0u; i < 4u; ++i) {
        laplace_proof_symbol_id_t tokens[] = {ctx->var_ids[i]};
        (void)laplace_proof_import_expr(&ctx->store,
            ctx->const_id, tokens, 1u, &ctx->expr_ids[i]);
    }

    laplace_proof_frame_desc_t fdesc = {0};
    (void)laplace_proof_import_frame(&ctx->store, &fdesc, &ctx->frame_id);

    (void)laplace_proof_import_float_hyp(&ctx->store,
        ctx->frame_id, ctx->expr_ids[0], &ctx->hyp_ids[0]);
    (void)laplace_proof_import_essential_hyp(&ctx->store,
        ctx->frame_id, ctx->expr_ids[1], &ctx->hyp_ids[1]);

    laplace_proof_assertion_desc_t adesc = {
        .kind = LAPLACE_PROOF_ASSERTION_THEOREM,
        .frame_id = ctx->frame_id,
        .conclusion_id = ctx->expr_ids[0],
        .mand_float_offset = ctx->hyp_ids[0],
        .mand_float_count = 1u,
    };
    (void)laplace_proof_import_assertion(&ctx->store, &adesc, &ctx->assertion_id);

    laplace_proof_assertion_desc_t ax_desc = {
        .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
        .frame_id = ctx->frame_id,
        .conclusion_id = ctx->expr_ids[0],
    };
    laplace_proof_assertion_id_t ax_id;
    (void)laplace_proof_import_assertion(&ctx->store, &ax_desc, &ax_id);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP, .ref_id = ctx->hyp_ids[0]},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION, .ref_id = ax_id},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = ctx->assertion_id,
        .steps = steps,
        .step_count = 2u,
        .flags = 0u,
    };
    (void)laplace_proof_import_theorem(&ctx->store, &tdesc, &ctx->theorem_id);
}

static void bench_proof_symbol_import(void* const context) {
    bench_proof_ctx_t* const ctx = (bench_proof_ctx_t*)context;

    laplace_proof_store_reset(&ctx->store);
    laplace_proof_symbol_id_t id;
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &id);
    ctx->sink_u32 = id;
}

static void bench_proof_expr_import(void* const context) {
    bench_proof_ctx_t* const ctx = (bench_proof_ctx_t*)context;

    laplace_proof_store_reset(&ctx->store);
    laplace_proof_symbol_id_t cid;
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &cid);
    laplace_proof_symbol_id_t vid;
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_VARIABLE, 1u, 0u, &vid);

    laplace_proof_symbol_id_t tokens[] = {vid};
    laplace_proof_expr_id_t eid;
    (void)laplace_proof_import_expr(&ctx->store,
        cid, tokens, 1u, &eid);
    ctx->sink_u32 = eid;
}

static void bench_proof_expr_query(void* const context) {
    bench_proof_ctx_t* const ctx = (bench_proof_ctx_t*)context;
    const laplace_proof_expr_t* e = laplace_proof_get_expr(
        &ctx->store, ctx->expr_ids[0]);
    ctx->sink_u32 = e ? e->token_count : 0u;
}

static void bench_proof_assertion_import(void* const context) {
    bench_proof_ctx_t* const ctx = (bench_proof_ctx_t*)context;

    laplace_proof_store_reset(&ctx->store);

    laplace_proof_symbol_id_t cid, vid;
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &cid);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_VARIABLE, 1u, 0u, &vid);

    laplace_proof_symbol_id_t tokens[] = {vid};
    laplace_proof_expr_id_t eid;
    (void)laplace_proof_import_expr(&ctx->store, cid, tokens, 1u, &eid);

    laplace_proof_frame_desc_t fdesc = {0};
    laplace_proof_frame_id_t fid;
    (void)laplace_proof_import_frame(&ctx->store, &fdesc, &fid);

    laplace_proof_assertion_desc_t adesc = {
        .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
        .frame_id = fid,
        .conclusion_id = eid,
    };
    laplace_proof_assertion_id_t aid;
    (void)laplace_proof_import_assertion(&ctx->store, &adesc, &aid);
    ctx->sink_u32 = aid;
}

static void bench_proof_theorem_import_fn(void* const context) {
    bench_proof_ctx_t* const ctx = (bench_proof_ctx_t*)context;

    laplace_proof_store_reset(&ctx->store);

    laplace_proof_symbol_id_t cid, vid;
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &cid);
    (void)laplace_proof_import_symbol(&ctx->store,
        LAPLACE_PROOF_SYMBOL_VARIABLE, 1u, 0u, &vid);

    laplace_proof_symbol_id_t tokens[] = {vid};
    laplace_proof_expr_id_t eid;
    (void)laplace_proof_import_expr(&ctx->store, cid, tokens, 1u, &eid);

    laplace_proof_frame_desc_t fdesc = {0};
    laplace_proof_frame_id_t fid;
    (void)laplace_proof_import_frame(&ctx->store, &fdesc, &fid);

    laplace_proof_expr_id_t fh_eid;
    (void)laplace_proof_import_expr(&ctx->store, cid, tokens, 1u, &fh_eid);
    laplace_proof_hyp_id_t fhid;
    (void)laplace_proof_import_float_hyp(&ctx->store, fid, fh_eid, &fhid);

    laplace_proof_assertion_desc_t ax_desc = {
        .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
        .frame_id = fid,
        .conclusion_id = eid,
    };
    laplace_proof_assertion_id_t ax_id;
    (void)laplace_proof_import_assertion(&ctx->store, &ax_desc, &ax_id);

    laplace_proof_assertion_desc_t thm_desc = {
        .kind = LAPLACE_PROOF_ASSERTION_THEOREM,
        .frame_id = fid,
        .conclusion_id = eid,
    };
    laplace_proof_assertion_id_t thm_assn_id;
    (void)laplace_proof_import_assertion(&ctx->store, &thm_desc, &thm_assn_id);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP, .ref_id = fhid},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION, .ref_id = ax_id},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps = steps,
        .step_count = 2u,
    };
    laplace_proof_theorem_id_t tid;
    (void)laplace_proof_import_theorem(&ctx->store, &tdesc, &tid);
    ctx->sink_u32 = tid;
}

void laplace_bench_proof(void) {
    bench_proof_setup();

    const laplace_bench_case_t benches[] = {
        {"proof_symbol_import", bench_proof_symbol_import,
         &g_bench_proof, 1000000u},
        {"proof_expr_import", bench_proof_expr_import,
         &g_bench_proof, 500000u},
        {"proof_expr_query", bench_proof_expr_query,
         &g_bench_proof, 5000000u},
        {"proof_assertion_import", bench_proof_assertion_import,
         &g_bench_proof, 500000u},
        {"proof_theorem_import", bench_proof_theorem_import_fn,
         &g_bench_proof, 200000u},
    };

    const size_t count = sizeof(benches) / sizeof(benches[0]);
    for (size_t i = 0u; i < count; ++i) {
        laplace_bench_run_case(&benches[i]);
    }
}
