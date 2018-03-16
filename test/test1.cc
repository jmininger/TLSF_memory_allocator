#include "../tlsf.h"
#include "../tlsf.c"
#include "gtest/gtest.h"

#include <bitset>


TEST(Bitmap, can_set_bits)
{
	Second_Level sl;
	sl.bitmap = 0;

	std::bitset<64> testBitset(136);

	sl.bitmap = setBit(sl.bitmap, 3);
	sl.bitmap = setBit(sl.bitmap, 7);

	const char *sl_bitset= std::bitset
		<sizeof(uint64_t)*8>(sl.bitmap).to_string().c_str();
	const char *testBitstring = testBitset.to_string().c_str();
	EXPECT_STREQ(sl_bitset, testBitstring);
}
// 
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

/* Tests to run:
	
		Make sure that 
*/

