#pragma once
#include <algorithm>
#include <climits>
#include <functional>
#include <vector>

template <typename T> class SegmentTree
{
    using Combiner = std::function<T(const T&, const T&)>;

    std::vector<T> _tree;
    int _size;
    T _identity;
    Combiner _combiner;

public:
    SegmentTree(int size, T identity, Combiner combiner) : _size(size), _identity(identity), _combiner(combiner)
    {
        // Optimization:
        // In the standard recursive implementation, we need 4*n space because the tree structure
        // follows 2*p and 2*p+1 indexing, which leaves gaps if n is not a power of 2.
        //
        // In this iterative implementation, we physically shift the structure:
        // - Leaves are stored continuously at indices [n, 2n-1]
        // - Internal nodes are stored at [1, n-1]
        // - Root is at 1
        // This dense packing guarantees we only need 2*n space.
        _tree.resize(2 * size, identity);
    }

    void build(const std::vector<T>& arr)
    {
        // Initialize leaves
        for (int i = 0; i < _size; ++i)
        {
            if (i < (int)arr.size())
                _tree[_size + i] = arr[i];
            else
                _tree[_size + i] = _identity;
        }

        // Build the tree by calculating parents
        for (int i = _size - 1; i > 0; --i)
        {
            _tree[i] = _combiner(_tree[2 * i], _tree[2 * i + 1]);
        }
    };

    // Query range [L, R] inclusive
    T query(int L, int R)
    {
        T leftRes = _identity;
        T rightRes = _identity;

        // Convert to open interval [l, r) for standard iterative logic
        // l starts at L + _size
        // r starts at R + 1 + _size
        for (int l = L + _size, r = R + 1 + _size; l < r; l /= 2, r /= 2)
        {
            if (l & 1)
                leftRes = _combiner(leftRes, _tree[l++]);

            if (r & 1)
                rightRes = _combiner(_tree[--r], rightRes);
        }

        return _combiner(leftRes, rightRes);
    }

    void update(int idx, T val)
    {
        int pos = idx + _size;
        _tree[pos] = val;

        // Move up to root
        for (pos /= 2; pos >= 1; pos /= 2)
        {
            _tree[pos] = _combiner(_tree[2 * pos], _tree[2 * pos + 1]);
        }
    };
};
