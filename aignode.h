#ifndef AIGNODE_H
#define AIGNODE_H

#include <assert.h>
#include <string>
#include "hash_set.h"
#include "hash_map.h"

using namespace std;

enum DependenceStatus
{
   NOTSET,
   DEPENDENT,
   NOTDEPENDENT
};

enum NodeType
{
   AIGCONST,
   AIGINPUT,
   AIGOUTPUT,
   AIGAND,
   AIGLATCH,
   AIGOR
};

struct eqNode
{
  bool operator()(const unsigned n1, const unsigned n2) const
  {
    if(n1 == n2)
      return true;
    else
      return false;
  }
};

class AigDef;

class AigNode {

public:
  AigNode(unsigned index, bool out_pol, AigNode* left, bool left_pol, AigNode* right, bool right_pol, NodeType type);
  AigNode(unsigned index);
  ~AigNode();

  // Accessor Methods
  bool get_lpol(void) const;
  bool get_rpol(void) const;
  unsigned get_index(void) const;
  unsigned get_refcount(void) const;
  unsigned get_dependence() const;
  unsigned get_type(void) const;
  AigNode* get_left(void) const;
  AigNode* get_right(void) const;

  bool operator==(const AigNode& other) const;
  size_t hash_key(void) const;

  unsigned ref_inc(void);
  unsigned ref_dec(void);

  unsigned set_dependence(DependenceStatus status);

  bool is_const() const;
  bool is_input() const;
  bool is_and() const;
  bool is_output() const;
  bool is_latch() const;

private:
  unsigned refcount;
  unsigned index;
  bool left_pol;
  bool right_pol;
  bool out_pol;
  AigNode* left;
  AigNode* right;
  DependenceStatus dependence;
  NodeType nodeType;
};

typedef AigNode Node;

struct node_hash {
  size_t operator()(const AigNode*) const;
};

struct node_eq
{
  bool operator()(const AigNode* n1,  const AigNode* n2) const;
};

struct eq_node
{
  bool operator()(const AigNode* n1, const AigNode* n2) const
  {
    if(n1 == n2)
      return true;
    else
      return false;
  }
};

typedef hash_map<const unsigned, AigNode*, hash<unsigned>, eqNode> NodeMap;
typedef hash_set<AigNode*, node_hash, node_eq> NodeSet;
typedef hash_map<unsigned, bool, hash<unsigned>, eqNode> valMap;
#endif

