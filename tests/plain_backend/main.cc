//
// Elaborates its sources with the plain test backend and writes the
// backend's trace to stdout. Its own options: `--caps <list>` declares the
// capabilities in the comma-separated list (`all`, or names of
// BackendGraphBuilderBase::Capabilities), `--default <call>` leaves an
// optional call to the base class. Exits with 0, 1 when slang or sv-elab
// reported errors, 2 on an internal error.
//
// Distributed under the terms of the ISC license, see LICENSE
//
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "slang/ast/Compilation.h"
#include "slang/ast/Expression.h"
#include "slang/ast/Statement.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/diagnostics/DiagnosticEngine.h"
#include "slang/driver/Driver.h"

#include "backend_builder.h"
#include "diag.h"
#include "slang_frontend.h"

namespace slang_frontend {

void transfer_attrs(NetlistContext &, const ast::Symbol &, AttributeGuard &) {}
void transfer_attrs(NetlistContext &, const ast::Statement &, AttributeGuard &) {}
void transfer_attrs(NetlistContext &, const ast::Expression &, AttributeGuard &) {}

}

using namespace slang_frontend;

static bool set_capabilities(BackendGraphBuilderBase::Capabilities &caps, const std::string &list)
{
	std::stringstream stream(list);
	std::string name;
	while (std::getline(stream, name, ',')) {
		bool all = name == "all";
		bool known = all;
		auto set = [&](const char *cap, bool &flag) {
			if (all || name == cap) {
				flag = true;
				known = true;
			}
		};
		set("memories", caps.memories);
		set("high_impedance", caps.high_impedance);
		set("tristates", caps.tristates);
		set("latches", caps.latches);
		set("checks", caps.checks);
		set("prints", caps.prints);
		set("blackbox_ports", caps.blackbox_ports);
		if (!known)
			return false;
	}
	return true;
}

static int run(PlainOptions &options, std::vector<const char *> &args)
{
	slang::driver::Driver driver;
	driver.addStandardArgs();
	SynthesisSettings settings;
	settings.addOptions(driver.cmdLine);
	diag::setup_messages(driver.diagEngine);

	if (!driver.parseCommandLine((int)args.size(), args.data()))
		return 1;
	fixup_options(settings, driver);
	if (!driver.processOptions())
		return 1;
	catch_forbidden_options(driver);
	if (!driver.parseAllSources())
		return 1;
	auto compilation = driver.createCompilation();
	driver.reportCompilation(*compilation, true);
	if (driver.diagEngine.getNumErrors())
		return driver.reportDiagnostics(true) ? 0 : 1;

	uint64_t next_net = ir::first_signal;
	HierarchyQueue hqueue;
	for (auto instance : compilation->getRoot().topInstances) {
		auto ref_body = &get_instance_body(settings, *instance);
		BackendGraphBuilder top(options, &next_net);
		hqueue.get_or_emplace(ref_body, top.start_new_graph(module_type_id(*ref_body)), settings, *compilation,
				*ref_body->parentInstance);
	}
	for (size_t i = 0; i < hqueue.queue.size(); i++) {
		NetlistContext &netlist = *hqueue.queue[i];
		populate_netlist(hqueue, netlist);
		slang::Diagnostics diags;
		diags.append_range(netlist.issued_diagnostics);
		diags.sort(driver.sourceManager);
		for (size_t j = 0; j < diags.size(); j++) {
			if (j > 0 && diags[j] == diags[j - 1])
				continue;
			driver.diagEngine.issue(diags[j]);
		}
	}
	return driver.reportDiagnostics(true) ? 0 : 1;
}

int main(int argc, char **argv)
{
	PlainOptions options;
	options.trace = &std::cout;
	std::vector<const char *> args{argv[0]};
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--caps" && i + 1 < argc) {
			if (!set_capabilities(options.capabilities, argv[++i])) {
				std::cerr << "unknown capability in " << argv[i] << "\n";
				return 1;
			}
		} else if (arg == "--default" && i + 1 < argc) {
			options.defaults.insert(argv[++i]);
		} else {
			args.push_back(argv[i]);
		}
	}
	try {
		return run(options, args);
	} catch (const InternalError &err) {
		std::cout << "internal error: " << err.what() << "\n";
		return 2;
	}
}
