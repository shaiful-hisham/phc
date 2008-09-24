/*
	void pre_php_script (MIR::PHP_script* in);
 * phc -- the open source PHP compiler
 * See doc/license/README.license for licensing information
 *
 * Manage all aspects of the pass queue
 */

#include "Pass_manager.h"
#include "process_ir/General.h"

#include "cmdline.h"
#include "ltdl.h"

#include "Plugin_pass.h"
#include "Visitor_pass.h"
#include "Transform_pass.h"

#include "process_ir/XML_unparser.h"
#include "process_ast/AST_unparser.h"
#include "process_hir/HIR_unparser.h"
#include "process_mir/MIR_unparser.h"
#include "process_ast/DOT_unparser.h"

#include "process_ast/Invalid_check.h"

#include "process_hir/HIR_to_AST.h"
#include "process_mir/MIR_to_AST.h"
#include "optimize/CFG.h"


// TODO remove, and put these into the pass_manager
#include "optimize/SCCP.h"
#include "optimize/Def_use.h"
#include "optimize/Dead_code_elimination.h"
#include "optimize/If_simplification.h"

Pass_manager::Pass_manager (gengetopt_args_info* args_info)
: args_info (args_info),
  check (false)
{
	ast_queue = new Pass_queue;
	hir_queue = new Pass_queue;
	mir_queue = new Pass_queue;
	optimization_queue = new Pass_queue;
	codegen_queue = new Pass_queue;

	queues = new List <Pass_queue* > (ast_queue, hir_queue, mir_queue);
	queues->push_back (optimization_queue);
	queues->push_back (codegen_queue);
}

void Pass_manager::add_ast_visitor (AST::Visitor* visitor, String* name, String* description)
{
	Pass* pass = new Visitor_pass (visitor, name, description);
	add_pass (pass, ast_queue);
}

void Pass_manager::add_ast_transform (AST::Transform* transform, String* name, String* description)
{
	Pass* pass = new Transform_pass (transform, name, description);
	add_pass (pass, ast_queue);
}

void Pass_manager::add_ast_pass (Pass* pass)
{
	add_pass (pass, ast_queue);
}

void Pass_manager::add_hir_visitor (HIR::Visitor* visitor, String* name, String* description)
{
	Pass* pass = new Visitor_pass (visitor, name, description);
	add_pass (pass, hir_queue);
}

void Pass_manager::add_hir_transform (HIR::Transform* transform, String* name, String* description)
{
	Pass* pass = new Transform_pass (transform, name, description);
	add_pass (pass, hir_queue);
}

void Pass_manager::add_hir_pass (Pass* pass)
{
	add_pass (pass, hir_queue);
}



void Pass_manager::add_mir_visitor (MIR::Visitor* visitor, String* name, String* description)
{
	Pass* pass = new Visitor_pass (visitor, name, description);
	add_pass (pass, mir_queue);
}

void Pass_manager::add_mir_transform (MIR::Transform* transform, String* name, String* description)
{
	Pass* pass = new Transform_pass (transform, name, description);
	add_pass (pass, mir_queue);
}

void Pass_manager::add_mir_pass (Pass* pass)
{
	add_pass (pass, mir_queue);
}

void Pass_manager::add_optimization (CFG_visitor* v, String* name, String* description)
{
	Pass* pass = new Optimization_pass (v, name, description);
	add_pass (pass, optimization_queue);
}

void Pass_manager::add_optimization_pass (Pass* pass)
{
	add_pass (pass, optimization_queue);
}

void Pass_manager::add_codegen_visitor (MIR::Visitor* visitor, String* name, String* description)
{
	Pass* pass = new Visitor_pass (visitor, name, description);
	add_pass (pass, codegen_queue);
}

void Pass_manager::add_codegen_transform (MIR::Transform* transform, String* name, String* description)
{
	Pass* pass = new Transform_pass (transform, name, description);
	add_pass (pass, codegen_queue);
}

void Pass_manager::add_codegen_pass (Pass* pass)
{
	add_pass (pass, codegen_queue);
}





void Pass_manager::add_pass (Pass* pass, Pass_queue* queue)
{
	assert (pass->name);
	queue->push_back (pass);
}



void Pass_manager::add_plugin (lt_dlhandle handle, String* name, String* option)
{
	Plugin_pass* pp = new Plugin_pass (name, handle, this, option);

	// LOAD
	typedef void (*load_function)(Pass_manager*, Plugin_pass*);
	load_function func = (load_function) lt_dlsym(handle, "load");
	if(func == NULL)
		phc_error ("Unable to find 'load' function in plugin %s: %s", pp->name->c_str (), lt_dlerror ());

	(*func)(this, pp);
	
}

void Pass_manager::add_after_each_ast_pass (Pass* pass)
{
	add_after_each_pass (pass, ast_queue);
}

void Pass_manager::add_after_each_hir_pass (Pass* pass)
{
	add_after_each_pass (pass, hir_queue);
}

void Pass_manager::add_after_each_mir_pass (Pass* pass)
{
	add_after_each_pass (pass, mir_queue);
}

void Pass_manager::add_after_each_pass (Pass* pass, Pass_queue* queue)
{
	for_li (queue, Pass, p)
	{
		p++;
		p = queue->insert (p, pass);
	}
}

void Pass_manager::remove_all ()
{
	for_li (queues, Pass_queue, q)
	{
		(*q)->clear ();
	}
}

void Pass_manager::remove_after_named_pass (String* name)
{
	String* n = name;

	bool remove = false;
	for_li (queues, Pass_queue, q)
	{
		for_li (*q, Pass, p) 
		{
			if (remove)
			{
				p = (*q)->erase (p); // advance
				p--; // for_li has an implicit p++
			}
			else if (*n == *((*p)->name))
				remove = true;
		}
	}
}

void Pass_manager::remove_pass_named (String* name)
{
	for_li (queues, Pass_queue, q)
	{
		for_li (*q, Pass, p) 
		{
			if (*name == *((*p)->name))
				p = (*q)->erase (p);
		}
	}
}



void Pass_manager::add_after_each_pass (Pass* pass)
{
	foreach (Pass_queue* q, *queues)
		add_after_each_pass (pass, q);
}



void Pass_manager::add_before_named_pass (Pass* pass, String* name)
{
	String* n = name;

	foreach (Pass_queue* q, *queues)
		for_li (q, Pass, p) 
			if (*n == *((*p)->name))
			{
				q->insert (p, pass);
				return;
			}


	phc_error ("No pass with name %s was found", name);
}

void Pass_manager::add_after_named_pass (Pass* pass, String* name)
{
	String* n = name;

	foreach (Pass_queue* q, *queues)
		for_li (q, Pass, p) 
			if (*n == *((*p)->name))
			{
				if (p == q->end ())
					q->push_back (pass);
				else
				{
					// insert before the next item
					p++;
					q->insert (p, pass);
				}
				return;
			}

	phc_error ("No pass with name %s was found", name);
}

// TODO this could be much nicer, but its not important
/* Format the string so that each line in LENGTH long, and all lines except the
 * first have WHITESPACE of leading whitespace */
String* format (String* str, int prefix_length)
{
	const int LINE_LENGTH = 80;
	assert (prefix_length < LINE_LENGTH);
	stringstream result;
	stringstream line;
	stringstream word;

	string leading_whitespace (prefix_length, ' ');


	for (unsigned int i = 0; i < str->size (); i++)
	{
		// add the letter to the word
		word << (*str)[i];

		// end of word
		if ((*str)[i] == ' ')
		{
			line << word.str();
			word.str(""); // erase
		}
		else
		{
			// end of line?
			if (line.str().size() + word.str().size() > (unsigned int)(LINE_LENGTH - prefix_length))
			{
				result << line.str() << "\n";
				line.str (""); // erase
				line << leading_whitespace;

				// only use the prefix on the first line
				prefix_length = 0;
			}
		}
	}

	// flush the remainder of the string
	result << line.str () << word.str ();

	return s(result.str ());
}

void Pass_manager::list_passes ()
{
	cout << "Passes:\n";
	foreach (Pass_queue* q, *queues)
		foreach (Pass* p, *q) 
		{
			const char* name = "AST";
			if (q == hir_queue) name = "HIR";
			if (q == mir_queue) name = "MIR";
			if (q == optimization_queue) name = "OPT";
			if (q == codegen_queue) name = "GEN";
			String* desc = p->description;

			printf ("%-15s    (%-8s - %3s)    %s\n", 
					p->name->c_str (),
					p->is_enabled (this) ? "enabled" : "disabled",
					name,
					desc ? (format (desc, 39)->c_str ()) : "No description");
		}
}

void Pass_manager::maybe_enable_debug (Pass* pass)
{
	disable_cdebug ();

	String* name = pass->name;
	for (unsigned int i = 0; i < args_info->debug_given; i++)
	{
		if (*name == args_info->debug_arg [i])
		{
			enable_cdebug ();
		}
	}
}

void Pass_manager::dump (IR::PHP_script* in, Pass* pass)
{
	String* name = pass->name;
	for (unsigned int i = 0; i < args_info->dump_given; i++)
	{
		if (*name == args_info->dump_arg [i])
		{
			if (in->is_AST ()) AST_unparser ().unparse (in->as_AST ());
			else if (in->is_HIR ()) HIR_unparser ().unparse (in->as_HIR ());
			else if (in->is_MIR ()) MIR_unparser ().unparse (in->as_MIR ());
			else assert (0);
		}
	}

	for (unsigned int i = 0; i < args_info->udump_given; i++)
	{
		if (*name == args_info->udump_arg [i])
		{
			if (in->is_MIR ())
				MIR_unparser().unparse_uppered (in->as_MIR ());

			// As pure HIR, this should be fine. As HIR with Foreign MIR nodes (during HIR-to-MIR lowering), ?
			if (in->is_HIR ())
			{
				// this needs to be fixed. It probably used to work when the HIR
				// was lowered to AST then uppered. However, since the uppering is
				// now in the MIR, we've nothing to upper this. I think
				// templatizing the Foreach_uppering should work. However, we want
				// to replace nodes which need uppering with foreign nodes.
				
				// I think not supporting this is fine
				phc_error ("Uppered dump is not supported during HIR pass: %s", name->c_str ());
			}

			if (in->is_AST())
				AST_unparser().unparse (in->as_AST()->clone ()); // TODO do we still need to clone?

		}
	}

	for (unsigned int i = 0; i < args_info->ddump_given; i++)
	{
		if (*name == args_info->ddump_arg [i])
		{
			// TODO: Works on AST only
			in->visit(new DOT_unparser());
		}
	}

	for (unsigned int i = 0; i < args_info->xdump_given; i++)
	{
		if (*name == args_info->xdump_arg [i])
		{
			xml_unparse (in, cout, !args_info->no_xml_attrs_flag);
		}
	}
}

void Pass_manager::run (IR::PHP_script* in, bool main)
{
	run_from_until (NULL, NULL, in, main);
}

// The pass manager is used to parse and transform small snippets of
// compiler-generated code aswell as the whole file. Set MAIN to false for
// small snippets, and to true for the main program.
void Pass_manager::run_pass (Pass* pass, IR::PHP_script* in, bool main)
{
	assert (pass->name);

	if (args_info->verbose_flag && main)
		cout << "Running pass: " << *pass->name << endl;

	if (main)
		maybe_enable_debug (pass);

	pass->run_pass (in, this);
	if (main)
		this->dump (in, pass);

	if (check)
		::check (in, false);

}

/* Run all passes between FROM and TO, inclusive. */
IR::PHP_script* Pass_manager::run_from (String* from, IR::PHP_script* in, bool main)
{
	return run_from_until (from, NULL, in, main);
}

/* Run all passes until TO, inclusive. */
IR::PHP_script* Pass_manager::run_until (String* to, IR::PHP_script* in, bool main)
{
	return run_from_until (NULL, to, in, main);
}



/* Run all passes between FROM and TO, inclusive. */
IR::PHP_script* Pass_manager::run_from_until (String* from, String* to, IR::PHP_script* in, bool main)
{
	bool exec = false;
	// AST
	foreach (Pass* p, *ast_queue)
	{
		// check for starting pass
		if (!exec && 
				((from == NULL) || *(p->name) == *from))
			exec = true;

		if (exec)
			run_pass (p, in, main);

		// check for last pass
		if (exec && (to != NULL) && *(p->name) == *to)
			return in;
	}

	// Sometimes folding can crash. If you went out of your way to remove the
	// passes in the later queues, dont fold.
	if (hir_queue->size() == 0 && mir_queue->size () == 0 
		&& optimization_queue->size () == 0 && codegen_queue->size() == 0)
		return in;

	// HIR
	in = in->fold_lower ();

	foreach (Pass* p, *hir_queue)
	{
		// check for starting pass
		if (!exec && 
				((from == NULL) || *(p->name) == *from))
			exec = true;

		if (exec)
			run_pass (p, in, main);

		// check for last pass
		if (exec && (to != NULL) && *(p->name) == *to)
			return in;
	}

	if (mir_queue->size () == 0 
		&& optimization_queue->size () == 0 && codegen_queue->size() == 0)
		return in;

	// MIR
	in = in->fold_lower ();

	foreach (Pass* p, *mir_queue)
	{
		// check for starting pass
		if (!exec && 
				((from == NULL) || *(p->name) == *from))
			exec = true;

		if (exec)
			run_pass (p, in, main);

		// check for last pass
		if (exec && (to != NULL) && *(p->name) == *to)
			return in;
	}

	run_optimization_passes (in->as_MIR());

	foreach (Pass* p, *codegen_queue)
	{
		// check for starting pass
		if (!exec && 
				((from == NULL) || *(p->name) == *from))
			exec = true;

		if (exec)
			run_pass (p, in, main);

		// check for last pass
		if (exec && (to != NULL) && *(p->name) == *to)
			return in;
	}

	return in;
}

void Pass_manager::post_process ()
{
	foreach (Pass_queue* q, *queues)
		foreach (Pass* p, *q)
		{
			p->post_process ();
		}
}


bool Pass_manager::has_pass_named (String* name)
{
	return (get_pass_named (name) != NULL);
}

Pass* Pass_manager::get_pass_named (String* name)
{
	foreach (Pass_queue* q, *queues)
		foreach (Pass* p, *q)
		{
			if (*name == *p->name)
				return p;
		}
	return NULL;
}

bool is_queue_pass (String* name, Pass_queue* queue)
{
	foreach (Pass* p, *queue)
	{
		if (*name == *p->name)
			return true;
	}
	return false;
}


void Pass_manager::run_optimization_passes (MIR::PHP_script* in)
{
	// TODO less hacky
	Pass* cfg_pass = optimization_queue->front();
	optimization_queue->pop_front();

	Pass* into_ssa_pass = optimization_queue->front();
	optimization_queue->pop_front();

	Pass* out_ssa_pass = optimization_queue->back();
	optimization_queue->pop_back();



	// Perform optimizations method-at-a-time.
	MIR::PHP_script* script = in->as_MIR();
	foreach (MIR::Statement* stmt, *script->statements)
	{
		// TODO: we should be optimizing all methods, not just functions.
		if (isa<MIR::Method> (stmt))
		{
			MIR::Method* method = dyc<MIR::Method> (stmt);

			maybe_enable_debug (cfg_pass);
			CFG* cfg = new CFG (method);
			for (unsigned int i = 0; i < args_info->cfg_dump_given; i++)
				if (*cfg_pass->name == args_info->cfg_dump_arg [i])
					cfg->dump_graphviz (cfg_pass->name);


			maybe_enable_debug (into_ssa_pass);
			cfg->convert_to_ssa_form ();
			for (unsigned int i = 0; i < args_info->cfg_dump_given; i++)
				if (*into_ssa_pass->name == args_info->cfg_dump_arg [i])
					cfg->dump_graphviz (into_ssa_pass->name);


			if (lexical_cast<int> (args_info->optimize_arg) > 0)
			{
				// iterate until it fix-points (or 10 times)
				for (int iter = 0; iter < 1; iter++) // TODO change back to 10
				{
					foreach (Pass* pass, *optimization_queue)
					{
						if (args_info->verbose_flag)
							cout << "Running pass: " << *pass->name << endl;

						maybe_enable_debug (pass);

						cfg->rebuild_ssa_form ();

						// Run optimization
						Optimization_pass* opt = dynamic_cast<Optimization_pass*> (pass);
						if (opt)
							opt->run (cfg, this);

						// Dump CFG
						for (unsigned int i = 0; i < args_info->cfg_dump_given; i++)
						{
							if (*pass->name == args_info->cfg_dump_arg [i])
							{
								stringstream name;
								name << *pass->name << " - iteration " << iter;
								cfg->dump_graphviz (s(name.str()));
							}
						}
					}
					// TODO Iteration didnt work. Fix it.
				}
			}

			maybe_enable_debug (out_ssa_pass);
			cfg->convert_out_of_ssa_form ();
			for (unsigned int i = 0; i < args_info->cfg_dump_given; i++)
				if (*out_ssa_pass->name == args_info->cfg_dump_arg [i])
					cfg->dump_graphviz (out_ssa_pass->name);

			method->statements = cfg->get_linear_statements ();
		}
	}
}
