/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

#pragma once

#include <libyul/ControlFlowSideEffects.h>
#include <libyul/SideEffects.h>

#include <fmt/format.h>

#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <numeric>
#include <map>

namespace solidity::yul
{

enum class LiteralKind;
struct Dialect;
struct Block;
struct BuiltinFunction;

class YulNameRepository
{
public:
	using YulName = size_t;

	struct BuiltinFunction
	{
		YulName name;
		std::vector<YulName> parameters;
		std::vector<YulName> returns;

		yul::BuiltinFunction const* data;
	};

	struct PredefinedHandles
	{
		YulName empty {};
		YulName verbatim {};
		YulName boolType {};
		YulName hashFunction {};
		YulName datasize {};
		YulName dataoffset {};
		YulName selfdestruct {};
		YulName memoryguard {};
		YulName eq {};
		YulName add {};
		YulName sub {};
		YulName tstore {};
		YulName defaultType {};
		YulName placeholder_zero {};
		YulName placeholder_one {};
		YulName placeholder_thirtytwo {};
	};

	explicit YulNameRepository(Dialect const& _dialect);
	YulNameRepository(YulNameRepository const&);
	~YulNameRepository();

	YulNameRepository(YulNameRepository&&) = default;
	YulNameRepository& operator=(YulNameRepository&&) = delete;
	YulNameRepository& operator=(YulNameRepository const&) = delete;

	YulName defineName(std::string_view _label);
	YulName deriveName(YulName _id);

	YulName addGhost();

	static constexpr YulName emptyName() { return 0; }

	std::string_view labelOf(YulName _name) const;

	YulName baseNameOf(YulName _name) const
	{
		while (isDerivedName(_name))
			_name = std::get<0>(m_names[_name]);
		return _name;
	}

	std::string_view baseLabelOf(YulName _name) const
	{
		return m_definedLabels[std::get<0>(m_names[baseNameOf(_name)])];
	}

	bool isDerivedName(YulName const _name) const { return std::get<1>(m_names[_name]); }
	bool isVerbatimFunction(YulName _name) const;

	PredefinedHandles const& predefined() const { return m_predefined; }

	[[nodiscard]] BuiltinFunction const* builtin(YulName _name) const;
	bool isBuiltinName(YulName _name) const;

	BuiltinFunction const* discardFunction(YulName _type) const;
	BuiltinFunction const* equalityFunction(YulName _type) const;
	BuiltinFunction const* booleanNegationFunction() const;

	BuiltinFunction const* memoryStoreFunction(YulName _type) const;
	BuiltinFunction const* memoryLoadFunction(YulName _type) const;
	BuiltinFunction const* storageStoreFunction(YulName _type) const;
	BuiltinFunction const* storageLoadFunction(YulName _type) const;
	YulName hashFunction(YulName _type) const;

	YulName nameOfLabel(std::string_view label) const;
	YulName nameOfBuiltin(std::string_view builtin) const;
	YulName nameOfType(std::string_view type) const;
	bool isType(YulName _name) const;

	Dialect const& dialect() const;

	bool isEvmDialect() const;

	void generateLabels(Block const& _ast, std::set<std::string> const& _illegal = {});

private:
	bool nameWithinBounds(YulName const _name) const { return _name < m_index; }

	size_t indexOfType(YulName _type) const;
	BuiltinFunction convertBuiltinFunction(YulName _name, yul::BuiltinFunction const& _builtin) const;
	BuiltinFunction const* fetchTypedPredefinedFunction(YulName _type, std::vector<std::optional<YulName>> const& _functions) const;

	Dialect const& m_dialect;
	std::vector<std::tuple<YulName, std::string>> m_dialectTypes;
	std::map<YulName, BuiltinFunction> m_builtinFunctions;

	struct PredefinedBuiltinFunctions
	{
		std::vector<std::optional<YulName>> discardFunctions;
		std::vector<std::optional<YulName>> equalityFunctions;
		std::optional<YulName> booleanNegationFunction;
		std::vector<std::optional<YulName>> memoryStoreFunctions;
		std::vector<std::optional<YulName>> memoryLoadFunctions;
		std::vector<std::optional<YulName>> storageStoreFunctions;
		std::vector<std::optional<YulName>> storageLoadFunctions;
		std::vector<YulName> hashFunctions;
	};
	PredefinedBuiltinFunctions m_predefinedBuiltinFunctions;

	size_t m_index {0};
	size_t m_nGhosts {0};
	std::vector<std::string> m_definedLabels {};
	std::vector<std::tuple<YulName, bool>> m_names {};
	std::map<std::tuple<size_t, size_t>, YulName> m_verbatimNames {};
	PredefinedHandles m_predefined{};

	struct IndexBoundaries;
	std::unique_ptr<IndexBoundaries> m_indexBoundaries;
};
using YulName = YulNameRepository::YulName;
using Type = YulName;

}
