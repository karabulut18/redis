#include "../lib/common/SegmentTree.h"
#include <cassert>
#include <iostream>
#include <vector>

void testSegmentTree()
{
    std::cout << "Starting SegmentTree tests..." << std::endl;
    // Array: [1, 5, 2, 7, 3]
    std::vector<int> data = {1, 5, 2, 7, 3};
    SegmentTree st(data.size());
    st.build(data);

    // Test Query
    assert(st.query(0, 4) == 1);
    assert(st.query(1, 3) == 2);
    assert(st.query(2, 2) == 2);
    assert(st.query(3, 4) == 3);

    std::cout << "Initial queries passed." << std::endl;

    // Test Update
    st.update(2, 0); // Array: [1, 5, 0, 7, 3]
    assert(st.query(0, 4) == 0);
    assert(st.query(1, 3) == 0);
    assert(st.query(2, 2) == 0);

    st.update(0, 10); // Array: [10, 5, 0, 7, 3]
    assert(st.query(0, 4) == 0);
    assert(st.query(0, 0) == 10);
    assert(st.query(0, 1) == 5);

    std::cout << "Update queries passed." << std::endl;

    // Large Update
    st.update(4, -1); // Array: [10, 5, 0, 7, -1]
    assert(st.query(0, 4) == -1);

    std::cout << "All SegmentTree tests passed!" << std::endl;
}

int main()
{
    testSegmentTree();
    return 0;
}
