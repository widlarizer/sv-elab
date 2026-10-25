//
// A backend without Yosys for testing the backend interface: it numbers
// the nets of every result and writes a line for each module, instance,
// blackbox and optional call
//
// Distributed under the terms of the ISC license, see LICENSE
//
#pragma once
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "slang/text/SourceLocation.h"
#include "slang_frontend.h"

namespace slang_frontend {

struct PlainOptions
{
	BackendGraphBuilderBase::Capabilities capabilities;
	// optional calls left to the base class, which must abort
	std::set<std::string> defaults;
	std::ostream *trace = nullptr;
};

struct BackendGraphBuilder : BackendGraphBuilderBase
{
	BackendGraphBuilder(const PlainOptions &options, uint64_t *next_net);

	const PlainOptions &options;
	uint64_t *next_net;
	unsigned next_id = 0;
	std::vector<std::unique_ptr<ir::Memory>> memories;
	std::vector<std::unique_ptr<ir::WritePort>> write_ports;

	std::string new_id(std::string base = std::string());
	ir::Value fresh(uint64_t width);
	std::ostream &trace();

	Capabilities capabilities() override;
	void add_tristate(const ir::Value &target, const ir::Value &data, ir::Net enable) override;
	void add_latch(std::string_view name, ir::Net enable, const ir::Value &d, const ir::Value &q) override;
	void add_check(std::string_view name, CheckKind kind, ir::Net a, ir::Net enable, const ProcessTiming &timing,
			int priority) override;
	void add_print(std::string_view task, ir::Net enable, const ProcessTiming &timing, int priority) override;

	ir::Value Unop(ast::UnaryOperator op, ir::Value a, bool a_signed, uint64_t y_width) override;
	ir::Value Biop(ast::BinaryOperator op, ir::Value a, ir::Value b, bool a_signed, bool b_signed,
			uint64_t y_width) override;
	ir::Value Demux(ir::Value a, ir::Value s) override;
	ir::Value Bwmux(ir::Value a, ir::Value b, ir::Value s) override;
	ir::Value Bmux(ir::Value a, ir::Value s) override;
	ir::Value Shift(ir::Value a, ir::Value s, bool s_signed, uint64_t result_width) override;
	ir::Value Shiftx(ir::Value a, ir::Value s, bool s_signed, uint64_t result_width) override;
	ir::Value Mux(ir::Value a, ir::Value b, ir::Net s) override;

	ir::Value add_placeholder_signal(uint64_t width, std::string_view name_suggestion = ""sv,
			bool public_name = false) override;
	void connect(ir::Value target, ir::Value source) override;
	void set_initialization(ir::Value signal, ir::Const init_value) override;

	ir::Memory *add_memory(std::string_view name, uint64_t width, slang::ConstantRange range) override;
	void add_read_port(ir::Memory *memory, ir::Value address, ir::Value data) override;
	ir::WritePort *add_write_port(ir::Memory *memory, std::span<ir::WritePort *> preceding, bool clocked,
			bool clock_polarity, ir::Net clk, const ir::Value &enable, const ir::Value &address,
			const ir::Value &data) override;
	void add_memory_init(ir::Memory *memory, uint64_t bit_offset, bool big_endian, ir::Const data) override;

	void add_instance(std::string_view cell_type, std::vector<PortConnection> ports) override;
	void instantiate_blackbox(std::string_view cell_type, std::string_view name,
			std::span<PortConnection> port_connections, std::span<ParameterValue> param_values) override;

	void add_dual_edge_aldff(const std::string &base_name, ir::Net clk, ir::Net aload, const ir::Value &d,
			const ir::Value &q, const ir::Value &ad, bool aload_polarity) override;
	void add_dff(std::string_view name, const ir::Net clk, const ir::Value &d, const ir::Value &q,
			bool clk_polarity = true) override;
	void add_dffe(std::string_view name, const ir::Net clk, const ir::Net en, const ir::Value &d,
			const ir::Value &q, bool clk_polarity = true, bool en_polarity = true) override;
	void add_aldff(std::string_view name, const ir::Net clk, const ir::Net aload, const ir::Value &d,
			const ir::Value &q, const ir::Value &ad, bool clk_polarity = true, bool aload_polarity = true) override;
	void add_aldffe(std::string_view name, const ir::Net clk, const ir::Net en, const ir::Net aload,
			const ir::Value &d, const ir::Value &q, const ir::Value &ad, bool clk_polarity = true,
			bool en_polarity = true, bool aload_polarity = true) override;

	std::unique_ptr<BackendGraphBuilder> start_new_graph(std::string_view graph_name);
	void finalize();

private:
	void ports(std::span<const PortConnection> ports);
};

class AttributeGuard
{
public:
	AttributeGuard(GraphBuilder &) {}
	void set(std::string_view, std::string_view) {}
	void set_source(slang::SourceRange) {}
};

}
