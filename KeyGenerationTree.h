#ifndef KEY_GENERATION_TREE_H
#define KEY_GENERATION_TREE_H

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <string>

class KeyGenerationTree
{
public:
    explicit KeyGenerationTree(uint32_t networkSize, uint32_t nodeId);
    void AddMultipleContributions(const std::string& contributionString);
    bool HasContribution(uint32_t contributorId) const;
    bool HasCompleteKey() const;
    std::vector<uint32_t> GetOwnedNodes() const;
    uint32_t GetNodeCount() const;
    uint32_t GetContributionCount() const;
    std::string TreeToString() const;
    void StringToTree(const std::string& treeString);
    void MergeTree(const KeyGenerationTree& otherTree);
    std::string GetForwardingContributions(const KeyGenerationTree& neighborTree) const;

private:
    // Complete binary tree index utility functions
    static inline uint32_t CeilLog2(uint32_t n);
    static inline uint32_t LeftChild(uint32_t i) { return 2 * i + 1; }
    static inline uint32_t RightChild(uint32_t i) { return 2 * i + 2; }
    static inline uint32_t Parent(uint32_t i) { return (i - 1) / 2; }
    
    uint32_t LeafToNodeIndex(uint32_t leafIdx) const;
    uint32_t NodeToLeafIndex(uint32_t nodeIdx) const;
    
    // Bubble up merge
    void BubbleUpMerge();
    
    // Member data
    uint32_t m_nodeId;           // Current node ID
    uint32_t m_leafCount;        // Real leaf count (network node count)
    uint32_t m_capacity;         // Ceiling 2^d leaf count
    uint32_t m_depth;            // Tree depth
    uint32_t m_nodeCount;        // Total node count
    std::vector<bool> m_owned;   // Mark which nodes are owned (bitmap)
};

#endif /* KEY_GENERATION_TREE_H */ 