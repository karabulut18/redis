#include "../lib/common/SegmentTree.h"
#include <algorithm>
#include <climits>
#include <iostream>
#include <vector>

using namespace std;

struct ListNode
{
    int value;
    ListNode* next;
    ListNode(int x) : value(x), next(nullptr)
    {
    }
};

struct RouteData
{
    vector<ListNode*> nodes;
    SegmentTree<int>* tree;

    RouteData(const vector<int>& points)
    {
        // Build linked list nodes and link them
        ListNode* prev = nullptr;
        for (int p : points)
        {
            ListNode* node = new ListNode(p);
            if (prev)
            {
                prev->next = node;
            }
            nodes.push_back(node);
            prev = node;
        }

        // Build Segment Tree (Min Combiner)
        // Store min value to help find strictly smaller nodes
        tree = new SegmentTree<int>(points.size(), INT_MAX, [](const int& a, const int& b) { return std::min(a, b); });
        tree->build(points);
    }

    ~RouteData()
    {
        delete tree;
        // Nodes are managed by the returned list, or leaked if unused.
    }
};

const int MAXN = 50005;

class OptimizeDeliveryRoutes
{
    vector<RouteData*> _routes;
    vector<bool> _isContinuation;
    size_t _processedCount = 0;

public:
    void addRoute(const vector<int>& deliveryPoints)
    {
        _routes.push_back(new RouteData(deliveryPoints));
        _isContinuation.push_back(false);
    }

    vector<ListNode*> returnOptimizedRoutes()
    {
        vector<ListNode*> optimizedHeads;
        if (_routes.empty())
            return optimizedHeads;

        // Start from the last processed route (or 0 if none)
        // We need to check link between _routes[_processedCount-1] and _routes[_processedCount]
        // So start index should be at least _processedCount - 1
        size_t start = (_processedCount > 0) ? _processedCount - 1 : 0;

        // Iterate through new routes and connect them
        for (size_t i = start; i < _routes.size(); ++i)
        {
            if (i < _routes.size() - 1)
            {
                RouteData* current = _routes[i];
                RouteData* next = _routes[i + 1];
                int target = next->nodes[0]->value;

                // Find rightmost node in 'current' strictly smaller than 'target'
                int idx = current->tree->findRightmost([target](const int& val) { return val < target; });

                if (idx != -1)
                {
                    // Connect current[idx] -> next[0]
                    current->nodes[idx]->next = next->nodes[0];
                    _isContinuation[i + 1] = true;
                }
            }
        }

        // Update processed count
        _processedCount = _routes.size();

        // Collect all heads (including old ones, as requested by 'returnOptimizedRoutes' usually implies returning the
        // full state)
        for (size_t i = 0; i < _routes.size(); ++i)
        {
            if (!_isContinuation[i])
            {
                optimizedHeads.push_back(_routes[i]->nodes[0]);
            }
        }

        return optimizedHeads;
    }

    // // Helper function to print all the linked lists
    void printAllRoutes(const vector<ListNode*>& routes) const;
};

void OptimizeDeliveryRoutes::printAllRoutes(const vector<ListNode*>& routes) const
{
    for (auto head : routes)
    {
        ListNode* current = head;
        while (current != nullptr)
        {
            cout << current->value;
            if (current->next != nullptr)
                cout << " ";
            current = current->next;
        }
        cout << endl;
    }
}

int main()
{
    OptimizeDeliveryRoutes handler;
    int totalNumberOfRequests;
    if (!(cin >> totalNumberOfRequests))
        return 0;

    for (int i = 0; i < totalNumberOfRequests; ++i)
    {
        string functionName;
        cin >> functionName;

        if (functionName == "addRoute")
        {
            vector<int> deliveryPoints;
            int num;
            cin >> num;
            for (int j = 0; j < num; j++)
            {
                int name;
                cin >> name;
                deliveryPoints.push_back(name);
            }
            handler.addRoute(deliveryPoints);
        }
        else if (functionName == "returnOptimizedRoutes")
        {
            vector<ListNode*> result = handler.returnOptimizedRoutes();
            handler.printAllRoutes(result);
        }
    }

    return 0;
}
