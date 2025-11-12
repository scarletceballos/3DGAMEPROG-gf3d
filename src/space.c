#include "space.h"
#include "simple_logger.h"
#include "body.h"

Space* space_new(void)
{
	Space* space = (Space*)gfc_allocate_array(sizeof(Space), 1);
	if (!space) return NULL;
	space->staticMeshes = gfc_list_new();
	space->bodies = gfc_list_new();
	space->staticBodies = gfc_list_new();
	space->iterations = 1;
	space->step = 1.0f;
	return space;
}

void space_free(Space* space)
{
	if (!space) return;
	if (space->staticMeshes) gfc_list_delete(space->staticMeshes);
	if (space->bodies) gfc_list_delete(space->bodies);
	if (space->staticBodies) gfc_list_delete(space->staticBodies);
	free(space);
}

void space_set_iterations(Space* space, Uint32 iterations)
{
	if (!space) return;
	if (iterations == 0)
	{
		slog("space_set_iterations: cannot set 0 iterations");
		return;
	}
	space->iterations = iterations;
	space->step = 1.0f / (float)iterations;
}

void space_step(Space* space)
{
	if (!space) return;
	// reset step state
	for (Uint32 i = 0; i < gfc_list_count(space->bodies); ++i)
	{
		Body* b = (Body*)gfc_list_nth(space->bodies, i);
		if (b) body_reset_for_updates(b, space->step);
	}
	// pairwise collision detection
	Uint32 count = gfc_list_count(space->bodies);
	for (Uint32 i = 0; i < count; ++i)
	{
		Body* a = (Body*)gfc_list_nth(space->bodies, i);
		if (!a) continue;
		for (Uint32 j = i + 1; j < count; ++j)
		{
			Body* b = (Body*)gfc_list_nth(space->bodies, j);
			if (!b) continue;
			(void)body_test_body(a, b);
		}
	}
}

void space_run(Space* space)
{
	if (!space) return;
	for (Uint32 i = 0; i < space->iterations; ++i)
	{
		space_step(space);
	}
}

void space_add_body(Space* space, Body* body)
{
	if (!space || !body) return;
	gfc_list_append(space->bodies, body);
}