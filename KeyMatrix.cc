#include "KeyMatrix.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstdlib> 
#include <stdint.h>
#include <cmath>

double KeyMatrix::RandomVariable() const
{ 
    double random = static_cast<double>(rand()) / RAND_MAX ;
    return random;
}

KeyMatrix::KeyMatrix() : m_networkSize(0), m_nodeId(0) {
}

KeyMatrix::KeyMatrix(uint32_t networkSize, uint32_t nodeId) : m_networkSize(networkSize), m_nodeId(nodeId) {
  m_matrix.resize(m_networkSize);
  for (uint32_t i = 0; i < m_networkSize; i++) {
    m_matrix[i].resize(m_networkSize);
    for (uint32_t j = 0; j < m_networkSize; j++) {
      m_matrix[i][j] = (i == j);
    } 
  }   
}

KeyMatrix::~KeyMatrix() {
}

void KeyMatrix::InitializeMatrix(uint32_t networkSize, uint32_t nodeId) {
  m_networkSize = networkSize;
  m_nodeId = nodeId;
  m_matrix.resize(m_networkSize);
  for (uint32_t i = 0; i < m_networkSize; i++) {
    m_matrix[i].resize(m_networkSize);
    for (uint32_t j = 0; j < m_networkSize; j++) {
      m_matrix[i][j] = (i == j);
    } 
  }   
}

// Check if a node has certain key contribution
bool KeyMatrix::HasKeyContribution(uint32_t AnyNodeId_i, uint32_t AnyNodeId_j) const
{   
  return m_matrix[AnyNodeId_i][AnyNodeId_j];
}   

// Receive key contribution
void KeyMatrix::ReceiveKeyContribution(uint32_t contributorId)
{
    m_matrix[m_nodeId][contributorId] = true;
}

// Check if KeyMatrix is all 1
bool KeyMatrix::IsFull1() const
{
  for (uint32_t i = 0; i < m_networkSize; i++) {
    for (uint32_t j = 0; j < m_networkSize; j++) {
      if (m_matrix[i][j] != true) {
        return false;   
      }
    }
  }
  return true;
}   

// Merge received KeyMatrix to local KeyMatrix
void KeyMatrix::MergeMatrix(const KeyMatrix& ReceivedMatrix)
{
  for (uint32_t i = 0; i < m_networkSize; i++) {
    for (uint32_t j = 0; j < m_networkSize; j++) {
      // If received matrix element is true, merge to local matrix
      m_matrix[i][j] = m_matrix[i][j] || ReceivedMatrix.m_matrix[i][j];
    }
  }
}

// Calculate Complement Rate (CR)  
double KeyMatrix::CalculateCR(uint32_t NeighborId) const
{
  uint32_t diffCount = 0;  
  uint32_t unionCount = 0; 

  for (uint32_t j = 0; j < m_networkSize; j++) {
    if (m_matrix[m_nodeId][j] && !m_matrix[NeighborId][j]) {
      diffCount++;
    }
    if (m_matrix[m_nodeId][j] || m_matrix[NeighborId][j]) {
      unionCount++;
    }
  }
  
  return static_cast<double>(diffCount) / unionCount;
}

// Calculate Forwarding Degree (FD)   
double KeyMatrix::CalculateFD(uint32_t ContributorId) const
{
  uint32_t receivedCount = 0; 
  for (uint32_t i = 0; i < m_networkSize; i++) {
    if (m_matrix[i][ContributorId]) {
      receivedCount++;
    }
  }
  return static_cast<double>(receivedCount) / m_networkSize;
}

// Check if self has all key contributions
bool KeyMatrix::SelfIsFull1() const
{
  for (uint32_t i = 0; i < m_networkSize; i++) {
    if (!m_matrix[m_nodeId][i]) {
      return false;
    }
  }
  return true;
}

std::string KeyMatrix::GetForwardingContributions(uint32_t NeighborId) const
{
  std::string forwardingContributions(m_networkSize, '0');

  if (SelfIsFull1()) {
    for (uint32_t i = 0; i < m_networkSize; i++) {
      forwardingContributions[i] = '1';
    }
  }  
  else {
    double cr = CalculateCR(NeighborId);
    if (cr > RandomVariable()) {
      for (uint32_t i = 0; i < m_networkSize; i++) {
        double fd = CalculateFD(i);
        if (m_matrix[m_nodeId][i] && !m_matrix[NeighborId][i] && fd < RandomVariable()) {
          forwardingContributions[i] = '1';
        }
      }
    }
  }
  return forwardingContributions;
}


// Convert matrix to string
std::string KeyMatrix::MatrixToString() const {
  std::string matrixString;
  for (uint32_t i = 0; i < m_networkSize; i++) {
    for (uint32_t j = 0; j < m_networkSize; j++) {
      // Use std::ostringstream instead of std::to_string
      std::ostringstream ss;
      ss << (m_matrix[i][j] ? 1 : 0);
      matrixString += ss.str();
    }
  }
  return matrixString;
}

// Convert string to matrix
KeyMatrix KeyMatrix::StringToMatrix(const std::string& matrixString) const {
  KeyMatrix result;
  result.InitializeMatrix(m_networkSize, m_nodeId);
  
  for (uint32_t i = 0; i < m_networkSize; i++) {
    for (uint32_t j = 0; j < m_networkSize; j++) {
      if (matrixString[i * m_networkSize + j] == '1') {
        result.m_matrix[i][j] = true;
      } else {
        result.m_matrix[i][j] = false;
      }
    }
  }
  return result;
}
