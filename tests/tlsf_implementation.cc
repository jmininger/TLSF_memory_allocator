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

