#include "tlsf.c"
#include "tlsf.h"
#include "googletest/googletest/include/gtest/gtest.h"

#include <vector>
#include <bitset>

	TEST(Alignment_Helper_Functions, is_aligned)
	{
		std::vector<size_t> aligned={24,32,64,40,48,128,0,56};
		std::vector<size_t> notAligned={15,17,18,19,20,21,22,23,26};
		for(auto i: aligned)
			EXPECT_EQ(isAligned(i), true);
		for(auto i: notAligned)
			EXPECT_NE(isAligned(i), true);
	}
	
	TEST(API, can_init_allocate_and_free)
	{
		void* p = malloc(1024*100);
		void *tlsf = tlsf_init(p, 1024*100);
		void* p1 = tlsf_malloc(tlsf,305);
		void* p2 = tlsf_malloc(tlsf,3456);
		void* p3 = tlsf_malloc(tlsf,23);
		void* p4 = tlsf_malloc(tlsf,305);
		void* p5 = tlsf_malloc(tlsf,987);
		void* p6 = tlsf_malloc(tlsf,346);
		void* p7 = tlsf_malloc(tlsf,634);
		tlsf_free(tlsf, p2);
		tlsf_free(tlsf, p3);
		void* p8 = tlsf_malloc(tlsf,233);
		void* p9 = tlsf_malloc(tlsf,10);
		tlsf_free(tlsf, p1);
		tlsf_free(tlsf, p5);
		tlsf_free(tlsf, p6);
		tlsf_free(tlsf, p4);
		tlsf_free(tlsf, p9);
		tlsf_free(tlsf, p7);
		tlsf_free(tlsf, p8);
		free(p);

	}

// TEST(Bitmap, can_set_bits)
// {
// 	Second_Level sl;
// 	sl.bitmap = 0;

// 	std::bitset<64> testBitset(136);

// 	sl.bitmap = setBit(sl.bitmap, 3);
// 	sl.bitmap = setBit(sl.bitmap, 7);

// 	const char *sl_bitset= std::bitset
// 		<sizeof(uint64_t)*8>(sl.bitmap).to_string().c_str();
// 	const char *testBitstring = testBitset.to_string().c_str();
// 	EXPECT_STREQ(sl_bitset, testBitstring);
// }
// // 
// int main(int argc, char **argv) {
//   ::testing::InitGoogleTest(&argc, argv);
//   return RUN_ALL_TESTS();
// }

/* Tests to run:
	
		Make sure that 
*/

