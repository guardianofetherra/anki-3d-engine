// Copyright (C) 2009-2018, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <tests/framework/Framework.h>
#include <anki/scene/Octree.h>
#include <anki/collision/Frustum.h>

namespace anki
{

ANKI_TEST(Scene, Octree)
{
	HeapAllocator<U8> alloc(allocAligned, nullptr);

	// Fuzzy
	{
		Octree octree(alloc);
		octree.init(Vec3(-100.0f), Vec3(100.0f), 4);

		OrthographicFrustum frustum(-200.0f, 200.0f, 200.0f, -200.0f, 200.0f, -200.0f);
		frustum.resetTransform(Transform::getIdentity());

		const U ITERATION_COUNT = 1000;
		Array<OctreePlaceable, ITERATION_COUNT> placeables;
		std::vector<U32> placed;
		for(U i = 0; i < ITERATION_COUNT; ++i)
		{
			F32 min = randRange(-100.0f, 100.0f - 1.0f);
			F32 max = randRange(min + 1.0f, 100.0f);
			Aabb volume(Vec4(Vec3(min), 0.0f), Vec4(Vec3(max), 0.0f));

			I mode = rand() % 3;
			if(mode == 0)
			{
				// Place
				octree.place(volume, &placeables[i]);
				placed.push_back(i);
			}
			else if(mode == 1 && placed.size() > 0)
			{
				// Remove
				octree.remove(placeables[placed.back()]);
				placed.pop_back();
			}
			else if(placed.size() > 0)
			{
				// Gather
				DynamicArrayAuto<OctreePlaceable*> arr(alloc);
				octree.gatherVisible(frustum, 0, arr);

				ANKI_TEST_EXPECT_EQ(arr.getSize(), placed.size());
				for(U32 idx : placed)
				{
					Bool found = false;
					for(OctreePlaceable* placeable : arr)
					{
						if(&placeables[idx] == placeable)
						{
							found = true;
							break;
						}
					}

					ANKI_TEST_EXPECT_EQ(found, true);
				}
			}
		}

		// Remove all
		while(placed.empty())
		{
			octree.remove(placeables[placed.back()]);
			placed.pop_back();
		}
	}
}

} // end namespace anki