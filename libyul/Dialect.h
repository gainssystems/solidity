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
/**
 * Yul dialect.
 */

#pragma once

#include <libyul/ControlFlowSideEffects.h>
#include <libyul/SideEffects.h>
#include <libyul/YulString.h>
#include <libyul/YulName.h>

#include <vector>
#include <set>
#include <optional>

namespace solidity::yul
{

class YulString;
enum class LiteralKind;
class LiteralValue;
struct Literal;

struct BuiltinFunction
{
	virtual ~BuiltinFunction() = default;

	std::string name;
	std::vector<std::string> parameters;
	std::vector<std::string> returns;
	SideEffects sideEffects;
	ControlFlowSideEffects controlFlowSideEffects;
	/// If true, this is the msize instruction or might contain it.
	bool isMSize = false;
	/// Must be empty or the same length as the arguments.
	/// If set at index i, the i'th argument has to be a literal which means it can't be moved to variables.
	std::vector<std::optional<LiteralKind>> literalArguments{};
	std::optional<LiteralKind> literalArgument(size_t i) const
	{
		return literalArguments.empty() ? std::nullopt : literalArguments.at(i);
	}
};

struct Dialect
{
	/// Noncopyable.
	Dialect(Dialect const&) = delete;
	Dialect& operator=(Dialect const&) = delete;

	/// Default type, can be omitted.
	std::string defaultType;
	/// Type used for the literals "true" and "false".
	std::string boolType;
	std::set<std::string> types = {{}};

	/// @returns the builtin function of the given name or a nullptr if it is not a builtin function.
	virtual BuiltinFunction const* builtin(std::string_view /*_name*/) const { return nullptr; }

	/// @returns true if the identifier is reserved. This includes the builtins too.
	virtual bool reservedIdentifier(std::string_view _name) const { return builtin(_name) != nullptr; }

	virtual BuiltinFunction const* discardFunction(std::string_view /* _type */) const { return nullptr; }
	virtual BuiltinFunction const* equalityFunction(std::string_view /* _type */) const { return nullptr; }
	virtual BuiltinFunction const* booleanNegationFunction() const { return nullptr; }

	virtual BuiltinFunction const* memoryStoreFunction(std::string_view /* _type */) const { return nullptr; }
	virtual BuiltinFunction const* memoryLoadFunction(std::string_view /* _type */) const { return nullptr; }
	virtual BuiltinFunction const* storageStoreFunction(std::string_view /* _type */) const { return nullptr; }
	virtual BuiltinFunction const* storageLoadFunction(std::string_view /* _type */) const { return nullptr; }

	virtual std::string_view hashFunction(std::string_view /* _type */ ) const { return {}; }

	virtual std::set<std::string> builtinNames() const { return {}; }

	/// Check whether the given type is legal for the given literal value.
	/// Should only be called if the type exists in the dialect at all.
	virtual bool validTypeForLiteral(LiteralKind _kind, LiteralValue const& _value, std::string_view _type) const;
	virtual bool validTypeForLiteral(LiteralKind _kind, LiteralValue const& _value, Type _type, YulNameRepository const& _nameRepository) const;

	virtual Literal zeroLiteralForType(YulName _type, YulNameRepository const& _nameRepository) const;

	Dialect() = default;
	virtual ~Dialect() = default;

	/// Old "yul" dialect. This is only used for testing.
	static Dialect const& yulDeprecated();
};

}
