#include "KeyGenerationTree.h"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

KeyGenerationTree::KeyGenerationTree(uint32_t networkSize, uint32_t nodeId) : m_nodeId(nodeId), m_leafCount(networkSize)
{
    if (networkSize == 0)
        throw std::invalid_argument("networkSize must be > 0");

    m_depth = CeilLog2(networkSize);
    m_capacity = 1u << m_depth;          
    m_nodeCount = 2u * m_capacity - 1;
    m_owned.resize(m_nodeCount, false);

    // --- (1) Directly set padded leaf nodes as owned --------------------------------
    for (uint32_t leaf = m_leafCount; leaf < m_capacity; ++leaf) {
        m_owned[LeafToNodeIndex(leaf)] = true;
    }

    // --- (2) Bottom-up merge aggregatable internal nodes ---------------------------
    for (int32_t idx = static_cast<int32_t>(m_capacity) - 2; idx >= 0; --idx) {
        size_t l = LeftChild(idx);
        size_t r = RightChild(idx);
        if (m_owned[l] && m_owned[r]) m_owned[idx] = true;
    } 
    AddContribution(nodeId);
}

// Calculate ceiling log2
uint32_t KeyGenerationTree::CeilLog2(uint32_t n)
{
    uint32_t l = 0;
    n -= 1;
    while (n > 0) { n >>= 1; ++l; }
    return l;
}

// Convert leaf index to tree node index
uint32_t KeyGenerationTree::LeafToNodeIndex(uint32_t leafIdx) const
{
    return m_capacity - 1 + leafIdx;
}

// Convert node index to leaf index
uint32_t KeyGenerationTree::NodeToLeafIndex(uint32_t nodeIdx) const
{
    if (nodeIdx < m_capacity - 1)
        throw std::invalid_argument("Not a leaf node");
    
    return nodeIdx - (m_capacity - 1);
}

// Bubble up merge
void KeyGenerationTree::BubbleUpMerge()
{
    std::set<uint32_t> nodesToCheck;
        for (uint32_t i = 0; i < m_leafCount; ++i) {
        uint32_t leafIdx = LeafToNodeIndex(i);
        if (m_owned[leafIdx]) {
            nodesToCheck.insert(Parent(leafIdx));
        }
    }
        while (!nodesToCheck.empty()) {
        uint32_t idx = *nodesToCheck.rbegin();
        nodesToCheck.erase(idx);
        if (m_owned[idx]) continue;
        if (m_owned[LeftChild(idx)] && m_owned[RightChild(idx)]) {
            m_owned[idx] = true;
            if (idx != 0) {
                nodesToCheck.insert(Parent(idx));
            }
        }
    }
}

// Batch add multiple key contributions
void KeyGenerationTree::AddMultipleContributions(const std::string& contributionString)
{
    if (contributionString.size() != m_leafCount) {
        throw std::invalid_argument("Invalid contribution string size");
    }
    
    bool anyNewContribution = false;
    
    for (uint32_t i = 0; i < m_leafCount; ++i) {
        if (contributionString[i] == '1') {
            uint32_t idx = LeafToNodeIndex(i);
            if (!m_owned[idx]) {  
                m_owned[idx] = true;
                anyNewContribution = true;
            }
        }
    }
    // If there are new contributions, perform one bubble up merge
    if (anyNewContribution) {
        BubbleUpMerge();
    }
}

// Check if has certain node's key contribution
bool KeyGenerationTree::HasContribution(uint32_t contributorId) const
{
    if (contributorId >= m_leafCount)
        throw std::out_of_range("contributorId >= networkSize");
    
    return m_owned[LeafToNodeIndex(contributorId)];
}

// Check if has complete group key
bool KeyGenerationTree::HasCompleteKey() const
{
    return m_owned[0];
}

// Get collected key contribution count
uint32_t KeyGenerationTree::GetContributionCount() const
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < m_leafCount; ++i) {
        if (m_owned[LeafToNodeIndex(i)]) {
            count++;
        }
    }
    return count;
}

// Convert tree state to string
std::string KeyGenerationTree::TreeToString() const
{
    std::string treeString;
    treeString.reserve(m_nodeCount);
    
    for (uint32_t i = 0; i < m_nodeCount; ++i) {
        treeString.push_back(m_owned[i] ? '1' : '0');
    }
    
    return treeString;
}

// Restore tree state from string
void KeyGenerationTree::StringToTree(const std::string& treeString)
{
    if (treeString.size() != m_nodeCount) {
        throw std::invalid_argument("Invalid tree string size");
    }
    
    for (uint32_t i = 0; i < m_nodeCount; ++i) {
        m_owned[i] = (treeString[i] == '1');
    }
}

// Merge contributions from another tree
void KeyGenerationTree::MergeTree(const KeyGenerationTree& otherTree)
{
    if (m_leafCount != otherTree.m_leafCount) {
        throw std::invalid_argument("Trees have different network sizes");
    }
        for (uint32_t i = 0; i < m_leafCount; ++i) {
        if (otherTree.HasContribution(i)) {
            AddContribution(i);
        }
    }
}

// Generate forwarding contribution flag string
std::string KeyGenerationTree::GetForwardingContributions(const KeyGenerationTree& neighborTree) const
{
    std::string forwardingContributions(m_leafCount, '0');
    if (HasCompleteKey()) {
        for (uint32_t i = 0; i < m_leafCount; ++i) {
            forwardingContributions[i] = '1';
        }
        return forwardingContributions;
    }
    for (uint32_t i = 0; i < m_leafCount; ++i) {
        if (HasContribution(i) && !neighborTree.HasContribution(i)) {
            forwardingContributions[i] = '1';
        }
    }
    return forwardingContributions;
} 