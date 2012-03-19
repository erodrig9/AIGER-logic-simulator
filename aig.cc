#include <fstream>
#include <iostream>
#include "aig.h"

// test

unsigned AigDef::aigerIndex(unsigned lit) {
  if(lit == 0)
    return lit;

  return lit * 2;
}

AigDef::AigDef() {
  AigNode *node0 = new AigNode(0);
  Node0 = node0;
  Node0->ref_inc();
  NodeTbl.insert(Node0);

  AigNode *node1 = new AigNode(numeric_limits<unsigned>::max());
  Node1 = node1;
  Node1->ref_inc();
  NodeTbl.insert(Node1);

  indexCount = 1;
}

AigDef::~AigDef() {

}

// Call when nodes for primary inputs need to be created.
AigNode* AigDef::NewInputNode(unsigned index) {
  AigNode* new_node = new AigNode(index);

  pair<NodeSet::iterator, bool> pr;
  pr = NodeTbl.insert(new_node);

  indexCount++;
  if(pr.second)
    return new_node;

  return *(pr.first);
}

// Call when nodes for latch need to be created.
//TODO change latch node?
AigNode* AigDef::NewLatchNode(unsigned index) {
  AigNode* new_node = new AigNode(index, false, 0, false, 0, false, AIGLATCH);

  pair<NodeSet::iterator, bool> pr;
  pr = NodeTbl.insert(new_node);

  indexCount++;
  if(pr.second){
    return new_node;
  }

  return *(pr.first);
}

// Call when 'and' node need to be created.
AigNode* AigDef::NewAndNode(AigNode* left, bool lpol, AigNode* right, bool rpol, unsigned index) {
  if (left == Node0 && right == Node0) {
    if (lpol && rpol)
      return Node1;
    else
      return Node0;
  } else if (left == Node1 && right == Node1) {
    if (!lpol && !rpol)
      return Node1;
    else
      return Node0;
  } else if (left == Node0 && right == Node1) {
    if (lpol && !rpol)
      return Node1;
    else
      return Node0;
  } else if (left == Node1 && right == Node0) {
    if (!lpol && rpol)
      return Node1;
    else
      return Node0;
  }

  if (left == Node0) {
    if (lpol && !rpol)
      return right;
    else if (!lpol)
      return Node0;
    else if(lpol){
      lpol = false;
      left = Node1;
    }
  } else if (left == Node1) {
    if (lpol)
      return Node0;
    else if (!lpol && !rpol)
      return right;
  } else if (right == Node0) {
    if (!lpol && rpol)
      return left;
    else if (!rpol)
      return Node0;
    else if(rpol){
      rpol = false;
      right = Node1;
    }
  } else if (right == Node1) {
    if (rpol)
      return Node0;
    else if (!lpol && !rpol)
      return left;
  }

  if (left == right) {
    if (!lpol && !rpol)
      return left;
    else if (lpol != rpol)
      return Node0;
    else{
      left = Node1;
      lpol = false;
    }
  }

  AigNode* new_node = new AigNode(index, false, left, lpol, right, rpol, AIGAND);

  pair<NodeSet::iterator, bool> pr;
  pr = NodeTbl.insert(new_node);

  indexCount++;
  if(pr.second)
    return new_node;

  left->ref_dec();
  right->ref_dec();
  return *(pr.first);
}

AigNode* AigDef::NewAndNode(AigNode* left, bool lpol, AigNode* right, bool rpol) {
  this->NewAndNode(left, lpol, right, rpol, indexCount);
}

bool AigDef::recursiveSim(AigNode* node, valMap &terminalValues, vector<AigNode*> &traversedNodes){
  bool lval, rval;

  if(node->get_dependence() == DEPENDENT)
    return true;
  else if(node->get_dependence() == NOTDEPENDENT)
    return false;
  if(node == Node0){
    return false;
  }
  else if(node == Node1){
    return true;
  }
  else if(node->is_input() || node->is_latch()){
    return terminalValues[node->get_index()];
  }
  else if (node->is_output()){
    lval = recursiveSim(node->get_left(), terminalValues, traversedNodes);
    if(node->get_lpol())
      lval ? lval = false : lval = true;

    return lval;
  }
  else if(node->is_and()){
    lval = recursiveSim(node->get_left(), terminalValues, traversedNodes);
    if(node->get_lpol())
      lval ? lval = false : lval = true;

    rval = recursiveSim(node->get_right(), terminalValues, traversedNodes);
    if(node->get_rpol())
      rval ? rval = false : rval = true;

    traversedNodes.push_back(node);
    if(lval && rval){
      node->set_dependence(DEPENDENT);
      return true;
    }
    else{
      node->set_dependence(NOTDEPENDENT);
      return false;
    }
  }
  else{
    cerr << "[aig.cc recursiveSim] Unknown node type" << endl;
    exit(1);
  }
}

void AigDef::sim(AigNode* function, vector<AigNode*> &latches, vector<AigNode*> &inputs, vector<AigNode*> &latchLogic, string inputFile, string outputFile){
  bool nextState;
  bool latchValues[(int)latches.size()];
  int currentCycle = 0;
  int latchIndex = 0;
  char inputValue;
  string line;
  AigNode* latch, *nsFunction;
  vector<AigNode*> traversedNodes;
  valMap terminalValues;

  // initialize latch values;
  for(int i = 0; i< latches.size(); i++){
    terminalValues[latches[i]->get_index()] = false;
  }

  if(!function){
    cerr << "[aig.cc sim] NULL function node" << endl;
    exit(1);
  }
  else if(function->is_const()){
    cerr << "[aig.cc sim] function is constant" << endl;
    return;
  }
  else if(function->is_input()){
    cerr << "[aig.cc sim] function is input" << endl;
    return;
  }

  ifstream in(inputFile.c_str(), ios::in);
  if(!in.is_open()){
    cerr << "Unable to open file " << inputFile << endl;
    exit(1);
  }

  ofstream out(outputFile.c_str());
    if(!out.is_open()){
      cerr << "Unable to open file " << outputFile << endl;
      exit(1);
    }

  while(true){
    currentCycle++;

    if(in.eof()){
      break;
    }

    getline(in, line);

    if(line.size()-1 != inputs.size()){
      break;
    }

    const char* inputValues = line.c_str();

    //set input values
    for (int i = 0; i < inputs.size(); i++) {
      if(inputValues[i] == '0'){
        terminalValues[inputs[i]->get_index()] = false;
        out << "0";
      }
      else if(inputValues[i] == '1'){
        terminalValues[inputs[i]->get_index()] = true;
        out << "1";
      }
      else{
        cerr << "Invalid input value on line " << currentCycle << endl;
        exit(1);
      }
    }

    out << " ";
    // for each latch sim cycle
    for (int j = 0; j < latchLogic.size(); j++){
      latch = latches[j];

      if(terminalValues[latch->get_index()])
        out << "1";
      else
        out << "0";

      nextState = recursiveSim(latchLogic[j], terminalValues, traversedNodes);
      for(int i = 0; i < traversedNodes.size(); i++){
        traversedNodes[i]->set_dependence(NOTSET);
      }
      traversedNodes.clear();

      if(latch->get_rpol())
        nextState ? nextState = false : nextState = true;

      latchValues[j] = nextState;
    }


    // set latch current state value
    latchIndex = 0;
    for(int i = 0; i < latches.size(); i++){
      terminalValues[latches[i]->get_index()] = latchValues[i];
    }

    out << " " << recursiveSim(function, terminalValues, traversedNodes) << endl;
    for(int i = 0; i < traversedNodes.size(); i++){
      traversedNodes[i]->set_dependence(NOTSET);
    }
    traversedNodes.clear();
  }

  in.close();
  out.close();
}
//
//void AigDef::sim(AigNode* function, unsigned cycles, NodeMap &latches, NodeMap &inputs, ostream &out){
//  bool nextState;
//  bool latchValues[(int)latches.size()];
//  int currentCycle = 0;
//  int latchIndex = 0;
//  AigNode* latch, *nsFunction;
//  vector<AigNode*> traversedNodes;
//  NodeMap::iterator it;
//
//  if(!function){
//    cerr << "[aig.cc state_space_traversal] NULL function node" << endl;
//    exit(1);
//  }
//  else if(function->is_const()){
//    out << "[aig.cc state_space_traversal] function is constant" << endl;
//    return;
//  }
//  else if(function->is_input()){
//    out << "[aig.cc state_space_traversal] function is input" << endl;
//    return;
//  }
//
//  srand((unsigned)time(0));
//
//  while(currentCycle < cycles){
//    currentCycle++;
//
//    //gen & set random input values
//    it = inputs.begin();
//    for (; it != inputs.end(); ++it) {
//      if(rand()%2){
//        it->second->set_left(Node1, false);
//        out << "1";
//      }
//      else{
//        it->second->set_left(Node0, false);
//        out << "0";
//      }
//    }
//
//    out << " ";
//    // for each latch sim cycle
//    latchIndex = 0;
//    it = latches.begin();
//    for (; it != latches.end(); ++it){
//      latch = it->second;
//
//      if(latch->get_left() == Node0)
//        out << "0";
//      else
//        out << "1";
//
//      nextState = recursiveSim(latch->get_right(), traversedNodes);
//      for(int i = 0; i < traversedNodes.size(); i++){
//        traversedNodes[i]->set_dependence(NOTSET);
//      }
//      traversedNodes.clear();
//
//      if(latch->get_rpol())
//        nextState ? nextState = false : nextState = true;
//
//      latchValues[latchIndex++] = nextState;
//    }
//
//    // set latch current state value
//    latchIndex = 0;
//    it = latches.begin();
//    for(; it != latches.end(); ++it){
//      latch = it->second;
//      if(latchValues[latchIndex++])
//        latch->set_left(Node1, false);
//      else
//        latch->set_left(Node0, false);
//    }
//
//    out << " " << recursiveSim(function, traversedNodes) << endl;
//    for(int i = 0; i < traversedNodes.size(); i++){
//      traversedNodes[i]->set_dependence(NOTSET);
//    }
//    traversedNodes.clear();
//  }
//}
//
//void AigDef::sim(AigNode* function, unsigned cycles){
//  bool nextState;
//  int currentCycle = 0;
//  int latchIndex = 0;
//  AigNode* latch, *nsFunction;
//  vector<AigNode*> traversedNodes;
//  NodeMap latches;
//  NodeMap inputs;
//
//  if(!function){
//    cerr << "[aig.cc state_space_traversal] NULL function node" << endl;
//    exit(1);
//  }
//  else if(function->is_const()){
//    cout << "[aig.cc state_space_traversal] function is constant" << endl;
//    return;
//  }
//  else if(function->is_input()){
//    cout << "[aig.cc state_space_traversal] function is input" << endl;
//    return;
//  }
//
//  //find latch variables
//  mapLatches(function, latches);
//  if(latches.size() == 0){
//    cout << "[aig.cc sim] function is combinational" << endl;
//    return;
//  }
//
//  //find input variables
//  mapInputs(function, inputs);
//
//  //temporary storage
//  bool latchValues[(int)latches.size()];
//
//  //find input variables
//  NodeMap::iterator it = latches.begin();
//  for (; it != latches.end(); ++it) {
//    latch = it->second;
//    mapInputs(latch->get_right(), inputs);
//
//    if(latch->get_left() == Node0)
//      nextState = false;
//    else
//      nextState = true;
//
//    latchValues[latchIndex++] = nextState;
//  }
//
//  for(int i = 0; i < inputs.size(); i++){
//    cout << "  ";
//  }
//  cout << " ";
//
//  for(int i = 0; i < latches.size(); ++i){
//    cout << latchValues[i] << " ";
//  }
//  cout << endl;
//
//  srand((unsigned)time(0));
//
//  while(currentCycle < cycles){
//    currentCycle++;
//
//    //gen & set random input values
//    it = inputs.begin();
//    for (; it != inputs.end(); ++it) {
//      if(rand()%2){
//        it->second->set_left(Node1, false);
//        cout << "1 ";
//      }
//      else{
//        it->second->set_left(Node0, false);
//        cout << "0 ";
//      }
//    }
//
//    cout << " ";
//    // for each latch sim cycle
//    latchIndex = 0;
//    it = latches.begin();
//    for (; it != latches.end(); ++it){
//      latch = it->second;
//      nextState = recursiveSim(latch->get_right(), traversedNodes);
//      for(int i = 0; i < traversedNodes.size(); i++){
//        traversedNodes[i]->set_dependence(NOTSET);
//      }
//      traversedNodes.clear();
//
//      if(latch->get_rpol())
//        nextState ? nextState = false : nextState = true;
//
//      cout << nextState << " ";
//      latchValues[latchIndex++] = nextState;
//    }
//
//    // set latch current state value
//    latchIndex = 0;
//    it = latches.begin();
//    for(; it != latches.end(); ++it){
//      latch = it->second;
//      if(latchValues[latchIndex++])
//        latch->set_left(Node1, false);
//      else
//        latch->set_left(Node0, false);
//    }
//
//    cout << " " << recursiveSim(function, traversedNodes) << " " << endl;
//    for(int i = 0; i < traversedNodes.size(); i++){
//      traversedNodes[i]->set_dependence(NOTSET);
//    }
//    traversedNodes.clear();
//  }
//}

void AigDef::clear_flags(void){
  AigNode* curr;

  // clear node dependence flag
  NodeSet::iterator it = NodeTbl.begin();
  for (; it != NodeTbl.end(); ++it) {
    curr = *it;
    curr->set_dependence(NOTSET);
  }
}

void AigDef::clear_flags(vector<AigNode*> &vec){
  AigNode* curr;

  // clear node dependence flag
  for (int i = 0; i < vec.size(); i++) {
    curr = vec[i];
    curr->set_dependence(NOTSET);
  }
}

//clean up dangling nodes
void AigDef::clean(void) {
  int i;
  AigNode* node;
  vector<AigNode*> danglingNodes;

  NodeSet::iterator it = NodeTbl.begin();
  for (; it != NodeTbl.end(); ++it) {
    node = *it;
    if (node->get_refcount() == 0 && !node->is_output() && !node->is_input())
      danglingNodes.push_back(node);
  }

  for (i = 0; i < danglingNodes.size(); i++)
    recursive_erase(danglingNodes[i]);
}

AigNode* AigDef::One(void) const {
  return Node1;
}

AigNode* AigDef::Zero(void) const {
  return Node0;
}

unsigned AigDef::getIndex(){
  return indexCount;
}

// attempt to recursively erase function
void AigDef::recursive_erase(AigNode* node) {
  if(!node)
    cerr << "[aig.cc recursive_erase] NULL node" << endl;

  if(node->get_refcount() != 0)
    return;
  else if(node->is_const())
    return;

  AigNode* l = node->get_left();
  AigNode* r = node->get_right();

  NodeSet::size_type n = NodeTbl.erase(node);
  if(n > 0){
    if (l) {
      l->ref_dec();
      recursive_erase(l);
    }

    if (r) {
      r->ref_dec();
      recursive_erase(r);
    }
  }
}

// erase single node
void AigDef::erase(AigNode* node) {
  if(!node)
      cerr << "[aig.cc erase] NULL node" << endl;

  if(node->get_refcount() != 0)
    return;
  else if(node == Node1 || node == Node0)
    return;

  NodeSet::size_type n = NodeTbl.erase(node);
  if(n > 0){
    if(node->get_right())
      node->get_right()->ref_dec();

    if(node->get_left())
      node->get_left()->ref_dec();
  }
}
