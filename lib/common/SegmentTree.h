#pragma once
#include <algorithm>
#include <climits>
#include <vector>

class SegmentTree
{
    std::vector<int> _tree;
    int _size;

public:
    SegmentTree(int size)
    {
        _size = size;
        // Optimization:
        // In the standard recursive implementation, we need 4*n space because the tree structure
        // follows 2*p and 2*p+1 indexing, which leaves gaps if n is not a power of 2.
        //
        // In this iterative implementation, we physically shift the structure:
        // - Leaves are stored continuously at indices [n, 2n-1]
        // - Internal nodes are stored at [1, n-1]
        // - Root is at 1
        // This dense packing guarantees we only need 2*n space.
        _tree.resize(2 * size, 0);
    }

    void build(const std::vector<int>& arr)
    {
        // Initialize leaves
        for (int i = 0; i < _size; ++i)
        {
            if (i < (int)arr.size())
                _tree[_size + i] = arr[i];
            else
                _tree[_size + i] = 0; // Default handling if arr is smaller
        }

        // Build the tree by calculating parents
        for (int i = _size - 1; i > 0; --i)
        {
            _tree[i] = std::min(_tree[2 * i], _tree[2 * i + 1]);
        }
    };

    // Query range [L, R] inclusive
    int query(int L, int R)
    {
        int res = INT_MAX;

        // Convert to open interval [l, r) for standard iterative logic
        // l starts at L + _size
        // r starts at R + 1 + _size
        for (int l = L + _size, r = R + 1 + _size; l < r; l /= 2, r /= 2)
        {
            if (l & 1)
                res = std::min(res, _tree[l++]);

            if (r & 1)
                res = std::min(res, _tree[--r]);
        }

        return res;
    }

    void update(int idx, int val)
    {
        int pos = idx + _size;
        _tree[pos] = val;

        // Move up to root
        for (pos /= 2; pos >= 1; pos /= 2)
        {
            _tree[pos] = std::min(_tree[2 * pos], _tree[2 * pos + 1]);
        }
    };
};
