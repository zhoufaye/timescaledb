/*
 * Copyright (c) 2016-2018  Timescale, Inc. All Rights Reserved.
 *
 * This file is licensed under the Apache License,
 * see LICENSE-APACHE at the top level directory.
 */
#include <postgres.h>
#include <nodes/extensible.h>
#include <nodes/makefuncs.h>
#include <nodes/nodeFuncs.h>
#include <utils/rel.h>
#include <catalog/pg_type.h>

#include "chunk_dispatch.h"
#include "chunk_insert_state.h"
#include "subspace_store.h"
#include "dimension.h"
#include "guc.h"

ChunkDispatch *
ts_chunk_dispatch_create(Hypertable *ht, EState *estate)
{
	ChunkDispatch *cd = palloc0(sizeof(ChunkDispatch));

	cd->hypertable = ht;
	cd->estate = estate;
	cd->hypertable_result_rel_info = NULL;
	cd->on_conflict = ONCONFLICT_NONE;
	cd->arbiter_indexes = NIL;
	cd->cmd_type = CMD_INSERT;
	cd->cache = ts_subspace_store_init(ht->space, estate->es_query_cxt, ts_guc_max_open_chunks_per_insert);

	return cd;
}

void
ts_chunk_dispatch_destroy(ChunkDispatch *cd)
{
	ts_subspace_store_free(cd->cache);
}

static void
destroy_chunk_insert_state(void *cis)
{
	ts_chunk_insert_state_destroy((ChunkInsertState *) cis);
}

/*
 * Get the chunk insert state for the chunk that matches the given point in the
 * partitioned hyperspace.
 */
extern ChunkInsertState *
ts_chunk_dispatch_get_chunk_insert_state(ChunkDispatch *dispatch, Point *point)
{
	ChunkInsertState *cis;

	cis = ts_subspace_store_get(dispatch->cache, point);

	if (NULL == cis)
	{
		Chunk	   *new_chunk;

		new_chunk = ts_hypertable_get_chunk(dispatch->hypertable, point);

		if (NULL == new_chunk)
			elog(ERROR, "no chunk found or created");

		cis = ts_chunk_insert_state_create(new_chunk, dispatch);
		ts_subspace_store_add(dispatch->cache, new_chunk->cube, cis, destroy_chunk_insert_state);
	}

	Assert(cis != NULL);
	return cis;
}
