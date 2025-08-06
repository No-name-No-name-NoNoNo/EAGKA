#ifndef KEY_MATRIX_H
#define KEY_MATRIX_H

#include <vector>
#include <set>
#include <string>
#include <stdint.h>

class KeyMatrix
{
public:
  KeyMatrix();
  KeyMatrix(uint32_t networkSize, uint32_t nodeId);
  ~KeyMatrix();
  

  void InitializeMatrix(uint32_t networkSize, uint32_t nodeId);
  bool HasKeyContribution(uint32_t AnyNodeId_i, uint32_t AnyNodeId_j) const;
  void ReceiveKeyContribution(uint32_t contributorId);
  void MergeMatrix(const KeyMatrix& ReceivedMatrix);
  double CalculateCR(uint32_t NeighborId) const;
  double CalculateFD(uint32_t ContributorId) const;
  double RandomVariable() const;
  std::string GetForwardingContributions(uint32_t NeighborId) const;

  bool IsFull1() const;
  bool SelfIsFull1() const;
  std::string MatrixToString() const;
  KeyMatrix StringToMatrix(const std::string& matrixString) const;

private:
  std::vector<std::vector<bool> > m_matrix; ///< Key contribution matrix, a 2D array where m_matrix[i][j] indicates if node i has node j's key contribution
  uint32_t m_networkSize;                  ///< Network node count
  uint32_t m_nodeId;                       ///< Current node ID
};

#endif /* KEY_MATRIX_H */ 