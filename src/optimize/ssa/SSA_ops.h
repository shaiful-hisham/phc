#ifndef PHC_SSA_OPS
#define PHC_SSA_OPS

#include "MIR.h"
#include "optimize/Basic_block.h"

/*
 * Regardless of the type of assignment or use, it can be represented in an
 * SSA_op of some kind.
 *
 * The idea is to be able to differentiate between the different types of
 * assignment. This means that if a def belongs to a phi node, we dont think
 * it belongs to the basic block.
 */


DECL (SSA_op);
DECL (SSA_def);
DECL (SSA_use);

class SSA_op : virtual public GC_obj
{
public:
	Basic_block* bb;
	Alias_name* name;
	int type_flag;

	SSA_op (Basic_block* bb, Alias_name* name, int type_flag);
	void dump();

public:
	// For defs, these are uses. For uses, these are defs.
	// For SSA_PHI, this is the LHS, or all the RHSs.
	// For SSA_CHI, this is the LHS, or the RHS
	// For SSA_BB, this is the must-defs, or all the uses.
	SSA_op_list aux_ops;
};

class SSA_def : public SSA_op
{
public:
	SSA_def (Basic_block* bb, Alias_name* name, int type_flag);

	SSA_use_list* get_uses ();
};

class SSA_use : public SSA_op
{
public:
	SSA_use (Basic_block* bb, Alias_name* name, int type_flag);

	SSA_def_list* get_defs ();
};

// Perform an arbitrary comparison, in order to allow the ops to index a map.
bool ssa_op_ptr_comparison (SSA_op* op1, SSA_op* op2);


#endif // PHC_SSA_OPS
