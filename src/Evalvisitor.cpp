#include "Evalvisitor.h"
#include <stdexcept>

std::any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx) {
    for (auto stmt : ctx->stmt()) {
        visit(stmt);
        if (returnFlag) break;
    }
    return nullptr;
}

std::any EvalVisitor::visitFuncdef(Python3Parser::FuncdefContext *ctx) {
    std::string funcName = ctx->NAME()->getText();
    FunctionDef funcDef;

    if (ctx->parameters()->typedargslist()) {
        auto paramInfo = std::any_cast<std::pair<std::vector<std::string>, std::vector<Value>>>(
            visit(ctx->parameters()->typedargslist()));
        funcDef.params = paramInfo.first;
        funcDef.defaults = paramInfo.second;
    }

    funcDef.body = ctx->suite();
    functions[funcName] = funcDef;

    return nullptr;
}

std::any EvalVisitor::visitParameters(Python3Parser::ParametersContext *ctx) {
    return nullptr;
}

std::any EvalVisitor::visitTypedargslist(Python3Parser::TypedargslistContext *ctx) {
    std::vector<std::string> params;
    std::vector<Value> defaults;

    auto tfpdefs = ctx->tfpdef();
    auto tests = ctx->test();

    size_t defaultStart = tfpdefs.size() - tests.size();

    for (size_t i = 0; i < tfpdefs.size(); i++) {
        params.push_back(tfpdefs[i]->NAME()->getText());
        if (i >= defaultStart) {
            defaults.push_back(std::any_cast<Value>(visit(tests[i - defaultStart])));
        }
    }

    return std::make_pair(params, defaults);
}

std::any EvalVisitor::visitStmt(Python3Parser::StmtContext *ctx) {
    if (ctx->simple_stmt()) {
        return visit(ctx->simple_stmt());
    } else {
        return visit(ctx->compound_stmt());
    }
}

std::any EvalVisitor::visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) {
    return visit(ctx->small_stmt());
}

std::any EvalVisitor::visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) {
    if (ctx->expr_stmt()) {
        return visit(ctx->expr_stmt());
    } else if (ctx->flow_stmt()) {
        return visit(ctx->flow_stmt());
    }
    return nullptr;
}

std::any EvalVisitor::visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) {
    auto testlists = ctx->testlist();

    if (testlists.size() == 1) {
        // Just an expression
        return visit(testlists[0]);
    }

    if (ctx->augassign()) {
        // Augmented assignment
        // Get variable name from left side
        auto leftTests = testlists[0]->test();
        if (leftTests.size() == 1) {
            // Get the variable name
            std::string varName = leftTests[0]->getText();
            Value current = getVariable(varName);

            auto rightList = std::any_cast<std::vector<Value>>(visit(testlists[1]));
            Value right = rightList[0];

            std::string op = ctx->augassign()->getText();
            Value result;

            if (op == "+=") {
                result = performAdd(current, right);
            } else if (op == "-=") {
                result = performSub(current, right);
            } else if (op == "*=") {
                result = performMul(current, right);
            } else if (op == "/=") {
                result = performDiv(current, right);
            } else if (op == "//=") {
                result = performFloorDiv(current, right);
            } else if (op == "%=") {
                result = performMod(current, right);
            }

            setVariable(varName, result);
        }
        return nullptr;
    }

    // Regular assignment or chained assignment
    auto rightList = std::any_cast<std::vector<Value>>(visit(testlists.back()));

    for (int i = testlists.size() - 2; i >= 0; i--) {
        auto leftTests = testlists[i]->test();

        // Assign values
        for (size_t j = 0; j < leftTests.size() && j < rightList.size(); j++) {
            std::string varName = leftTests[j]->getText();
            setVariable(varName, rightList[j]);
        }
    }

    return nullptr;
}

std::any EvalVisitor::visitAugassign(Python3Parser::AugassignContext *ctx) {
    return nullptr;
}

std::any EvalVisitor::visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) {
    if (ctx->break_stmt()) {
        return visit(ctx->break_stmt());
    } else if (ctx->continue_stmt()) {
        return visit(ctx->continue_stmt());
    } else if (ctx->return_stmt()) {
        return visit(ctx->return_stmt());
    }
    return nullptr;
}

std::any EvalVisitor::visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) {
    breakFlag = true;
    return nullptr;
}

std::any EvalVisitor::visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) {
    continueFlag = true;
    return nullptr;
}

std::any EvalVisitor::visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) {
    returnFlag = true;
    if (ctx->testlist()) {
        auto values = std::any_cast<std::vector<Value>>(visit(ctx->testlist()));
        if (values.size() == 1) {
            returnValue = values[0];
        } else {
            returnValue = Value::Tuple(values);
        }
    } else {
        returnValue = Value::None();
    }
    return nullptr;
}

std::any EvalVisitor::visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) {
    if (ctx->if_stmt()) {
        return visit(ctx->if_stmt());
    } else if (ctx->while_stmt()) {
        return visit(ctx->while_stmt());
    } else if (ctx->funcdef()) {
        return visit(ctx->funcdef());
    }
    return nullptr;
}

std::any EvalVisitor::visitIf_stmt(Python3Parser::If_stmtContext *ctx) {
    auto tests = ctx->test();
    auto suites = ctx->suite();

    for (size_t i = 0; i < tests.size(); i++) {
        Value condition = std::any_cast<Value>(visit(tests[i]));
        if (condition.toBool()) {
            visit(suites[i]);
            return nullptr;
        }
    }

    // else clause
    if (suites.size() > tests.size()) {
        visit(suites.back());
    }

    return nullptr;
}

std::any EvalVisitor::visitWhile_stmt(Python3Parser::While_stmtContext *ctx) {
    while (true) {
        Value condition = std::any_cast<Value>(visit(ctx->test()));
        if (!condition.toBool()) break;

        visit(ctx->suite());

        if (breakFlag) {
            breakFlag = false;
            break;
        }
        if (continueFlag) {
            continueFlag = false;
            continue;
        }
        if (returnFlag) {
            break;
        }
    }
    return nullptr;
}

std::any EvalVisitor::visitSuite(Python3Parser::SuiteContext *ctx) {
    if (ctx->simple_stmt()) {
        return visit(ctx->simple_stmt());
    }

    for (auto stmt : ctx->stmt()) {
        visit(stmt);
        if (breakFlag || continueFlag || returnFlag) {
            break;
        }
    }
    return nullptr;
}

std::any EvalVisitor::visitTest(Python3Parser::TestContext *ctx) {
    return visit(ctx->or_test());
}

std::any EvalVisitor::visitOr_test(Python3Parser::Or_testContext *ctx) {
    auto andTests = ctx->and_test();
    Value result = std::any_cast<Value>(visit(andTests[0]));

    for (size_t i = 1; i < andTests.size(); i++) {
        if (result.toBool()) {
            return result;  // Short-circuit
        }
        result = std::any_cast<Value>(visit(andTests[i]));
    }

    return result;
}

std::any EvalVisitor::visitAnd_test(Python3Parser::And_testContext *ctx) {
    auto notTests = ctx->not_test();
    Value result = std::any_cast<Value>(visit(notTests[0]));

    for (size_t i = 1; i < notTests.size(); i++) {
        if (!result.toBool()) {
            return result;  // Short-circuit
        }
        result = std::any_cast<Value>(visit(notTests[i]));
    }

    return result;
}

std::any EvalVisitor::visitNot_test(Python3Parser::Not_testContext *ctx) {
    if (ctx->NOT()) {
        Value val = std::any_cast<Value>(visit(ctx->not_test()));
        return Value::Bool(!val.toBool());
    }
    return visit(ctx->comparison());
}

std::any EvalVisitor::visitComparison(Python3Parser::ComparisonContext *ctx) {
    auto exprs = ctx->arith_expr();
    if (exprs.size() == 1) {
        return visit(exprs[0]);
    }

    // Handle chained comparisons
    std::vector<Value> values;
    for (auto expr : exprs) {
        values.push_back(std::any_cast<Value>(visit(expr)));
    }

    auto ops = ctx->comp_op();
    for (size_t i = 0; i < ops.size(); i++) {
        std::string op = ops[i]->getText();
        Value cmpResult = performCompare(values[i], values[i + 1], op);
        if (!cmpResult.toBool()) {
            return Value::Bool(false);
        }
    }

    return Value::Bool(true);
}

std::any EvalVisitor::visitComp_op(Python3Parser::Comp_opContext *ctx) {
    return nullptr;
}

std::any EvalVisitor::visitArith_expr(Python3Parser::Arith_exprContext *ctx) {
    auto terms = ctx->term();
    Value result = std::any_cast<Value>(visit(terms[0]));

    auto ops = ctx->addorsub_op();
    for (size_t i = 0; i < ops.size(); i++) {
        Value right = std::any_cast<Value>(visit(terms[i + 1]));
        std::string op = ops[i]->getText();

        if (op == "+") {
            result = performAdd(result, right);
        } else {
            result = performSub(result, right);
        }
    }

    return result;
}

std::any EvalVisitor::visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx) {
    return nullptr;
}

std::any EvalVisitor::visitTerm(Python3Parser::TermContext *ctx) {
    auto factors = ctx->factor();
    Value result = std::any_cast<Value>(visit(factors[0]));

    auto ops = ctx->muldivmod_op();
    for (size_t i = 0; i < ops.size(); i++) {
        Value right = std::any_cast<Value>(visit(factors[i + 1]));
        std::string op = ops[i]->getText();

        if (op == "*") {
            result = performMul(result, right);
        } else if (op == "/") {
            result = performDiv(result, right);
        } else if (op == "//") {
            result = performFloorDiv(result, right);
        } else if (op == "%") {
            result = performMod(result, right);
        }
    }

    return result;
}

std::any EvalVisitor::visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx) {
    return nullptr;
}

std::any EvalVisitor::visitFactor(Python3Parser::FactorContext *ctx) {
    if (ctx->ADD() || ctx->MINUS()) {
        Value val = std::any_cast<Value>(visit(ctx->factor()));
        if (ctx->MINUS()) {
            if (val.type == ValueType::INT) {
                return Value::Int(-val.intVal);
            } else if (val.type == ValueType::FLOAT) {
                return Value::Float(-val.floatVal);
            }
        }
        return val;
    }
    return visit(ctx->atom_expr());
}

std::any EvalVisitor::visitAtom_expr(Python3Parser::Atom_exprContext *ctx) {
    Value result = std::any_cast<Value>(visit(ctx->atom()));

    if (ctx->trailer()) {
        auto trailerResult = visit(ctx->trailer());

        // Check if it's a function call
        if (trailerResult.type() == typeid(std::pair<std::vector<Value>, std::map<std::string, Value>>)) {
            auto args = std::any_cast<std::pair<std::vector<Value>, std::map<std::string, Value>>>(trailerResult);

            if (result.type == ValueType::STRING) {
                // Function name
                std::string funcName = result.strVal;
                result = callFunction(funcName, args.first, args.second);
            }
        }
    }

    return result;
}

std::any EvalVisitor::visitTrailer(Python3Parser::TrailerContext *ctx) {
    if (ctx->arglist()) {
        return visit(ctx->arglist());
    } else if (ctx->OPEN_PAREN()) {
        // Empty argument list
        return std::make_pair(std::vector<Value>(), std::map<std::string, Value>());
    }
    return nullptr;
}

std::any EvalVisitor::visitAtom(Python3Parser::AtomContext *ctx) {
    if (ctx->NONE()) {
        return Value::None();
    }

    if (ctx->TRUE()) {
        return Value::Bool(true);
    }

    if (ctx->FALSE()) {
        return Value::Bool(false);
    }

    if (ctx->NAME()) {
        std::string name = ctx->NAME()->getText();

        // Check if it's a variable or function name
        if (functions.find(name) != functions.end() ||
            name == "print" || name == "int" || name == "float" ||
            name == "str" || name == "bool") {
            return Value::String(name);  // Return function name as string
        }

        return getVariable(name);
    }

    if (ctx->NUMBER()) {
        std::string numStr = ctx->NUMBER()->getText();
        if (numStr.find('.') != std::string::npos) {
            return Value::Float(std::stod(numStr));
        } else {
            return Value::Int(BigInteger(numStr));
        }
    }

    if (!ctx->STRING().empty()) {
        auto strings = ctx->STRING();
        std::string result;
        for (auto str : strings) {
            std::string s = str->getText();
            // Remove quotes
            s = s.substr(1, s.length() - 2);
            result += s;
        }
        return Value::String(result);
    }

    if (ctx->format_string()) {
        return visit(ctx->format_string());
    }

    if (ctx->test()) {
        return visit(ctx->test());
    }

    return Value::None();
}

std::any EvalVisitor::visitFormat_string(Python3Parser::Format_stringContext *ctx) {
    std::string result;

    for (size_t i = 0; i < ctx->children.size(); i++) {
        auto child = ctx->children[i];

        // Check if it's a FORMAT_STRING_LITERAL
        if (auto terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
            if (terminal->getSymbol()->getType() == Python3Parser::FORMAT_STRING_LITERAL) {
                std::string text = terminal->getText();
                // Replace {{ with { and }} with }
                std::string processed;
                for (size_t j = 0; j < text.length(); j++) {
                    if (j + 1 < text.length() && text[j] == '{' && text[j+1] == '{') {
                        processed += '{';
                        j++;
                    } else if (j + 1 < text.length() && text[j] == '}' && text[j+1] == '}') {
                        processed += '}';
                        j++;
                    } else {
                        processed += text[j];
                    }
                }
                result += processed;
            }
        } else if (auto testlistCtx = dynamic_cast<Python3Parser::TestlistContext*>(child)) {
            // This is an expression inside {}
            auto values = std::any_cast<std::vector<Value>>(visit(testlistCtx));
            for (size_t j = 0; j < values.size(); j++) {
                if (j > 0) result += ", ";

                // For format strings, bool should be printed as True/False
                if (values[j].type == ValueType::BOOL) {
                    result += values[j].boolVal ? "True" : "False";
                } else if (values[j].type == ValueType::STRING) {
                    result += values[j].strVal;
                } else {
                    result += values[j].toString();
                }
            }
        }
    }

    return Value::String(result);
}

std::any EvalVisitor::visitTestlist(Python3Parser::TestlistContext *ctx) {
    std::vector<Value> values;
    for (auto test : ctx->test()) {
        values.push_back(std::any_cast<Value>(visit(test)));
    }
    return values;
}

std::any EvalVisitor::visitArglist(Python3Parser::ArglistContext *ctx) {
    std::vector<Value> posArgs;
    std::map<std::string, Value> kwArgs;

    for (auto arg : ctx->argument()) {
        auto argResult = visit(arg);

        if (argResult.type() == typeid(std::pair<std::string, Value>)) {
            auto kwArg = std::any_cast<std::pair<std::string, Value>>(argResult);
            kwArgs[kwArg.first] = kwArg.second;
        } else {
            posArgs.push_back(std::any_cast<Value>(argResult));
        }
    }

    return std::make_pair(posArgs, kwArgs);
}

std::any EvalVisitor::visitArgument(Python3Parser::ArgumentContext *ctx) {
    auto tests = ctx->test();

    if (tests.size() == 2) {
        // Keyword argument
        std::string name = tests[0]->getText();  // This is simplified
        Value value = std::any_cast<Value>(visit(tests[1]));
        return std::make_pair(name, value);
    } else {
        // Positional argument
        return visit(tests[0]);
    }
}

// Helper functions

Value EvalVisitor::performAdd(const Value& a, const Value& b) {
    if (a.type == ValueType::INT && b.type == ValueType::INT) {
        return Value::Int(a.intVal + b.intVal);
    } else if (a.type == ValueType::FLOAT || b.type == ValueType::FLOAT) {
        double aVal = (a.type == ValueType::FLOAT) ? a.floatVal : a.intVal.toDouble();
        double bVal = (b.type == ValueType::FLOAT) ? b.floatVal : b.intVal.toDouble();
        return Value::Float(aVal + bVal);
    } else if (a.type == ValueType::STRING && b.type == ValueType::STRING) {
        return Value::String(a.strVal + b.strVal);
    } else if (a.type == ValueType::STRING && b.type == ValueType::INT) {
        std::string result;
        BigInteger count = b.intVal;
        BigInteger zero(0);
        while (count > zero) {
            result += a.strVal;
            count = count - BigInteger(1);
        }
        return Value::String(result);
    }
    return Value::None();
}

Value EvalVisitor::performSub(const Value& a, const Value& b) {
    if (a.type == ValueType::INT && b.type == ValueType::INT) {
        return Value::Int(a.intVal - b.intVal);
    } else if (a.type == ValueType::FLOAT || b.type == ValueType::FLOAT) {
        double aVal = (a.type == ValueType::FLOAT) ? a.floatVal : a.intVal.toDouble();
        double bVal = (b.type == ValueType::FLOAT) ? b.floatVal : b.intVal.toDouble();
        return Value::Float(aVal - bVal);
    }
    return Value::None();
}

Value EvalVisitor::performMul(const Value& a, const Value& b) {
    if (a.type == ValueType::INT && b.type == ValueType::INT) {
        return Value::Int(a.intVal * b.intVal);
    } else if (a.type == ValueType::FLOAT || b.type == ValueType::FLOAT) {
        double aVal = (a.type == ValueType::FLOAT) ? a.floatVal : a.intVal.toDouble();
        double bVal = (b.type == ValueType::FLOAT) ? b.floatVal : b.intVal.toDouble();
        return Value::Float(aVal * bVal);
    } else if (a.type == ValueType::STRING && b.type == ValueType::INT) {
        std::string result;
        BigInteger count = b.intVal;
        BigInteger zero(0);
        while (count > zero) {
            result += a.strVal;
            count = count - BigInteger(1);
        }
        return Value::String(result);
    } else if (a.type == ValueType::INT && b.type == ValueType::STRING) {
        return performMul(b, a);
    }
    return Value::None();
}

Value EvalVisitor::performDiv(const Value& a, const Value& b) {
    double aVal = (a.type == ValueType::FLOAT) ? a.floatVal : a.intVal.toDouble();
    double bVal = (b.type == ValueType::FLOAT) ? b.floatVal : b.intVal.toDouble();
    return Value::Float(aVal / bVal);
}

Value EvalVisitor::performFloorDiv(const Value& a, const Value& b) {
    if (a.type == ValueType::INT && b.type == ValueType::INT) {
        BigInteger result = a.intVal / b.intVal;
        // Floor division: if signs differ and there's a remainder, subtract 1
        BigInteger remainder = a.intVal % b.intVal;
        if (!remainder.isZero() && (a.intVal.isNegative() != b.intVal.isNegative())) {
            result = result - BigInteger(1);
        }
        return Value::Int(result);
    } else {
        double aVal = (a.type == ValueType::FLOAT) ? a.floatVal : a.intVal.toDouble();
        double bVal = (b.type == ValueType::FLOAT) ? b.floatVal : b.intVal.toDouble();
        return Value::Float(std::floor(aVal / bVal));
    }
}

Value EvalVisitor::performMod(const Value& a, const Value& b) {
    if (a.type == ValueType::INT && b.type == ValueType::INT) {
        // a % b = a - (a // b) * b
        BigInteger floorDiv = a.intVal / b.intVal;
        BigInteger remainder = a.intVal % b.intVal;
        if (!remainder.isZero() && (a.intVal.isNegative() != b.intVal.isNegative())) {
            floorDiv = floorDiv - BigInteger(1);
        }
        return Value::Int(a.intVal - floorDiv * b.intVal);
    } else {
        double aVal = (a.type == ValueType::FLOAT) ? a.floatVal : a.intVal.toDouble();
        double bVal = (b.type == ValueType::FLOAT) ? b.floatVal : b.intVal.toDouble();
        return Value::Float(aVal - std::floor(aVal / bVal) * bVal);
    }
}

Value EvalVisitor::performCompare(const Value& a, const Value& b, const std::string& op) {
    bool result = false;

    if (op == "==") {
        if (a.type == b.type) {
            if (a.type == ValueType::INT) result = (a.intVal == b.intVal);
            else if (a.type == ValueType::FLOAT) result = (a.floatVal == b.floatVal);
            else if (a.type == ValueType::STRING) result = (a.strVal == b.strVal);
            else if (a.type == ValueType::BOOL) result = (a.boolVal == b.boolVal);
            else if (a.type == ValueType::NONE) result = true;
        } else if ((a.type == ValueType::INT || a.type == ValueType::FLOAT) &&
                   (b.type == ValueType::INT || b.type == ValueType::FLOAT)) {
            double aVal = (a.type == ValueType::FLOAT) ? a.floatVal : a.intVal.toDouble();
            double bVal = (b.type == ValueType::FLOAT) ? b.floatVal : b.intVal.toDouble();
            result = (aVal == bVal);
        }
    } else if (op == "!=") {
        return Value::Bool(!performCompare(a, b, "==").boolVal);
    } else if (op == "<") {
        if (a.type == ValueType::INT && b.type == ValueType::INT) {
            result = (a.intVal < b.intVal);
        } else if ((a.type == ValueType::INT || a.type == ValueType::FLOAT) &&
                   (b.type == ValueType::INT || b.type == ValueType::FLOAT)) {
            double aVal = (a.type == ValueType::FLOAT) ? a.floatVal : a.intVal.toDouble();
            double bVal = (b.type == ValueType::FLOAT) ? b.floatVal : b.intVal.toDouble();
            result = (aVal < bVal);
        } else if (a.type == ValueType::STRING && b.type == ValueType::STRING) {
            result = (a.strVal < b.strVal);
        }
    } else if (op == ">") {
        return performCompare(b, a, "<");
    } else if (op == "<=") {
        return Value::Bool(!performCompare(b, a, "<").boolVal);
    } else if (op == ">=") {
        return Value::Bool(!performCompare(a, b, "<").boolVal);
    }

    return Value::Bool(result);
}

Value EvalVisitor::convertToInt(const Value& v) {
    if (v.type == ValueType::INT) return v;
    if (v.type == ValueType::FLOAT) return Value::Int(BigInteger((long long)v.floatVal));
    if (v.type == ValueType::BOOL) return Value::Int(BigInteger(v.boolVal ? 1 : 0));
    if (v.type == ValueType::STRING) {
        return Value::Int(BigInteger(v.strVal));
    }
    return Value::Int(BigInteger(0));
}

Value EvalVisitor::convertToFloat(const Value& v) {
    if (v.type == ValueType::FLOAT) return v;
    if (v.type == ValueType::INT) return Value::Float(v.intVal.toDouble());
    if (v.type == ValueType::BOOL) return Value::Float(v.boolVal ? 1.0 : 0.0);
    if (v.type == ValueType::STRING) {
        return Value::Float(std::stod(v.strVal));
    }
    return Value::Float(0.0);
}

Value EvalVisitor::convertToStr(const Value& v) {
    if (v.type == ValueType::STRING) return v;
    return Value::String(v.toString());
}

Value EvalVisitor::convertToBool(const Value& v) {
    return Value::Bool(v.toBool());
}

void EvalVisitor::printValue(const Value& v) {
    if (v.type == ValueType::STRING) {
        std::cout << v.strVal;
    } else {
        std::cout << v.toString();
    }
}

Value EvalVisitor::callBuiltinFunction(const std::string& name, const std::vector<Value>& args) {
    if (name == "print") {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) std::cout << " ";
            printValue(args[i]);
        }
        std::cout << std::endl;
        return Value::None();
    } else if (name == "int") {
        if (!args.empty()) {
            return convertToInt(args[0]);
        }
    } else if (name == "float") {
        if (!args.empty()) {
            return convertToFloat(args[0]);
        }
    } else if (name == "str") {
        if (!args.empty()) {
            return convertToStr(args[0]);
        }
    } else if (name == "bool") {
        if (!args.empty()) {
            return convertToBool(args[0]);
        }
    }
    return Value::None();
}

Value EvalVisitor::callFunction(const std::string& name, const std::vector<Value>& posArgs,
                                const std::map<std::string, Value>& kwArgs) {
    // Check for built-in functions
    if (name == "print" || name == "int" || name == "float" || name == "str" || name == "bool") {
        return callBuiltinFunction(name, posArgs);
    }

    // User-defined function
    if (functions.find(name) == functions.end()) {
        return Value::None();
    }

    FunctionDef& func = functions[name];

    // Create new scope
    scopes.push_back(std::map<std::string, Value>());

    // Bind parameters
    size_t numParams = func.params.size();
    size_t numDefaults = func.defaults.size();
    size_t firstDefaultIdx = numParams - numDefaults;

    // Bind positional arguments
    for (size_t i = 0; i < posArgs.size() && i < numParams; i++) {
        scopes.back()[func.params[i]] = posArgs[i];
    }

    // Bind keyword arguments
    for (const auto& kw : kwArgs) {
        scopes.back()[kw.first] = kw.second;
    }

    // Bind default values for missing parameters
    for (size_t i = 0; i < numParams; i++) {
        if (scopes.back().find(func.params[i]) == scopes.back().end()) {
            if (i >= firstDefaultIdx) {
                scopes.back()[func.params[i]] = func.defaults[i - firstDefaultIdx];
            }
        }
    }

    // Execute function body
    returnFlag = false;
    returnValue = Value::None();
    visit(func.body);

    Value result = returnValue;
    returnFlag = false;
    returnValue = Value::None();

    // Pop scope
    scopes.pop_back();

    return result;
}
