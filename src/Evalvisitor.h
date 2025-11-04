#pragma once
#ifndef PYTHON_INTERPRETER_EVALVISITOR_H
#define PYTHON_INTERPRETER_EVALVISITOR_H

#include "Python3ParserBaseVisitor.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>

// BigInteger class for arbitrary precision arithmetic
class BigInteger {
private:
    std::vector<int> digits;  // Store digits in reverse order (least significant first)
    bool negative;

    void removeLeadingZeros() {
        while (digits.size() > 1 && digits.back() == 0) {
            digits.pop_back();
        }
        if (digits.size() == 1 && digits[0] == 0) {
            negative = false;
        }
    }

public:
    BigInteger() : digits(1, 0), negative(false) {}

    BigInteger(long long num) {
        if (num == 0) {
            digits.push_back(0);
            negative = false;
        } else {
            negative = num < 0;
            num = std::abs(num);
            while (num > 0) {
                digits.push_back(num % 10);
                num /= 10;
            }
        }
    }

    BigInteger(const std::string& str) {
        if (str.empty() || str == "0") {
            digits.push_back(0);
            negative = false;
            return;
        }

        size_t start = 0;
        negative = (str[0] == '-');
        if (str[0] == '-' || str[0] == '+') start = 1;

        for (int i = str.length() - 1; i >= (int)start; i--) {
            digits.push_back(str[i] - '0');
        }
        removeLeadingZeros();
    }

    std::string toString() const {
        if (digits.empty() || (digits.size() == 1 && digits[0] == 0)) {
            return "0";
        }
        std::string result;
        if (negative) result += '-';
        for (int i = digits.size() - 1; i >= 0; i--) {
            result += char('0' + digits[i]);
        }
        return result;
    }

    bool isZero() const {
        return digits.size() == 1 && digits[0] == 0;
    }

    bool isNegative() const {
        return negative && !isZero();
    }

    BigInteger abs() const {
        BigInteger result = *this;
        result.negative = false;
        return result;
    }

    BigInteger operator-() const {
        BigInteger result = *this;
        if (!isZero()) {
            result.negative = !result.negative;
        }
        return result;
    }

    bool absLess(const BigInteger& other) const {
        if (digits.size() != other.digits.size()) {
            return digits.size() < other.digits.size();
        }
        for (int i = digits.size() - 1; i >= 0; i--) {
            if (digits[i] != other.digits[i]) {
                return digits[i] < other.digits[i];
            }
        }
        return false;
    }

    bool operator<(const BigInteger& other) const {
        if (negative != other.negative) {
            return negative;
        }
        if (negative) {
            return other.absLess(*this);
        }
        return absLess(other);
    }

    bool operator>(const BigInteger& other) const {
        return other < *this;
    }

    bool operator<=(const BigInteger& other) const {
        return !(other < *this);
    }

    bool operator>=(const BigInteger& other) const {
        return !(*this < other);
    }

    bool operator==(const BigInteger& other) const {
        return negative == other.negative && digits == other.digits;
    }

    bool operator!=(const BigInteger& other) const {
        return !(*this == other);
    }

    BigInteger operator+(const BigInteger& other) const {
        if (negative == other.negative) {
            BigInteger result;
            result.negative = negative;
            result.digits.clear();

            int carry = 0;
            size_t maxSize = std::max(digits.size(), other.digits.size());
            for (size_t i = 0; i < maxSize || carry; i++) {
                int sum = carry;
                if (i < digits.size()) sum += digits[i];
                if (i < other.digits.size()) sum += other.digits[i];
                result.digits.push_back(sum % 10);
                carry = sum / 10;
            }
            return result;
        } else {
            if (negative) {
                return other - this->abs();
            } else {
                return *this - other.abs();
            }
        }
    }

    BigInteger operator-(const BigInteger& other) const {
        if (negative != other.negative) {
            BigInteger result = this->abs() + other.abs();
            result.negative = negative;
            return result;
        }

        if (this->abs() < other.abs()) {
            BigInteger result = other.abs() - this->abs();
            result.negative = !negative;
            return result;
        }

        BigInteger result;
        result.negative = negative;
        result.digits.clear();

        int borrow = 0;
        for (size_t i = 0; i < digits.size(); i++) {
            int diff = digits[i] - borrow;
            if (i < other.digits.size()) diff -= other.digits[i];

            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result.digits.push_back(diff);
        }

        result.removeLeadingZeros();
        return result;
    }

    BigInteger operator*(const BigInteger& other) const {
        BigInteger result;
        result.digits.assign(digits.size() + other.digits.size(), 0);

        for (size_t i = 0; i < digits.size(); i++) {
            int carry = 0;
            for (size_t j = 0; j < other.digits.size() || carry; j++) {
                long long cur = result.digits[i + j] +
                               digits[i] * 1LL * (j < other.digits.size() ? other.digits[j] : 0) + carry;
                result.digits[i + j] = cur % 10;
                carry = cur / 10;
            }
        }

        result.removeLeadingZeros();
        result.negative = (negative != other.negative) && !result.isZero();
        return result;
    }

    BigInteger operator/(const BigInteger& other) const {
        if (other.isZero()) {
            throw std::runtime_error("Division by zero");
        }

        BigInteger dividend = this->abs();
        BigInteger divisor = other.abs();

        if (dividend < divisor) {
            return BigInteger(0);
        }

        BigInteger result;
        result.digits.clear();
        BigInteger current;
        current.digits.clear();

        for (int i = dividend.digits.size() - 1; i >= 0; i--) {
            current.digits.insert(current.digits.begin(), dividend.digits[i]);
            current.removeLeadingZeros();

            int quotient = 0;
            while (!(current < divisor)) {
                current = current - divisor;
                quotient++;
            }
            result.digits.insert(result.digits.begin(), quotient);
        }

        result.removeLeadingZeros();
        result.negative = (negative != other.negative) && !result.isZero();
        return result;
    }

    BigInteger operator%(const BigInteger& other) const {
        BigInteger quotient = *this / other;
        BigInteger result = *this - quotient * other;
        return result;
    }

    double toDouble() const {
        double result = 0;
        double base = 1;
        for (size_t i = 0; i < digits.size(); i++) {
            result += digits[i] * base;
            base *= 10;
        }
        return negative ? -result : result;
    }
};

enum class ValueType {
    NONE,
    BOOL,
    INT,
    FLOAT,
    STRING,
    TUPLE,
    FUNCTION
};

class Value {
public:
    ValueType type;
    bool boolVal;
    BigInteger intVal;
    double floatVal;
    std::string strVal;
    std::vector<Value> tupleVal;

    Value() : type(ValueType::NONE) {}

    static Value None() {
        return Value();
    }

    static Value Bool(bool b) {
        Value v;
        v.type = ValueType::BOOL;
        v.boolVal = b;
        return v;
    }

    static Value Int(const BigInteger& i) {
        Value v;
        v.type = ValueType::INT;
        v.intVal = i;
        return v;
    }

    static Value Float(double f) {
        Value v;
        v.type = ValueType::FLOAT;
        v.floatVal = f;
        return v;
    }

    static Value String(const std::string& s) {
        Value v;
        v.type = ValueType::STRING;
        v.strVal = s;
        return v;
    }

    static Value Tuple(const std::vector<Value>& t) {
        Value v;
        v.type = ValueType::TUPLE;
        v.tupleVal = t;
        return v;
    }

    std::string toString() const {
        switch (type) {
            case ValueType::NONE:
                return "None";
            case ValueType::BOOL:
                return boolVal ? "True" : "False";
            case ValueType::INT:
                return intVal.toString();
            case ValueType::FLOAT: {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(6) << floatVal;
                return oss.str();
            }
            case ValueType::STRING:
                return strVal;
            case ValueType::TUPLE: {
                if (tupleVal.empty()) return "()";
                std::string result = "(";
                for (size_t i = 0; i < tupleVal.size(); i++) {
                    if (i > 0) result += ", ";
                    result += tupleVal[i].toString();
                }
                if (tupleVal.size() == 1) result += ",";
                result += ")";
                return result;
            }
            default:
                return "";
        }
    }

    bool toBool() const {
        switch (type) {
            case ValueType::NONE:
                return false;
            case ValueType::BOOL:
                return boolVal;
            case ValueType::INT:
                return !intVal.isZero();
            case ValueType::FLOAT:
                return floatVal != 0.0;
            case ValueType::STRING:
                return !strVal.empty();
            case ValueType::TUPLE:
                return !tupleVal.empty();
            default:
                return false;
        }
    }
};

struct FunctionDef {
    std::vector<std::string> params;
    std::vector<Value> defaults;
    antlr4::ParserRuleContext* body;
};

class EvalVisitor : public Python3ParserBaseVisitor {
private:
    std::map<std::string, Value> globalVars;
    std::vector<std::map<std::string, Value>> scopes;
    std::map<std::string, FunctionDef> functions;

    bool breakFlag = false;
    bool continueFlag = false;
    bool returnFlag = false;
    Value returnValue;

    void setVariable(const std::string& name, const Value& value) {
        if (scopes.empty()) {
            globalVars[name] = value;
        } else {
            scopes.back()[name] = value;
        }
    }

    Value getVariable(const std::string& name) {
        for (int i = scopes.size() - 1; i >= 0; i--) {
            if (scopes[i].find(name) != scopes[i].end()) {
                return scopes[i][name];
            }
        }
        if (globalVars.find(name) != globalVars.end()) {
            return globalVars[name];
        }
        return Value::None();
    }

    Value performAdd(const Value& a, const Value& b);
    Value performSub(const Value& a, const Value& b);
    Value performMul(const Value& a, const Value& b);
    Value performDiv(const Value& a, const Value& b);
    Value performFloorDiv(const Value& a, const Value& b);
    Value performMod(const Value& a, const Value& b);
    Value performCompare(const Value& a, const Value& b, const std::string& op);
    Value convertToInt(const Value& v);
    Value convertToFloat(const Value& v);
    Value convertToStr(const Value& v);
    Value convertToBool(const Value& v);
    void printValue(const Value& v);
    Value callBuiltinFunction(const std::string& name, const std::vector<Value>& args);
    Value callFunction(const std::string& name, const std::vector<Value>& posArgs,
                      const std::map<std::string, Value>& kwArgs);

public:
    std::any visitFile_input(Python3Parser::File_inputContext *ctx) override;
    std::any visitFuncdef(Python3Parser::FuncdefContext *ctx) override;
    std::any visitParameters(Python3Parser::ParametersContext *ctx) override;
    std::any visitTypedargslist(Python3Parser::TypedargslistContext *ctx) override;
    std::any visitStmt(Python3Parser::StmtContext *ctx) override;
    std::any visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) override;
    std::any visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) override;
    std::any visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) override;
    std::any visitAugassign(Python3Parser::AugassignContext *ctx) override;
    std::any visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) override;
    std::any visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) override;
    std::any visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) override;
    std::any visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) override;
    std::any visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) override;
    std::any visitIf_stmt(Python3Parser::If_stmtContext *ctx) override;
    std::any visitWhile_stmt(Python3Parser::While_stmtContext *ctx) override;
    std::any visitSuite(Python3Parser::SuiteContext *ctx) override;
    std::any visitTest(Python3Parser::TestContext *ctx) override;
    std::any visitOr_test(Python3Parser::Or_testContext *ctx) override;
    std::any visitAnd_test(Python3Parser::And_testContext *ctx) override;
    std::any visitNot_test(Python3Parser::Not_testContext *ctx) override;
    std::any visitComparison(Python3Parser::ComparisonContext *ctx) override;
    std::any visitComp_op(Python3Parser::Comp_opContext *ctx) override;
    std::any visitArith_expr(Python3Parser::Arith_exprContext *ctx) override;
    std::any visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx) override;
    std::any visitTerm(Python3Parser::TermContext *ctx) override;
    std::any visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx) override;
    std::any visitFactor(Python3Parser::FactorContext *ctx) override;
    std::any visitAtom_expr(Python3Parser::Atom_exprContext *ctx) override;
    std::any visitTrailer(Python3Parser::TrailerContext *ctx) override;
    std::any visitAtom(Python3Parser::AtomContext *ctx) override;
    std::any visitFormat_string(Python3Parser::Format_stringContext *ctx) override;
    std::any visitTestlist(Python3Parser::TestlistContext *ctx) override;
    std::any visitArglist(Python3Parser::ArglistContext *ctx) override;
    std::any visitArgument(Python3Parser::ArgumentContext *ctx) override;
};

#endif//PYTHON_INTERPRETER_EVALVISITOR_H
