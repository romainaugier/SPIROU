/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 - Present Romain Augier */
/* All rights reserved. */

#include "spirou/passes.h"

#include "libromano/memory.h"

static void push_region_results(SpirouFunction* func, SpirouRegionId region, Vector* worklist)
{
    SpirouRegion* r = spirou_region(func, region);
    uint32_t i;

    for(i = 0; i < r->num_results; i++)
    {
        SpirouValue result = spirou_region_result(func, region, i);

        if(result != SPIROU_INVALID)
            vector_push_back(worklist, &result);
    }
}

uint32_t spirou_function_dce(SpirouFunction* func)
{
    uint32_t num_nodes = spirou_function_num_nodes(func);
    uint32_t num_regions = spirou_function_num_regions(func);
    uint8_t* live;
    Vector worklist;
    uint32_t removed = 0;
    SpirouRegionId region;

    if(spirou_function_is_import(func))
        return 0;

    live = (uint8_t*)romano_calloc(num_nodes + 1, 1);
    vector_init(&worklist, 128, sizeof(SpirouValue));

    push_region_results(func, func->body, &worklist);

    while(vector_size(&worklist) > 0)
    {
        SpirouValue value = *(SpirouValue*)vector_back(&worklist);
        SpirouNodeId node;
        SpirouNode* n;
        uint32_t i;

        vector_pop(&worklist);

        node = spirou_value_node(func, value);

        if(node == SPIROU_INVALID || live[node])
            continue;

        live[node] = 1;
        n = spirou_node(func, node);

        for(i = 0; i < n->num_inputs; i++)
        {
            SpirouValue input = spirou_node_input(func, node, i);
            vector_push_back(&worklist, &input);
        }

        for(i = 0; i < spirou_node_num_regions(func, node); i++)
            push_region_results(func, spirou_node_subregion(func, node, i), &worklist);
    }

    for(region = 0; region < num_regions; region++)
    {
        SpirouRegion* r = spirou_region(func, region);
        SpirouNodeId node = r->first;
        SpirouNodeId last = SPIROU_INVALID;

        r->first = SPIROU_INVALID;

        while(node != SPIROU_INVALID)
        {
            SpirouNode* n = spirou_node(func, node);
            SpirouNodeId next = n->next;

            if(live[node])
            {
                if(last == SPIROU_INVALID)
                    r->first = node;
                else
                    spirou_node(func, last)->next = node;

                last = node;
            }
            else
            {
                n->flags |= SpirouNodeFlags_Dead;
                n->next = SPIROU_INVALID;
                removed++;
            }

            node = next;
        }

        if(last != SPIROU_INVALID)
            spirou_node(func, last)->next = SPIROU_INVALID;

        r->last = last;
    }

    vector_release(&worklist);
    romano_free(live);

    return removed;
}
