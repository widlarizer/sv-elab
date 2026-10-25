//
// A backend without Yosys for testing the backend interface
//
// Distributed under the terms of the ISC license, see LICENSE
//
#include "backend_builder.h"

#include <iostream>

namespace slang_frontend {

BackendGraphBuilder::BackendGraphBuilder(const PlainOptions &options, uint64_t *next_net)
	: options(options), next_net(next_net)
{
}

std::string BackendGraphBuilder::new_id(std::string base)
{
	return "$" + base + "$" + std::to_string(next_id++);
}

ir::Value BackendGraphBuilder::fresh(uint64_t width)
{
	ir::Value value;
	for (uint64_t i = 0; i < width; i++)
		value.append(ir::Net::of((*next_net)++));
	return value;
}

std::ostream &BackendGraphBuilder::trace() { return *options.trace; }

BackendGraphBuilderBase::Capabilities BackendGraphBuilder::capabilities() { return options.capabilities; }

void BackendGraphBuilder::add_tristate(const ir::Value &target, const ir::Value &data, ir::Net enable)
{
	if (options.defaults.count("add_tristate"))
		return BackendGraphBuilderBase::add_tristate(target, data, enable);
	trace() << "tristate " << target.size() << "\n";
}

void BackendGraphBuilder::add_latch(std::string_view name, ir::Net enable, const ir::Value &d, const ir::Value &q)
{
	if (options.defaults.count("add_latch"))
		return BackendGraphBuilderBase::add_latch(name, enable, d, q);
	trace() << "latch " << q.size() << "\n";
}

void BackendGraphBuilder::add_check(std::string_view name, CheckKind kind, ir::Net a, ir::Net enable,
		const ProcessTiming &timing, int priority)
{
	if (options.defaults.count("add_check"))
		return BackendGraphBuilderBase::add_check(name, kind, a, enable, timing, priority);
	trace() << "check " << (kind == CheckKind::Assert ? "assert" : kind == CheckKind::Assume ? "assume" : "cover")
			<< "\n";
}

void BackendGraphBuilder::add_print(std::string_view task, ir::Net enable, const ProcessTiming &timing, int priority)
{
	if (options.defaults.count("add_print"))
		return BackendGraphBuilderBase::add_print(task, enable, timing, priority);
	trace() << "print " << task << "\n";
}

ir::Value BackendGraphBuilder::Unop(ast::UnaryOperator, ir::Value, bool, uint64_t y_width) { return fresh(y_width); }

ir::Value BackendGraphBuilder::Biop(ast::BinaryOperator, ir::Value, ir::Value, bool, bool, uint64_t y_width)
{
	return fresh(y_width);
}

ir::Value BackendGraphBuilder::Demux(ir::Value a, ir::Value s) { return fresh(a.size() << s.size()); }

ir::Value BackendGraphBuilder::Bwmux(ir::Value a, ir::Value, ir::Value) { return fresh(a.size()); }

ir::Value BackendGraphBuilder::Bmux(ir::Value a, ir::Value s) { return fresh(a.size() >> s.size()); }

ir::Value BackendGraphBuilder::Shift(ir::Value, ir::Value, bool, uint64_t result_width) { return fresh(result_width); }

ir::Value BackendGraphBuilder::Shiftx(ir::Value, ir::Value, bool, uint64_t result_width)
{
	return fresh(result_width);
}

ir::Value BackendGraphBuilder::Mux(ir::Value a, ir::Value, ir::Net) { return fresh(a.size()); }

ir::Value BackendGraphBuilder::add_placeholder_signal(uint64_t width, std::string_view, bool) { return fresh(width); }

void BackendGraphBuilder::connect(ir::Value, ir::Value) {}

void BackendGraphBuilder::set_initialization(ir::Value, ir::Const) {}

ir::Memory *BackendGraphBuilder::add_memory(std::string_view name, uint64_t width, slang::ConstantRange)
{
	trace() << "memory " << name << " " << width << "\n";
	memories.push_back(std::make_unique<ir::Memory>());
	return memories.back().get();
}

void BackendGraphBuilder::add_read_port(ir::Memory *, ir::Value, ir::Value) {}

ir::WritePort *BackendGraphBuilder::add_write_port(ir::Memory *, std::span<ir::WritePort *>, bool, bool, ir::Net,
		const ir::Value &, const ir::Value &, const ir::Value &)
{
	write_ports.push_back(std::make_unique<ir::WritePort>());
	return write_ports.back().get();
}

void BackendGraphBuilder::add_memory_init(ir::Memory *, uint64_t, bool, ir::Const) {}

void BackendGraphBuilder::ports(std::span<const PortConnection> ports)
{
	for (auto &port : ports) {
		trace() << " " << port.name << ":"
				<< (port.direction == PortConnection::kInput    ? "in"
						   : port.direction == PortConnection::kOutput ? "out"
																	   : "inout")
				<< ":" << port.value.size();
		if (!port.connected)
			trace() << ":open";
	}
	trace() << "\n";
}

void BackendGraphBuilder::add_instance(std::string_view cell_type, std::vector<PortConnection> connections)
{
	trace() << "instance " << cell_type;
	ports(connections);
}

void BackendGraphBuilder::instantiate_blackbox(std::string_view cell_type, std::string_view,
		std::span<PortConnection> port_connections, std::span<ParameterValue>)
{
	trace() << "blackbox " << cell_type;
	ports(port_connections);
}

void BackendGraphBuilder::add_dual_edge_aldff(const std::string &, ir::Net, ir::Net, const ir::Value &,
		const ir::Value &, const ir::Value &, bool)
{
}

void BackendGraphBuilder::add_dff(std::string_view, const ir::Net, const ir::Value &, const ir::Value &, bool) {}

void BackendGraphBuilder::add_dffe(std::string_view, const ir::Net, const ir::Net, const ir::Value &,
		const ir::Value &, bool, bool)
{
}

void BackendGraphBuilder::add_aldff(std::string_view, const ir::Net, const ir::Net, const ir::Value &,
		const ir::Value &, const ir::Value &, bool, bool)
{
}

void BackendGraphBuilder::add_aldffe(std::string_view, const ir::Net, const ir::Net, const ir::Net,
		const ir::Value &, const ir::Value &, const ir::Value &, bool, bool, bool)
{
}

std::unique_ptr<BackendGraphBuilder> BackendGraphBuilder::start_new_graph(std::string_view graph_name)
{
	trace() << "module " << graph_name << "\n";
	return std::make_unique<BackendGraphBuilder>(options, next_net);
}

void BackendGraphBuilder::finalize() {}

}
