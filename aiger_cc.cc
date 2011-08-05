#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cctype>
#include <cstdarg>
#include "aiger_cc.h"

#define ENLARGE(p,s) \
  do { \
    size_t old_size = (s); \
    size_t new_size = old_size ? 2 * old_size : 1; \
    REALLOCN (priv, p,old_size,new_size); \
    (s) = new_size; \
  } while (0)

#define PUSH(p,t,s,l) \
  do { \
    if ((t) == (s)) \
      ENLARGE (p, s); \
    (p)[(t)++] = (l); \
  } while (0)

#define FIT(p,m,n) \
  do { \
    size_t old_size = (m); \
    size_t new_size = (n); \
    if (old_size < new_size) \
    { \
	    REALLOCN (priv, p,old_size,new_size); \
	    (m) = new_size; \
    } \
  } while (0)


#define NEWN(p,n) \
  do { \
    size_t bytes = (n) * sizeof (*(p)); \
    (p) = (char*)aigtoaig_malloc ((memory*)priv->memory_mgr, bytes); \
    memset ((p), 0, bytes); \
  } while (0)

#define DELETEN(p,n) \
  do { \
    size_t bytes = (n) * sizeof (*(p)); \
    aigtoaig_free((memory*)priv->memory_mgr, (p), bytes); \
    (p) = 0; \
  } while (0)

#define IMPORT_priv_FROM(p) \
  aiger_priv * priv = (aiger_priv*) (p)

#define EXPORT_pub_FROM(p) \
  aiger * pub = &(p)->pub

struct aiger_type
{
  unsigned input:1;
  unsigned latch:1;
  unsigned andNode:1;

  unsigned mark:1;
  unsigned onstack:1;

  /* Index int to 'pub->{inputs,latches,ands}'.
   */
  unsigned idx;
};

struct aiger_priv
{
  aiger pub;

  aiger_type *types;		/* [0..maxvar] */
  unsigned size_types;

  unsigned char * coi;
  unsigned size_coi;

  unsigned size_inputs;
  unsigned size_latches;
  unsigned size_outputs;
  unsigned size_ands;

  unsigned num_comments;
  unsigned size_comments;

  void *memory_mgr;

  char *error;
};

struct aiger_reader
{
  void *state;
  aiger_get get;

  int ch;

  unsigned lineno;
  unsigned charno;

  unsigned lineno_at_last_token_start;

  int done_with_reading_header;
  int looks_like_aag;

  aiger_mode mode;
  unsigned maxvar;
  unsigned inputs;
  unsigned latches;
  unsigned outputs;
  unsigned ands;

  char *buffer;
  unsigned top_buffer;
  unsigned size_buffer;
};

void *
aigtoaig_malloc (memory * m, size_t bytes)
{
  m->bytes += bytes;
  assert (m->bytes);
  if (m->bytes > m->max)
    m->max = m->bytes;
  return malloc (bytes);
}

void
aigtoaig_free (memory * m, void *ptr, size_t bytes)
{
  assert (m->bytes >= bytes);
  m->bytes -= bytes;
  free (ptr);
}

aiger *
aiger_init_mem (memory *memory_mgr)
{
  aiger_priv *priv;
  aiger *pub;

  priv = (aiger_priv*)aigtoaig_malloc(memory_mgr, sizeof (*priv));
  memset (priv, 0, sizeof (*priv));
  priv->memory_mgr = memory_mgr;
  pub = &priv->pub;

  
  if (priv->num_comments == priv->size_comments){
    size_t old_size = priv->size_comments; 
    size_t new_size = old_size ? 2 * old_size : 1; 
    size_t mbytes = old_size * sizeof (*pub->comments); 
    size_t nbytes = new_size * sizeof (*pub->comments); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, pub->comments, minbytes); 
    if (nbytes > mbytes) 
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free ((memory*)priv->memory_mgr, pub->comments, mbytes); 
    pub->comments = (char**)res; 
    priv->size_comments = new_size; 
  }
  pub->comments[priv->num_comments++] = 0; 
  return pub;
}

static int
aiger_default_get (FILE * file)
{
  return getc (file);
}

static const char *
aiger_error_s (aiger_priv * priv, const char *s, const char *a)
{
  unsigned tmp_len, error_len;
  char *tmp;
  assert (!priv->error);
  tmp_len = strlen (s) + strlen (a) + 1;
  NEWN (tmp, tmp_len);  //p, n
  sprintf (tmp, s, a);
  error_len = strlen (tmp) + 1;
  NEWN (priv->error, error_len);  //p,n
  memcpy (priv->error, tmp, error_len);
  DELETEN (tmp, tmp_len); //p,n
  return priv->error;
}

static const char *
aiger_error_u (aiger_priv * priv, const char *s, unsigned u)
{
  unsigned tmp_len, error_len;
  char *tmp;
  assert (!priv->error);
  tmp_len = strlen (s) + sizeof (u) * 4 + 1;
  NEWN (tmp, tmp_len);
  sprintf (tmp, s, u);
  error_len = strlen (tmp) + 1;
  NEWN (priv->error, error_len);
  memcpy (priv->error, tmp, error_len);
  DELETEN (tmp, tmp_len);
  return priv->error;
}

static const char *
aiger_error_uu (aiger_priv * priv, const char *s, unsigned a,
		unsigned b)
{
  unsigned tmp_len, error_len;
  char *tmp;
  assert (!priv->error);
  tmp_len = strlen (s) + sizeof (a) * 4 + sizeof (b) * 4 + 1;
  NEWN (tmp, tmp_len);
  sprintf (tmp, s, a, b);
  error_len = strlen (tmp) + 1;
  NEWN (priv->error, error_len);
  memcpy (priv->error, tmp, error_len);
  DELETEN (tmp, tmp_len);
  return priv->error;
}

const char *
aiger_error (aiger * pub)
{
  IMPORT_priv_FROM (pub);
  return priv->error;
}

static char *
aiger_copy_str (aiger_priv * priv, const char *str)
{
  char *res;

  if (!str || !str[0])
    return 0;

  NEWN (res, strlen (str) + 1);
  strcpy (res, str);

  return res;
}

static int
aiger_literal_defined (aiger_priv * priv, unsigned lit)
{
  unsigned var = aiger_lit2var (lit);
#ifndef NDEBUG
  EXPORT_pub_FROM (priv);
#endif
  aiger_type *type;

  assert (var <= pub->maxvar);
  if (!var)
    return 1;

  type = priv->types + var;

  return type->andNode || type->input || type->latch;
}

static aiger_type *
aiger_import_literal (aiger_priv * priv, unsigned lit)
{
  unsigned var = aiger_lit2var (lit);
  EXPORT_pub_FROM (priv);

  if (var > pub->maxvar)
    pub->maxvar = var;

  while (var >= priv->size_types){
    //ENLARGE (priv->types, priv->size_types);  ENLARGE(p,s)
    size_t old_size = priv->size_types; 
    size_t new_size = old_size ? 2 * old_size : 1; 
    //REALLOCN (priv->types,old_size,new_size);  REALLOCN(p,m,n)
    size_t mbytes = old_size * sizeof (*priv->types); 
    size_t nbytes = new_size * sizeof (*priv->types); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, priv->types, minbytes); 
    if (nbytes > mbytes) \
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free ((memory*)priv->memory_mgr, priv->types, mbytes); 
    priv->types = (aiger_type*)res; 
    priv->size_types = new_size;
  }

  return priv->types + var;
}

void
aiger_add_input (aiger * pub, unsigned lit, const char *name)
{
  IMPORT_priv_FROM (pub);
  aiger_symbol symbol;
  aiger_type *type;

  assert (!aiger_error (pub));

  assert (lit);
  assert (!aiger_sign (lit));

  type = aiger_import_literal (priv, lit);

  assert (!type->input);
  assert (!type->latch);
  assert (!type->andNode);

  type->input = 1;
  type->idx = pub->num_inputs;

  size_t old_size, new_size;
  
  symbol.lit = lit;
  symbol.name = aiger_copy_str (priv, name);
  symbol.next = 0;
  //PUSH (pub->inputs, pub->num_inputs, priv->size_inputs, symbol);  PUSH(p,t,s,l)
  if (pub->num_inputs == priv->size_inputs) {
    //ENLARGE (pub->inputs, priv->size_inputs);  ENLARGE(p,s)
    old_size = priv->size_inputs; 
    new_size = old_size ? 2 * old_size : 1; 
    //REALLOCN (pub->inputs,old_size,new_size);  REALLOCN(p,m,n) 
    size_t mbytes = old_size * sizeof (*pub->inputs); 
    size_t nbytes = new_size * sizeof (*pub->inputs); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, pub->inputs, minbytes); 
    if (nbytes > mbytes) 
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free((memory*)priv->memory_mgr, pub->inputs, mbytes); 
    pub->inputs = (aiger_symbol*)res;
    priv->size_inputs = new_size;
  }

  pub->inputs[pub->num_inputs++] = symbol; 
}

void
aiger_add_latch (aiger * pub,
		 unsigned lit, unsigned next, const char *name)
{
  IMPORT_priv_FROM (pub);
  aiger_symbol symbol;
  aiger_type *type;

  assert (!aiger_error (pub));

  assert (lit);
  assert (!aiger_sign (lit));

  type = aiger_import_literal (priv, lit);

  assert (!type->input);
  assert (!type->latch);
  assert (!type->andNode);

  /* Warning: importing 'next' makes 'type' invalid.
   */
  type->latch = 1;
  type->idx = pub->num_latches;

  aiger_import_literal (priv, next);

  symbol.lit = lit;
  symbol.next = next;
  symbol.name = aiger_copy_str (priv, name);

  //PUSH (pub->latches, pub->num_latches, priv->size_latches, symbol);  PUSH(p,t,s,l)
  if (pub->num_latches == priv->size_latches) {
      //ENLARGE (pub->latches, priv->size_latches);  ENLARGE(p,s)
    size_t old_size = priv->size_latches; 
    size_t new_size = old_size ? 2 * old_size : 1; 
    //REALLOCN (pub->latches,old_size,new_size);  REALLOCN(p,m,n)
    size_t mbytes = old_size * sizeof (*pub->latches); 
    size_t nbytes = new_size * sizeof (*pub->latches); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, pub->latches, minbytes); 
    if (nbytes > mbytes) \
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free ((memory*)priv->memory_mgr, pub->latches, mbytes); 
    pub->latches = (aiger_symbol*)res; 
    priv->size_latches = new_size;  
  }
  pub->latches[pub->num_latches++] = symbol;
}

void
aiger_add_output (aiger * pub, unsigned lit, const char *name)
{
  IMPORT_priv_FROM (pub);
  aiger_symbol symbol;
  aiger_import_literal (priv, lit);
  symbol.lit = lit;
  symbol.name = aiger_copy_str (priv, name);
  symbol.next = 0;
  //PUSH (pub->outputs, pub->num_outputs, priv->size_outputs, symbol); PUSH(p,t,s,l) 
  if (pub->num_outputs == priv->size_outputs) {
    //ENLARGE (pub->outputs, priv->size_outputs);  ENLARGE(p,s) 
    size_t old_size = priv->size_outputs; 
    size_t new_size = old_size ? 2 * old_size : 1; 
    //REALLOCN (pub->outputs,old_size,new_size);   REALLOCN(p,m,n) 
    size_t mbytes = old_size * sizeof (*pub->outputs); 
    size_t nbytes = new_size * sizeof (*pub->outputs); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, pub->outputs, minbytes); 
    if (nbytes > mbytes) \
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free ((memory*)priv->memory_mgr, pub->outputs, mbytes); 
    pub->outputs = (aiger_symbol*)res; 
    priv->size_outputs = new_size; 
  }
  pub->outputs[pub->num_outputs++] = symbol;
}

void
aiger_add_and (aiger * pub, unsigned lhs, unsigned rhs0, unsigned rhs1)
{
  IMPORT_priv_FROM (pub);
  aiger_type *type;
  aiger_and *andNode;

  assert (!aiger_error (pub));

  assert (lhs > 1);
  assert (!aiger_sign (lhs));

  type = aiger_import_literal (priv, lhs);

  assert (!type->input);
  assert (!type->latch);
  assert (!type->andNode);

  type->andNode = 1;
  type->idx = pub->num_ands;

  aiger_import_literal (priv, rhs0);
  aiger_import_literal (priv, rhs1);

  size_t old_size, new_size;
  
  if (pub->num_ands == priv->size_ands){
    //ENLARGE (pub->ands, priv->size_ands); ENLARGE(p,s)
    old_size = priv->size_ands; 
    new_size = old_size ? 2 * old_size : 1; 
    //REALLOCN (pub->ands,old_size,new_size);  REALLOCN(p,m,n) 
    size_t mbytes = old_size * sizeof (*pub->ands); 
    size_t nbytes = new_size * sizeof (*pub->ands); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, pub->ands, minbytes); 
    if (nbytes > mbytes) \
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free ((memory*)priv->memory_mgr, pub->ands, mbytes); 
    pub->ands = (aiger_and*)res;  
    priv->size_ands = new_size;
  }

  andNode = pub->ands + pub->num_ands;

  andNode->lhs = lhs;
  andNode->rhs0 = rhs0;
  andNode->rhs1 = rhs1;

  pub->num_ands++;
}

static void
aiger_check_next_defined (aiger_priv * priv)
{
  EXPORT_pub_FROM (priv);
  unsigned i, next, latch;
  aiger_symbol *symbol;

  if (priv->error)
    return;

  for (i = 0; !priv->error && i < pub->num_latches; i++)
    {
      symbol = pub->latches + i;
      latch = symbol->lit;
      next = symbol->next;

      assert (!aiger_sign (latch));
      assert (priv->types[aiger_lit2var (latch)].latch);

      if (!aiger_literal_defined (priv, next))
	      aiger_error_uu (priv,
			    "next state function %u of latch %u undefined",
			    next, latch);
    }
}

static void
aiger_check_right_hand_side_defined (aiger_priv * priv, aiger_and * andNode,
				     unsigned rhs)
{
  if (priv->error)
    return;

  assert (andNode);
  if (!aiger_literal_defined (priv, rhs))
    aiger_error_uu (priv, "literal %u in AND %u undefined", rhs, andNode->lhs);
}

static void
aiger_check_right_hand_sides_defined (aiger_priv * priv)
{
  EXPORT_pub_FROM (priv);
  aiger_and *andNode;
  unsigned i;

  if (priv->error)
    return;

  for (i = 0; !priv->error && i < pub->num_ands; i++)
  {
    andNode = pub->ands + i;
    aiger_check_right_hand_side_defined (priv, andNode, andNode->rhs0);
    aiger_check_right_hand_side_defined (priv, andNode, andNode->rhs1);
  }
}

static void
aiger_check_outputs_defined (aiger_priv * priv)
{
  EXPORT_pub_FROM (priv);
  unsigned i, output;

  if (priv->error)
    return;

  for (i = 0; !priv->error && i < pub->num_outputs; i++)
  {
    output = pub->outputs[i].lit;
    output = aiger_strip (output);
    if (output <= 1)
	    continue;

    if (!aiger_literal_defined (priv, output))
	    aiger_error_u (priv, "output %u undefined", output);
  }
}

static void
aiger_check_for_cycles (aiger_priv * priv)
{
  unsigned i, j, *stack, size_stack, top_stack, tmp;
  EXPORT_pub_FROM (priv);
  aiger_type *type;
  aiger_and *andNode;

  if (priv->error)
    return;

  stack = 0;
  size_stack = top_stack = 0; 
  
  for (i = 1; !priv->error && i <= pub->maxvar; i++)
  {
    type = priv->types + i;

    if (!type->andNode || type->mark)
      continue;

    size_t old_size, new_size;
    //PUSH (stack, top_stack, size_stack, i);  PUSH(p,t,s,l) 
    if (top_stack == size_stack){ 
      //ENLARGE (stack, size_stack);  ENLARGE(p,s)
      old_size = size_stack; 
      new_size = old_size ? 2 * old_size : 1; 
      //REALLOCN (stack,old_size,new_size);  REALLOCN(p,m,n)
      size_t mbytes = old_size * sizeof (*stack,old_size); 
      size_t nbytes = new_size * sizeof (*stack,old_size); 
      size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
      void * res = aigtoaig_malloc ((memory*)priv->memory_mgr, nbytes); 
      memcpy (res, stack, minbytes); 
      if (nbytes > mbytes) 
        memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
      aigtoaig_free((memory*)priv->memory_mgr, stack, mbytes); 
      stack = (unsigned *)res;
      size_stack = new_size;
    }
    (stack)[top_stack++] = i; 

    while (top_stack)
	  {
	    j = stack[top_stack - 1];

	    if (j)
	    {
	      type = priv->types + j;
	      if (type->mark && type->onstack)
		    {
		      aiger_error_u (priv,
				    "cyclic definition for and gate %u", j);
		      break;
	      }

	      if (!type->andNode || type->mark)
		    {
		      top_stack--;
		      continue;
		    }

	      /* Prefix code.
	       */
	      type->mark = 1;
	      type->onstack = 1;
	      //PUSH (stack, top_stack, size_stack, 0);
	      if (top_stack == size_stack){ 
          //ENLARGE (stack, size_stack);  ENLARGE(p,s)
          old_size = size_stack; 
          new_size = old_size ? 2 * old_size : 1; 
          //REALLOCN (stack,old_size,new_size);  REALLOCN(p,m,n)
          size_t mbytes = old_size * sizeof (*stack,old_size); 
          size_t nbytes = new_size * sizeof (*stack,old_size); 
          size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
          void * res = aigtoaig_malloc ((memory*)priv->memory_mgr, nbytes); 
          memcpy (res, stack, minbytes); 
          if (nbytes > mbytes) 
            memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
          aigtoaig_free((memory*)priv->memory_mgr, stack, mbytes); 
          stack = (unsigned *)res;
          size_stack = new_size;
        }
        (stack)[top_stack++] = 0;
	      
	      assert (type->idx < pub->num_ands);
	      andNode = pub->ands + type->idx;

	      tmp = aiger_lit2var (andNode->rhs0);
	      if (tmp){
		      //PUSH (stack, top_stack, size_stack, tmp);  PUSH(p,t,s,l)  
          if (top_stack == size_stack){ 
            //ENLARGE (stack, size_stack);  ENLARGE(p,s)
            old_size = size_stack; 
            new_size = old_size ? 2 * old_size : 1; 
            //REALLOCN (stack,old_size,new_size);  REALLOCN(p,m,n)
            size_t mbytes = old_size * sizeof (*stack,old_size); 
            size_t nbytes = new_size * sizeof (*stack,old_size); 
            size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
            void * res = aigtoaig_malloc ((memory*)priv->memory_mgr, nbytes);
            memcpy (res, stack, minbytes);
            if (nbytes > mbytes) 
              memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
            aigtoaig_free((memory*)priv->memory_mgr, stack, mbytes); 
            stack = (unsigned *)res;
            size_stack = new_size;
          }
          (stack)[top_stack++] = tmp; 
		    }
	      tmp = aiger_lit2var (andNode->rhs1);
	      if (tmp){
		      //PUSH (stack, top_stack, size_stack, tmp);  PUSH(p,t,s,l)
		      if (top_stack == size_stack){ 
            //ENLARGE (stack, size_stack);  ENLARGE(p,s)
            old_size = size_stack; 
            new_size = old_size ? 2 * old_size : 1; 
            //REALLOCN (stack,old_size,new_size);  REALLOCN(p,m,n)
            size_t mbytes = old_size * sizeof (*stack,old_size); 
            size_t nbytes = new_size * sizeof (*stack,old_size); 
            size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
            void * res = aigtoaig_malloc ((memory*)priv->memory_mgr, nbytes); 
            memcpy (res, stack, minbytes); 
            if (nbytes > mbytes) 
              memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
            aigtoaig_free((memory*)priv->memory_mgr, stack, mbytes); 
            stack = (unsigned *)res;
            size_stack = new_size;
          }
          (stack)[top_stack++] = tmp;
		    }
	    }
	    else
	    {
	      /* All descendends traversed.  This is the postfix code.
	       */
	      assert (top_stack >= 2);
	      top_stack -= 2;
	      j = stack[top_stack];
	      assert (j);
	      type = priv->types + j;
	      assert (type->mark);
	      assert (type->onstack);
	      type->onstack = 0;
	    }
    }
  }

  DELETEN (stack, size_stack);
}

const char *
aiger_check (aiger * pub)
{
  IMPORT_priv_FROM (pub);

  assert (!aiger_error (pub));
  aiger_check_next_defined (priv);
  aiger_check_outputs_defined (priv);
  aiger_check_right_hand_sides_defined (priv);
  aiger_check_for_cycles (priv);

  return priv->error;
}

static int
aiger_has_suffix (const char *str, const char *suffix)
{
  if (strlen (str) < strlen (suffix))
    return 0;

  return !strcmp (str + strlen (str) - strlen (suffix), suffix);
}

static unsigned
aiger_max_input_or_latch (aiger * pub)
{
  unsigned i, tmp, res;

  res = 0;

  for (i = 0; i < pub->num_inputs; i++)
    {
      tmp = pub->inputs[i].lit;
      assert (!aiger_sign (tmp));
      if (tmp > res)
	res = tmp;
    }

  for (i = 0; i < pub->num_latches; i++)
    {
      tmp = pub->latches[i].lit;
      assert (!aiger_sign (tmp));
      if (tmp > res)
	res = tmp;
    }

  return res;
}

static const char *
aiger_already_defined (aiger * pub, aiger_reader * reader, unsigned lit)
{
  IMPORT_priv_FROM (pub);
  aiger_type *type;
  unsigned var;

  assert (lit);
  assert (!aiger_sign (lit));

  var = aiger_lit2var (lit);
  if (pub->maxvar < var)
    return 0;

  type = priv->types + var;
  if (type->input)
    return aiger_error_uu (priv,
			   "line %u: literal %u already defined as input",
			   reader->lineno_at_last_token_start, lit);

  if (type->latch)
    return aiger_error_uu (priv,
			   "line %u: literal %u already defined as latch",
			   reader->lineno_at_last_token_start, lit);

  if (type->andNode)
    return aiger_error_uu (priv,
			   "line %u: literal %u already defined as AND",
			   reader->lineno_at_last_token_start, lit);

  return 0;
}

static int
aiger_next_ch (aiger_reader * reader)
{
  int res;

  res = reader->get (reader->state);

  if (isspace (reader->ch) && !isspace (res))
    reader->lineno_at_last_token_start = reader->lineno;

  reader->ch = res;

  if (reader->done_with_reading_header && reader->looks_like_aag)
    {
      if (!isspace (res) && !isdigit (res) && res != EOF)
	reader->looks_like_aag = 0;
    }

  if (res == '\n')
    reader->lineno++;

  if (res != EOF)
    reader->charno++;

  return res;
}

/* Read a number assuming that the current character has already been
 * checked to be a digit, e.g. the start of the number to be read.
 */
static unsigned
aiger_read_number (aiger_reader * reader)
{
  unsigned res;

  assert (isdigit (reader->ch));
  res = reader->ch - '0';

  while (isdigit (aiger_next_ch (reader)))
    res = 10 * res + (reader->ch - '0');

  return res;
}

/* Expect and read an unsigned number followed by at least one white space
 * character.  The white space should either the space character or a new
 * line as specified by the 'followed_by' parameter.  If a number can not be
 * found or there is no white space after the number, an apropriate error
 * message is returned.
 */
static const char *
aiger_read_literal (aiger_priv * priv,
		    aiger_reader * reader,
		    unsigned *res_ptr, char followed_by)
{
  unsigned res;

  assert (followed_by == ' ' || followed_by == '\n');

  if (!isdigit (reader->ch))
    return aiger_error_u (priv,
			  "line %u: expected literal", reader->lineno);

  res = aiger_read_number (reader);

  if (followed_by == ' ')
    {
      if (reader->ch != ' ')
	return aiger_error_uu (priv,
			       "line %u: expected space after literal %u",
			       reader->lineno_at_last_token_start, res);
    }
  else
    {
      if (reader->ch != '\n')
	return aiger_error_uu (priv,
			       "line %u: expected new line after literal %u",
			       reader->lineno_at_last_token_start, res);
    }

  aiger_next_ch (reader);	/* skip white space */

  *res_ptr = res;

  return 0;
}

static const char *
aiger_read_header (aiger * pub, aiger_reader * reader)
{
  IMPORT_priv_FROM (pub);
  unsigned i, lit, next;
  const char *error;
  
  aiger_next_ch (reader);
  if (reader->ch != 'a')
    return aiger_error_u (priv,
			  "line %u: expected 'a' as first character",
			  reader->lineno);

  if (aiger_next_ch (reader) != 'i' && reader->ch != 'a')
    return aiger_error_u (priv,
			  "line %u: expected 'i' or 'a' after 'a'",
			  reader->lineno);

  if (reader->ch == 'a')
    reader->mode = aiger_ascii_mode;
  else
    reader->mode = aiger_binary_mode;

  if (aiger_next_ch (reader) != 'g')
    return aiger_error_u (priv,
			  "line %u: expected 'g' after 'a[ai]'",
			  reader->lineno);

  if (aiger_next_ch (reader) != ' ')
    return aiger_error_u (priv,
			  "line %u: expected ' ' after 'a[ai]g'",
			  reader->lineno);

  aiger_next_ch (reader);

  if (aiger_read_literal (priv, reader, &reader->maxvar, ' ') ||
      aiger_read_literal (priv, reader, &reader->inputs, ' ') ||
      aiger_read_literal (priv, reader, &reader->latches, ' ') ||
      aiger_read_literal (priv, reader, &reader->outputs, ' ') ||
      aiger_read_literal (priv, reader, &reader->ands, '\n'))
    {
      assert (priv->error);
      return priv->error;
    }
  
  if (reader->mode == aiger_binary_mode)
    {
      i = reader->inputs;
      i += reader->latches;
      i += reader->ands;

      if (i != reader->maxvar)
	return aiger_error_u (priv,
			      "line %u: invalid maximal variable index",
			      reader->lineno);
    }

  size_t old_size, new_size;
  
  pub->maxvar = reader->maxvar;
  //FIT (priv->types, priv->size_types, pub->maxvar + 1);  FIT(p,m,n)
  old_size = priv->size_types; 
  new_size = pub->maxvar + 1; 
  if (old_size < new_size) 
  { 
	  //REALLOCN (priv->types,old_size,new_size);   REALLOCN(p,m,n)
	  size_t mbytes = old_size * sizeof (*priv->types); 
    size_t nbytes = new_size * sizeof (*priv->types); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, priv->types, minbytes); 
    if (nbytes > mbytes) 
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free ((memory*)priv->memory_mgr, priv->types, mbytes); 
    priv->types = (aiger_type*)res; 
	  priv->size_types = new_size; 
  } 
  
  
  //FIT (pub->inputs, priv->size_inputs, reader->inputs);  FIT(p,m,n)
  old_size = priv->size_inputs; 
  new_size = reader->inputs; 
  if (old_size < new_size) 
  { 
	  //REALLOCN (pub->inputs,old_size,new_size);   REALLOCN(p,m,n) 
	  size_t mbytes = old_size * sizeof (*pub->inputs); 
    size_t nbytes = new_size * sizeof (*pub->inputs); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, pub->inputs, minbytes); 
    if (nbytes > mbytes) 
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free((memory*)priv->memory_mgr, pub->inputs, mbytes); 
    pub->inputs = (aiger_symbol*)res;
	  priv->size_inputs = new_size; 
  } 
  
  //FIT (pub->latches, priv->size_latches, reader->latches);  FIT(p,m,n)
  old_size = priv->size_latches; 
  new_size = reader->latches; 
  if (old_size < new_size) 
  { 
	  //REALLOCN (pub->latches,old_size,new_size); REALLOCN(p,m,n)
	  size_t mbytes = old_size * sizeof (*pub->latches); 
    size_t nbytes = new_size * sizeof (*pub->latches); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, pub->latches, minbytes); 
    if (nbytes > mbytes) 
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free ((memory*)priv->memory_mgr, pub->latches, mbytes); 
    pub->latches = (aiger_symbol*)res; 
	  priv->size_latches = new_size; 
  } 
      
  //FIT (pub->outputs, priv->size_outputs, reader->outputs);  FIT(p,m,n)
  old_size = priv->size_outputs; 
  new_size = reader->outputs; 
  if (old_size < new_size) 
  { 
	  //REALLOCN (pub->outputs,old_size,new_size);  REALLOCN(p,m,n)
	  size_t mbytes = old_size * sizeof (*pub->outputs); 
    size_t nbytes = new_size * sizeof (*pub->outputs); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc ((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, pub->outputs, minbytes); 
    if (nbytes > mbytes) 
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free ((memory*)priv->memory_mgr, pub->outputs, mbytes); 
    pub->outputs = (aiger_symbol*)res;
	  priv->size_outputs = new_size; 
  } 
  
  //FIT (pub->ands, priv->size_ands, reader->ands); FIT(p,m,n)
  old_size = priv->size_ands; 
  new_size = reader->ands; 
  if (old_size < new_size) 
  { 
	  //REALLOCN (pub->ands,old_size,new_size);  REALLOCN(p,m,n)
	  size_t mbytes = old_size * sizeof (*pub->ands); 
    size_t nbytes = new_size * sizeof (*pub->ands); 
    size_t minbytes = (mbytes < nbytes) ? mbytes : nbytes; 
    void * res = aigtoaig_malloc ((memory*)priv->memory_mgr, nbytes); 
    memcpy (res, pub->ands, minbytes); 
    if (nbytes > mbytes) 
      memset (((char*)res) + mbytes, 0, nbytes - mbytes); 
    aigtoaig_free ((memory*)priv->memory_mgr, pub->ands, mbytes); 
    pub->ands = (aiger_and*)res;
	  priv->size_ands = new_size; 
  } 
  
  for (i = 0; i < reader->inputs; i++)
  {
    if (reader->mode == aiger_ascii_mode)
	  {
	    error = aiger_read_literal (priv, reader, &lit, '\n');
	    if (error)
	      return error;

	    if (!lit || aiger_sign (lit)
	      || aiger_lit2var (lit) > pub->maxvar)
	      return aiger_error_uu (priv,
				   "line %u: literal %u is not a valid input",
				   reader->lineno_at_last_token_start, lit);
      
	    error = aiger_already_defined (pub, reader, lit);
	    if (error)
	      return error;
	  }
    else
	    lit = 2 * (i + 1);

    aiger_add_input (pub, lit, 0);
  }

  for (i = 0; i < reader->latches; i++)
    {
      if (reader->mode == aiger_ascii_mode)
	{
	  error = aiger_read_literal (priv, reader, &lit, ' ');
	  if (error)
	    return error;

	  if (!lit || aiger_sign (lit)
	      || aiger_lit2var (lit) > pub->maxvar)
	    return aiger_error_uu (priv,
				   "line %u: literal %u is not a valid latch",
				   reader->lineno_at_last_token_start, lit);

	  error = aiger_already_defined (pub, reader, lit);
	  if (error)
	    return error;
	}
      else
	lit = 2 * (i + reader->inputs + 1);

      error = aiger_read_literal (priv, reader, &next, '\n');
      if (error)
	return error;

      if (aiger_lit2var (next) > pub->maxvar)
	return aiger_error_uu (priv,
			       "line %u: literal %u is not a valid literal",
			       reader->lineno_at_last_token_start, next);

      aiger_add_latch (pub, lit, next, 0);
    }

  for (i = 0; i < reader->outputs; i++)
    {
      error = aiger_read_literal (priv, reader, &lit, '\n');
      if (error)
	return error;

      if (aiger_lit2var (lit) > pub->maxvar)
	return aiger_error_uu (priv,
			       "line %u: literal %u is not a valid output",
			       reader->lineno_at_last_token_start, lit);

      aiger_add_output (pub, lit, 0);
    }

  reader->done_with_reading_header = 1;
  reader->looks_like_aag = 1;
  
  return 0;
}

static const char *
aiger_read_ascii (aiger * pub, aiger_reader * reader)
{
  IMPORT_priv_FROM (pub);
  unsigned i, lhs, rhs0, rhs1;
  const char *error;

  for (i = 0; i < reader->ands; i++)
    {
      error = aiger_read_literal (priv, reader, &lhs, ' ');
      if (error)
	return error;

      if (!lhs || aiger_sign (lhs) || aiger_lit2var (lhs) > pub->maxvar)
	return aiger_error_uu (priv,
			       "line %u: "
			       "literal %u is not a valid LHS of AND",
			       reader->lineno_at_last_token_start, lhs);

      error = aiger_already_defined (pub, reader, lhs);
      if (error)
	return error;

      error = aiger_read_literal (priv, reader, &rhs0, ' ');
      if (error)
	return error;

      if (aiger_lit2var (rhs0) > pub->maxvar)
	return aiger_error_uu (priv,
			       "line %u: literal %u is not a valid literal",
			       reader->lineno_at_last_token_start, rhs0);

      error = aiger_read_literal (priv, reader, &rhs1, '\n');
      if (error)
	return error;

      if (aiger_lit2var (rhs1) > pub->maxvar)
	return aiger_error_uu (priv,
			       "line %u: literal %u is not a valid literal",
			       reader->lineno_at_last_token_start, rhs1);

      aiger_add_and (pub, lhs, rhs0, rhs1);
    }

  return 0;
}

static const char *
aiger_read_delta (aiger_priv * priv, aiger_reader * reader,
		  unsigned *res_ptr)
{
  unsigned res, i, charno;
  unsigned char ch;

  if (reader->ch == EOF)
  UNEXPECTED_EOF:
    return aiger_error_u (priv,
			  "character %u: unexpected end of file",
			  reader->charno);
  i = 0;
  res = 0;
  ch = reader->ch;

  charno = reader->charno;

  while ((ch & 0x80))
    {
      assert (sizeof (unsigned) == 4);

      if (i == 5)
      INVALID_CODE:
	return aiger_error_u (priv, "character %u: invalid code", charno);

      res |= (ch & 0x7f) << (7 * i++);
      aiger_next_ch (reader);
      if (reader->ch == EOF)
	goto UNEXPECTED_EOF;

      ch = reader->ch;
    }

  if (i == 5 && ch >= 8)
    goto INVALID_CODE;

  res |= ch << (7 * i);
  *res_ptr = res;

  aiger_next_ch (reader);

  return 0;
}

static const char *
aiger_read_binary (aiger * pub, aiger_reader * reader)
{
  unsigned i, lhs, rhs0, rhs1, delta, charno;
  IMPORT_priv_FROM (pub);
  const char *error;

  lhs = aiger_max_input_or_latch (pub);

  for (i = 0; i < reader->ands; i++)
    {
      lhs += 2;
      charno = reader->charno;
      error = aiger_read_delta (priv, reader, &delta);
      if (error)
	return error;

      if (delta > lhs)		/* can at most be the same */
      INVALID_DELTA:
	return aiger_error_u (priv, "character %u: invalid delta", charno);

      rhs0 = lhs - delta;

      charno = reader->charno;
      error = aiger_read_delta (priv, reader, &delta);
      if (error)
	return error;

      if (delta > rhs0)		/* can well be the same ! */
	goto INVALID_DELTA;

      rhs1 = rhs0 - delta;

      aiger_add_and (pub, lhs, rhs0, rhs1);
    }

  return 0;
}

const char *
aiger_read_generic (aiger * pub, void *state, aiger_get get)
{
  IMPORT_priv_FROM (pub);
  aiger_reader reader;
  const char *error;

  assert (!aiger_error (pub));

  memset (&reader, 0, sizeof (reader));

  reader.lineno = 1;
  reader.state = state;
  reader.get = get;
  reader.ch = ' ';

  error = aiger_read_header (pub, &reader);
  if (error)
    return error;

  if (reader.mode == aiger_ascii_mode)
    error = aiger_read_ascii (pub, &reader);
  else
    error = aiger_read_binary (pub, &reader);

  if (error)
    return error;

  /*error = aiger_read_symbols (pub, &reader);
  if (!error)
    error = aiger_read_comments (pub, &reader);

  DELETEN (reader.buffer, reader.size_buffer);

  if (error)
    return error;*/
  
  return aiger_check (pub);
}

const char *
aiger_read_from_file (aiger * pub, FILE * file)
{
  assert (!aiger_error (pub));
  return aiger_read_generic (pub, file, (aiger_get) aiger_default_get);
}

const char *
aiger_open_and_read_from_file (aiger * pub, const char *file_name)
{
  IMPORT_priv_FROM (pub);
  char *cmd, size_cmd;
  const char *res;
  int pclose_file;
  FILE *file;

  assert (!aiger_error (pub));

  if (aiger_has_suffix (file_name, ".gz"))
  {
    // not used
  }
  else
  {
    file = fopen (file_name, "r");
    pclose_file = 0;
  }

  if (!file)
    return aiger_error_s (priv, "can not read '%s'", file_name);

  res = aiger_read_from_file (pub, file);

  if (pclose_file)
    pclose (file);
  else
    fclose (file);

  return res;
}

aiger* read_aiger (const char* srcLocation)
{
  const char *error;
  aiger *aiger;
  memory memory;
  unsigned i;

  memory.max = memory.bytes = 0;
  aiger = aiger_init_mem (&memory);

  error = aiger_open_and_read_from_file (aiger, srcLocation);
  if (error)
  {
    fprintf (stderr, "*** [aigtoaig] %s\n", error);
    exit(1);
  }

  return aiger;
}
