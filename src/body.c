#include "simple_logger.h"
#include <string.h>

#include "body.h"

Body* body_new(void)
{
	// log constantly to check body works
	slog("body_new: allocating Body struct...");
	Body* b = (Body*)gfc_allocate_array(sizeof(Body), 1);
	if (!b) {
		slog("body_new: Body allocation failed");
		return NULL;
	}
	slog("body_new: creating volumes list...");
	b->volumes = gfc_list_new();
	if (!b->volumes) {
		slog("body_new: volumes list creation failed");
		free(b);
		return NULL;
	}
	slog("body_new: Body created successfully");
	b->position = gfc_vector3d(0,0,0);
	b->velocity = gfc_vector3d(0,0,0);
	b->stepPosition = b->position;
	b->stepVelocity = b->velocity;
	b->stopped = 0;
	b->onCollide = NULL;
	b->data = NULL;
	b->isStatic = 0;
	return b;
}

void body_free(Body* b)
{
	if (!b) return;
	if (b->volumes)
	{
		// free each allocated primitive
		for (Uint32 i = 0; i < gfc_list_count(b->volumes); ++i)
		{
			void* p = gfc_list_nth(b->volumes, i);
			if (p) free(p);
		}
		gfc_list_delete(b->volumes);
		b->volumes = NULL;
	}
	free(b);
}

void body_add_volume(Body* b, GFC_Primitive v)
{
	if (!b) return;
	GFC_Primitive* p = (GFC_Primitive*)gfc_allocate_array(sizeof(GFC_Primitive), 1);
	if (!p) return;
	memcpy(p, &v, sizeof(GFC_Primitive));
	gfc_list_append(b->volumes, p);
}

void body_set_collision(Body* b, body_collide_func* collide, void* data)
{
	if (!b) return;
	b->onCollide = collide;
	b->data = data;
}

void body_set_static(Body* b, Uint8 isStatic)
{
	if (!b) return;
	b->isStatic = isStatic ? 1 : 0;
}

void body_reset_for_updates(Body* b, float factor)
{
	if (!b) return;
	b->stepPosition = b->position;
	gfc_vector3d_scale(b->stepVelocity, b->velocity, factor);
	b->stopped = 0;
}

static int primitive_overlap(GFC_Primitive a, GFC_Primitive b)
{
	if (a.type == GPT_BOX && b.type == GPT_BOX)
	{
		return gfc_box_overlap(a.s.b, b.s.b) ? 1 : 0;
	}
	if (a.type == GPT_SPHERE && b.type == GPT_SPHERE)
	{
		return gfc_sphere_overlap(a.s.s, b.s.s) ? 1 : 0;
	}
	// Others not exsting go here
	return 0;
}

int body_test_body(Body* a, Body* b)
{
	if (!a || !b) return 0;

	Uint32 ac = gfc_list_count(a->volumes);
	for (Uint32 i = 0; i < ac; ++i)
	{
		GFC_Primitive* ap = (GFC_Primitive*)gfc_list_nth(a->volumes, i);
		if (!ap) continue;
		GFC_Primitive apTest = gfc_primitive_offset(*ap, a->stepPosition);

		Uint32 bc = gfc_list_count(b->volumes);
		for (Uint32 j = 0; j < bc; ++j)
		{
			GFC_Primitive* bp = (GFC_Primitive*)gfc_list_nth(b->volumes, j);
			if (!bp) continue;
			GFC_Primitive bpTest = gfc_primitive_offset(*bp, b->stepPosition);

			if (primitive_overlap(apTest, bpTest))
			{
				// basic resolution for box/box
				if (apTest.type == GPT_BOX && bpTest.type == GPT_BOX)
				{
					GFC_Box A = apTest.s.b;
					GFC_Box B = bpTest.s.b;
					float aMinX = A.x, aMaxX = A.x + A.w;
					float aMinY = A.y, aMaxY = A.y + A.h;
					float aMinZ = A.z, aMaxZ = A.z + A.d;
					float bMinX = B.x, bMaxX = B.x + B.w;
					float bMinY = B.y, bMaxY = B.y + B.h;
					float bMinZ = B.z, bMaxZ = B.z + B.d;

					float overlapX = fminf(aMaxX - bMinX, bMaxX - aMinX);
					float overlapY = fminf(aMaxY - bMinY, bMaxY - aMinY);
					float overlapZ = fminf(aMaxZ - bMinZ, bMaxZ - aMinZ);

					// choose smallest penetration axis
					GFC_Vector3D push = gfc_vector3d(0,0,0);
					if (overlapX <= overlapY && overlapX <= overlapZ)
					{
						float aCx = aMinX + A.w * 0.5f;
						float bCx = bMinX + B.w * 0.5f;
						push.x = (aCx < bCx ? -overlapX : overlapX);
					}
					else if (overlapY <= overlapX && overlapY <= overlapZ)
					{
						float aCy = aMinY + A.h * 0.5f;
						float bCy = bMinY + B.h * 0.5f;
						push.y = (aCy < bCy ? -overlapY : overlapY);
					}
					else
					{
						float aCz = aMinZ + A.d * 0.5f;
						float bCz = bMinZ + B.d * 0.5f;
						push.z = (aCz < bCz ? -overlapZ : overlapZ);
					}

					if (a->isStatic && b->isStatic)
					{
						// nothing moves
					}
					else if (a->isStatic)
					{
						// move B only
						gfc_vector3d_add(b->stepPosition, b->stepPosition, push);
					}
					else if (b->isStatic)
					{
						// move A only in opposite direction
						GFC_Vector3D nPush; gfc_vector3d_negate(nPush, push);
						gfc_vector3d_add(a->stepPosition, a->stepPosition, nPush);
					}
					else
					{
						// split movement
						GFC_Vector3D halfPush; gfc_vector3d_scale(halfPush, push, 0.5f);
						GFC_Vector3D nHalfPush; gfc_vector3d_negate(nHalfPush, halfPush);
						gfc_vector3d_add(a->stepPosition, a->stepPosition, nHalfPush);
						gfc_vector3d_add(b->stepPosition, b->stepPosition, halfPush);
					}
				}

				if (a->onCollide) a->onCollide(a, b, a->data);
				if (b->onCollide) b->onCollide(b, a, b->data);
				return 1;
			}
		}
	}
	return 0;
}