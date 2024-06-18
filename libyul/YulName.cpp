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

#include <libyul/YulName.h>

#include <libyul/backends/evm/EVMDialect.h>

#include <libyul/optimiser/NameCollector.h>

#include <libyul/Dialect.h>
#include <libyul/Exceptions.h>
#include <libyul/AST.h>

#include <fmt/compile.h>

namespace solidity::yul
{

struct YulNameRepository::IndexBoundaries
{
	size_t beginTypes {};
	size_t endTypes {};
	size_t beginBuiltins {};
	size_t endBuiltins {};
};

YulNameRepository::YulNameRepository(solidity::yul::Dialect const& _dialect):
	m_dialect(_dialect), m_indexBoundaries(std::make_unique<IndexBoundaries>())
{
	defineName("");

	for (auto const& type: _dialect.types)
		if (type.empty())
		{
			m_indexBoundaries->beginTypes = 0;
			m_dialectTypes.emplace_back(emptyName(), type);
		}
		else
		{
			m_indexBoundaries->beginTypes = 1;
			m_dialectTypes.emplace_back(defineName(type), type);
		}
	m_indexBoundaries->endTypes = m_index;
	m_indexBoundaries->beginBuiltins = m_index;

	auto const& builtinNames = _dialect.builtinNames();
	m_predefined.verbatim = defineName("@ verbatim");
	for (auto const& label: builtinNames)
		if (!label.empty())
		{
			auto const name = defineName(label);
			if (auto const* function = m_dialect.builtin(label))
				m_builtinFunctions[name] = convertBuiltinFunction(name, *function);
		}
	m_indexBoundaries->endBuiltins = m_index;

	m_predefined.boolType = nameOfType(_dialect.boolType);
	m_predefined.defaultType = nameOfType(_dialect.defaultType);
	if (builtinNames.count("dataoffset") > 0)
		m_predefined.dataoffset = nameOfBuiltin("dataoffset");
	else
		m_predefined.dataoffset = defineName("dataoffset");
	if (builtinNames.count("datasize") > 0)
		m_predefined.datasize = nameOfBuiltin("datasize");
	else
		m_predefined.datasize = defineName("datasize");
	if (builtinNames.count("selfdestruct") > 0)
		m_predefined.selfdestruct = nameOfBuiltin("selfdestruct");
	else
		m_predefined.selfdestruct = defineName("selfdestruct");
	if (builtinNames.count("tstore") > 0)
		m_predefined.tstore = nameOfBuiltin("tstore");
	else
		m_predefined.tstore = defineName("tstore");
	if (builtinNames.count("memoryguard") > 0)
		m_predefined.memoryguard = nameOfBuiltin("memoryguard");
	else
		m_predefined.memoryguard = defineName("memoryguard");
	if (builtinNames.count("eq") > 0)
		m_predefined.eq = nameOfBuiltin("eq");
	else
		m_predefined.eq = defineName("eq");
	if (builtinNames.count("add") > 0)
		m_predefined.add = nameOfBuiltin("add");
	else
		m_predefined.add = defineName("add");
	if (builtinNames.count("sub") > 0)
		m_predefined.sub = nameOfBuiltin("sub");
	else
		m_predefined.sub = defineName("sub");

	{
		auto types = m_dialectTypes;
		if (types.empty())
			types.emplace_back(0, "");

		for (auto const& [typeName, typeLabel]: types)
		{
			if (auto const* discardFunction = m_dialect.discardFunction(typeLabel))
				m_predefinedBuiltinFunctions.discardFunctions.emplace_back(nameOfBuiltin(discardFunction->name));
			else
				m_predefinedBuiltinFunctions.discardFunctions.emplace_back(std::nullopt);

			if (auto const* equalityFunction = m_dialect.equalityFunction(typeLabel))
				m_predefinedBuiltinFunctions.equalityFunctions.emplace_back(nameOfBuiltin(equalityFunction->name));
			else
				m_predefinedBuiltinFunctions.equalityFunctions.emplace_back(std::nullopt);

			if (auto const* booleanNegationFunction = m_dialect.booleanNegationFunction())
				m_predefinedBuiltinFunctions.booleanNegationFunction = nameOfBuiltin(booleanNegationFunction->name);
			else
				m_predefinedBuiltinFunctions.booleanNegationFunction = std::nullopt;

			if (auto const* memStoreFunction = m_dialect.memoryStoreFunction(typeLabel))
				m_predefinedBuiltinFunctions.memoryStoreFunctions.emplace_back(nameOfBuiltin(memStoreFunction->name));
			else
				m_predefinedBuiltinFunctions.memoryStoreFunctions.emplace_back(std::nullopt);

			if (auto const* memLoadFunction = m_dialect.memoryLoadFunction(typeLabel))
				m_predefinedBuiltinFunctions.memoryLoadFunctions.emplace_back(nameOfBuiltin(memLoadFunction->name));
			else
				m_predefinedBuiltinFunctions.memoryLoadFunctions.emplace_back(std::nullopt);

			if (auto const* storageStoreFunction = m_dialect.storageStoreFunction(typeLabel))
				m_predefinedBuiltinFunctions.storageStoreFunctions.emplace_back(nameOfBuiltin(storageStoreFunction->name));
			else
				m_predefinedBuiltinFunctions.storageStoreFunctions.emplace_back(std::nullopt);

			if (auto const* storageLoadFunction = m_dialect.storageLoadFunction(typeLabel))
				m_predefinedBuiltinFunctions.storageLoadFunctions.emplace_back(nameOfBuiltin(storageLoadFunction->name));
			else
				m_predefinedBuiltinFunctions.storageLoadFunctions.emplace_back(std::nullopt);

			m_predefinedBuiltinFunctions.hashFunctions.emplace_back(nameOfBuiltin(m_dialect.hashFunction(typeLabel)));
		}
	}

	m_predefined.placeholder_zero = defineName("@ 0");
	m_predefined.placeholder_one = defineName("@ 1");
	m_predefined.placeholder_thirtytwo = defineName("@ 32");
}

YulNameRepository::BuiltinFunction const* YulNameRepository::builtin(YulName const _name) const
{
	yulAssert(nameWithinBounds(_name), "YulName exceeds repository size, probably stems from another instance.");
	auto const baseName = baseNameOf(_name);
	if (isBuiltinName(baseName))
	{
		auto const it = m_builtinFunctions.find(_name);
		if (it != m_builtinFunctions.end())
			return &it->second;
		else
			return nullptr;
	}
	return nullptr;
}

std::string_view YulNameRepository::labelOf(YulName const _name) const
{
	yulAssert(_name < m_names.size(), "YulName exceeds repository size, probably stems from another instance.");
	if (!isDerivedName(_name))
	{
		// if the parent is directly a defined label, we take that one
		return m_definedLabels[std::get<0>(m_names[_name])];
	}
	else
	{
		if (isVerbatimFunction(_name))
		{
			auto const* builtinFun = builtin(_name);
			yulAssert(builtinFun);
			return builtinFun->data->name;
		}
		yulAssert(false, "Derived name was not yet defined.");
	}
}

YulNameRepository::BuiltinFunction const* YulNameRepository::fetchTypedPredefinedFunction(YulName const _type, std::vector<std::optional<YulName>> const& _functions) const
{
	yulAssert(nameWithinBounds(_type), "Type exceeds repository size, probably stems from another instance.");
	auto const typeIndex = indexOfType(_type);
	yulAssert(typeIndex < _functions.size());
	auto const& functionName = _functions[typeIndex];
	if (!functionName)
		return nullptr;
	return builtin(*functionName);
}

YulNameRepository::BuiltinFunction const* YulNameRepository::discardFunction(YulName const _type) const
{
	return fetchTypedPredefinedFunction(_type, m_predefinedBuiltinFunctions.discardFunctions);
}

YulNameRepository::BuiltinFunction const* YulNameRepository::equalityFunction(YulName const _type) const
{
	return fetchTypedPredefinedFunction(_type, m_predefinedBuiltinFunctions.equalityFunctions);
}

YulNameRepository::BuiltinFunction const* YulNameRepository::booleanNegationFunction() const
{
	if (!m_predefinedBuiltinFunctions.booleanNegationFunction)
		return nullptr;
	return builtin(*m_predefinedBuiltinFunctions.booleanNegationFunction);
}

YulNameRepository::BuiltinFunction const* YulNameRepository::memoryLoadFunction(YulName const _type) const
{
	return fetchTypedPredefinedFunction(_type, m_predefinedBuiltinFunctions.memoryLoadFunctions);
}

YulNameRepository::BuiltinFunction const* YulNameRepository::memoryStoreFunction(YulName const _type) const
{
	return fetchTypedPredefinedFunction(_type, m_predefinedBuiltinFunctions.memoryStoreFunctions);
}

YulNameRepository::BuiltinFunction const* YulNameRepository::storageLoadFunction(YulName _type) const
{
	return fetchTypedPredefinedFunction(_type, m_predefinedBuiltinFunctions.storageLoadFunctions);
}

YulNameRepository::BuiltinFunction const* YulNameRepository::storageStoreFunction(YulName const _type) const
{
	return fetchTypedPredefinedFunction(_type, m_predefinedBuiltinFunctions.storageStoreFunctions);
}

YulName YulNameRepository::hashFunction(YulName const _type) const
{
	yulAssert(nameWithinBounds(_type), "Type exceeds repository size, probably stems from another instance.");
	auto const typeIndex = indexOfType(_type);
	return m_predefinedBuiltinFunctions.hashFunctions[typeIndex];
}

bool YulNameRepository::isBuiltinName(YulName const _name) const
{
	yulAssert(nameWithinBounds(_name), "YulName exceeds repository size, probably stems from another instance.");
	auto const baseName = baseNameOf(_name);
	return baseName >= m_indexBoundaries->beginBuiltins && baseName < m_indexBoundaries->endBuiltins;
}

YulNameRepository::BuiltinFunction YulNameRepository::convertBuiltinFunction(YulName const _name, yul::BuiltinFunction const& _builtin) const
{
	yulAssert(nameWithinBounds(_name), "YulName exceeds repository size, probably stems from another instance.");
	BuiltinFunction result;
	result.name = _name;
	for (auto const& type: _builtin.parameters)
		result.parameters.push_back(nameOfType(type));
	for (auto const& type: _builtin.returns)
		result.returns.push_back(nameOfType(type));
	result.data = &_builtin;
	return result;
}

YulName YulNameRepository::nameOfLabel(std::string_view const label) const
{
	auto const it = std::find(m_definedLabels.begin(), m_definedLabels.end(), label);
	if (it != m_definedLabels.end())
	{
		auto const labelIndex = static_cast<size_t>(std::distance(m_definedLabels.begin(), it));
		// mostly it'll be iota
		if (!isDerivedName(labelIndex) && std::get<0>(m_names[labelIndex]) == static_cast<size_t>(labelIndex))
			return labelIndex;
		// if not iota, we have to search
		auto itName = std::find(m_names.rbegin(), m_names.rend(), std::make_tuple(labelIndex, false));
		if (itName != m_names.rend())
			return YulName{static_cast<size_t>(std::distance(itName, m_names.rend())) - 1};
	}
	return emptyName();
}

YulName YulNameRepository::nameOfBuiltin(std::string_view const builtin) const
{
	for (size_t i = m_indexBoundaries->beginBuiltins; i < m_indexBoundaries->endBuiltins; ++i)
		if (baseLabelOf(std::get<0>(m_names[i])) == builtin)
			return i;
	return emptyName();
}

YulName YulNameRepository::nameOfType(std::string_view const _type) const
{
	if (!m_dialectTypes.empty())
	{
		for (auto const& m_dialectType: m_dialectTypes)
			if (std::get<1>(m_dialectType) == _type)
				return std::get<0>(m_dialectType);
		yulAssert(false, "only defined for (some) dialect types");
	}
	else
		return emptyName();
}

size_t YulNameRepository::indexOfType(YulName const _type) const
{
	if (m_dialectTypes.empty())
		return 0;
	auto const it = std::find_if(m_dialectTypes.begin(), m_dialectTypes.end(), [&](auto const& element) { return std::get<0>(element) == _type; });
	yulAssert(it != m_dialectTypes.end());
	return static_cast<size_t>(std::distance(m_dialectTypes.begin(), it));
}

Dialect const& YulNameRepository::dialect() const
{
	return m_dialect;
}

bool YulNameRepository::isVerbatimFunction(YulName const _name) const
{
	yulAssert(nameWithinBounds(_name), "YulName exceeds repository size, probably stems from another instance.");
	return baseNameOf(_name) == predefined().verbatim;
}

YulName YulNameRepository::defineName(std::string_view const _label)
{
	if (auto const* builtin = m_dialect.builtin(_label))
	{
		if (builtin->name.substr(0, std::string_view("verbatim").size()) == "verbatim")
		{
			auto const key = std::make_tuple(builtin->parameters.size(), builtin->returns.size());
			auto [it, emplaced] = m_verbatimNames.try_emplace(key);
			if (emplaced)
			{
				it->second = deriveName(predefined().verbatim);
				m_builtinFunctions[it->second] = convertBuiltinFunction(it->second, *builtin);
			}
			return it->second;
		}
		else
		{
			auto const builtinName = nameOfBuiltin(_label);
			if (builtinName == emptyName())
			{
				m_definedLabels.emplace_back(_label);
				m_names.emplace_back(m_definedLabels.size() - 1, false);
				return m_index++;
			}
			else
				return builtinName;
		}
	}
	else
	{
		if (auto const name = nameOfLabel(_label); name != emptyName())
			return name;

		m_definedLabels.emplace_back(_label);
		m_names.emplace_back(m_definedLabels.size() - 1, false);
		return m_index++;
	}
}

YulName YulNameRepository::deriveName(YulName const _name)
{
	m_names.emplace_back(_name, true);
	return m_index++;
}

YulName YulNameRepository::addGhost()
{
	return defineName(fmt::format(FMT_COMPILE("GHOST[{}]"), m_nGhosts++));
}

bool YulNameRepository::isType(YulName const _name) const {
	return _name >= m_indexBoundaries->beginTypes && _name < m_indexBoundaries->endTypes;
}

bool YulNameRepository::isEvmDialect() const { return dynamic_cast<EVMDialect const*>(&m_dialect) != nullptr; }

void YulNameRepository::generateLabels(Block const& _ast, std::set<std::string> const& _illegal)
{
	auto const names = NameCollector(_ast).names();

	std::set<std::string> used (m_definedLabels.begin(), m_definedLabels.begin() + static_cast<std::ptrdiff_t>(m_indexBoundaries->endBuiltins));
	for (auto const name: names)
		if (!isDerivedName(name) || isVerbatimFunction(name))
			used.emplace(labelOf(name));
	std::vector<std::tuple<std::string, YulName>> generated;

	auto namesIt = names.begin();
	for (size_t name = m_indexBoundaries->endBuiltins; name < m_names.size(); ++name)
	{
		if (namesIt != names.end() && name == *namesIt)
		{
			if (isDerivedName(name) && !isVerbatimFunction(name))
			{
				std::string const baseLabel(baseLabelOf(name));
				std::string label (baseLabel);
				size_t bump = 1;
				while (used.count(label) > 0 || _illegal.count(label) > 0)
				{
					label = fmt::format(FMT_COMPILE("{}_{}"), baseLabel, bump++);
				}
				if (auto const existingDefinedName = nameOfLabel(label); existingDefinedName != emptyName() || name == emptyName())
					m_names[name] = m_names[existingDefinedName];
				else
					generated.emplace_back(label, name);
				used.insert(label);
			}
			++namesIt;
		}
	}

	for (auto const& [label, name] : generated)
	{
		m_definedLabels.emplace_back(label);
		std::get<0>(m_names[name]) = m_definedLabels.size() - 1;
		std::get<1>(m_names[name]) = false;
	}
}

YulNameRepository::YulNameRepository(YulNameRepository const& _rhs):
	m_dialect(_rhs.m_dialect),
	m_dialectTypes(_rhs.m_dialectTypes),
	m_builtinFunctions(_rhs.m_builtinFunctions),
	m_predefinedBuiltinFunctions(_rhs.m_predefinedBuiltinFunctions),
	m_index(_rhs.m_index),
	m_nGhosts(_rhs.m_nGhosts),
	m_definedLabels(_rhs.m_definedLabels),
	m_names(_rhs.m_names),
	m_verbatimNames(_rhs.m_verbatimNames),
	m_predefined(_rhs.m_predefined),
	m_indexBoundaries(std::make_unique<IndexBoundaries>(*_rhs.m_indexBoundaries))
{}

YulNameRepository::~YulNameRepository() = default;

}
