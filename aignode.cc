#include "aignode.h"
#include "aig.h"

AigNode::AigNode(unsigned index, bool out_pol, AigNode* left, bool left_pol, AigNode* right, bool right_pol, NodeType type)
{
  if(right != 0 && right < left){
    AigNode* tempNode = left;
    bool temp_pol = left_pol;

    this->left = right;
    this->left_pol = right_pol;

    this->right = tempNode;
    this->right_pol = temp_pol;
  }
  else{
    this->left = left;
    this->left_pol = left_pol;
    this->right = right;
    this->right_pol = right_pol;
  }

  this->index = index;
  this->refcount = 0;
  this->out_pol = out_pol;
  this->dependence = (DependenceStatus)0;
  this->nodeType = type;

  if(this->left){
    this->left->ref_inc();
  }

  if(this->right){
    this->right->ref_inc();
  }
}

AigNode::AigNode(unsigned index)
{
  this->index = index;
  this->left = 0;
  this->left_pol = false;
  this->right = 0;
  this->right_pol = false;
  this->out_pol = false;
  this->refcount = 0;
  this->dependence = (DependenceStatus)0;

  if(index == 0 || index == numeric_limits<unsigned>::max())
    this->nodeType = AIGCONST;
  else
    this->nodeType = AIGINPUT;
}

AigNode::~AigNode()
{
  if(left) {
    unsigned cur_lrc = left->ref_dec();
    if(cur_lrc==0) delete left;
  }

  if(right) {
    unsigned cur_rrc = right->ref_dec();
    if(cur_rrc==0) delete right;
  }
}

bool AigNode::operator==(const AigNode& other) const
{
  // If both nodes are terminal, compare their indices.
  if(left==0 && right==0 && other.left==0 && other.right==0)
    return index==other.index;

  // If one is terminal while the other one is not, always return false.
  if(left && right && other.left==0 && other.right==0)
    return false;

  if(left==0 && right==0 && other.left && other.right)
    return false;

  // If both are not terminal, compare their children.
  if(left==(other.left) && left_pol==other.left_pol && right==(other.right) && right_pol==other.right_pol && out_pol==other.out_pol)
    return true;

  // Otherwise, 'this' not equal to 'other'.
  return false;
}

size_t AigNode::hash_key(void) const
{
  //TODO smaller string?
  char key[4096];

  if(this->is_input() || this->is_const() || this->is_latch()){
    hash<unsigned> h;
    return h(index);
  }
  else if(this->is_and() || this->is_output()){
    hash<const char*> h;
    sprintf(key, "%d:%p#%d:%p#%d", out_pol, left, left_pol, right, right_pol);
    return h(key);
  }
  else{
    cerr << "[aignode.cc hash_key] invalid node type" << endl;
    exit(1);
  }
}

unsigned AigNode::ref_inc(void)
{
  return ++refcount;
}

unsigned AigNode::ref_dec(void)
{
  return --refcount;
}

unsigned AigNode::get_index(void) const{
  return this->index;
}

AigNode* AigNode::get_left(void) const{
  return this->left;
}

AigNode* AigNode::get_right(void) const{
  return this->right;
}

bool AigNode::get_lpol(void) const{
  return this->left_pol;
}

bool AigNode::get_rpol(void) const{
  return this->right_pol;
}

unsigned AigNode::get_refcount(void) const{
  return refcount;
}

unsigned AigNode::get_type(void) const{
  return this->nodeType;
}

unsigned AigNode::get_dependence() const{
  return this->dependence;
}


unsigned AigNode::set_dependence(DependenceStatus status){
  this->dependence = status;
}

bool AigNode::is_const() const{
  return this->nodeType == AIGCONST;
}

bool AigNode::is_input() const{
  return this->nodeType == AIGINPUT;
}

bool AigNode::is_and() const{
  return this->nodeType == AIGAND;
}

bool AigNode::is_output() const{
  return this->nodeType == AIGOUTPUT;
}

bool AigNode::is_latch() const{
  return this->nodeType == AIGLATCH;
}

size_t node_hash::operator()(const AigNode* node) const
{
  return node->hash_key();
}

bool node_eq::operator()(const AigNode* n1,  const AigNode* n2) const
{
  return *n1==*n2;
}


