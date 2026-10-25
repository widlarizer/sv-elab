//
// sv-elab ir for backends without Yosys: nets are plain uint64_t values,
// 0 to 3 the constants S0, S1, Sx and Sz, the others signals the backend
// numbers from first_signal on
//
// Distributed under the terms of the ISC license, see LICENSE
//
#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <vector>

namespace ir {

struct Trit
{
	uint8_t raw_;

	constexpr Trit() : raw_(2) {}
	constexpr explicit Trit(uint8_t raw) : raw_(raw) {}

	constexpr bool operator==(Trit o) const { return raw_ == o.raw_; }
	constexpr bool operator!=(Trit o) const { return raw_ != o.raw_; }
};

inline constexpr Trit S0{uint8_t(0)};
inline constexpr Trit S1{uint8_t(1)};
inline constexpr Trit Sx{uint8_t(2)};
inline constexpr Trit Sz{uint8_t(3)};

inline constexpr uint64_t first_signal = 4;

class Const
{
	std::vector<Trit> bits_;

public:
	Const() {}
	explicit Const(int val, uint64_t width)
	{
		bits_.reserve(width);
		for (uint64_t i = 0; i < width; i++)
			bits_.push_back(i < 32 ? Trit(uint8_t((val >> i) & 1)) : (val < 0 ? S1 : S0));
	}
	explicit Const(Trit t, uint64_t width) : bits_(width, t) {}
	Const(const std::vector<Trit> &bits) : bits_(bits) {}

	uint64_t size() const { return bits_.size(); }
	bool empty() const { return bits_.empty(); }
	const Trit *data() const { return bits_.data(); }

	Const extract(int offset, int length) const
	{
		return std::vector<Trit>(bits_.begin() + offset, bits_.begin() + offset + length);
	}
	void append(Const other) { bits_.insert(bits_.end(), other.bits_.begin(), other.bits_.end()); }
	void append(Trit t) { bits_.push_back(t); }

	int as_int(bool is_signed = false) const
	{
		int32_t ret = 0;
		for (size_t i = 0; i < bits_.size() && i < 32; i++)
			if (bits_[i] == S1)
				ret |= int32_t(1) << i;
		if (is_signed && !bits_.empty() && bits_.size() < 32 && bits_.back() == S1)
			for (size_t i = bits_.size(); i < 32; i++)
				ret |= int32_t(1) << i;
		return ret;
	}
	bool is_fully_undef() const
	{
		for (auto t : bits_)
			if (t != Sx)
				return false;
		return true;
	}

	void set(int i, Trit t) { bits_[i] = t; }
	Trit operator[](int i) const { return bits_[i]; }

	std::vector<Trit>::const_iterator begin() const { return bits_.begin(); }
	std::vector<Trit>::const_iterator end() const { return bits_.end(); }
};

struct Value;

struct Net
{
	uint64_t raw_;

	Net() : raw_(Sx.raw_) {}
	Net(Trit t) : raw_(t.raw_) {}
	static Net of(uint64_t raw)
	{
		Net n;
		n.raw_ = raw;
		return n;
	}

	Value repeat(uint64_t width);

	Trit trit() const { return is_const() ? Trit(uint8_t(raw_)) : Sx; }

	bool operator==(Net o) const { return raw_ == o.raw_; }
	bool operator!=(Net o) const { return raw_ != o.raw_; }
	bool operator==(Trit t) const { return raw_ == t.raw_; }
	bool operator!=(Trit t) const { return raw_ != t.raw_; }

	bool is_const() const { return raw_ < first_signal; }
	bool is_def() const { return raw_ < 2; }
};

class NetProxy
{
	std::vector<uint64_t> &bits_;
	uint64_t idx_;

public:
	NetProxy(std::vector<uint64_t> &bits, uint64_t idx) : bits_(bits), idx_(idx) {}
	operator Net() const { return Net::of(bits_[idx_]); }
	NetProxy &operator=(Net n)
	{
		bits_[idx_] = n.raw_;
		return *this;
	}
	NetProxy &operator=(const NetProxy &o)
	{
		bits_[idx_] = o.bits_[o.idx_];
		return *this;
	}
	bool operator==(Net n) const { return bits_[idx_] == n.raw_; }
	bool operator!=(Net n) const { return bits_[idx_] != n.raw_; }
	bool operator==(Trit t) const { return bits_[idx_] == t.raw_; }
	bool operator!=(Trit t) const { return bits_[idx_] != t.raw_; }
	Trit trit() const { return Net(*this).trit(); }
};

struct Value
{
	std::vector<uint64_t> raw_;

	Value() {}
	Value(Net bit) : raw_(1, bit.raw_) {}
	Value(Trit t) : raw_(1, t.raw_) {}
	Value(Trit t, uint64_t width) : raw_(width, t.raw_) {}
	Value(const Const &c)
	{
		raw_.reserve(c.size());
		for (auto t : c)
			raw_.push_back(t.raw_);
	}
	Value(std::initializer_list<Value> parts)
	{
		for (auto it = parts.end(); it != parts.begin();) {
			--it;
			raw_.insert(raw_.end(), it->raw_.begin(), it->raw_.end());
		}
	}
	static Value of(std::vector<uint64_t> bits)
	{
		Value v;
		v.raw_ = std::move(bits);
		return v;
	}

	uint64_t size() const { return raw_.size(); }
	uint64_t width() const { return raw_.size(); }
	bool empty() const { return raw_.empty(); }
	const uint64_t *data() const { return raw_.data(); }
	uint64_t *data() { return raw_.data(); }

	Net operator[](uint64_t i) const { return Net::of(raw_[i]); }
	NetProxy operator[](uint64_t i) { return NetProxy(raw_, i); }

	Value extract(uint64_t offset, uint64_t width) const
	{
		assert(offset + width <= raw_.size());
		return of(std::vector<uint64_t>(raw_.begin() + offset, raw_.begin() + offset + width));
	}
	Value extract_end(uint64_t offset) const { return extract(offset, raw_.size() - offset); }
	void append(Net bit) { raw_.push_back(bit.raw_); }
	void append(const NetProxy &p) { raw_.push_back(Net(p).raw_); }
	void append(Trit t) { raw_.push_back(t.raw_); }
	void append(const Value &other) { raw_.insert(raw_.end(), other.raw_.begin(), other.raw_.end()); }
	void remove(uint64_t index, uint64_t width = 1)
	{
		raw_.erase(raw_.begin() + index, raw_.begin() + index + width);
	}
	Value repeat(uint64_t count) const
	{
		Value ret;
		ret.raw_.reserve(raw_.size() * count);
		for (uint64_t i = 0; i < count; i++)
			ret.append(*this);
		return ret;
	}
	void extend_u0(uint64_t width, bool is_signed = false)
	{
		if (width <= raw_.size()) {
			raw_.resize(width);
			return;
		}
		uint64_t fill = is_signed && !raw_.empty() ? raw_.back() : S0.raw_;
		raw_.resize(width, fill);
	}
	Net msb() const { return Net::of(raw_.back()); }
	void reserve(uint64_t n) { raw_.reserve(n); }

	bool is_fully_const() const
	{
		for (auto b : raw_)
			if (b >= first_signal)
				return false;
		return true;
	}
	bool is_fully_def() const
	{
		for (auto b : raw_)
			if (b >= 2)
				return false;
		return true;
	}
	bool is_fully_zero() const
	{
		for (auto b : raw_)
			if (b != S0.raw_)
				return false;
		return true;
	}
	bool is_fully_ones() const
	{
		for (auto b : raw_)
			if (b != S1.raw_)
				return false;
		return true;
	}

	Net as_net() const
	{
		assert(raw_.size() == 1);
		return Net::of(raw_[0]);
	}
	Const as_const() const
	{
		std::vector<Trit> bits;
		bits.reserve(raw_.size());
		for (auto b : raw_)
			bits.push_back(b < first_signal ? Trit(uint8_t(b)) : Sx);
		return bits;
	}
	bool as_bool() const
	{
		for (auto b : raw_)
			if (b == S1.raw_)
				return true;
		return false;
	}
	int as_int(bool is_signed = false) const { return as_const().as_int(is_signed); }

	struct const_iterator
	{
		using iterator_category = std::input_iterator_tag;
		using value_type = Net;
		using difference_type = std::ptrdiff_t;
		using pointer = const Net *;
		using reference = Net;

		std::vector<uint64_t>::const_iterator it;
		Net operator*() const { return Net::of(*it); }
		const_iterator &operator++()
		{
			++it;
			return *this;
		}
		const_iterator operator++(int)
		{
			auto tmp = *this;
			++it;
			return tmp;
		}
		bool operator!=(const const_iterator &o) const { return it != o.it; }
		bool operator==(const const_iterator &o) const { return it == o.it; }
	};

	const_iterator begin() const { return {raw_.begin()}; }
	const_iterator end() const { return {raw_.end()}; }

	bool operator==(const Value &o) const { return raw_ == o.raw_; }
	bool operator!=(const Value &o) const { return raw_ != o.raw_; }
};

inline Value Net::repeat(uint64_t width) { return Value(*this).repeat(width); }

}
