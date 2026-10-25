//
// Yosys slang frontend
//
// Copyright Martin Povišer <povik@cutebit.org>
// Distributed under the terms of the ISC license, see LICENSE
//
// clang-format off
#include <exception>
#include <string>
#include <optional>
#include "slang/ast/statements/MiscStatements.h"
#include "slang/ast/SemanticFacts.h"
#ifndef SLANG_NO_YOSYS
#include "kernel/rtlil.h"
#endif
#include "slang/ast/expressions/AssertionExpr.h"
#include "slang/ast/symbols/BlockSymbols.h"
#include "slang/text/SourceLocation.h"
#include "slang/util/ScopeGuard.h"

#include "slang_frontend.h"
#include "statements.h"
#include "diag.h"

namespace slang_frontend {

// Process a 'concurrent assertion'
//
// Any top level clocking expressions have been stripped. Clocking is part
// of the created procedural context.
void process_sva_property(const ast::ConcurrentAssertionStatement &statement,
						  const ast::StatementBlockSymbol *block,
						  ProceduralContext &procedural, const ast::AssertionExpr &top_expr)
{
	auto &netlist = procedural.netlist;

	const ast::AssertionExpr *expr = &top_expr;
	slang::SourceRange source_range = expr->syntax ? expr->syntax->sourceRange() : statement.sourceRange;

	// Extract disable iff condition if present; the extracted switch
	// needs to live until the end of the function to be picked up
	// by set_effects_trigger
	std::optional<SwitchHelper> switch_;
	auto guard = slang::ScopeGuard([&] {
		// a backend may abort the elaboration by throwing from any call
		if (std::uncaught_exceptions())
			return;
		if (switch_.has_value()) {
			switch_->exit_branch();
			switch_->finish(netlist);
		}
	});
	if (ast::DisableIffAssertionExpr::isKind(expr->kind)) {
		auto &disable = expr->as<ast::DisableIffAssertionExpr>();
		switch_.emplace(procedural, netlist.ReduceBool(procedural.eval.sva(disable.condition)));
		switch_->enter_branch({ir::S0});
		expr = &disable.expr;
		source_range = expr->syntax ? expr->syntax->sourceRange() : statement.sourceRange;
	}

	if (!ast::SimpleAssertionExpr::isKind(expr->kind)) {
		netlist.add_diag(diag::UnsupportedSVAFeature, source_range);
		return;
	}

	auto &simple_assertion = expr->as<ast::SimpleAssertionExpr>();

	// We don't support repetition in simple assertions
	if (simple_assertion.repetition.has_value()) {
		netlist.add_diag(diag::RepetitionsUnsupported, source_range);
		return;
	}

	using CheckKind = BackendGraphBuilderBase::CheckKind;
	CheckKind kind;
	switch (statement.assertionKind) {
	case ast::AssertionKind::Assert:        kind = CheckKind::Assert; break;
	case ast::AssertionKind::Assume:        kind = CheckKind::Assume; break;
	case ast::AssertionKind::CoverProperty: kind = CheckKind::Cover; break;
	default:                                netlist.add_diag(diag::AssertionUnsupported, statement.sourceRange); return;
	}

	std::string cell_name;

	if (block && unwrap_statement(block->tryGetStatement()) == &statement && !block->name.empty()) {
		// If we are the sole statement in a block, use the block's label
		cell_name = netlist.id(*block);
	} else {
		cell_name = netlist.backend->new_id();
	}

	ir::Net a = netlist.ReduceBool(procedural.eval.sva(simple_assertion.expr));

	ir::Net case_enable = procedural.case_enable();
	ir::Net enable = netlist.LogicAnd(procedural.timing.background_enable, case_enable);
	int priority = --procedural.effects_priority;
	AttributeGuard attributes(netlist);
	transfer_attrs(netlist, statement, attributes);
	netlist.backend->add_check(cell_name, kind, a, enable, procedural.timing, priority);
}

void process_freestanding_sva_property(NetlistContext &netlist,
									   const ast::ConcurrentAssertionStatement &statement,
						  			   const ast::StatementBlockSymbol *block)
{
	const ast::AssertionExpr &spec = statement.propertySpec;

	if (ast::ClockingAssertionExpr::isKind(spec.kind)) {
		// Need to strip clocking
		const auto &clocking_expr = spec.as<ast::ClockingAssertionExpr>();
		const auto &clocking = clocking_expr.clocking;

		if (!ast::SignalEventControl::isKind(clocking.kind)) {
			netlist.add_diag(diag::UnsupportedSVAFeature, clocking.sourceRange);
			return;
		}

		const auto &signal_event = clocking.as<ast::SignalEventControl>();

		ProcessTiming timing(ProcessTiming::EdgeTriggered);
		switch (signal_event.edge) {
		case ast::EdgeKind::None:
			netlist.add_diag(diag::SVAClockingRequiresEdge, signal_event.sourceRange);
			return;

		case ast::EdgeKind::PosEdge:
		case ast::EdgeKind::NegEdge:
			timing.triggers.push_back(ProcessTiming::Sensitivity {
				.signal = netlist.eval(signal_event.expr).as_net(),
				.edge_polarity = (signal_event.edge == ast::EdgeKind::PosEdge),
				.ast_node = &clocking
			});
			break;

		case ast::EdgeKind::BothEdges:
			netlist.add_diag(diag::BothEdgesUnsupported, signal_event.sourceRange);
			return;
		}

		if (signal_event.iffCondition) {
			// TODO
			netlist.add_diag(diag::IffUnsupported, signal_event.iffCondition->sourceRange);
		}

		ProceduralContext procedure(netlist, timing);
		process_sva_property(statement, block, procedure, clocking_expr.expr);

#ifndef SLANG_NO_YOSYS
		RTLIL::Process *rtlil_proc = netlist.backend->canvas->addProcess(netlist.backend->new_id());
		transfer_attrs(netlist, statement, rtlil_proc);
#ifndef SLANG_MUX_LOWERING
		procedure.copy_case_tree_into(rtlil_proc->root_case);
#endif
#endif
	} else {
		// No clocking
		ProceduralContext procedure(netlist, ProcessTiming::implicit);
		process_sva_property(statement, block, procedure, spec);

#ifndef SLANG_NO_YOSYS
		RTLIL::Process *rtlil_proc = netlist.backend->canvas->addProcess(netlist.backend->new_id());
		transfer_attrs(netlist, statement, rtlil_proc);
#ifndef SLANG_MUX_LOWERING
		procedure.copy_case_tree_into(rtlil_proc->root_case);
#endif
#endif
	}
}

};
