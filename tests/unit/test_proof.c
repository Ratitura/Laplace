#include <string.h>

#include "test_harness.h"
#include "laplace/proof.h"
#include "laplace/proof_artifact.h"

#define TEST_PROOF_ARENA_SIZE (32u * 1024u * 1024u)  /* 32 MB */

static _Alignas(64) uint8_t g_proof_arena_buf[TEST_PROOF_ARENA_SIZE];

typedef struct test_proof_fixture {
    laplace_arena_t        arena;
    laplace_proof_store_t  store;
} test_proof_fixture_t;

static laplace_error_t test_proof_fixture_init(test_proof_fixture_t* f) {
    memset(f, 0, sizeof(*f));
    laplace_error_t err = laplace_arena_init(&f->arena,
        g_proof_arena_buf, sizeof(g_proof_arena_buf));
    if (err != LAPLACE_OK) return err;
    return laplace_proof_store_init(&f->store, &f->arena);
}

static laplace_error_t test_import_symbols(
    laplace_proof_store_t* store,
    uint32_t num_constants,
    uint32_t num_variables,
    laplace_proof_symbol_id_t* first_const_id,
    laplace_proof_symbol_id_t* first_var_id) {

    laplace_proof_symbol_id_t cid = 0u, vid = 0u;
    for (uint32_t i = 0u; i < num_constants; ++i) {
        laplace_proof_symbol_id_t id;
        laplace_error_t err = laplace_proof_import_symbol(
            store, LAPLACE_PROOF_SYMBOL_CONSTANT, i, 0u, &id);
        if (err != LAPLACE_OK) return err;
        if (i == 0u) cid = id;
    }
    for (uint32_t i = 0u; i < num_variables; ++i) {
        laplace_proof_symbol_id_t id;
        laplace_error_t err = laplace_proof_import_symbol(
            store, LAPLACE_PROOF_SYMBOL_VARIABLE, num_constants + i, 0u, &id);
        if (err != LAPLACE_OK) return err;
        if (i == 0u) vid = id;
    }
    if (first_const_id) *first_const_id = cid;
    if (first_var_id) *first_var_id = vid;
    return LAPLACE_OK;
}

static int test_proof_store_init(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(f.store.initialized == true);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_count(&f.store) == 0u);
    LAPLACE_TEST_ASSERT(laplace_proof_expr_count(&f.store) == 0u);
    LAPLACE_TEST_ASSERT(laplace_proof_frame_count(&f.store) == 0u);
    LAPLACE_TEST_ASSERT(laplace_proof_hyp_count(&f.store) == 0u);
    LAPLACE_TEST_ASSERT(laplace_proof_assertion_count(&f.store) == 0u);
    LAPLACE_TEST_ASSERT(laplace_proof_dv_pair_count(&f.store) == 0u);
    LAPLACE_TEST_ASSERT(laplace_proof_theorem_count(&f.store) == 0u);
    return 0;
}

static int test_proof_store_init_null(void) {
    laplace_arena_t arena;
    laplace_proof_store_t store;
    LAPLACE_TEST_ASSERT(laplace_proof_store_init(NULL, &arena) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_proof_store_init(&store, NULL) == LAPLACE_ERR_INVALID_ARGUMENT);
    return 0;
}

static int test_proof_store_reset(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t sid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &sid) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_count(&f.store) == 1u);

    laplace_proof_store_reset(&f.store);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_count(&f.store) == 0u);
    LAPLACE_TEST_ASSERT(laplace_proof_expr_count(&f.store) == 0u);
    return 0;
}

static int test_proof_symbol_import(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t id1, id2, id3;

    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &id1) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(id1 == 1u);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_count(&f.store) == 1u);

    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_VARIABLE, 1u, 0u, &id2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(id2 == 2u);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_count(&f.store) == 2u);

    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_CONSTANT, 2u, 0u, &id3) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(id3 == 3u);

    const laplace_proof_symbol_t* s1 = laplace_proof_get_symbol(&f.store, id1);
    LAPLACE_TEST_ASSERT(s1 != NULL);
    LAPLACE_TEST_ASSERT(s1->kind == LAPLACE_PROOF_SYMBOL_CONSTANT);
    LAPLACE_TEST_ASSERT(s1->compiler_index == 0u);

    const laplace_proof_symbol_t* s2 = laplace_proof_get_symbol(&f.store, id2);
    LAPLACE_TEST_ASSERT(s2 != NULL);
    LAPLACE_TEST_ASSERT(s2->kind == LAPLACE_PROOF_SYMBOL_VARIABLE);

    LAPLACE_TEST_ASSERT(laplace_proof_get_symbol(&f.store, 0u) == NULL);
    LAPLACE_TEST_ASSERT(laplace_proof_get_symbol(&f.store, 999u) == NULL);

    return 0;
}

static int test_proof_symbol_kind_checks(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &cid) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_VARIABLE, 1u, 0u, &vid) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_proof_symbol_is_constant(&f.store, cid) == true);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_is_variable(&f.store, cid) == false);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_is_constant(&f.store, vid) == false);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_is_variable(&f.store, vid) == true);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_is_valid(&f.store, cid) == true);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_is_valid(&f.store, 0u) == false);

    return 0;
}

static int test_proof_symbol_invalid_kind(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_INVALID, 0u, 0u, &id) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, (laplace_proof_symbol_kind_t)99u, 0u, 0u, &id) == LAPLACE_ERR_INVALID_ARGUMENT);
    LAPLACE_TEST_ASSERT(laplace_proof_symbol_count(&f.store) == 0u);

    return 0;
}

static int test_proof_expr_import(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t wff, x, y, arrow;
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &wff) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_VARIABLE, 1u, 0u, &x) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_VARIABLE, 2u, 0u, &y) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_CONSTANT, 3u, 0u, &arrow) == LAPLACE_OK);

    laplace_proof_symbol_id_t tokens[] = {arrow, x, y};
    laplace_proof_expr_id_t eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, wff, tokens, 3u, &eid) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(eid == 1u);
    LAPLACE_TEST_ASSERT(laplace_proof_expr_count(&f.store) == 1u);

    const laplace_proof_expr_t* e = laplace_proof_get_expr(&f.store, eid);
    LAPLACE_TEST_ASSERT(e != NULL);
    LAPLACE_TEST_ASSERT(e->typecode == wff);
    LAPLACE_TEST_ASSERT(e->token_count == 3u);

    const laplace_proof_symbol_id_t* out_tokens;
    uint32_t out_count;
    LAPLACE_TEST_ASSERT(laplace_proof_get_expr_tokens(
        &f.store, eid, &out_tokens, &out_count) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(out_count == 3u);
    LAPLACE_TEST_ASSERT(out_tokens[0] == arrow);
    LAPLACE_TEST_ASSERT(out_tokens[1] == x);
    LAPLACE_TEST_ASSERT(out_tokens[2] == y);

    laplace_proof_expr_id_t eid2;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, wff, NULL, 0u, &eid2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(eid2 == 2u);
    const laplace_proof_expr_t* e2 = laplace_proof_get_expr(&f.store, eid2);
    LAPLACE_TEST_ASSERT(e2 != NULL);
    LAPLACE_TEST_ASSERT(e2->token_count == 0u);

    return 0;
}

static int test_proof_expr_invalid_typecode(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t var_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_VARIABLE, 0u, 0u, &var_id) == LAPLACE_OK);

    laplace_proof_expr_id_t eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, var_id, NULL, 0u, &eid) == LAPLACE_ERR_INVALID_ARGUMENT);

    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, 999u, NULL, 0u, &eid) == LAPLACE_ERR_INVALID_ARGUMENT);

    return 0;
}

static int test_proof_expr_invalid_token(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &cid) == LAPLACE_OK);

    laplace_proof_symbol_id_t bad_tokens[] = {999u};
    laplace_proof_expr_id_t eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, bad_tokens, 1u, &eid) == LAPLACE_ERR_INVALID_ARGUMENT);

    return 0;
}

static int test_proof_frame_import(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 2u, 3u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_frame_desc_t desc0 = {
        .parent_id = LAPLACE_PROOF_FRAME_ID_INVALID,
        .mandatory_vars = NULL,
        .mandatory_var_count = 0u
    };
    laplace_proof_frame_id_t fid0;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &desc0, &fid0) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(fid0 == 1u);

    laplace_proof_symbol_id_t mvars[] = {vid, vid + 1u};
    laplace_proof_frame_desc_t desc1 = {
        .parent_id = fid0,
        .mandatory_vars = mvars,
        .mandatory_var_count = 2u
    };
    laplace_proof_frame_id_t fid1;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &desc1, &fid1) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(fid1 == 2u);

    const laplace_proof_frame_t* fr0 = laplace_proof_get_frame(&f.store, fid0);
    LAPLACE_TEST_ASSERT(fr0 != NULL);
    LAPLACE_TEST_ASSERT(fr0->parent_id == LAPLACE_PROOF_FRAME_ID_INVALID);

    const laplace_proof_frame_t* fr1 = laplace_proof_get_frame(&f.store, fid1);
    LAPLACE_TEST_ASSERT(fr1 != NULL);
    LAPLACE_TEST_ASSERT(fr1->parent_id == fid0);
    LAPLACE_TEST_ASSERT(fr1->mandatory_var_count == 2u);

    LAPLACE_TEST_ASSERT(laplace_proof_frame_count(&f.store) == 2u);

    return 0;
}

static int test_proof_frame_invalid_parent(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_frame_desc_t desc = {
        .parent_id = 999u,
        .mandatory_vars = NULL,
        .mandatory_var_count = 0u
    };
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &desc, &fid) == LAPLACE_ERR_INVALID_ARGUMENT);

    return 0;
}

static int test_proof_float_hyp_import(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 1u, 2u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_symbol_id_t tokens[] = {vid};
    laplace_proof_expr_id_t eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, tokens, 1u, &eid) == LAPLACE_OK);

    laplace_proof_frame_desc_t fdesc = {
        .parent_id = LAPLACE_PROOF_FRAME_ID_INVALID,
        .mandatory_vars = NULL,
        .mandatory_var_count = 0u
    };
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &fdesc, &fid) == LAPLACE_OK);

    laplace_proof_hyp_id_t hid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_float_hyp(
        &f.store, fid, eid, &hid) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(hid == 1u);

    const laplace_proof_hyp_t* h = laplace_proof_get_hyp(&f.store, hid);
    LAPLACE_TEST_ASSERT(h != NULL);
    LAPLACE_TEST_ASSERT(h->kind == LAPLACE_PROOF_HYP_FLOATING);
    LAPLACE_TEST_ASSERT(h->expr_id == eid);
    LAPLACE_TEST_ASSERT(h->frame_id == fid);
    LAPLACE_TEST_ASSERT(h->order == 0u);

    const laplace_proof_frame_t* fr = laplace_proof_get_frame(&f.store, fid);
    LAPLACE_TEST_ASSERT(fr->float_hyp_count == 1u);
    LAPLACE_TEST_ASSERT(fr->float_hyp_offset == hid);

    return 0;
}

static int test_proof_essential_hyp_import(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 2u, 2u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_symbol_id_t tokens[] = {vid, vid + 1u};
    laplace_proof_expr_id_t eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, tokens, 2u, &eid) == LAPLACE_OK);

    laplace_proof_frame_desc_t fdesc = {
        .parent_id = LAPLACE_PROOF_FRAME_ID_INVALID,
        .mandatory_vars = NULL,
        .mandatory_var_count = 0u
    };
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &fdesc, &fid) == LAPLACE_OK);

    laplace_proof_hyp_id_t hid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_essential_hyp(
        &f.store, fid, eid, &hid) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(hid == 1u);

    const laplace_proof_hyp_t* h = laplace_proof_get_hyp(&f.store, hid);
    LAPLACE_TEST_ASSERT(h != NULL);
    LAPLACE_TEST_ASSERT(h->kind == LAPLACE_PROOF_HYP_ESSENTIAL);
    LAPLACE_TEST_ASSERT(h->expr_id == eid);

    const laplace_proof_frame_t* fr = laplace_proof_get_frame(&f.store, fid);
    LAPLACE_TEST_ASSERT(fr->essential_hyp_count == 1u);

    return 0;
}

static int test_proof_hyp_invalid_args(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_hyp_id_t hid;

    LAPLACE_TEST_ASSERT(laplace_proof_import_float_hyp(
        &f.store, 999u, 1u, &hid) == LAPLACE_ERR_INVALID_ARGUMENT);

    laplace_proof_symbol_id_t cid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &cid) == LAPLACE_OK);

    laplace_proof_frame_desc_t fdesc = {
        .parent_id = LAPLACE_PROOF_FRAME_ID_INVALID,
        .mandatory_vars = NULL,
        .mandatory_var_count = 0u
    };
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &fdesc, &fid) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_proof_import_float_hyp(
        &f.store, fid, 999u, &hid) == LAPLACE_ERR_INVALID_ARGUMENT);

    return 0;
}

static int test_proof_dv_pair_import(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 1u, 3u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_frame_desc_t fdesc = {
        .parent_id = LAPLACE_PROOF_FRAME_ID_INVALID,
        .mandatory_vars = NULL,
        .mandatory_var_count = 0u
    };
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &fdesc, &fid) == LAPLACE_OK);

    laplace_proof_dv_id_t dvid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_dv_pair(
        &f.store, fid, vid + 1u, vid, &dvid) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_dv_pair_count(&f.store) == 1u);

    const laplace_proof_dv_pair_t* pairs;
    uint32_t pair_count;
    LAPLACE_TEST_ASSERT(laplace_proof_get_frame_dv_pairs(
        &f.store, fid, &pairs, &pair_count) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(pair_count == 1u);
    LAPLACE_TEST_ASSERT(pairs[0].var_a < pairs[0].var_b);
    LAPLACE_TEST_ASSERT(pairs[0].var_a == vid);
    LAPLACE_TEST_ASSERT(pairs[0].var_b == vid + 1u);

    laplace_proof_dv_id_t dvid2;
    LAPLACE_TEST_ASSERT(laplace_proof_import_dv_pair(
        &f.store, fid, vid, vid + 1u, &dvid2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_dv_pair_count(&f.store) == 1u);

    LAPLACE_TEST_ASSERT(laplace_proof_import_dv_pair(
        &f.store, fid, vid, vid + 2u, NULL) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_dv_pair_count(&f.store) == 2u);

    return 0;
}

static int test_proof_dv_pair_invalid(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 1u, 2u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_frame_desc_t fdesc = {
        .parent_id = LAPLACE_PROOF_FRAME_ID_INVALID,
        .mandatory_vars = NULL,
        .mandatory_var_count = 0u
    };
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &fdesc, &fid) == LAPLACE_OK);

    laplace_proof_dv_id_t dvid;

    LAPLACE_TEST_ASSERT(laplace_proof_import_dv_pair(
        &f.store, fid, vid, vid, &dvid) == LAPLACE_ERR_INVALID_ARGUMENT);

    LAPLACE_TEST_ASSERT(laplace_proof_import_dv_pair(
        &f.store, fid, cid, vid, &dvid) == LAPLACE_ERR_INVALID_ARGUMENT);

    LAPLACE_TEST_ASSERT(laplace_proof_import_dv_pair(
        &f.store, 999u, vid, vid + 1u, &dvid) == LAPLACE_ERR_INVALID_ARGUMENT);

    return 0;
}

static int test_proof_assertion_import(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 2u, 2u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_symbol_id_t tokens[] = {vid, vid + 1u};
    laplace_proof_expr_id_t concl_eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, tokens, 2u, &concl_eid) == LAPLACE_OK);

    laplace_proof_symbol_id_t fh_tokens[] = {vid};
    laplace_proof_expr_id_t fh_eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, fh_tokens, 1u, &fh_eid) == LAPLACE_OK);

    laplace_proof_frame_desc_t fdesc = {
        .parent_id = LAPLACE_PROOF_FRAME_ID_INVALID,
        .mandatory_vars = NULL,
        .mandatory_var_count = 0u
    };
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &fdesc, &fid) == LAPLACE_OK);

    laplace_proof_hyp_id_t fhid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_float_hyp(
        &f.store, fid, fh_eid, &fhid) == LAPLACE_OK);

    laplace_proof_assertion_desc_t adesc = {
        .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
        .frame_id = fid,
        .conclusion_id = concl_eid,
        .mand_float_offset = fhid,
        .mand_float_count = 1u,
        .mand_ess_offset = 0u,
        .mand_ess_count = 0u,
        .dv_offset = 0u,
        .dv_count = 0u,
        .compiler_index = 0u
    };
    laplace_proof_assertion_id_t aid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &adesc, &aid) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(aid == 1u);

    const laplace_proof_assertion_t* a = laplace_proof_get_assertion(&f.store, aid);
    LAPLACE_TEST_ASSERT(a != NULL);
    LAPLACE_TEST_ASSERT(a->kind == LAPLACE_PROOF_ASSERTION_AXIOM);
    LAPLACE_TEST_ASSERT(a->frame_id == fid);
    LAPLACE_TEST_ASSERT(a->conclusion_id == concl_eid);
    LAPLACE_TEST_ASSERT(a->theorem_id == LAPLACE_PROOF_THEOREM_ID_INVALID);

    return 0;
}

static int test_proof_assertion_invalid_args(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_assertion_id_t aid;

    laplace_proof_assertion_desc_t bad_kind = {
        .kind = LAPLACE_PROOF_ASSERTION_INVALID,
        .frame_id = 1u,
        .conclusion_id = 1u,
    };
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &bad_kind, &aid) == LAPLACE_ERR_INVALID_ARGUMENT);

    laplace_proof_assertion_desc_t bad_frame = {
        .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
        .frame_id = 999u,
        .conclusion_id = 1u,
    };
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &bad_frame, &aid) == LAPLACE_ERR_INVALID_ARGUMENT);

    return 0;
}

static int test_proof_theorem_import(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 2u, 2u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_symbol_id_t tokens_concl[] = {vid};
    laplace_proof_expr_id_t concl_eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, tokens_concl, 1u, &concl_eid) == LAPLACE_OK);

    laplace_proof_symbol_id_t tokens_fh[] = {vid};
    laplace_proof_expr_id_t fh_eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, tokens_fh, 1u, &fh_eid) == LAPLACE_OK);

    laplace_proof_frame_desc_t fdesc = {
        .parent_id = LAPLACE_PROOF_FRAME_ID_INVALID,
        .mandatory_vars = NULL,
        .mandatory_var_count = 0u
    };
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &fdesc, &fid) == LAPLACE_OK);

    laplace_proof_hyp_id_t fhid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_float_hyp(
        &f.store, fid, fh_eid, &fhid) == LAPLACE_OK);

    laplace_proof_assertion_desc_t ax_desc = {
        .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
        .frame_id = fid,
        .conclusion_id = concl_eid,
        .mand_float_offset = fhid,
        .mand_float_count = 1u,
    };
    laplace_proof_assertion_id_t ax_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &ax_desc, &ax_id) == LAPLACE_OK);

    laplace_proof_assertion_desc_t thm_desc = {
        .kind = LAPLACE_PROOF_ASSERTION_THEOREM,
        .frame_id = fid,
        .conclusion_id = concl_eid,
        .mand_float_offset = fhid,
        .mand_float_count = 1u,
    };
    laplace_proof_assertion_id_t thm_assn_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &thm_desc, &thm_assn_id) == LAPLACE_OK);

    laplace_proof_step_t steps[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP, .ref_id = fhid},
        {.kind = LAPLACE_PROOF_STEP_ASSERTION, .ref_id = ax_id},
    };
    laplace_proof_theorem_desc_t tdesc = {
        .assertion_id = thm_assn_id,
        .steps = steps,
        .step_count = 2u,
        .flags = 0u
    };
    laplace_proof_theorem_id_t tid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &tdesc, &tid) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(tid == 1u);
    LAPLACE_TEST_ASSERT(laplace_proof_theorem_count(&f.store) == 1u);

    const laplace_proof_theorem_t* thm = laplace_proof_get_theorem(&f.store, tid);
    LAPLACE_TEST_ASSERT(thm != NULL);
    LAPLACE_TEST_ASSERT(thm->assertion_id == thm_assn_id);
    LAPLACE_TEST_ASSERT(thm->step_count == 2u);

    const laplace_proof_step_t* out_steps;
    uint32_t out_count;
    LAPLACE_TEST_ASSERT(laplace_proof_get_theorem_steps(
        &f.store, tid, &out_steps, &out_count) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(out_count == 2u);
    LAPLACE_TEST_ASSERT(out_steps[0].kind == LAPLACE_PROOF_STEP_HYP);
    LAPLACE_TEST_ASSERT(out_steps[0].ref_id == fhid);
    LAPLACE_TEST_ASSERT(out_steps[1].kind == LAPLACE_PROOF_STEP_ASSERTION);
    LAPLACE_TEST_ASSERT(out_steps[1].ref_id == ax_id);

    const laplace_proof_assertion_t* linked =
        laplace_proof_get_assertion(&f.store, thm_assn_id);
    LAPLACE_TEST_ASSERT(linked->theorem_id == tid);

    return 0;
}

static int test_proof_theorem_invalid_steps(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 1u, 1u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_symbol_id_t tokens[] = {vid};
    laplace_proof_expr_id_t eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, tokens, 1u, &eid) == LAPLACE_OK);

    laplace_proof_frame_desc_t fdesc = {0};
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &fdesc, &fid) == LAPLACE_OK);

    laplace_proof_assertion_desc_t adesc = {
        .kind = LAPLACE_PROOF_ASSERTION_THEOREM,
        .frame_id = fid,
        .conclusion_id = eid,
    };
    laplace_proof_assertion_id_t aid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &adesc, &aid) == LAPLACE_OK);

    laplace_proof_theorem_id_t tid;

    laplace_proof_step_t bad_kind[] = {
        {.kind = LAPLACE_PROOF_STEP_INVALID, .ref_id = 1u}
    };
    laplace_proof_theorem_desc_t td1 = {
        .assertion_id = aid, .steps = bad_kind, .step_count = 1u
    };
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &td1, &tid) == LAPLACE_ERR_INVALID_ARGUMENT);

    laplace_proof_step_t bad_ref[] = {
        {.kind = LAPLACE_PROOF_STEP_HYP, .ref_id = 999u}
    };
    laplace_proof_theorem_desc_t td2 = {
        .assertion_id = aid, .steps = bad_ref, .step_count = 1u
    };
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &td2, &tid) == LAPLACE_ERR_OUT_OF_RANGE);

    laplace_proof_assertion_desc_t ax_desc = {
        .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
        .frame_id = fid,
        .conclusion_id = eid,
    };
    laplace_proof_assertion_id_t ax_id;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &ax_desc, &ax_id) == LAPLACE_OK);

    laplace_proof_theorem_desc_t td3 = {
        .assertion_id = ax_id, .steps = NULL, .step_count = 0u
    };
    LAPLACE_TEST_ASSERT(laplace_proof_import_theorem(
        &f.store, &td3, &tid) == LAPLACE_ERR_INVALID_ARGUMENT);

    return 0;
}

static int test_proof_base_layout_sanity(void) {
    LAPLACE_TEST_ASSERT(sizeof(laplace_proof_subst_entry_t) == 8u);
    LAPLACE_TEST_ASSERT(sizeof(laplace_proof_stack_entry_t) == 4u);

    laplace_proof_subst_map_t subst;
    memset(&subst, 0, sizeof(subst));
    LAPLACE_TEST_ASSERT(subst.count == 0u);
    subst.entries[0].variable = 1u;
    subst.entries[0].expr_id  = 2u;
    subst.count = 1u;
    LAPLACE_TEST_ASSERT(subst.entries[0].variable == 1u);
    LAPLACE_TEST_ASSERT(subst.entries[0].expr_id == 2u);

    laplace_proof_state_t state;
    memset(&state, 0, sizeof(state));
    LAPLACE_TEST_ASSERT(state.stack_depth == 0u);
    state.stack[0].expr_id = 42u;
    state.stack_depth = 1u;
    LAPLACE_TEST_ASSERT(state.stack[0].expr_id == 42u);

    laplace_proof_verifier_context_t vctx;
    memset(&vctx, 0, sizeof(vctx));
    LAPLACE_TEST_ASSERT(vctx.error_code == 0u);
    LAPLACE_TEST_ASSERT(vctx.error_step == 0u);

    return 0;
}

static int test_proof_validation_valid(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 2u, 3u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_symbol_id_t tokens[] = {vid, vid + 1u};
    laplace_proof_expr_id_t eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, tokens, 2u, &eid) == LAPLACE_OK);

    laplace_proof_symbol_id_t fh_tokens[] = {vid};
    laplace_proof_expr_id_t fh_eid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, fh_tokens, 1u, &fh_eid) == LAPLACE_OK);

    laplace_proof_frame_desc_t fdesc = {0};
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &fdesc, &fid) == LAPLACE_OK);

    laplace_proof_hyp_id_t fhid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_float_hyp(
        &f.store, fid, fh_eid, &fhid) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_proof_import_dv_pair(
        &f.store, fid, vid, vid + 1u, NULL) == LAPLACE_OK);

    laplace_proof_assertion_desc_t adesc = {
        .kind = LAPLACE_PROOF_ASSERTION_AXIOM,
        .frame_id = fid,
        .conclusion_id = eid,
        .mand_float_offset = fhid,
        .mand_float_count = 1u,
    };
    laplace_proof_assertion_id_t aid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_assertion(
        &f.store, &adesc, &aid) == LAPLACE_OK);

    laplace_proof_validation_result_t result;
    laplace_proof_validate_all(&f.store, &result);
    LAPLACE_TEST_ASSERT(result.valid == true);
    LAPLACE_TEST_ASSERT(result.error_count == 0u);

    return 0;
}

static int test_proof_validation_symbols(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t sid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_symbol(
        &f.store, LAPLACE_PROOF_SYMBOL_CONSTANT, 0u, 0u, &sid) == LAPLACE_OK);

    laplace_proof_validation_result_t result;
    laplace_proof_validate_symbols(&f.store, &result);
    LAPLACE_TEST_ASSERT(result.valid == true);

    return 0;
}

static int test_proof_validation_null_store(void) {
    laplace_proof_validation_result_t result;
    laplace_proof_validate_all(NULL, &result);
    LAPLACE_TEST_ASSERT(result.valid == false);
    LAPLACE_TEST_ASSERT(result.error_count == 1u);
    LAPLACE_TEST_ASSERT(result.errors[0] == LAPLACE_PROOF_VALIDATE_NULL_STORE);

    return 0;
}

static int test_proof_validation_uninit_store(void) {
    laplace_proof_store_t store;
    memset(&store, 0, sizeof(store));

    laplace_proof_validation_result_t result;
    laplace_proof_validate_all(&store, &result);
    LAPLACE_TEST_ASSERT(result.valid == false);
    LAPLACE_TEST_ASSERT(result.errors[0] == LAPLACE_PROOF_VALIDATE_STORE_NOT_INITIALIZED);

    return 0;
}

static int test_proof_validation_error_strings(void) {
    const char* s = laplace_proof_validation_error_string(LAPLACE_PROOF_VALIDATE_OK);
    LAPLACE_TEST_ASSERT(s != NULL);
    LAPLACE_TEST_ASSERT(s[0] != '\0');

    s = laplace_proof_validation_error_string(LAPLACE_PROOF_VALIDATE_INVALID_SYMBOL_KIND);
    LAPLACE_TEST_ASSERT(s != NULL);
    LAPLACE_TEST_ASSERT(s[0] != '\0');

    s = laplace_proof_validation_error_string(LAPLACE_PROOF_VALIDATE_DV_NOT_NORMALIZED);
    LAPLACE_TEST_ASSERT(s != NULL);

    s = laplace_proof_validation_error_string((laplace_proof_validation_error_t)255u);
    LAPLACE_TEST_ASSERT(s != NULL);

    return 0;
}

static int test_proof_validation_dv_pairs(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 1u, 3u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_frame_desc_t fdesc = {0};
    laplace_proof_frame_id_t fid;
    LAPLACE_TEST_ASSERT(laplace_proof_import_frame(
        &f.store, &fdesc, &fid) == LAPLACE_OK);

    LAPLACE_TEST_ASSERT(laplace_proof_import_dv_pair(
        &f.store, fid, vid, vid + 1u, NULL) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_import_dv_pair(
        &f.store, fid, vid + 1u, vid + 2u, NULL) == LAPLACE_OK);

    laplace_proof_validation_result_t result;
    laplace_proof_validate_dv_pairs(&f.store, &result);
    LAPLACE_TEST_ASSERT(result.valid == true);

    return 0;
}

static int test_proof_expr_token_fidelity(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 3u, 4u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_symbol_id_t tok1[] = {vid, vid + 1u, cid + 1u};
    laplace_proof_expr_id_t e1;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, tok1, 3u, &e1) == LAPLACE_OK);

    laplace_proof_symbol_id_t tok2[] = {vid + 2u, vid + 3u};
    laplace_proof_expr_id_t e2;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid + 1u, tok2, 2u, &e2) == LAPLACE_OK);

    const laplace_proof_symbol_id_t* out1;
    uint32_t cnt1;
    LAPLACE_TEST_ASSERT(laplace_proof_get_expr_tokens(
        &f.store, e1, &out1, &cnt1) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(cnt1 == 3u);
    LAPLACE_TEST_ASSERT(out1[0] == vid);
    LAPLACE_TEST_ASSERT(out1[1] == vid + 1u);
    LAPLACE_TEST_ASSERT(out1[2] == cid + 1u);

    const laplace_proof_symbol_id_t* out2;
    uint32_t cnt2;
    LAPLACE_TEST_ASSERT(laplace_proof_get_expr_tokens(
        &f.store, e2, &out2, &cnt2) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(cnt2 == 2u);
    LAPLACE_TEST_ASSERT(out2[0] == vid + 2u);
    LAPLACE_TEST_ASSERT(out2[1] == vid + 3u);

    return 0;
}

static int test_proof_expr_hash_determinism(void) {
    test_proof_fixture_t f;
    LAPLACE_TEST_ASSERT(test_proof_fixture_init(&f) == LAPLACE_OK);

    laplace_proof_symbol_id_t cid, vid;
    LAPLACE_TEST_ASSERT(test_import_symbols(&f.store, 1u, 2u, &cid, &vid) == LAPLACE_OK);

    laplace_proof_symbol_id_t tokens[] = {vid, vid + 1u};

    laplace_proof_expr_id_t e1, e2;
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, tokens, 2u, &e1) == LAPLACE_OK);
    LAPLACE_TEST_ASSERT(laplace_proof_import_expr(
        &f.store, cid, tokens, 2u, &e2) == LAPLACE_OK);

    const laplace_proof_expr_t* ex1 = laplace_proof_get_expr(&f.store, e1);
    const laplace_proof_expr_t* ex2 = laplace_proof_get_expr(&f.store, e2);
    LAPLACE_TEST_ASSERT(ex1->hash == ex2->hash);

    return 0;
}

int laplace_test_proof(void) {
    typedef struct {
        const char* name;
        int (*fn)(void);
    } subtest_t;

    const subtest_t subtests[] = {
        {"store_init",              test_proof_store_init},
        {"store_init_null",         test_proof_store_init_null},
        {"store_reset",            test_proof_store_reset},
        {"symbol_import",          test_proof_symbol_import},
        {"symbol_kind_checks",     test_proof_symbol_kind_checks},
        {"symbol_invalid_kind",    test_proof_symbol_invalid_kind},
        {"expr_import",            test_proof_expr_import},
        {"expr_invalid_typecode",  test_proof_expr_invalid_typecode},
        {"expr_invalid_token",     test_proof_expr_invalid_token},
        {"expr_token_fidelity",    test_proof_expr_token_fidelity},
        {"expr_hash_determinism",  test_proof_expr_hash_determinism},
        {"frame_import",           test_proof_frame_import},
        {"frame_invalid_parent",   test_proof_frame_invalid_parent},
        {"float_hyp_import",       test_proof_float_hyp_import},
        {"essential_hyp_import",   test_proof_essential_hyp_import},
        {"hyp_invalid_args",       test_proof_hyp_invalid_args},
        {"dv_pair_import",         test_proof_dv_pair_import},
        {"dv_pair_invalid",        test_proof_dv_pair_invalid},
        {"assertion_import",       test_proof_assertion_import},
        {"assertion_invalid_args", test_proof_assertion_invalid_args},
        {"theorem_import",         test_proof_theorem_import},
        {"theorem_invalid_steps",  test_proof_theorem_invalid_steps},
        {"base_layout_sanity",     test_proof_base_layout_sanity},
        {"validation_valid",       test_proof_validation_valid},
        {"validation_symbols",     test_proof_validation_symbols},
        {"validation_null_store",  test_proof_validation_null_store},
        {"validation_uninit_store", test_proof_validation_uninit_store},
        {"validation_error_strings", test_proof_validation_error_strings},
        {"validation_dv_pairs",    test_proof_validation_dv_pairs},
    };

    const size_t count = sizeof(subtests) / sizeof(subtests[0]);
    for (size_t i = 0u; i < count; ++i) {
        int result = subtests[i].fn();
        if (result != 0) {
            fprintf(stderr, "  proof subtest FAIL: %s\n", subtests[i].name);
            return 1;
        }
    }
    return 0;
}
