#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include "aig.h"
#include "aiger_cc.h"
#include "hash_map.h"

typedef hash_map<const unsigned, aiger_and*, hash<unsigned>, eqNode> AndMap;

unsigned aiger_literal(unsigned lit)
{
  return (lit/2) * 2;
}

unsigned aig_index(unsigned lit)
{
  return (lit/2);
}

bool polarity(unsigned lit)
{
  unsigned index = lit/2;

  if((index*2) == lit)
    return false;
  else
    return true;
}

void traverse_nodes(AigDef& mgr, NodeMap& aigNodes, AndMap& aigerAndNodes, aiger_and* createNode){
  unsigned index;
  bool rpol, lpol;
  AigNode* left;
  AigNode* right;

  //check if node has already been created
  index = aig_index(createNode->lhs);
  if(aigNodes[index] != NULL)
    return;

  index = aig_index(createNode->rhs0);
  left = aigNodes[index];
  if(left == NULL){
    index = aiger_literal(createNode->rhs0);
    traverse_nodes(mgr, aigNodes, aigerAndNodes, aigerAndNodes[index]);

    index = aig_index(createNode->rhs0);
    left = aigNodes[index];
    if(left == NULL){
      cerr << "left == null " << index << endl;
      exit(1);
    }
  }
  lpol = polarity(createNode->rhs0);

  index = aig_index(createNode->rhs1);
  right = aigNodes[index];
  if(right == NULL){
    index = aiger_literal(createNode->rhs1);
    traverse_nodes(mgr, aigNodes, aigerAndNodes, aigerAndNodes[index]);

    index = aig_index(createNode->rhs1);
    right = aigNodes[index];
    if(right == NULL){
      cerr << "right == null " << index << endl;
      exit(1);
    }
  }
  rpol = polarity(createNode->rhs1);

  index = aig_index(createNode->lhs);
  aigNodes[index] = mgr.NewAndNode(left, lpol, right, rpol, index);
}

AigNode* aiger_to_aig(AigDef &mgr, aiger* aiger, vector<AigNode*> &latches, vector<AigNode*> &inputs, vector<AigNode*> &latchLogic, bool verbose){
  unsigned i, index, latchNext;
  bool lpol, rpol;
  AigNode* left;
  AigNode* right;
  AigNode* next;
  AigNode* latch;
  AigNode* f;
  NodeMap aigNodes;
  AndMap aigerAndNodes;

  if(verbose)
    cout << "     * creating " << aiger->num_inputs << " input nodes" << endl;

  // create primary inputs
  for(i=0; i<aiger->num_inputs; i++){
    index = aig_index(aiger->inputs[i].lit);
    f = mgr.NewInputNode(index);
    aigNodes[index] = f;
//    inputs[f->get_index()] = f;
    inputs.push_back(f);
  }

  if(verbose)
    cout << "     * storing aiger and nodes" << endl;

  // store aiger 'and' node in hashmap
  for(i=0; i<aiger->num_ands; i++){
    aigerAndNodes[aiger->ands[i].lhs] = &(aiger->ands[i]);
  }

  if(verbose)
    cout << endl << "     * creating " << aiger->num_latches << " latch nodes" << endl;

  // create latch nodes
  for(i=0; i<aiger->num_latches; i++){
    index = aig_index(aiger->latches[i].lit);
    f = mgr.NewLatchNode(index);
    aigNodes[index] = f;
//    latches[f->get_index()] = f;
    latches.push_back(f);
  }

  if(verbose)
    cout << "     * creating " << aigerAndNodes.size() << " and nodes" << endl;

  // traverse and create aig 'and' nodes
  AndMap::iterator it = aigerAndNodes.begin();
  for (; it != aigerAndNodes.end(); ++it){
    //check if node has been created
    index = aig_index(it->first);
    if(aigNodes[index] == NULL)
      traverse_nodes(mgr, aigNodes, aigerAndNodes, it->second);
  }

  if(verbose)
    cout << "     * creating " << aiger->num_outputs << " output nodes" << endl;

  // create output nodes
  for(i=0; i<aiger->num_outputs; i++){
    index = aig_index(aiger->outputs[i].lit);
    lpol = polarity(aiger->outputs[i].lit);

    left = aigNodes[index];

    if(lpol)
      f = mgr.NewAndNode(left, lpol, mgr.One(), false);
    else
      f = left;
  }

  //TODO map latch node to logic cone and create next state var
  for(i=0; i<aiger->num_latches; i++){
    index = aig_index(aiger->latches[i].lit);
    latch = aigNodes[index];

    if(aiger->latches[i].next == 1){
      rpol = false;
      right = mgr.One();
    }
    else if(aiger->latches[i].next == 0){
      rpol = false;
      right = mgr.Zero();
    }
    else{
      latchNext = aig_index(aiger->latches[i].next);
      rpol = polarity(aiger->latches[i].next);
      right = aigNodes[latchNext];
    }

    AigNode* logicCone;
    if(rpol)
      logicCone = mgr.NewAndNode(right, rpol, mgr.One(), false);
    else
      logicCone = right;

    latchLogic.push_back(logicCone);
  }

  return f;
}

int main(int argc, char *argv[])
{
  bool src = false;
  bool dst = false;
  bool in = false;
  bool cycles = false;
  bool verbose = false;
  int iterations = 10000;
  string aigerFile;
  string outputFile;
  string inputFile;
  aiger* aiger;
  AigDef mgr;
  vector<AigNode*> inputs;
  vector<AigNode*> latches;
  vector<AigNode*> latchLogic;

  for (int i = 1; i < argc; i++)
  {
    if(cycles){
      iterations = atoi(argv[i]);

      if(!iterations){
        cerr << USAGE << endl;
        exit (1);
      }
      cycles = false;
    }
    else if (!strcmp (argv[i], "-h"))
    {
      cerr << USAGE << endl;
      exit (0);
    }
    else if(!strcmp(argv[i], "-v"))
      verbose = true;
    else if(!strcmp(argv[i], "-c"))
      cycles = true;
    else if (argv[i][0] == '-'){
      cerr << "[main.cc main] invalid command line option " << argv[i] << endl;
      cerr << USAGE << endl;
      exit (1);
    }
    else if (!src){
      aigerFile = argv[i];
      src = true;
    }
    else if(!dst){
      outputFile = argv[i];
      dst = true;
    }
    else if(!in){
      inputFile = argv[i];
      in = true;
    }
    else{
      cerr << USAGE << endl;
      exit (1);
    }
  }

  if(!in){
    cerr << USAGE << endl;
    exit (1);
  }

  if(verbose)
    cout << " *** creating aiger data structure" << endl;

  aiger = read_aiger(aigerFile.c_str());

  if(verbose)
    cout << " *** converting aiger to aig" << endl;

  AigNode* f = aiger_to_aig(mgr, aiger, latches, inputs, latchLogic, verbose);

  if(verbose)
    cout << endl << " *** cleaning up nodes" << endl;

  mgr.clean();

  if(verbose)
    cout << " *** deleted aiger data structure" << endl << endl;

  delete aiger;

  if(verbose)
    cout << " *** sim" << endl;

  mgr.sim(f, latches, inputs, latchLogic, inputFile, outputFile);
}
