#include "../lib/common/SegmentTree.h"
#include <cassert>
#include <climits>
#include <iostream>
#include <vector>

void testMinSegmentTree()
{
    std::cout << "Starting Min SegmentTree tests..." << std::endl;
    // Array: [1, 5, 2, 7, 3]
    std::vector<int> data = {1, 5, 2, 7, 3};

    // Min Segment Tree
    SegmentTree<int> st(data.size(), INT_MAX, [](int a, int b) { return std::min(a, b); });
    st.build(data);

    // Test Query
    assert(st.query(0, 4) == 1);
    assert(st.query(1, 3) == 2);
    assert(st.query(2, 2) == 2);
    assert(st.query(3, 4) == 3);

    std::cout << "Min: Initial queries passed." << std::endl;

    // Test Update
    st.update(2, 0); // Array: [1, 5, 0, 7, 3]
    assert(st.query(0, 4) == 0);
    assert(st.query(1, 3) == 0);
    assert(st.query(2, 2) == 0);

    st.update(0, 10); // Array: [10, 5, 0, 7, 3]
    assert(st.query(0, 4) == 0);
    assert(st.query(0, 0) == 10);
    assert(st.query(0, 1) == 5);

    std::cout << "Min: Update queries passed." << std::endl;

    // Large Update
    st.update(4, -1); // Array: [10, 5, 0, 7, -1]
    assert(st.query(0, 4) == -1);

    std::cout << "Min SegmentTree tests passed!" << std::endl;
}

void testSumSegmentTree()
{
    std::cout << "Starting Sum SegmentTree tests..." << std::endl;
    // Array: [1, 10, 100, 1000]
    std::vector<long> data = {1, 10, 100, 1000};

    // Sum Segment Tree
    SegmentTree<long> st(data.size(), 0, [](long a, long b) { return a + b; });
    st.build(data);

    // Test Query
    assert(st.query(0, 3) == 1111);
    assert(st.query(0, 1) == 11);
    assert(st.query(2, 3) == 1100);

    st.update(0, 2); // [2, 10, 100, 1000]
    assert(st.query(0, 3) == 1112);

    std::cout << "Sum SegmentTree tests passed!" << std::endl;
}

int main()
{
    testMinSegmentTree();
    testSumSegmentTree();
    return 0;
}
